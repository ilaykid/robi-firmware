set -e
cd /opt/robi/stage28
# 1. Node.js for the OpenGifted CLI (official build, into /opt/robi/node)
if ! /opt/robi/node/bin/node --version >/dev/null 2>&1; then
  A=$(uname -m); case "$A" in aarch64) A=arm64;; x86_64) A=x64;; esac
  V=v20.18.0
  curl -fsSL -o /tmp/node.tar.xz "https://nodejs.org/dist/$V/node-$V-linux-$A.tar.xz"
  sudo mkdir -p /opt/robi/node && sudo tar xJf /tmp/node.tar.xz -C /opt/robi/node --strip-components=1
fi
echo "node $(/opt/robi/node/bin/node --version)"
# 2. files
sudo mkdir -p /opt/robi/og /opt/robi/secrets && sudo chmod 700 /opt/robi/secrets
sudo cp robi-apply.cjs /opt/robi/og/ && sudo cp robi_og_sync.py /opt/robi/robi_og_sync.py
sudo cp robi_gifted.py /opt/robi/server/plugins_func/functions/robi_gifted.py
# 3. check the CLI against the live site files, with an empty test child (nothing is sent anywhere)
sudo /opt/robi/venv/bin/python - <<'PY'
import sys; sys.path.insert(0, "/opt/robi")
import robi_og_sync as s
s.refresh_site(force=True)
out = s.apply_cli({"id": "00000000-0000-0000-0000-000000000000", "name": "test", "grade": "ב", "focus": "A",
                   "test_date": None, "state": {}, "updated_at": "2026-10-23T00:00:00+00:00"},
                  [{"t": "2026-10-23T10:00:00+03:00", "q": "ex-1-001", "answer": 70, "ok": True, "ms": 5000}])
print("CLI ok:", out["applied"], "levels:", out["levels"])
url, key = s.supa(); print("supabase url found:", url.startswith("https://"), "| anon key found:", len(key) > 20)
PY
# 4. timer: every 5 minutes (does nothing until the link token exists)
sudo tee /etc/systemd/system/robi-og-sync.service >/dev/null <<'U'
[Unit]
Description=Robi -> OpenGifted progress sync
After=network-online.target
[Service]
Type=oneshot
ExecStart=/opt/robi/venv/bin/python /opt/robi/robi_og_sync.py
U
sudo tee /etc/systemd/system/robi-og-sync.timer >/dev/null <<'U'
[Unit]
Description=Robi -> OpenGifted sync every 5 minutes
[Timer]
OnBootSec=2min
OnUnitActiveSec=5min
[Install]
WantedBy=timers.target
U
sudo systemctl daemon-reload && sudo systemctl enable --now robi-og-sync.timer
sudo /opt/robi/venv/bin/python /opt/robi/robi_og_sync.py
sudo systemctl restart robi
for i in $(seq 1 30); do curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && break; sleep 3; done
curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && echo RESTARTED_OK || echo NOT_STARTED
