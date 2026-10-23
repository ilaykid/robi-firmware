# Robi: weekly report for dad (Fridays). Reads the progress notebook and the last 7 days
# of conversations, asks Gemini for a short Hebrew summary per child, writes an HTML page
# to /var/www/robi-log/report/ (served under the secret path). Prints no secrets.
import html
import json
import os
import urllib.request
from datetime import datetime, timedelta, timezone

import yaml

S = "/opt/robi/server"
OUT = "/var/www/robi-log/report"
IL = timezone(timedelta(hours=3))
KIDS = ("לביא", "אורי")

cfg = yaml.safe_load(open(S + "/data/.config.yaml", encoding="utf-8"))
llm = cfg["LLM"]["RobiGemini"]
mem = {}
if os.path.exists(S + "/data/.memory.yaml"):
    mem = yaml.safe_load(open(S + "/data/.memory.yaml", encoding="utf-8")) or {}
notebook = next(iter(mem.values()), "") if mem else ""
since = datetime.now(IL) - timedelta(days=7)
rows = []
if os.path.exists(S + "/data/robi-sessions.jsonl"):
    for line in open(S + "/data/robi-sessions.jsonl", encoding="utf-8"):
        try:
            r = json.loads(line)
            if datetime.fromisoformat(r["t"]) >= since:
                rows.append(r)
        except Exception:
            pass

gifted = {k: {} for k in KIDS}
if os.path.exists(S + "/data/robi-gifted-log.jsonl"):
    for line in open(S + "/data/robi-gifted-log.jsonl", encoding="utf-8"):
        try:
            r = json.loads(line)
            if datetime.fromisoformat(r["t"]) >= since and r.get("ילד") in gifted:
                t = gifted[r["ילד"]].setdefault(r["type"], [0, 0])
                t[0] += 1 if r.get("ok") else 0
                t[1] += 1
        except Exception:
            pass
GNAME = {"wordA": "בעיות מילוליות", "exercises": "תרגילים", "triangle": "הספרה החסרה", "uMath": "חשבון",
         "uLogic": "היגיון", "uSeq": "סדרות", "uAnalogy": "אנלוגיות", "uSent": "משפטים"}

stats = {k: {"שיחות": 0, "הצלחות": 0, "ניסיונות": 0, "פעילויות": {}} for k in KIDS}
for r in rows:
    k = r.get("ילד")
    if k in stats:
        st = stats[k]
        st["שיחות"] += 1
        st["הצלחות"] += r.get("הצלחות", 0)
        st["ניסיונות"] += r.get("ניסיונות", 0)
        a = r.get("פעילות", "שיחה")
        st["פעילויות"][a] = st["פעילויות"].get(a, 0) + 1

ask = (
    "You write a short weekly report for a father about his two sons' learning with Robi, "
    "a voice robot toy. Lavi (לביא) is 10, Ori (אורי) is 7 and preparing for the gifted test. "
    "Use only the facts below; if there is little data, say so honestly. Warm, practical Hebrew.\n"
    "Return only JSON: {\"לביא\": {\"תקציר\": \"\", \"התקדם\": [], \"לעבוד_על\": [], \"רעיון_לאבא\": \"\"}, "
    "\"אורי\": {same keys}, \"כללי\": \"\"}\n\n"
    f"Progress notebook:\n{notebook}\n\nConversations this week (one per line):\n"
    + "\n".join(json.dumps(r, ensure_ascii=False) for r in rows)
    + f"\n\nCounts:\n{json.dumps(stats, ensure_ascii=False)}"
    + "\n\nGifted-test practice with real OpenGifted questions (type: [correct, answered]):\n"
    + json.dumps({k: {GNAME.get(t, t): v for t, v in g.items()} for k, g in gifted.items()}, ensure_ascii=False)
)
body = {
    "model": llm["model_name"],
    "messages": [{"role": "user", "content": ask}],
    "temperature": 0.3,
    "reasoning_effort": "low",
    "response_format": {"type": "json_object"},
}
req = urllib.request.Request(
    (llm.get("base_url") or llm.get("url")).rstrip("/") + "/chat/completions",
    data=json.dumps(body).encode(),
    headers={"Authorization": "Bearer " + llm["api_key"], "Content-Type": "application/json"},
)
try:
    with urllib.request.urlopen(req, timeout=120) as r:
        txt = json.loads(r.read())["choices"][0]["message"]["content"]
    rep = json.loads(txt[txt.find("{"): txt.rfind("}") + 1])
except Exception as e:
    print("report LLM failed:", type(e).__name__)
    rep = {"כללי": "לא הצלחתי לכתוב סיכום השבוע (תקלה זמנית). המספרים למטה מעודכנים."}

E = html.escape


def ul(items):
    items = [i for i in (items or []) if str(i).strip()]
    return "<ul>" + "".join(f"<li>{E(str(i))}</li>" for i in items) + "</ul>" if items else "<p class=m>—</p>"


def gift_html(k):
    g = gifted.get(k) or {}
    if not g:
        return ""
    rows = "".join(f"<li>{E(GNAME.get(t, t))}: {ok} מתוך {n} נכונות</li>" for t, (ok, n) in sorted(g.items()))
    tot_ok, tot_n = sum(v[0] for v in g.values()), sum(v[1] for v in g.values())
    return f"<h3>מחוננים (שאלות OpenGifted): {tot_ok} מתוך {tot_n}</h3><ul>{rows}</ul>"


cards = ""
for k, color in (("לביא", "#2E86DE"), ("אורי", "#F39C12")):
    r = rep.get(k) or {}
    st = stats[k]
    acts = ", ".join(f"{E(a)} ×{n}" for a, n in st["פעילויות"].items()) or "—"
    cards += f"""<section style="border-top:6px solid {color}">
<h2>{k}</h2>
<div class=nums><div><b>{st['שיחות']}</b>שיחות</div><div><b>{st['הצלחות']}</b>תשובות נכונות</div><div><b>{st['ניסיונות']}</b>שאלות</div></div>
<p class=m>פעילויות: {acts}</p>
{gift_html(k)}
<p>{E(str(r.get('תקציר', '')))}</p>
<h3>התקדם ב</h3>{ul(r.get('התקדם'))}
<h3>כדאי לעבוד על</h3>{ul(r.get('לעבוד_על'))}
<h3>רעיון לאבא</h3><p>{E(str(r.get('רעיון_לאבא', '')) or '—')}</p>
</section>"""

today = datetime.now(IL).strftime("%Y-%m-%d")
os.makedirs(OUT, exist_ok=True)
older = sorted((f for f in os.listdir(OUT) if f[:2] == "20" and f.endswith(".html") and f[:10] != today), reverse=True)
arch = " · ".join(f'<a href="{f}">{f[:10]}</a>' for f in older[:12]) or "—"
page = f"""<!doctype html><html lang=he dir=rtl><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>הדוח של רובי</title>
<style>body{{font-family:system-ui,Arial,sans-serif;background:#f6f4ef;color:#222;margin:0;padding:16px;line-height:1.5}}
main{{max-width:720px;margin:auto}}h1{{margin:.2em 0}}section{{background:#fff;border-radius:14px;padding:14px 18px;margin:14px 0;box-shadow:0 1px 4px #0001}}
h2{{margin:.2em 0}}h3{{margin:.8em 0 .2em;font-size:1em;color:#555}}.nums{{display:flex;gap:10px}}.nums div{{flex:1;background:#f3f3f3;border-radius:10px;padding:8px;text-align:center;font-size:.85em}}
.nums b{{display:block;font-size:1.6em}}.m{{color:#777;font-size:.9em}}ul{{margin:.2em 0;padding-inline-start:1.2em}}
@media (prefers-color-scheme:dark){{body{{background:#161616;color:#eee}}section{{background:#222}}.nums div{{background:#2c2c2c}}h3{{color:#aaa}}}}</style></head>
<body><main><h1>🤖 הדוח השבועי של רובי</h1><p class=m>{today} · 7 הימים האחרונים</p>
<section><p>{E(str(rep.get('כללי', '')))}</p></section>{cards}
<p class=m>דוחות קודמים: {arch}</p></main></body></html>"""
for name in (f"{today}.html", "index.html"):
    with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
        f.write(page)
print("report written:", today, "sessions:", len(rows))
