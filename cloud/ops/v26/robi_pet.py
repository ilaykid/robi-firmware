# Robi the Tamagotchi: energy (fed by learning) and joy (fed by playing and talking).
# State in data/robi-pet.json, one line per conversation in data/robi-sessions.jsonl
# (the weekly report for dad reads it). Served to the device at /robi/pet.
import json
import os
import threading
import time
from datetime import datetime, timedelta, timezone

DATA = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data")
PET = os.path.join(DATA, "robi-pet.json")
LOG = os.path.join(DATA, "robi-sessions.jsonl")
IL = timezone(timedelta(hours=3))
_lock = threading.Lock()


def _load():
    try:
        with open(PET, encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        now = time.time()
        return {"energy": 60.0, "joy": 60.0, "t": now, "played": now}


def _save(st):
    tmp = PET + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(st, f, ensure_ascii=False)
    os.replace(tmp, PET)


def _decay(st):
    now = time.time()
    h = max(0.0, (now - st.get("t", now)) / 3600.0)
    st["energy"] = max(5.0, st["energy"] - 1.0 * h)
    st["joy"] = max(5.0, st["joy"] - 1.5 * h)
    st["t"] = now
    return st


def get():
    with _lock:
        return _decay(_load())


def _int(v):
    try:
        return max(0, int(v))
    except Exception:
        return 0


def after_session(notebook_json, msgs):
    """Take the newest conversation's summary out of the notebook, feed Robi, log it."""
    try:
        d = json.loads(notebook_json)
    except Exception:
        return notebook_json
    s = d.pop("_שיחה", None)
    if not isinstance(s, dict):
        s = {}
    turns = sum(1 for m in msgs if getattr(m, "role", "") == "user")
    ok = _int(s.get("הצלחות"))
    tries = max(ok, _int(s.get("ניסיונות")))
    with _lock:
        st = _decay(_load())
        st["energy"] = min(100.0, st["energy"] + min(40, 6 * ok + 2 * (tries - ok)))
        st["joy"] = min(100.0, st["joy"] + min(35, 5 + 3 * turns))
        st["played"] = time.time()
        _save(st)
    row = {
        "t": datetime.now(IL).isoformat(timespec="minutes"),
        "ילד": s.get("ילד", "לא ידוע"),
        "פעילות": s.get("פעילות", "שיחה"),
        "הצלחות": ok,
        "ניסיונות": tries,
        "תורות": turns,
        "סיכום": str(s.get("סיכום", ""))[:300],
    }
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(json.dumps(row, ensure_ascii=False) + "\n")
    return json.dumps(d, ensure_ascii=False)


def describe():
    st = get()
    e, j = int(st["energy"]), int(st["joy"])
    ew = "רעב מאוד ללמוד" if e < 30 else ("קצת רעב ללמוד" if e < 60 else "מלא אנרגיה")
    jw = "משועמם ומתגעגע" if j < 30 else ("בסדר" if j < 60 else "שמח מאוד")
    h = (time.time() - st.get("played", time.time())) / 3600.0
    ago = "ממש עכשיו" if h < 1 else (f"לפני {int(h)} שעות" if h < 48 else f"לפני {int(h // 24)} ימים")
    return f"[מצב רובי עכשיו] אנרגיה: {e}/100 ({ew}). שמחה: {j}/100 ({jw}). שיחקו איתו לאחרונה {ago}."


async def handle_pet(request):
    from aiohttp import web
    st = get()
    return web.json_response({"energy": int(st["energy"]), "joy": int(st["joy"])})
