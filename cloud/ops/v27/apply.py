# Robi 2.7 server: gifted practice with real OpenGifted questions for Ori AND Lavi
# (two LLM tools), report shows it. Runs as root from /opt/robi/stage27. Prints no secrets.
import re
import shutil

S = "/opt/robi/server"
CFG = S + "/data/.config.yaml"
shutil.copy(CFG, CFG + ".bak-v27")
shutil.copy("/opt/robi/stage27/robi_gifted.py", S + "/plugins_func/functions/robi_gifted.py")
shutil.copy("/opt/robi/stage27/robi_report.py", "/opt/robi/robi_report.py")

c = open(CFG, encoding="utf-8").read()

# 1. tools: robi_gifted in the function_call intent's list
m = re.search(r"(?m)^  function_call:\n(?:(?:    .*|\s*)\n)*?    functions:[ \t]*(\[\])?\n", c)
if "robi_gifted" not in c:
    if m:
        start = m.end()
        c = c[:m.start()] + m.group(0).replace("functions: []", "functions:") + "      - robi_gifted\n" + c[start:]
    else:
        c = re.sub(r"(?m)^(  function_call:\n    type: function_call\n)", r"\1    functions:\n      - robi_gifted\n", c, count=1)

# 2. prompt: gifted practice uses the tools, for both boys
GIFT = (
    "  [מחוננים]\n"
    "  - אורי (כיתה ב', שלב א') ולביא (מבחן כיתות ה'-ט') מתכוננים למבחן המחוננים. השאלות מגיעות רק מהכלי robi_gifted_question (שאלות אמיתיות של OpenGifted).\n"
    "  - כשילד מבקש להתאמן: קרא ל-robi_gifted_question עם השם שלו, ותקרא בדיוק את השאלה ואת האפשרויות הממוספרות. אל תמציא שאלות מחוננים בעצמך ואל תגלה את התשובה לפני שהוא עונה.\n"
    "  - כשהוא עונה: קרא ל-robi_gifted_answer (מספר האפשרות שבחר, או המספר שאמר). אם צדק: שבח קצר. אם טעה: התשובה הנכונה והסבר פשוט במשפט אחד. ואז שאל אם להמשיך.\n"
    "  - אם הוא מבקש לחזור על השאלה, תקרא אותה שוב בלי לקרוא לכלי. אם הוא לא יודע, תן רמז קטן אחד ולא את התשובה.\n"
    "  - בלי לחץ ובלי זמנים: זה משחק. אחרי כ-10 שאלות תציע הפסקה.\n"
    "\n"
)
c2, n = re.subn(r"(?ms)^  \[מחוננים - רק עם אורי\]\n.*?(?=^  \[רובי הטמגוצ'י\])", lambda _: GIFT, c, count=1)
if n == 0 and "  [מחוננים]\n" not in c:
    c2 = c.replace("  [רובי הטמגוצ'י]", GIFT + "  [רובי הטמגוצ'י]", 1)
open(CFG, "w", encoding="utf-8").write(c2)

import yaml  # noqa: E402
y = yaml.safe_load(open(CFG, encoding="utf-8"))
sel = y["selected_module"]["Intent"]
print("intent:", sel, "| functions:", y["Intent"][sel].get("functions"), "| gifted prompt:", "robi_gifted_question" in y["prompt"])
