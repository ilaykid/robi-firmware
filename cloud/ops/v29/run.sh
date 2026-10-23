set -e
sudo cp /opt/robi/stage29/robi_og_sync.py /opt/robi/robi_og_sync.py
sudo chmod -R a+rX /opt/robi/og /opt/robi/node
sudo /opt/robi/venv/bin/python - <<'PY'
import sys, subprocess, json; sys.path.insert(0, "/opt/robi")
import robi_og_sync as s
out = s.apply_cli({"id": "00000000-0000-0000-0000-000000000000", "name": "test", "grade": "ב", "focus": "A",
                   "test_date": None, "state": {}, "updated_at": "2026-10-23T00:00:00+00:00"},
                  [{"t": "2026-10-23T10:00:00+03:00", "q": "ex-1-001", "answer": 70, "ok": True, "ms": 5000}])
print("sandboxed CLI ok:", out["applied"])
probe = ("const fs=require('fs');let r=[];"
         "try{fs.readFileSync('/opt/robi/secrets/opengifted.token');r.push('TOKEN READABLE')}catch(e){r.push('token blocked')}"
         "try{fs.readdirSync('/opt/robi/server/data');r.push('DATA READABLE')}catch(e){r.push('data blocked')}"
         "try{fs.writeFileSync('/opt/robi/og/x','1');r.push('OG WRITABLE')}catch(e){r.push('og read-only')}"
         "require('https').get('https://example.com',()=>{r.push('NETWORK OPEN');console.log(r.join(', '))})"
         ".on('error',()=>{r.push('network blocked');console.log(r.join(', '))})")
p = subprocess.run(s.SANDBOX + [s.NODE, "-e", probe], capture_output=True, text=True, timeout=60)
print("sandbox probe:", p.stdout.strip() or p.stderr.strip()[:200])
PY
sudo systemctl start robi-og-sync.service; sleep 3
sudo journalctl -u robi-og-sync.service -n 3 --no-pager -o cat | tail -n 2
