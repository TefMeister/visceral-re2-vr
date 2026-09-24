"""disasm2.py <VA-hex> <count> : like disasm.py but never stops at ret, and names call/jmp targets
from il2cpp_dump.json (nearest lower method start)."""
import sys, re, io, bisect, pefile, capstone
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
EXE = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2\re2.exe"
DUMP = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2\il2cpp_dump.json"
pe = pefile.PE(EXE, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
data = open(EXE, "rb").read()
def va2off(va):
    rva = va - base
    for s in pe.sections:
        if s.VirtualAddress <= rva < s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData):
            return rva - s.VirtualAddress + s.PointerToRawData
    raise SystemExit("VA not in a section")
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
def owner(va):
    i = bisect.bisect_right(keys, va) - 1
    if i < 0: return "?"
    a = addrs[i]
    d = va - a[0]
    short = a[1].split(".")[-1] + "." + re.sub(r"\d+$", "", a[2])
    return short if d == 0 else "%s+0x%x" % (short, d)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = True
va = int(sys.argv[1], 16)
count = int(sys.argv[2]) if len(sys.argv) > 2 else 200
off = va2off(va)
n = 0
for ins in md.disasm(data[off:off + 65536], va):
    note = ""
    if ins.mnemonic in ("call", "jmp") and ins.op_str.startswith("0x"):
        note = "  ; " + owner(int(ins.op_str, 16))
    for op in ins.operands:
        if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
            tgt = ins.address + ins.size + op.mem.disp
            note = "  ; rip-> 0x%x" % tgt
            try:
                o = va2off(tgt)
                q = int.from_bytes(data[o:o + 8], "little")
                note += " = 0x%x" % q
            except SystemExit:
                pass
    print("%x  %-7s %s%s" % (ins.address, ins.mnemonic, ins.op_str, note))
    n += 1
    if n >= count:
        break
