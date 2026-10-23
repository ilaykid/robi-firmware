import re, shutil, yaml
CFG = "/opt/robi/server/data/.config.yaml"
shutil.copy(CFG + ".bak-v27b", CFG)
c = open(CFG, encoding="utf-8").read()
c2, n = re.subn(r"(?m)^(\s+)functions: \[\]([ \t]*#[^\n]*)?$", lambda m: m.group(1) + "functions:" + (m.group(2) or "") + "\n" + m.group(1) + "  - robi_gifted", c, count=1)
y = yaml.safe_load(c2)
f = y["Intent"][y["selected_module"]["Intent"]]["functions"]
assert f == ["robi_gifted"], f
open(CFG, "w", encoding="utf-8").write(c2)
print("fixed:", n, f)
