# Robi: never say "notebook" aloud; remember quietly like a friend would.
import shutil
S = "/opt/robi/server"
CFG = S + "/data/.config.yaml"
TPL = S + "/data/robi-prompt.txt"
for f in (CFG, TPL):
    shutil.copy(f, f + ".bak-quiet")
c = open(CFG, encoding="utf-8").read()
old = "אם במחברת יש משהו עליו, תציע להמשיך משם; אם אין, תציע משהו כיפי."
new = ("אם אתה זוכר משהו שעשיתם יחד, תציע להמשיך משם כמו חבר שזוכר "
       "(למשל: \"רוצה עוד תרגילי חשבון כמו בפעם הקודמת?\"); אם לא, תציע משהו כיפי. "
       "לעולם אל תגיד \"מחברת\", \"רשמתי\", \"ראיתי\" או \"זיכרון\".")
n = c.count(old)
c = c.replace(old, new)
# (the family joke "אבא הכי חמוד כאן" was removed on 28.10.2026 at dad's request)
open(CFG, "w", encoding="utf-8").write(c)
t = open(TPL, encoding="utf-8").read()
o2 = "Never read the notebook aloud and never mention that you have one."
n2 = ("Never read the notebook aloud and never mention that you have one: do not say "
      "the words notebook, memory, wrote down or saw (מחברת, זיכרון, רשמתי, ראיתי). "
      "Just remember naturally, like a friend.")
m = t.count(o2)
t = t.replace(o2, n2)
open(TPL, "w", encoding="utf-8").write(t)
import yaml  # noqa: E402
yaml.safe_load(open(CFG, encoding="utf-8"))
print("greet:", n, "template:", m, "yaml ok")
