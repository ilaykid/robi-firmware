#!/bin/bash
# Robi: speech detector back to normal sensitivity + never listen to one utterance > 12 s
CFG=/opt/robi/server/data/.config.yaml
VAD=/opt/robi/server/core/providers/vad/silero.py
sudo cp $VAD $VAD.bak-cap
sudo sed -i -E 's/^(    threshold:).*/\1 0.5/; s/^(    threshold_low:).*/\1 0.35/' $CFG
sudo /opt/robi/venv/bin/python - <<'PY'
p = "/opt/robi/server/core/providers/vad/silero.py"
s = open(p, encoding="utf-8").read()
old = "            return client_have_voice\n"
new = ("            # Robi: never listen to one utterance for more than 12 s (background noise)\n"
       "            now_ms = time.time() * 1000\n"
       "            if not conn.client_have_voice:\n"
       "                conn._robi_t0 = 0\n"
       "            elif not getattr(conn, '_robi_t0', 0):\n"
       "                conn._robi_t0 = now_ms\n"
       "            elif now_ms - conn._robi_t0 > 12000:\n"
       "                conn.client_voice_stop = True\n"
       "            return client_have_voice\n")
if "_robi_t0" not in s and s.count(old) == 1:
    s = s.replace(old, new)
    open(p, "w", encoding="utf-8").write(s)
print("cap patched:", "_robi_t0" in open(p, encoding="utf-8").read())
PY
sudo grep -nE '^    (threshold|threshold_low|min_silence_duration_ms):' $CFG
sudo systemctl restart robi
for i in $(seq 1 30); do curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && break; sleep 3; done
curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && echo RESTARTED_OK || echo NOT_STARTED
