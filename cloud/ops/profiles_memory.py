# Robi: two child profiles (Lavi 10, Ori 7) + a per-child progress notebook (memory).
# Runs on the server as root. Prints no secrets.
import re
import shutil

S = "/opt/robi/server"
CFG = S + "/data/.config.yaml"
TPL = S + "/data/robi-prompt.txt"
MEM = S + "/core/providers/memory/mem_local_short/mem_local_short.py"
for f in (CFG, TPL, MEM):
    shutil.copy(f, f + ".bak-profiles")

# ---- 1. the notebook prompt (what to remember, per child) ----
NOTEBOOK = '''
You keep the progress notebook of Robi, a toy robot that teaches and plays with two Israeli brothers:
Lavi (לביא), age 10, and Ori (אורי), age 7.
You get the newest conversation and the previous notebook. Return the updated notebook.

Rules:
- A conversation belongs to the child who opened it with "זה לביא" or "זה אורי", or who said his name. If you cannot tell who spoke, change nothing for either child.
- Keep everything from the previous notebook that is still true; add what is new; fix what changed.
- Write only facts that happened in the conversations. Never invent.
- Short notes in Hebrew. The whole notebook must stay under 1500 characters: merge and drop old details when needed.
- No addresses, phone numbers, school names or other private details.

Return only this JSON (no explanation):
```json
{
  "לביא": {
    "חשבון": {"רמה": "", "הצליח": [], "מתקשה": []},
    "אנגלית": {"יודע": [], "מתקשה": []},
    "סיפור": "איפה עצרנו",
    "אוהב": [],
    "בפעם הבאה": ""
  },
  "אורי": {
    "חשבון": {"רמה": "", "הצליח": [], "מתקשה": []},
    "אנגלית": {"יודע": [], "מתקשה": []},
    "סיפור": "איפה עצרנו",
    "אוהב": [],
    "בפעם הבאה": ""
  },
  "עודכן": "YYYY-MM-DD"
}
```
'''
m = open(MEM, encoding="utf-8").read()
new_m, n = re.subn(r'short_term_memory_prompt = """.*?"""', lambda _: 'short_term_memory_prompt = """' + NOTEBOOK + '"""', m, count=1, flags=re.S)
assert n == 1, "memory prompt not found"
open(MEM, "w", encoding="utf-8").write(new_m)
print("notebook prompt: ok")

# ---- 2. the prompt template gets the notebook ----
t = open(TPL, encoding="utf-8").read()
if "<memory>" not in t:
    block = (
        "<progress_notebook>\n"
        "What you remember about each child from earlier conversations (may be empty):\n"
        "<memory>\n</memory>\n"
        "Use it naturally: continue from where you stopped, practice what was hard, and move up a level after a few successes in a row. "
        "Never read the notebook aloud and never mention that you have one.\n"
        "</progress_notebook>\n\n"
    )
    t = t.replace("<context>", block + "<context>", 1)
    open(TPL, "w", encoding="utf-8").write(t)
print("template memory block:", "<memory>" in open(TPL, encoding="utf-8").read())

# ---- 3. config: who is who + memory on (notebook written by Gemini, kept on this server) ----
c = open(CFG, encoding="utf-8").read()
WHO = (
    "  [מי מדבר]\n"
    "  - יש שני ילדים: לביא בן 10, ואורי בן 7.\n"
    "  - כשהודעה מתחילה ב\"זה לביא\" או \"זה אורי\", הילד לחץ על הכפתור שלו במסך: תגיד לו שלום בשמו ותציע להמשיך ממה שעשיתם בפעם הקודמת.\n"
    "  - אם אתה לא יודע מי מדבר, תשאל בקצרה: \"מי מדבר איתי, לביא או אורי?\"\n"
    "  - עם אורי (7): משפטים קצרים מאוד, מילים פשוטות, הרבה עידוד.\n"
    "  - עם לביא (10): קצת יותר מידע ואתגרים, אבל עדיין קצר.\n"
)
c, n1 = re.subn(r"(?ms)^  \[מי מדבר\]\n.*?(?=^  \[מה אתה עושה\])", lambda _: WHO, c, count=1)
c, n2 = re.subn(r"(?m)^(\s+Memory:\s*)\w+", r"\g<1>mem_local_short", c, count=1)
if not re.search(r"(?m)^Memory:", c):
    c = c.rstrip("\n") + "\n\nMemory:\n  mem_local_short:\n    type: mem_local_short\n    llm: RobiGemini\n"
open(CFG, "w", encoding="utf-8").write(c)
print("who-is-who:", n1, "memory selected:", n2)

import yaml  # noqa: E402
y = yaml.safe_load(open(CFG, encoding="utf-8"))
print("check:", y["selected_module"]["Memory"], y["Memory"]["mem_local_short"], "לביא" in y["prompt"])
