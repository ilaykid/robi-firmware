set -e
cd /opt/robi/stage
sudo /opt/robi/venv/bin/python apply.py
sudo tee /etc/systemd/system/robi-report.service >/dev/null <<'U'
[Unit]
Description=Robi weekly report for dad
After=network-online.target
[Service]
Type=oneshot
ExecStart=/opt/robi/venv/bin/python /opt/robi/robi_report.py
U
sudo tee /etc/systemd/system/robi-report.timer >/dev/null <<'U'
[Unit]
Description=Robi weekly report, Fridays 08:00 Israel time
[Timer]
OnCalendar=Fri *-*-* 08:00:00 Asia/Jerusalem
Persistent=true
[Install]
WantedBy=timers.target
U
sudo systemctl daemon-reload
sudo systemctl enable --now robi-report.timer
sudo systemctl restart robi
for i in $(seq 1 30); do curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && break; sleep 3; done
curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && echo RESTARTED_OK || { echo NOT_STARTED; sudo tail -n 30 /opt/robi/server/tmp/server.log | sed -E 's/[A-Za-z0-9_-]{30,}/<x>/g'; }
echo "pet: $(curl -s http://127.0.0.1:8003/robi/pet)"
sudo /opt/robi/venv/bin/python /opt/robi/robi_report.py
systemctl list-timers robi-report.timer --no-pager | sed -n 2p
