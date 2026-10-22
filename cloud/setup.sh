#!/bin/bash
# Robi server v2 - one-shot setup on a fresh Ubuntu VM (run by cloud-init as root).
# Everything is logged to /var/log/robi-setup.log, which is also served at
# https://<host>/<secret>/_log/ so it can be checked from a browser.
set -x
SECRET="__SECRET__"
COMMIT="f614489d814aa70f6703b647fecd347c1af8b62f"
CADDY_VER="2.8.4"
ROOT=/opt/robi
PUB=/var/www/robi-log
mkdir -p "$ROOT" "$PUB"
ln -sf /var/log/robi-setup.log "$PUB/setup.log"
echo "STARTED v2 $(date -u)" > "$PUB/status.txt"

# ---- 1. firewall (Oracle Ubuntu images only open port 22) + swap ----
iptables -I INPUT 1 -p tcp --dport 80 -j ACCEPT
iptables -I INPUT 1 -p tcp --dport 443 -j ACCEPT
netfilter-persistent save || iptables-save > /etc/iptables/rules.v4 || true
if ! swapon --show | grep -q swapfile; then
  fallocate -l 4G /swapfile && chmod 600 /swapfile && mkswap /swapfile && swapon /swapfile
  echo '/swapfile none swap sw 0 0' >> /etc/fstab
fi

# ---- 2. Caddy (HTTPS) ----
ARCH=$(dpkg --print-architecture)
curl -fsSL -o /tmp/caddy.tgz "https://github.com/caddyserver/caddy/releases/download/v${CADDY_VER}/caddy_${CADDY_VER}_linux_${ARCH}.tar.gz"
tar -xzf /tmp/caddy.tgz -C /usr/local/bin caddy
mkdir -p /etc/caddy /var/lib/caddy
cat > /etc/systemd/system/caddy.service <<'UNIT'
[Unit]
Description=Caddy (Robi HTTPS)
After=network-online.target
Wants=network-online.target
[Service]
Environment=XDG_DATA_HOME=/var/lib/caddy XDG_CONFIG_HOME=/var/lib/caddy
ExecStart=/usr/local/bin/caddy run --config /etc/caddy/Caddyfile --adapter caddyfile
Restart=always
RestartSec=5
[Install]
WantedBy=multi-user.target
UNIT

# ---- 3. public name <ip-with-dashes>.sslip.io, kept in step with the public IP ----
# robi-host.sh runs now, at boot and every 2 minutes: a reserved public IP that is
# attached later is picked up by itself (new HTTPS name + server addresses).
sed "s|@ROOT@|$ROOT|; s|@PUB@|$PUB|; s|@SECRET@|$SECRET|" "$ROOT/seed/robi-host.sh" > "$ROOT/robi-host.sh"
chmod 700 "$ROOT/robi-host.sh"
cat > /etc/systemd/system/robi-host.service <<UNIT
[Unit]
Description=Robi public name check
After=network-online.target
Wants=network-online.target
[Service]
Type=oneshot
ExecStart=$ROOT/robi-host.sh
UNIT
cat > /etc/systemd/system/robi-host.timer <<'UNIT'
[Unit]
Description=Robi public name check every 2 minutes
[Timer]
OnBootSec=20
OnUnitActiveSec=2min
[Install]
WantedBy=timers.target
UNIT
systemctl daemon-reload
"$ROOT/robi-host.sh"
HOST=$(cat "$ROOT/host.txt")
systemctl enable caddy
systemctl restart caddy
systemctl enable --now robi-host.timer
echo "CADDY UP https://$HOST/$SECRET/_log/ $(date -u)" > "$PUB/status.txt"

# ---- 4. Robi server code (pinned, tested version) ----
cd "$ROOT"
curl -fsSL -o server.zip "https://github.com/xinnan-tech/xiaozhi-esp32-server/archive/$COMMIT.zip"
rm -rf src server && mkdir src && unzip -q server.zip -d src
mv "src/xiaozhi-esp32-server-$COMMIT/main/xiaozhi-server" server
# let the OpenAI-style ASR provider (Groq) send a fixed language + hint (Hebrew)
python3 - <<'PY'
p = "/opt/robi/server/core/providers/asr/openai.py"
s = open(p, encoding="utf-8").read()
a = 'self.model = config.get("model_name")'
b = '"model": self.model'
if "asr_prompt" not in s and a in s and b in s:
    s = s.replace(a, a + '; self.language = config.get("language"); self.asr_prompt = config.get("prompt")', 1)
    s = s.replace(b, b + ', **({"language": self.language} if self.language else {}), **({"prompt": self.asr_prompt} if self.asr_prompt else {})', 1)
    open(p, "w", encoding="utf-8").write(s)
    print("ASR patched")
PY
# Robi's Gemini ears (Gemini hears the audio; Groq Whisper is the backup)
cp "$ROOT/seed/gemini_audio.py" server/core/providers/asr/gemini_audio.py
mkdir -p server/data server/tmp server/data/bin
cp "$ROOT/seed/.config.yaml" server/data/.config.yaml
chmod 600 server/data/.config.yaml
cp "$ROOT/seed/robi-prompt.txt" server/data/robi-prompt.txt
HOST=$(cat "$ROOT/host.txt")   # may have changed (reserved IP attached during setup)
sed -i "s|__WS__|wss://$HOST/$SECRET/xiaozhi/v1/|; s|__VISION__|https://$HOST/$SECRET/mcp/vision/explain|" server/data/.config.yaml
ln -sf "$ROOT/server/tmp/server.log" "$PUB/server.log"

# ---- 5. Python environment ----
echo "INSTALLING PYTHON PACKAGES (10-20 min) $(date -u)" > "$PUB/status.txt"
python3 -m venv "$ROOT/venv"
"$ROOT/venv/bin/pip" install --upgrade pip
if [ "$ARCH" = "amd64" ]; then   # CPU-only torch, skips gigabytes of CUDA
  "$ROOT/venv/bin/pip" install torch==2.2.2 torchaudio==2.2.2 --index-url https://download.pytorch.org/whl/cpu
fi
"$ROOT/venv/bin/pip" install -r server/requirements.txt || { echo "PIP FAILED $(date -u)" > "$PUB/status.txt"; exit 1; }

# ---- 6. always-on service ----
cat > /etc/systemd/system/robi.service <<UNIT
[Unit]
Description=Robi server (xiaozhi-esp32-server)
After=network-online.target
Wants=network-online.target
[Service]
WorkingDirectory=$ROOT/server
Environment=PYTHONUTF8=1 PYTHONIOENCODING=utf-8
ExecStart=$ROOT/venv/bin/python app.py
Restart=always
RestartSec=10
[Install]
WantedBy=multi-user.target
UNIT
systemctl daemon-reload
systemctl enable --now robi

for i in $(seq 1 60); do
  curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && break
  sleep 5
done
HOST=$(cat "$ROOT/host.txt")
if curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null; then
  echo "READY v2 OTA=https://$HOST/$SECRET/xiaozhi/ota/ $(date -u)" > "$PUB/status.txt"
else
  echo "SERVER DID NOT START $(date -u) - see server.log" > "$PUB/status.txt"
fi
