#!/bin/bash
# Robi: measure Gemini latency from the server, with and without "thinking".
# Prints only timings, HTTP status and short error text - never the key.
sudo /opt/robi/venv/bin/python - <<'PY'
import json, re, time, urllib.request, urllib.error, yaml
cfg = yaml.safe_load(open("/opt/robi/server/data/.config.yaml", encoding="utf-8"))
KEY = cfg["LLM"]["RobiGemini"]["api_key"]
MODEL = cfg["LLM"]["RobiGemini"]["model_name"]
BASE = "https://generativelanguage.googleapis.com/v1beta"

def post(url, body, headers):
    req = urllib.request.Request(url, data=json.dumps(body).encode(), headers=headers, method="POST")
    t = time.monotonic()
    try:
        with urllib.request.urlopen(req, timeout=40) as r:
            data = json.loads(r.read())
            return r.status, time.monotonic() - t, data
    except urllib.error.HTTPError as e:
        txt = e.read().decode(errors="replace")
        return e.code, time.monotonic() - t, {"error": re.sub(r"AIza\S+", "<key>", txt)[:220]}
    except Exception as e:
        return 0, time.monotonic() - t, {"error": type(e).__name__ + " " + str(e)[:150]}

msgs = [{"role": "system", "content": "ענה בעברית במשפט אחד קצר."},
        {"role": "user", "content": "רובי, מה אתה אוהב לעשות?"}]
print("MODEL", MODEL)
for label, extra in [("chat default", {}), ("chat effort=minimal", {"reasoning_effort": "minimal"}),
                     ("chat effort=low", {"reasoning_effort": "low"}), ("chat effort=none", {"reasoning_effort": "none"})]:
    body = {"model": MODEL, "messages": msgs, "max_tokens": 200, **extra}
    st, dt, d = post(BASE + "/openai/chat/completions", body,
                     {"Authorization": "Bearer " + KEY, "Content-Type": "application/json"})
    if st == 200:
        u = d.get("usage", {})
        print(f"{label:24s} {st} {dt:5.2f}s tokens={u.get('completion_tokens')} reasoning={u.get('completion_tokens_details', {})} :: {d['choices'][0]['message']['content'][:60]}")
    else:
        print(f"{label:24s} {st} {dt:5.2f}s ERR {d.get('error')}")

for label, gc in [("native default", {}), ("native level=minimal", {"thinkingConfig": {"thinkingLevel": "minimal"}}),
                  ("native budget=0", {"thinkingConfig": {"thinkingBudget": 0}})]:
    body = {"contents": [{"role": "user", "parts": [{"text": "כתוב רק: שלום רובי"}]}],
            "generationConfig": {"temperature": 0, "maxOutputTokens": 100, **gc}}
    st, dt, d = post(f"{BASE}/models/{MODEL}:generateContent", body,
                     {"x-goog-api-key": KEY, "Content-Type": "application/json"})
    if st == 200:
        um = d.get("usageMetadata", {})
        print(f"{label:24s} {st} {dt:5.2f}s thoughts={um.get('thoughtsTokenCount')}")
    else:
        print(f"{label:24s} {st} {dt:5.2f}s ERR {d.get('error')}")
PY
