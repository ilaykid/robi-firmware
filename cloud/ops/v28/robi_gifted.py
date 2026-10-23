# Robi: gifted-test practice with real OpenGifted questions (opengifted.com).
# Two tools for the LLM: robi_gifted_question (next question for a child, read aloud exactly)
# and robi_gifted_answer (checks the child's answer). Questions come from the public files
# opengifted.com/data/bank.json (Ori, stage A) and /data/upper.json (Lavi, grades 6-9), cached
# in data/. Only questions that work by voice are used. State: data/robi-gifted.json,
# every answer also goes to data/robi-gifted-log.jsonl (weekly report, later sync to the site).
import html
import json
import os
import random
import re
import threading
import time
import urllib.request
from datetime import datetime, timedelta, timezone

from plugins_func.register import register_function, ToolType, ActionResponse, Action

DATA = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "data")
STATE = os.path.join(DATA, "robi-gifted.json")
LOG = os.path.join(DATA, "robi-gifted-log.jsonl")
SITE_LEVELS = os.path.join(DATA, "robi-og-levels.json")  # the site's level per type (robi_og_sync.py)
SITE = "https://opengifted.com/data/"
IL = timezone(timedelta(hours=3))
_lock = threading.Lock()

# which child practises what (from the OpenGifted profiles: Ori grade 2 stage A, Lavi grades 6-9)
PLAN = {
    "אורי": {"file": "bank.json", "types": {"wordA": "בעיות מילוליות", "exercises": "תרגילים",
                                             "triangle": "הספרה החסרה"}},
    "לביא": {"file": "upper.json", "types": {"uMath": "חשבון", "uLogic": "היגיון", "uSeq": "סדרות",
                                              "uAnalogy": "אנלוגיות", "uSent": "משפטים"}},
}
NAMES = {"לביא": "לביא", "lavi": "לביא", "אורי": "אורי", "ori": "אורי"}
HEB_NUM = {"אחת": 1, "אחד": 1, "ראשונה": 1, "ראשון": 1, "שתיים": 2, "שניים": 2, "שנייה": 2, "שני": 2,
           "שלוש": 3, "שלושה": 3, "שלישית": 3, "שלישי": 3, "ארבע": 4, "ארבעה": 4, "רביעית": 4, "רביעי": 4}
NIKUD = re.compile(r"[֑-ׇ]")
_bank = {}


def _fetch(name):
    path = os.path.join(DATA, "og-" + name)
    fresh = os.path.exists(path) and time.time() - os.path.getmtime(path) < 86400
    if not fresh:
        try:
            req = urllib.request.Request(SITE + name, headers={"User-Agent": "Robi/1.0"})
            with urllib.request.urlopen(req, timeout=15) as r:
                body = r.read()
            json.loads(body)
            with open(path + ".tmp", "wb") as f:
                f.write(body)
            os.replace(path + ".tmp", path)
        except Exception:
            if not os.path.exists(path):
                raise
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def _text(h, blank="סימן שאלה"):
    """HTML of a question part -> words a TTS can say."""
    h = h or ""
    h = re.sub(r"<(svg|script|style)\b.*?</\1>", " ", h, flags=re.S)
    h = re.sub(r'<span class="(blank|q)"[^>]*>\s*(\?)?\s*</span>', " ? ", h)
    h = re.sub(r"</(div|td|p)>", " , ", h)
    h = re.sub(r"</span>(?=\s*<span)", " , ", h)
    h = re.sub(r"<[^>]+>", "", h)
    h = html.unescape(h).replace("←", " , ").replace("→", " , ")
    h = h.replace("△", " משולש ").replace("×", " כפול ").replace("−", " פחות ").replace("÷", " חלקי ")
    h = re.sub(r"(\d)\s*:\s*(\d)", r"\1 חלקי \2", h)
    h = re.sub(r"\s=\s", " שווה ", h)
    h = re.sub(r"(^|\s)\?(?=\s|$|,)", r"\1 " + blank + " ", h)
    h = re.sub(r"\s*,\s*(,\s*)+", " , ", h)
    h = re.sub(r"\s+", " ", h).strip(" ,")
    return h


def _voice_ok(q, kind):
    if q.get("retired") or q.get("place") or q.get("pid") or q.get("passage"):
        return False
    if q["type"] not in PLAN[kind]["types"]:
        return False
    v = q.get("visual") or ""
    if re.search(r'\d\s*<span class="tri">|tri">△</span>\s*\d', v):
        return False  # a hidden digit inside a bigger number cannot be said clearly
    if "<svg" in v or "<img" in v or "<table" in v:
        return False
    opts = q.get("options")
    if opts and any("<svg" in o or "<img" in o for o in opts):
        return False
    return bool(opts) or q.get("input")


def _items(kid):
    if kid not in _bank or time.time() - _bank[kid][0] > 86400:
        data = _fetch(PLAN[kid]["file"])
        items = [q for q in data.get("items", []) if _voice_ok(q, kid)]
        _bank[kid] = (time.time(), items)
    return _bank[kid][1]


def _load():
    try:
        with open(STATE, encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def _save(st):
    with open(STATE + ".tmp", "w", encoding="utf-8") as f:
        json.dump(st, f, ensure_ascii=False)
    os.replace(STATE + ".tmp", STATE)


def _kid(name):
    name = NAMES.get(str(name or "").strip().lower(), NAMES.get(str(name or "").strip()))
    return name


def _pick(kid, k, want_type):
    items = _items(kid)
    types = PLAN[kid]["types"]
    if want_type and want_type in types:
        ty = want_type
    else:
        # the weaker the child is in a type, the more often it comes; untried types first
        weights = []
        for t in types:
            ok, n = k["stats"].get(t, [0, 0])
            weights.append((t, 3.0 if n < 3 else 0.4 + (1 - ok / n) * 2))
        ty = random.choices([w[0] for w in weights], [w[1] for w in weights])[0]
    lvl = k["lvl"].get(ty, 1)
    # the site's level wins, unless Robi changed this type's level after the site's level was fetched
    try:
        site = json.load(open(SITE_LEVELS, encoding="utf-8")).get(kid, {}).get(ty)
        if site and k.get("lvl_t", {}).get(ty, 0) < os.path.getmtime(SITE_LEVELS):
            lvl = int(site)
    except Exception:
        pass
    seen = set(k["seen"])
    pool = [q for q in items if q["type"] == ty and q.get("lvl", 1) == lvl and q["id"] not in seen]
    if not pool:
        pool = [q for q in items if q["type"] == ty and q["id"] not in seen] or [q for q in items if q["type"] == ty]
    return random.choice(pool)


def _spoken(q):
    parts = [_text(q.get("prompt"))]
    if q.get("visual"):
        parts.append(_text(q["visual"], "(מילה חסרה)" if q["type"] in ("uSent", "sentence") else "סימן שאלה"))
    if q.get("options"):
        opts = [f"{i + 1}) {_text(o)}" for i, o in enumerate(q["options"])]
        return " ".join(p for p in parts if p), opts
    return " ".join(p for p in parts if p), None


robi_gifted_question_desc = {
    "type": "function",
    "function": {
        "name": "robi_gifted_question",
        "description": (
            "Get the next real gifted-test practice question (from OpenGifted) for Lavi or Ori. "
            "Use it whenever the child practises for the gifted test (מחוננים). Read the question and the "
            "numbered options EXACTLY as returned, in Hebrew, then wait for the child's answer. Never invent "
            "gifted questions yourself and never reveal the answer before the child answers."
        ),
        "parameters": {
            "type": "object",
            "properties": {
                "child": {"type": "string", "description": "לביא or אורי"},
                "kind": {"type": "string", "description": "optional question type code to practise, from the last result"},
            },
            "required": ["child"],
        },
    },
}

robi_gifted_answer_desc = {
    "type": "function",
    "function": {
        "name": "robi_gifted_answer",
        "description": (
            "Check the child's answer to the current gifted question. For multiple choice pass the option "
            "NUMBER he chose (1-4; he may say the number or the option's words, you map it). For open "
            "questions pass the number he said. Then tell him if he was right, in one or two short sentences "
            "using the explanation, and ask if he wants another question."
        ),
        "parameters": {
            "type": "object",
            "properties": {
                "child": {"type": "string", "description": "לביא or אורי"},
                "answer": {"type": "string", "description": "option number 1-4, or the number for an open question"},
            },
            "required": ["child", "answer"],
        },
    },
}


def _reply(obj):
    return ActionResponse(Action.REQLLM, json.dumps(obj, ensure_ascii=False), None)


@register_function("robi_gifted_question", robi_gifted_question_desc, ToolType.WAIT)
def robi_gifted_question(child=None, kind=None):
    kid = _kid(child)
    if kid not in PLAN:
        return _reply({"error": "צריך לדעת מי מתאמן: לביא או אורי. תשאל אותו."})
    try:
        with _lock:
            st = _load()
            k = st.setdefault(kid, {"lvl": {}, "streak": {}, "stats": {}, "seen": []})
            q = _pick(kid, k, kind)
            text, opts = _spoken(q)
            k["pending"] = {"id": q["id"], "t": time.time()}
            _save(st)
    except Exception as e:
        return _reply({"error": f"אין גישה למאגר השאלות כרגע ({type(e).__name__}). תציע לו משהו אחר."})
    out = {"type": q["type"], "סוג": PLAN[kid]["types"][q["type"]], "שאלה": text}
    if opts:
        out["אפשרויות"] = opts
        out["איך לענות"] = "אפשר להגיד את המספר של התשובה או את התשובה עצמה"
    else:
        out["איך לענות"] = "תשובה במספר"
    return _reply(out)


def _num(s):
    s = NIKUD.sub("", str(s or "")).strip()
    m = re.search(r"-?\d+(\.\d+)?", s.replace(",", "."))
    if m:
        return float(m.group(0))
    for w, n in HEB_NUM.items():
        if w in s:
            return float(n)
    return None


@register_function("robi_gifted_answer", robi_gifted_answer_desc, ToolType.WAIT)
def robi_gifted_answer(child=None, answer=None):
    kid = _kid(child)
    if kid not in PLAN:
        return _reply({"error": "לא ברור מי ענה: לביא או אורי."})
    with _lock:
        st = _load()
        k = st.get(kid) or {}
        p = k.get("pending")
        if not p:
            return _reply({"error": "אין שאלה פתוחה. תביא שאלה חדשה עם robi_gifted_question."})
        q = next((x for x in _items(kid) if x["id"] == p["id"]), None)
        if q is None:
            k.pop("pending", None)
            _save(st)
            return _reply({"error": "השאלה כבר לא במאגר. תביא שאלה חדשה."})
        a = _num(answer)
        if q.get("options"):
            ok = a is not None and int(a) - 1 == q.get("answer")
            right = f"{q['answer'] + 1}) {_text(q['options'][q['answer']])}"
        else:
            ok = a is not None and abs(a - float(q.get("num"))) < 1e-9
            right = _text(str(q.get("num")))
        if a is None:
            ans = None
        elif q.get("options"):
            ans = int(a) - 1
        else:
            ans = int(a) if float(a).is_integer() else a
        ty = q["type"]
        okn, n = k["stats"].get(ty, [0, 0])
        k["stats"][ty] = [okn + (1 if ok else 0), n + 1]
        s = k["streak"].get(ty, 0)
        s = (s + 1 if s >= 0 else 1) if ok else (s - 1 if s <= 0 else -1)
        lvl = q.get("lvl", k["lvl"].get(ty, 1))
        if s >= 3 and lvl < 3:
            lvl, s = lvl + 1, 0
        elif s <= -2 and lvl > 1:
            lvl, s = lvl - 1, 0
        if lvl != k["lvl"].get(ty, 1):
            k.setdefault("lvl_t", {})[ty] = time.time()
        k["lvl"][ty], k["streak"][ty] = lvl, s
        k["seen"] = (k["seen"] + [q["id"]])[-400:]
        k.pop("pending", None)
        _save(st)
    row = {"t": datetime.now(IL).isoformat(timespec="seconds"), "ילד": kid, "q": q["id"], "type": ty,
           "lvl": q.get("lvl"), "ok": ok, "answer": str(answer)[:40], "ans": ans, "ms": int((time.time() - p["t"]) * 1000)}
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(json.dumps(row, ensure_ascii=False) + "\n")
    return _reply({"נכון": ok, "התשובה הנכונה": right, "הסבר": _text(q.get("ex")),
                   "רמה עכשיו": lvl, "type": ty})
