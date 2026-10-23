# Robi -> OpenGifted: puts the gifted-test answers the kids gave Robi into their OpenGifted progress
# (levels, mistake notebook, parents page) and brings back the site's level per question type, so Robi
# asks at the level the site would. Runs every few minutes (robi-og-sync.timer). Does nothing until the
# link token exists in /opt/robi/secrets/opengifted.token (created by the parent at opengifted.com/#robi).
# Uses only the public anon key (read from opengifted.com/config.js) and the token. Prints no secrets.
import json
import os
import re
import subprocess
import sys
import time
import urllib.request
from datetime import datetime

ROOT = "/opt/robi"
DATA = ROOT + "/server/data"
TOKEN = ROOT + "/secrets/opengifted.token"
SITE = ROOT + "/og/site"
CLI = ROOT + "/og/robi-apply.cjs"
NODE = ROOT + "/node/bin/node"
LOG = DATA + "/robi-gifted-log.jsonl"
STATE = DATA + "/robi-og-sync.json"
LEVELS = DATA + "/robi-og-levels.json"
UA = {"User-Agent": "Robi/1.0"}


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=20) as r:
        return r.read()


def refresh_site(force=False):
    os.makedirs(SITE + "/data", exist_ok=True)
    stamp = SITE + "/.fetched"
    if not force and os.path.exists(stamp) and time.time() - os.path.getmtime(stamp) < 86400:
        return
    for rel, url in (("index.html", "https://opengifted.com/"), ("config.js", "https://opengifted.com/config.js"),
                     ("data/bank.json", "https://opengifted.com/data/bank.json"),
                     ("data/upper.json", "https://opengifted.com/data/upper.json")):
        body = get(url)
        with open(SITE + "/" + rel + ".tmp", "wb") as f:
            f.write(body)
        os.replace(SITE + "/" + rel + ".tmp", SITE + "/" + rel)
    open(stamp, "w").close()


def supa():
    cfg = open(SITE + "/config.js", encoding="utf-8").read()
    url = re.search(r"supabaseUrl\s*:\s*['\"]([^'\"]+)", cfg).group(1).rstrip("/")
    key = re.search(r"supabaseAnonKey\s*:\s*['\"]([^'\"]+)", cfg) or re.search(r"(?:anonKey|supabaseKey)\s*:\s*['\"]([^'\"]+)", cfg)
    return url, key.group(1)


def rpc(name, body):
    url, key = supa()
    req = urllib.request.Request(url + "/rest/v1/rpc/" + name, data=json.dumps(body).encode(), method="POST",
                                 headers={**UA, "apikey": key, "Authorization": "Bearer " + key,
                                          "Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.loads(r.read() or b"null")
    except urllib.error.HTTPError as e:
        try:
            err = json.loads(e.read())
        except Exception:
            err = {}
        raise RuntimeError(f"{name}: HTTP {e.code} {err.get('code', '')} {str(err.get('message', ''))[:120]}")


# The CLI runs the site's own app script (downloaded daily from opengifted.com) in Node's vm module, which is not a
# security boundary. So it runs in a throwaway sandbox: a dynamic unprivileged user, no network, read-only system,
# no access to Robi's secrets or data, limited memory and time. It only gets the child's row and the answers on stdin.
SANDBOX = ["systemd-run", "--quiet", "--pipe", "--wait", "--collect", "--service-type=exec",
           "-p", "DynamicUser=yes", "-p", "PrivateNetwork=yes", "-p", "PrivateTmp=yes", "-p", "PrivateDevices=yes",
           "-p", "ProtectSystem=strict", "-p", "ProtectHome=yes", "-p", "NoNewPrivileges=yes",
           "-p", "ProtectKernelTunables=yes", "-p", "ProtectKernelModules=yes", "-p", "ProtectControlGroups=yes",
           "-p", "RestrictAddressFamilies=AF_UNIX", "-p", "LockPersonality=yes",
           "-p", "InaccessiblePaths=-/opt/robi/secrets", "-p", "InaccessiblePaths=-/opt/robi/server",
           "-p", "InaccessiblePaths=-/var/www", "-p", "InaccessiblePaths=-/etc/caddy",
           "-p", "MemoryMax=512M", "-p", "RuntimeMaxSec=90"]


def apply_cli(kid, answers):
    p = subprocess.run(SANDBOX + [NODE, CLI, "--site", SITE], input=json.dumps({"kid": kid, "answers": answers}),
                       capture_output=True, text=True, timeout=120)
    if p.returncode != 0:
        raise RuntimeError("robi-apply: " + p.stderr.strip()[:300])
    return json.loads(p.stdout)


def load_json(path, default):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return default


def save_json(path, obj):
    with open(path + ".tmp", "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False)
    os.replace(path + ".tmp", path)


def main():
    if not os.path.exists(TOKEN):
        print("no OpenGifted link yet (token file missing): nothing to do")
        return 0
    token = open(TOKEN, encoding="utf-8").read().strip()
    st = load_json(STATE, {"line": 0, "levels_at": 0})
    lines = open(LOG, encoding="utf-8").read().splitlines() if os.path.exists(LOG) else []
    new = []
    for ln in lines[st["line"]:]:
        try:
            r = json.loads(ln)
        except Exception:
            continue
        if "ans" in r:  # older lines (before the link) have no normalized answer: skip them
            new.append(r)
    if not new and time.time() - st.get("levels_at", 0) < 3600:
        return 0
    refresh_site(force="--refresh" in sys.argv)
    by_name = {}
    for r in new:
        by_name.setdefault(r["ילד"], []).append({"t": r["t"], "q": r["q"], "answer": r["ans"], "ok": r["ok"],
                                                  "ms": r.get("ms")})
    levels = load_json(LEVELS, {})
    report = []
    for attempt in range(3):
        kids = rpc("robi_kids", {"token": token}) or []
        conflict = False
        for kid in kids:
            name = kid.get("name", "").strip()
            answers = by_name.get(name, [])
            out = apply_cli(kid, answers)
            levels[name] = {t: v.get("lvl") for t, v in (out.get("levels") or {}).items()}
            if out.get("changed"):
                res = rpc("robi_save", {"token": token, "kid_id": out["kid_id"], "version": out["version"],
                                        "new_state": out["state"]})
                if not (res or {}).get("saved"):
                    conflict = True
                    continue
            report.append(f"{name}: +{len(out.get('applied', []))} skipped {len(out.get('skipped', []))}")
            by_name.pop(name, None)
        if not conflict:
            break
        report.append("conflict with a site save: retrying")
    else:
        raise RuntimeError("still conflicting after 3 tries; will try again next run")
    save_json(LEVELS, levels)
    st["line"], st["levels_at"] = len(lines), time.time()
    save_json(STATE, st)
    print(datetime.now().isoformat(timespec="seconds"), "; ".join(report) or "levels refreshed",
          "| unknown names:", list(by_name) or "none")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        print("sync failed:", type(e).__name__, str(e)[:300])
        sys.exit(1)
