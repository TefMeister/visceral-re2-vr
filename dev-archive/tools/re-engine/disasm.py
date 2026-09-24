"""disasm.py <VA-hex> [count] : disassemble re2.exe at a static VA with capstone (fast, local).
Stops at the first `ret` after `count` instructions unless count is reached first."""
import sys, pefile, capstone
P = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2\re2.exe"
pe = pefile.PE(P, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
data = open(P, "rb").read()

def va2off(va):
    rva = va - base
    for s in pe.sections:
        if s.VirtualAddress <= rva < s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData):
            return rva - s.VirtualAddress + s.PointerToRawData
    raise SystemExit("VA not in a section")

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = True
va = int(sys.argv[1], 16)
count = int(sys.argv[2]) if len(sys.argv) > 2 else 120
off = va2off(va)
n = 0
for ins in md.disasm(data[off:off + 4096], va):
    ops = ins.op_str
    note = ""
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
    print("%x  %-7s %s%s" % (ins.address, ins.mnemonic, ops, note))
    n += 1
    if n >= count or (ins.mnemonic == "ret" and n > 8):
        break
