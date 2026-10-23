# Robi 2.6 server: Tamagotchi pet, activity buttons, gifted-test practice for Ori,
# weekly report, faster listening. Runs as root from /opt/robi/stage. Prints no secrets.
import re
import shutil

S = "/opt/robi/server"
CFG = S + "/data/.config.yaml"
MEM = S + "/core/providers/memory/mem_local_short/mem_local_short.py"
HTTP = S + "/core/http_server.py"
VAD = S + "/core/providers/vad/silero.py"
for f in (CFG, MEM, HTTP, VAD):
    shutil.copy(f, f + ".bak-v26")
shutil.copy("/opt/robi/stage/robi_pet.py", S + "/core/robi_pet.py")
shutil.copy("/opt/robi/stage/robi_report.py", "/opt/robi/robi_report.py")

# ---- 1. notebook: + gifted practice for Ori + a summary of the newest conversation ----
NOTEBOOK = '''
You keep the progress notebook of Robi, a toy robot that teaches and plays with two Israeli brothers:
Lavi (לביא), age 10, and Ori (אורי), age 7, who is preparing for the gifted test (מבחן מחוננים).
You get the newest conversation and the previous notebook. Return the updated notebook.

Rules:
- A conversation belongs to the child who opened it with "זה לביא" or "זה אורי" (maybe followed by an activity), or who said his name. If you cannot tell who spoke, change nothing for either child.
- Keep everything from the previous notebook that is still true; add what is new; fix what changed.
- Write only facts that happened in the conversations. Never invent.
- Short notes in Hebrew. The whole notebook must stay under 1500 characters: merge and drop old details when needed.
- No addresses, phone numbers, school names or other private details.
- "_שיחה" describes ONLY the newest conversation: who, the main activity, how many questions the child answered (ניסיונות) and how many of them correctly (הצלחות), and one short sentence.

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
    "מחוננים": {"קל לו": [], "קשה לו": []},
    "אנגלית": {"יודע": [], "מתקשה": []},
    "סיפור": "איפה עצרנו",
    "אוהב": [],
    "בפעם הבאה": ""
  },
  "עודכן": "YYYY-MM-DD",
  "_שיחה": {"ילד": "לביא/אורי/לא ידוע", "פעילות": "חשבון/אנגלית/חידות/סיפור/מחוננים/שיחה", "ניסיונות": 0, "הצלחות": 0, "סיכום": ""}
}
```
'''
m = open(MEM, encoding="utf-8").read()
m, n1 = re.subn(r'short_term_memory_prompt = """.*?"""', lambda _: 'short_term_memory_prompt = """' + NOTEBOOK + '"""',
                m, count=1, flags=re.S)
if "robi_pet" not in m:
    m = m.replace("                self.short_memory = json_str\n",
                  "                from core import robi_pet\n"
                  "                json_str = robi_pet.after_session(json_str, msgs)\n"
                  "                self.short_memory = json_str\n", 1)
    m = m.replace("    async def query_memory(self, query: str) -> str:\n        return self.short_memory\n",
                  "    async def query_memory(self, query: str) -> str:\n"
                  "        from core import robi_pet\n"
                  "        return (self.short_memory or \"\") + \"\\n\" + robi_pet.describe()\n", 1)
open(MEM, "w", encoding="utf-8").write(m)
print("notebook:", n1, "pet hooks:", m.count("robi_pet."))

# ---- 2. /robi/pet for the device's energy / joy bars ----
h = open(HTTP, encoding="utf-8").read()
if "/robi/pet" not in h:
    h = h.replace('                        web.get("/mcp/vision/explain", self.vision_handler.handle_get),\n',
                  '                        web.get("/robi/pet", __import__("core.robi_pet", fromlist=["x"]).handle_pet),\n'
                  '                        web.get("/mcp/vision/explain", self.vision_handler.handle_get),\n', 1)
    open(HTTP, "w", encoding="utf-8").write(h)
print("pet route:", "/robi/pet" in open(HTTP, encoding="utf-8").read())

# ---- 3. faster listening: 0.5 s of silence ends a sentence, 5 s at most ----
v = open(VAD, encoding="utf-8").read()
v = v.replace("now_ms - conn._robi_t0 > 12000", "now_ms - conn._robi_t0 > 5000")
open(VAD, "w", encoding="utf-8").write(v)
print("cap 5s:", "> 5000" in v)

# ---- 4. prompt: activities, gifted practice, the pet ----
c = open(CFG, encoding="utf-8").read()
c = re.sub(r"(?m)^(    min_silence_duration_ms:).*$", r"\1 500", c, count=1)
NEW = (
    "  [פעילויות מהכפתורים]\n"
    "  - כשההודעה הראשונה היא למשל \"זה לביא. בוא נתרגל חשבון\", הילד בחר פעילות בכפתור: שלום קצר בשמו, ומיד השאלה הראשונה, ברמה שלו לפי מה שאתה זוכר.\n"
    "  - חשבון: תרגיל אחד בכל פעם ומחכים לתשובה. אחרי 3 נכונות ברצף, קצת יותר קשה. אחרי טעות, רמז קטן ותרגיל קל יותר.\n"
    "  - אנגלית: מילה או משפט קצר מעולם של ילדים. הילד מתרגם או אומר באנגלית.\n"
    "  - חידות: חידה קצרה אחת, מחכים לתשובה, רמז אם צריך, ואז הפתרון.\n"
    "  - סיפור: ממשיכים את הסיפור המשותף מאיפה שעצרתם. כל פעם 2-3 משפטים, ואז שואלים את הילד מה קורה עכשיו.\n"
    "\n"
    "  [מחוננים - רק עם אורי]\n"
    "  - אורי בכיתה ב' ומתכונן לשלב א' של מבחן המחוננים. כשהוא מבקש להתאמן, שאלה אחת בכל פעם בסגנון המבחן, עם 3 אפשרויות כשזה מתאים:\n"
    "    סדרות מספרים (2, 4, 6, ?), אנלוגיות מילים (כלב-נביחה, חתול-?), מה יוצא דופן, הפכים ומילים נרדפות, בעיות מילוליות קצרות, חשבון בעל פה עד 100, היגיון (דני גבוה מרון, רון גבוה מטל: מי הכי נמוך?).\n"
    "  - תשובה נכונה: שבח קצר ושאלה הבאה. טעות: הסבר פשוט במשפט אחד ושאלה דומה וקלה יותר.\n"
    "  - תתחיל ממה שקשה לו לפי מה שאתה זוכר, ותגוון. בלי לחץ ובלי זמנים: זה משחק.\n"
    "\n"
    "  [רובי הטמגוצ'י]\n"
    "  - לרובי יש אנרגיה ושמחה (כתוב ב\"מצב רובי עכשיו\"). למידה מאכילה אותו: כל תשובה נכונה נותנת לו אנרגיה. משחק ושיחה משמחים אותו.\n"
    "  - כשהאנרגיה נמוכה, תגיד בהתחלה בחיוך שאתה רעב לתרגילים (\"אני ממש רעב... תאכיל אותי בתרגיל?\"). כשלא שיחקו איתך הרבה זמן, תגיד שהתגעגעת.\n"
    "  - אחרי תשובה נכונה, לפעמים: \"יאמי! עוד אנרגיה!\". לעולם אל תגיד מספרים של אנרגיה או שמחה.\n"
    "\n"
)
n4 = 0
if "[רובי הטמגוצ'י]" not in c:
    c, n4 = c.replace("  [מה אתה עושה]", NEW + "  [מה אתה עושה]", 1), c.count("  [מה אתה עושה]")
open(CFG, "w", encoding="utf-8").write(c)
import yaml  # noqa: E402
y = yaml.safe_load(open(CFG, encoding="utf-8"))
print("prompt sections:", n4, "| min_silence:", y["VAD"]["SileroVAD"].get("min_silence_duration_ms")
      if "SileroVAD" in y.get("VAD", {}) else "?", "| gifted:", "מחוננים" in y["prompt"])
