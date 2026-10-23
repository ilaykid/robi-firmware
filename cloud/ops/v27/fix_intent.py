# Add robi_gifted to Intent.function_call.functions, whatever the layout. Prints the Intent block (masked).
import re, shutil, yaml
CFG = "/opt/robi/server/data/.config.yaml"
lines = open(CFG, encoding="utf-8").read().split("\n")
i0 = next(i for i, l in enumerate(lines) if re.match(r"^Intent:\s*$", l))
i1 = next((i for i in range(i0 + 1, len(lines)) if lines[i] and not lines[i].startswith((" ", "#"))), len(lines))
print("Intent block:")
for l in lines[i0:i1]:
    print("  |", re.sub(r"[A-Za-z0-9_-]{24,}", "<x>", l))
y = yaml.safe_load("\n".join(lines))
sel = y["selected_module"]["Intent"]
fc = next(i for i in range(i0, i1) if re.match(r"^\s+%s:\s*$" % re.escape(sel), lines[i]))
ind = len(lines[fc]) - len(lines[fc].lstrip())
end = next((i for i in range(fc + 1, i1) if lines[i].strip() and not lines[i].strip().startswith("#")
            and len(lines[i]) - len(lines[i].lstrip()) <= ind), i1)
fl = next((i for i in range(fc + 1, end) if re.match(r"^\s+functions:", lines[i])), None)
if "robi_gifted" not in (y["Intent"][sel].get("functions") or []):
    if fl is None:
        lines.insert(fc + 1, " " * (ind + 2) + "functions:")
        lines.insert(fc + 2, " " * (ind + 4) + "- robi_gifted")
    else:
        fi = len(lines[fl]) - len(lines[fl].lstrip())
        m = re.match(r"^(\s+functions:)\s*\[(.*)\]\s*$", lines[fl])
        if m:
            old = [x.strip() for x in m.group(2).split(",") if x.strip()]
            lines[fl] = m.group(1)
            for k, name in enumerate(["robi_gifted"] + old):
                lines.insert(fl + 1 + k, " " * (fi + 2) + "- " + name)
        else:
            lines.insert(fl + 1, " " * (fi + 2) + "- robi_gifted")
    shutil.copy(CFG, CFG + ".bak-v27b")
    open(CFG, "w", encoding="utf-8").write("\n".join(lines))
y = yaml.safe_load(open(CFG, encoding="utf-8"))
print("functions now:", y["Intent"][sel].get("functions"))
for n, l in enumerate(open(CFG, encoding="utf-8").read().split("\n")):
    if l.strip() == "- robi_gifted":
        print("robi_gifted at line", n + 1)
