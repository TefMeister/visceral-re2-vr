"""owner.py <VA> ... : name the il2cpp_dump method that contains each VA (nearest lower method start)."""
import sys, re, io, bisect
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
DUMP = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2\il2cpp_dump.json"
dump = open(DUMP, "rb").read()
addrs = []
cur_type = None; cur_method = None
for m in re.finditer(rb'\n    "([^"]+)": \{|\n            "([^"]+)": \{|"function": "([0-9a-f]+)"', dump):
    if m.group(1): cur_type = m.group(1)
    elif m.group(2): cur_method = m.group(2)
    else:
        try: addrs.append((int(m.group(3), 16), cur_type.decode(), cur_method.decode() if cur_method else "?"))
        except Exception: pass
addrs.sort()
keys = [a[0] for a in addrs]
for arg in sys.argv[1:]:
    va = int(arg, 16)
    i = bisect.bisect_right(keys, va) - 1
    a = addrs[i]
    print("0x%x -> %s.%s @0x%x (+0x%x)" % (va, a[1], a[2], a[0], va - a[0]))
