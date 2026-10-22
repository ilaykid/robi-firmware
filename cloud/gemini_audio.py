"""Robi: Hebrew speech recognition with Gemini (audio in, text out).

Gemini hears the whole utterance and writes what the child said, which copes with
children's speech and mixed Hebrew/English much better than plain Whisper.
If Gemini fails (quota, network), falls back to an OpenAI-style Whisper endpoint
(Groq) so Robi never goes deaf.

config (ASR section):
  RobiGeminiASR:
    type: gemini_audio
    api_key: <gemini key>
    model_name: gemini-3.5-flash-lite
    prompt: "<hint about the conversation>"
    fallback_base_url: https://api.groq.com/openai/v1/audio/transcriptions
    fallback_api_key: <groq key>
    fallback_model: whisper-large-v3
    rpm_cap: 6        # Gemini calls per minute for hearing (the rest is left for answers)
    rpd_cap: 220      # Gemini calls per day for hearing (free tier: 15/min, 500/day shared)
    output_dir: tmp/

The free tier is shared with the LLM (same key, same model), so hearing uses Gemini
only within its own budget and after a 429 rests for a while; Groq takes over meanwhile.
"""
import base64
import io
import time
import wave
from collections import deque
from datetime import datetime, timedelta, timezone
from typing import List, Optional, Tuple

import aiohttp

from config.logger import setup_logging
from core.providers.asr.base import ASRProviderBase
from core.providers.asr.dto.dto import InterfaceType

TAG = __name__
logger = setup_logging()

GEMINI_URL = "https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent"

INSTRUCTION = (
    "You are the ears of Robi, a small toy robot that talks with two Israeli children "
    "(ages 7 and 10) at home. Transcribe exactly what the speaker says in this recording. "
    "The speech is usually Hebrew, sometimes with English words (English practice). "
    "Write Hebrew in Hebrew letters and English words in English letters. "
    "Fix nothing and answer nothing: output only the words that were said. "
    "If there is no clear human speech (silence, noise, music, only the robot's own voice), "
    "output exactly: <empty>"
)


def _wav_bytes(pcm: bytes) -> bytes:
    if len(pcm) % 2:
        pcm = pcm[:-1]
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(16000)
        w.writeframes(pcm)
    return buf.getvalue()


class ASRProvider(ASRProviderBase):
    def __init__(self, config: dict, delete_audio_file: bool):
        super().__init__()
        self.interface_type = InterfaceType.NON_STREAM
        self.api_key = config.get("api_key")
        self.model = config.get("model_name") or "gemini-3.5-flash-lite"
        self.hint = config.get("prompt") or ""
        self.timeout = float(config.get("timeout", 12))
        self.fb_url = config.get("fallback_base_url")
        self.fb_key = config.get("fallback_api_key")
        self.fb_model = config.get("fallback_model") or "whisper-large-v3"
        self.output_dir = config.get("output_dir") or "tmp/"
        self.rpm_cap = int(config.get("rpm_cap", 6))
        self.rpd_cap = int(config.get("rpd_cap", 220))
        self._recent = deque()          # times of Gemini calls in the last minute
        self._day = None                # quota day (resets at midnight Pacific)
        self._day_count = 0
        self._rest_until = 0.0          # after a 429, let the LLM have the quota
        self.delete_audio_file = delete_audio_file

    @staticmethod
    def _quota_day():
        # Gemini daily quotas reset at midnight Pacific time (UTC-8 / UTC-7)
        return (datetime.now(timezone.utc) - timedelta(hours=7)).date()

    def _gemini_allowed(self) -> bool:
        now = time.monotonic()
        if now < self._rest_until:
            return False
        day = self._quota_day()
        if day != self._day:
            self._day, self._day_count = day, 0
        while self._recent and now - self._recent[0] > 60:
            self._recent.popleft()
        return len(self._recent) < self.rpm_cap and self._day_count < self.rpd_cap

    def _count_gemini(self):
        self._recent.append(time.monotonic())
        self._day_count += 1

    async def _gemini(self, wav: bytes) -> Optional[str]:
        text = INSTRUCTION + (f"\nContext: {self.hint}" if self.hint else "")
        body = {
            "contents": [{
                "role": "user",
                "parts": [
                    {"text": text},
                    {"inline_data": {"mime_type": "audio/wav", "data": base64.b64encode(wav).decode()}},
                ],
            }],
            "generationConfig": {"temperature": 0, "maxOutputTokens": 200},
        }
        url = GEMINI_URL.format(model=self.model)
        timeout = aiohttp.ClientTimeout(total=self.timeout)
        async with aiohttp.ClientSession(timeout=timeout) as s:
            async with s.post(url, json=body, headers={"x-goog-api-key": self.api_key}) as r:
                data = await r.json(content_type=None)
                if r.status != 200:
                    raise RuntimeError(f"Gemini {r.status}: {str(data)[:300]}")
        parts = (data.get("candidates") or [{}])[0].get("content", {}).get("parts", [])
        out = "".join(p.get("text", "") for p in parts).strip()
        if not out or "<empty>" in out:
            return ""
        return out

    async def _fallback(self, wav: bytes) -> str:
        if not (self.fb_url and self.fb_key):
            return ""
        form = aiohttp.FormData()
        form.add_field("file", wav, filename="audio.wav", content_type="audio/wav")
        form.add_field("model", self.fb_model)
        form.add_field("language", "he")
        if self.hint:
            form.add_field("prompt", self.hint)
        timeout = aiohttp.ClientTimeout(total=self.timeout)
        async with aiohttp.ClientSession(timeout=timeout) as s:
            async with s.post(self.fb_url, data=form, headers={"Authorization": f"Bearer {self.fb_key}"}) as r:
                data = await r.json(content_type=None)
                if r.status != 200:
                    raise RuntimeError(f"fallback {r.status}: {str(data)[:300]}")
        return (data.get("text") or "").strip()

    async def speech_to_text(
        self, opus_data: List[bytes], session_id: str, artifacts=None
    ) -> Tuple[Optional[str], Optional[str]]:
        if artifacts is None or not artifacts.pcm_bytes:
            return "", None
        seconds = len(artifacts.pcm_bytes) / 32000
        if seconds < 0.3:
            return "", None
        wav = _wav_bytes(artifacts.pcm_bytes)
        t = time.monotonic()
        if self._gemini_allowed():
            self._count_gemini()
            try:
                text = await self._gemini(wav)
                logger.bind(tag=TAG).info(f"Gemini ASR {seconds:.1f}s audio in {time.monotonic() - t:.2f}s: {text}")
                return text, None
            except Exception as e:
                if "429" in str(e)[:12]:
                    self._rest_until = time.monotonic() + 120
                logger.bind(tag=TAG).warning(f"Gemini ASR failed ({str(e)[:200]}); using fallback")
        else:
            logger.bind(tag=TAG).info(f"Gemini hearing budget used (today {self._day_count}/{self.rpd_cap}); using fallback")
        try:
            t = time.monotonic()
            text = await self._fallback(wav)
            logger.bind(tag=TAG).info(f"Fallback ASR in {time.monotonic() - t:.2f}s: {text}")
            return text, None
        except Exception as e:
            logger.bind(tag=TAG).error(f"Fallback ASR failed: {e}")
            return "", None
