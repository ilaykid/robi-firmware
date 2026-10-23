sudo systemctl disable --now robi-og-sync.timer 2>/dev/null; sudo rm -f /etc/systemd/system/robi-og-sync.timer /etc/systemd/system/robi-og-sync.service; sudo systemctl daemon-reload
sudo rm -rf /opt/robi/og /opt/robi/node /opt/robi/secrets /opt/robi/robi_og_sync.py /opt/robi/stage28 /opt/robi/stage29 /tmp/rb28 /tmp/rb29 /tmp/rbra /tmp/ra.cjs
sudo rm -f /opt/robi/server/data/robi-og-sync.json /opt/robi/server/data/robi-og-levels.json
F=/opt/robi/server/plugins_func/functions/robi_gifted.py
sudo sed -i 's/    fresh = os.path.exists(path) and time.time() - os.path.getmtime(path) < 86400/    fresh = os.path.exists(path)  # one-time copy of the OpenGifted questions: never downloaded again/' $F
grep -c "one-time copy" $F; ls -la /opt/robi/server/data/og-bank.json /opt/robi/server/data/og-upper.json | awk '{print $5, $9}'
sudo systemctl restart robi; for i in $(seq 1 30); do curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && break; sleep 3; done
curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && echo RESTARTED_OK || echo NOT_STARTED
ls /opt/robi; systemctl list-timers --all --no-pager | grep -c robi-og || true
