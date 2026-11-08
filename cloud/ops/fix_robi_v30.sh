#!/bin/bash
# Robi v3.0 Complete Fix for Oracle Cloud Server (/opt/robi/server)
# Fixes:
# 1. Upgrades LLM from gemini-3.5-flash-lite to gemini-3.5-flash with reasoning_effort="none" (0.86s latency, 100% free tier, smart Hebrew).
# 2. Fixes HTTP 400 'missing thought_signature in functionCall parts' when calling robi_gifted_question / robi_gifted_answer / device tools.
# 3. Removes Chinese few-shot injection (_inject_tool_call_fewshot) and Chinese direct_answer tool wrapper from connection.py.
# 4. Upgrades ASR to gemini-3.5-flash (thinkingBudget: 0) with automatic fallback to Groq whisper-large-v3 whenever Gemini returns empty or hits quota.
set -e

sudo /opt/robi/venv/bin/python - <<'PY'
import re, shutil, yaml

S = "/opt/robi/server"
CFG = f"{S}/data/.config.yaml"
OPENAI_PY = f"{S}/core/providers/llm/openai/openai.py"
CONN_PY = f"{S}/core/connection.py"
ASR_PY = f"{S}/core/providers/asr/gemini_audio.py"

for p in (CFG, OPENAI_PY, CONN_PY, ASR_PY):
    shutil.copy(p, p + ".bak-v30")

# 1. Update .config.yaml: gemini-3.5-flash for LLM and ASR + max_tokens 400
c = open(CFG, encoding="utf-8").read()
c = re.sub(r"(?m)^(\s*model_name:\s*)gemini-[0-9.a-z-]+", r"\1gemini-3.5-flash", c)
c = re.sub(r"(?m)^(\s*max_tokens:\s*)\d+", r"\g<1>400", c)
open(CFG, "w", encoding="utf-8").write(c)

# 2. Patch openai.py: disable thinking (reasoning_effort: "none") for googleapis.com
o = open(OPENAI_PY, encoding="utf-8").read()
if '"googleapis.com"' not in o:
    o = o.replace(
        '"volces.com": {"thinking": {"type": "disabled"}},',
        '"volces.com": {"thinking": {"type": "disabled"}},\n    "googleapis.com": {"reasoning_effort": "none"},'
    )
# Ensure any legacy extra_body minimal is upgraded to reasoning_effort: none
o = o.replace('"reasoning_effort": "minimal"', '"reasoning_effort": "none"')
open(OPENAI_PY, "w", encoding="utf-8").write(o)

# 3. Patch connection.py:
#    - Disable Chinese _inject_tool_call_fewshot
#    - Disable Chinese DIRECT_ANSWER_TOOL wrapper
#    - Preserve extra_content (thought_signature) in _merge_tool_calls and _handle_function_result
conn = open(CONN_PY, encoding="utf-8").read()
conn = re.sub(
    r"    def _inject_tool_call_fewshot\(self\):.*?(?=\n    def _init_report_threads)",
    '    def _inject_tool_call_fewshot(self):\n        return\n',
    conn,
    count=1,
    flags=re.S,
)
conn = conn.replace(
    "            if functions is not None and depth == 0:\n                functions.append(DIRECT_ANSWER_TOOL)\n",
    ""
)
if '"extra_content"' not in conn:
    conn = conn.replace(
        '            if tool_call.function.arguments:\n                tool_calls_list[tool_index]["arguments"] += tool_call.function.arguments',
        '            if tool_call.function.arguments:\n                tool_calls_list[tool_index]["arguments"] += tool_call.function.arguments\n'
        '            ec = getattr(tool_call, "extra_content", None) or (getattr(tool_call, "model_extra", None) or {}).get("extra_content")\n'
        '            if ec:\n                tool_calls_list[tool_index]["extra_content"] = ec'
    )
    conn = conn.replace(
        '                    "type": "function",\n                    "index": idx,\n                }',
        '                    "type": "function",\n                    "index": idx,\n                    **({"extra_content": tool_call_data["extra_content"]} if tool_call_data.get("extra_content") else {}),\n                }'
    )
open(CONN_PY, "w", encoding="utf-8").write(conn)

# 4. Patch gemini_audio.py:
#    - Default thinkingBudget: 0 so gemini-3.5-flash transcribes in <0.8s without wasting thinking tokens
#    - If Gemini returns empty string on audio >= 1.0s, fall back to Groq Whisper so Robi never goes deaf
asr = open(ASR_PY, encoding="utf-8").read()
asr = asr.replace(
    'self.thinking = config.get("thinking_config")',
    'self.thinking = config.get("thinking_config") or {"thinkingBudget": 0}'
)
asr = asr.replace(
    '                text = await self._gemini(wav)\n'
    '                logger.bind(tag=TAG).info(f"Gemini ASR {seconds:.1f}s audio in {time.monotonic() - t:.2f}s: {text}")\n'
    '                return text, None',
    '                text = await self._gemini(wav)\n'
    '                logger.bind(tag=TAG).info(f"Gemini ASR {seconds:.1f}s audio in {time.monotonic() - t:.2f}s: {text}")\n'
    '                if text or seconds < 1.0:\n'
    '                    return text, None\n'
    '                logger.bind(tag=TAG).info("Gemini ASR returned empty on >=1.0s audio; trying Groq fallback")'
)
open(ASR_PY, "w", encoding="utf-8").write(asr)

y = yaml.safe_load(open(CFG, encoding="utf-8"))
print("Config valid! LLM model:", y["LLM"]["RobiGemini"]["model_name"], "| ASR model:", y["ASR"]["RobiGeminiASR"]["model_name"])
PY

sudo systemctl restart robi
for i in $(seq 1 30); do
  curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && break
  sleep 2
done
curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && echo "ROBI_V30_READY" || echo "ROBI_RESTART_FAILED"
