set -e
cd /opt/robi/stage27
sudo /opt/robi/venv/bin/python apply.py
cd /opt/robi/server && sudo /opt/robi/venv/bin/python -c "
from plugins_func.functions import robi_gifted as g
print('voice questions: Ori', len(g._items('אורי')), '| Lavi', len(g._items('לביא')))
t, o = g._spoken(g._items('אורי')[0]); print('sample ok:', bool(t))" 2>&1 | grep -v INFO | tail -3
sudo systemctl restart robi
for i in $(seq 1 30); do curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && break; sleep 3; done
curl -fs http://127.0.0.1:8003/xiaozhi/ota/ >/dev/null && echo RESTARTED_OK || { echo NOT_STARTED; sudo tail -n 30 /opt/robi/server/tmp/server.log | sed -E 's/[A-Za-z0-9_-]{30,}/<x>/g'; }
sudo grep -iE "robi_gifted|Traceback|Error" /opt/robi/server/tmp/server.log | tail -n 5 | sed -E 's/[A-Za-z0-9_-]{30,}/<x>/g'
sudo /opt/robi/venv/bin/python /opt/robi/robi_report.py
