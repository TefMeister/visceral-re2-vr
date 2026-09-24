"""dumpls.py <regex> : list type names in il2cpp_dump.json matching regex (case-insensitive).
dumpls.py --fn <hexprefix> : list methods whose function address starts with hexprefix (e.g. 14037b)."""
import sys, re, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
P = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2\il2cpp_dump.json"
data = open(P, "rb").read()
if sys.argv[1] == "--fn":
    pref = sys.argv[2].lower().encode()
    # find every '"function": "<addr>"' and report owning type + method name
    cur_type = None
    cur_method = None
    for m in re.finditer(rb'\n    "([^"]+)": \{|\n        "([^"]+)": \{|"function": "([0-9a-f]+)"', data):
        if m.group(1):
            cur_type = m.group(1)
        elif m.group(2):
            cur_method = m.group(2)
        else:
            fn = m.group(3)
            if fn.startswith(pref):
                print(fn.decode(), cur_type.decode() if cur_type else "?", cur_method.decode() if cur_method else "?")
else:
    rx = re.compile(sys.argv[1].encode(), re.I)
    for m in re.finditer(rb'\n    "([^"]+)": \{', data):
        n = m.group(1)
        if rx.search(n):
            print(n.decode())
