# Robi: hearing back to Gemini (thinking off, ~1 s, far better with children's Hebrew),
# and Robi offers to continue only when the notebook really has something.
import re
import shutil
CFG = "/opt/robi/server/data/.config.yaml"
shutil.copy(CFG, CFG + ".bak-ears")
c = open(CFG, encoding="utf-8").read()
c, n1 = re.subn(r"(?m)^([ \t]+ASR:[ \t]*)GroqASR\b", r"\g<1>RobiGeminiASR", c, count=1)
old = "תגיד לו שלום בשמו ותציע להמשיך ממה שעשיתם בפעם הקודמת."
new = ("תגיד לו שלום בשמו. אם במחברת יש משהו עליו, תציע להמשיך משם; אם אין, תציע משהו כיפי. "
       "אל תמציא דברים שלא קרו.")
n2 = c.count(old)
c = c.replace(old, new)
open(CFG, "w", encoding="utf-8").write(c)
import yaml  # noqa: E402
y = yaml.safe_load(open(CFG, encoding="utf-8"))
print("ASR:", y["selected_module"]["ASR"], "| thinking:", y["ASR"]["RobiGeminiASR"].get("thinking_config"),
      "| greet fix:", n2)
