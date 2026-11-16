# Run inside the WLED folder. Registers the usermod, bumps build number, patches names.
import os, re
# 1) register the usermod
p = "wled00/usermods_list.cpp"
s = open(p, encoding="utf-8").read()
s = s.replace('#include "wled.h"', '#include "wled.h"\n#ifdef USERMOD_ASHISH_CLOCK7SEG_BAT\n  #include "../usermods/ashish_clock_7seg_battery/usermod_ashish_clock_7seg_battery.h"\n#endif', 1)
i = s.index("void registerUsermods()")
j = s.index("{", i) + 1
s = s[:j] + "\n  #ifdef USERMOD_ASHISH_CLOCK7SEG_BAT\n  usermods.add(new UsermodAshishClock7SegBat());\n  #endif" + s[j:]
open(p, "w", encoding="utf-8").write(s)
print("usermod registered")
# 2) unique build number
p = "wled00/wled.h"
s = open(p, encoding="utf-8").read()
m = re.search(r"#define VERSION (\d+)", s)
if m:
    newv = int(m.group(1)) + int(os.environ.get("RUN_NUMBER", "0"))
    s = s[:m.start()] + "#define VERSION %d" % newv + s[m.end():]
    open(p, "w", encoding="utf-8").write(s)
    print("build number", m.group(1), "->", newv)
# 3) welcome page name
try:
    p = "wled00/data/welcome.htm"
    s = open(p, encoding="utf-8").read()
    tag = '\n\t\t\t<h2 style="margin:0 0 14px 0;color:#ffb300;letter-spacing:1px"><b>Ashish Automation</b></h2>'
    m = re.search(r"<h1[^>]*>[^<]*Welcome[^<]*</h1>", s)
    if m:
        s = s[:m.end()] + tag + s[m.end():]
        open(p, "w", encoding="utf-8").write(s)
        print("welcome page patched")
except Exception as e:
    print("WARNING welcome patch:", e)
# 4) info page name
try:
    p = "wled00/data/index.js"
    lines = open(p, encoding="utf-8").read().split("\n")
    tag = '<br><b style="font-size:22px;color:#ffb300">Ashish Automation</b>'
    for n, line in enumerate(lines):
        if "${i.ver}" in line and "<br><br><table>" in line:
            k = line.rfind("<br><br><table>")
            lines[n] = line[:k] + tag + line[k:]
            print("info page name added")
            break
    open(p, "w", encoding="utf-8").write("\n".join(lines))
except Exception as e:
    print("WARNING info patch:", e)
