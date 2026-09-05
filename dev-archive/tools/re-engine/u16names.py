"""List UTF-16LE strings that look like RE Engine motion names (pl##_...) in a file."""
import re, sys
for p in sys.argv[1:]:
    d = open(p, "rb").read()
    names = [m.group(0).decode("utf-16-le") for m in re.finditer(rb"(?:p\x00l\x00\d\x00\d\x00_\x00)(?:[\x20-\x7e]\x00){4,80}", d)]
    print("FILE", p.split("/")[-1], "count", len(names))
    for n in sorted(set(names)):
        print("  ", n, "x%d" % names.count(n))
