"""xrefs.py <VA-hex> [...] : static xrefs in re2.exe to a VA: E8/E9 rel32 call/jmp sites in .text,
RIP-relative lea/mov sites, and 8-byte absolute pointers anywhere (vtables / function tables).
Also prints, for each call site, the nearest il2cpp_dump method whose address is <= the site."""
import sys, re, struct, io, bisect, pefile
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
EXE = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2\re2.exe"
DUMP = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2\il2cpp_dump.json"
pe = pefile.PE(EXE, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
data = open(EXE, "rb").read()
secs = [(s.Name.rstrip(b"\0").decode(), s.VirtualAddress, max(s.Misc_VirtualSize, s.SizeOfRawData), s.PointerToRawData, s.SizeOfRawData) for s in pe.sections]
def off2va(off):
    for n, va, vs, po, ps in secs:
        if po <= off < po + ps:
            return base + va + (off - po)
    return None
def va2off(va):
    rva = va - base
    for n, va_, vs, po, ps in secs:
        if va_ <= rva < va_ + vs:
            return rva - va_ + po
    return None

# method index from the dump
dump = open(DUMP, "rb").read()
addrs = []
cur_type = None; cur_method = None
for m in re.finditer(rb'\n    "([^"]+)": \{|\n            "([^"]+)": \{|"function": "([0-9a-f]+)"', dump):
    if m.group(1): cur_type = m.group(1)
    elif m.group(2): cur_method = m.group(2)
    else:
        try: addrs.append((int(m.group(3), 16), cur_type.decode(), cur_method.decode()))
        except Exception: pass
addrs.sort()
keys = [a[0] for a in addrs]
def owner(va):
    i = bisect.bisect_right(keys, va) - 1
    if i < 0: return "?"
    a = addrs[i]
    return "%s.%s (+0x%x)" % (a[1], a[2], va - a[0])

text = [s for s in secs if s[0] == ".text"][0]
tpo, tps = text[3], text[4]
for arg in sys.argv[1:]:
    tgt = int(arg, 16)
    print("=== xrefs to 0x%x  [%s]" % (tgt, owner(tgt)))
    # rel32 call/jmp in .text
    i = tpo
    end = tpo + tps
    hits = 0
    while True:
        j = data.find(b"\xe8", i, end)
        k = data.find(b"\xe9", i, end)
        cands = [x for x in (j, k) if x >= 0]
        if not cands: break
        p = min(cands)
        i = p + 1
        if p + 5 > end: break
        rel = struct.unpack_from("<i", data, p + 1)[0]
        site = off2va(p)
        if site is None: continue
        if site + 5 + rel == tgt:
            print("  %s @0x%x  in %s" % ("call" if data[p] == 0xe8 else "jmp ", site, owner(site)))
            hits += 1
    # RIP-relative lea (48 8d 05/0d/15/...) : opcode 8D with modrm mod=00 rm=101
    for m in re.finditer(rb"[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]", data[tpo:end]):
        p = tpo + m.start()
        rel = struct.unpack_from("<i", data, p + 3)[0]
        site = off2va(p)
        if site + 7 + rel == tgt:
            print("  lea  @0x%x  in %s" % (site, owner(site)))
            hits += 1
    # absolute 8-byte pointers anywhere in the image
    needle = struct.pack("<Q", tgt)
    p = 0
    while True:
        p = data.find(needle, p)
        if p < 0: break
        va = off2va(p)
        if va is not None:
            print("  ptr  @0x%x  (section %s)" % (va, [s[0] for s in secs if s[3] <= p < s[3] + s[4]][0]))
            hits += 1
        p += 1
    print("  total %d" % hits)
