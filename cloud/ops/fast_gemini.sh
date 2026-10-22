#!/bin/bash
# Robi: turn off Gemini "thinking" (8s -> under 1s) for answers and hearing,
# and give up on Gemini hearing after 5s (Groq takes over). Prints no secrets.
sudo /opt/robi/venv/bin/python - <<'PY'
import json, re, shutil, time, urllib.request, urllib.error, yaml
S = "/opt/robi/server"
CFG = S + "/data/.config.yaml"
ASR = S + "/core/providers/asr/gemini_audio.py"
LLM = S + "/core/providers/llm/openai/openai.py"
for f in (CFG, ASR, LLM):
    shutil.copy(f, f + ".bak-fast")

# 1. answers: reasoning_effort=minimal for Google (OpenAI-style endpoint)
s = open(LLM, encoding="utf-8").read()
if "googleapis.com" not in s:
    s = s.replace('THINKING_DISABLED_DOMAINS = {\n', 'THINKING_DISABLED_DOMAINS = {\n    "googleapis.com": {"reasoning_effort": "minimal"},\n', 1)
    open(LLM, "w", encoding="utf-8").write(s)
print("LLM patched:", "googleapis.com" in open(LLM, encoding="utf-8").read())

# 2. hearing: optional thinkingConfig + clearer error + 5s timeout
a = open(ASR, encoding="utf-8").read()
if "thinking_config" not in a:
    a = a.replace('        self.timeout = float(config.get("timeout", 12))\n',
                  '        self.timeout = float(config.get("timeout", 5))\n'
                  '        self.thinking = config.get("thinking_config")\n')
    a = a.replace('"generationConfig": {"temperature": 0, "maxOutputTokens": 200},',
                  '"generationConfig": {"temperature": 0, "maxOutputTokens": 200, '
                  '**({"thinkingConfig": self.thinking} if self.thinking else {})},')
    a = a.replace('Gemini ASR failed ({str(e)[:200]})', 'Gemini ASR failed ({type(e).__name__} {str(e)[:200]})')
    open(ASR, "w", encoding="utf-8").write(a)
print("ASR patched:", "thinking_config" in open(ASR, encoding="utf-8").read())

# 3. pick the thinking setting the hearing model accepts (fastest that works)
cfg = yaml.safe_load(open(CFG, encoding="utf-8"))
key = cfg["ASR"]["RobiGeminiASR"]["api_key"]
model = cfg["ASR"]["RobiGeminiASR"]["model_name"]
url = f"https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent"
best = None
for tc in ({"thinkingLevel": "minimal"}, {"thinkingBudget": 0}):
    body = {"contents": [{"role": "user", "parts": [{"text": "Write only: hello"}]}],
            "generationConfig": {"temperature": 0, "maxOutputTokens": 50, "thinkingConfig": tc}}
    req = urllib.request.Request(url, data=json.dumps(body).encode(),
                                 headers={"x-goog-api-key": key, "Content-Type": "application/json"})
    t = time.monotonic()
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            d = json.loads(r.read())
        dt = time.monotonic() - t
        print("native", tc, "OK", f"{dt:.2f}s", "thoughts", d.get("usageMetadata", {}).get("thoughtsTokenCount"))
        if best is None or dt < best[1]:
            best = (tc, dt)
    except urllib.error.HTTPError as e:
        print("native", tc, "HTTP", e.code)
    except Exception as e:
        print("native", tc, "ERR", type(e).__name__)

# 4. config: 5s timeout + chosen thinking setting (text edit keeps the comments)
c = open(CFG, encoding="utf-8").read()
m = re.search(r"(?m)^  RobiGeminiASR:\s*\n((?:    .*\n)+)", c)
blk = m.group(1)
nb = re.sub(r"(?m)^    timeout:.*\n", "", blk)
nb = re.sub(r"(?m)^    thinking_config:.*\n", "", nb)
nb += "    timeout: 5\n"
if best:
    nb += "    thinking_config: " + json.dumps(best[0]) + "\n"
c = c[:m.start(1)] + nb + c[m.end(1):]
open(CFG, "w", encoding="utf-8").write(c)
y = yaml.safe_load(open(CFG, encoding="utf-8"))["ASR"]["RobiGeminiASR"]
print("config: timeout", y.get("timeout"), "thinking", y.get("thinking_config"))
PY
sudo systemctl restart robi
for i in $(seq 1 30); do curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && break; sleep 3; done
curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && echo "ROBI RESTARTED OK" || { echo "ROBI DID NOT START"; sudo tail -n 20 /opt/robi/server/tmp/server.log | sed -E 's/[A-Za-z0-9_-]{30,}/<x>/g'; }
