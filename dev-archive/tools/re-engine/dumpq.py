"""dumpq.py [-a] <type-name> [...] : print fields / methods of a type from il2cpp_dump.json.
-a prints every method; default hides plain get_/set_ accessors."""
import sys, json, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
P = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2\il2cpp_dump.json"
data = open(P, "rb").read()
show_all = "-a" in sys.argv
names = [a for a in sys.argv[1:] if a != "-a"]

def block(name):
    key = ('    "%s": {' % name).encode()
    i = data.find(key)
    if i < 0:
        return None
    j = data.find(b'\n    "', i + len(key))          # start of the next type
    if j < 0:
        j = len(data)
    txt = data[i + len(key) - 1 : j].rstrip()
    if txt.endswith(b","):
        txt = txt[:-1]
    return json.loads(txt.decode("utf-8", "replace"))

for name in names:
    b = block(name)
    print("=== %s" % name)
    if b is None:
        print("  (not found)"); continue
    print("  parent:", b.get("parent"), " addr:", b.get("address"))
    fs = b.get("fields", {})
    def off(f):
        o = f.get("offset_from_base", "0x0")
        return int(o, 16) if isinstance(o, str) else 0
    for fn, f in sorted(fs.items(), key=lambda kv: off(kv[1])):
        extra = " = %r" % (f["default"],) if "default" in f else ""
        print("  F %-6s %-44s %s%s" % (f.get("offset_from_base"), fn, f.get("type"), extra))
    for mn, m in sorted(b.get("methods", {}).items()):
        if not show_all and (mn.startswith("get_") or mn.startswith("set_")):
            continue
        ps = ", ".join("%s %s" % (p.get("type"), p.get("name")) for p in m.get("params", []))
        print("  M %s %s(%s)  @%s" % (m.get("returns", {}).get("type"), mn, ps, m.get("function")))
