"""Read-only motlist header/entry dumper. Layout per CAF motlist_format_guide.md (MIT).
Prints container version, name, entry count, and per-entry mot version/name/frames/bones.
Works on v85 (non-RT) and is used here to PROBE v524 (RT) - fields that look wrong are reported as such.
"""
import struct, sys, os

def u16s(data, off, maxlen=256):
    end = off
    while end + 1 < len(data) and not (data[end] == 0 and data[end + 1] == 0):
        end += 2
        if end - off > maxlen * 2:
            break
    try:
        return data[off:end].decode("utf-16-le", errors="replace")
    except Exception:
        return "?"

def peek(path, verbose=True):
    d = open(path, "rb").read()
    ver, magic = struct.unpack_from("<I4s", d, 0)
    ptrs, col, nameoff = struct.unpack_from("<QQQ", d, 0x10)
    num = struct.unpack_from("<I", d, 0x30)[0]
    name = u16s(d, nameoff)
    print("FILE %s  size=%d" % (os.path.basename(path), len(d)))
    print("  container ver=%d magic=%r ptrs=0x%x col=0x%x nameoff=0x%x num=%d name=%s" % (ver, magic, ptrs, col, nameoff, num, name))
    if num == 0 or num > 4096:
        return
    offs = struct.unpack_from("<%dQ" % num, d, ptrs)
    seen_ver = set()
    for i, o in enumerate(offs):
        if o + 0x74 > len(d):
            print("  [%d] off=0x%x OUT OF RANGE" % (i, o)); continue
        mver, mmagic = struct.unpack_from("<I4s", d, o)
        namesoff = struct.unpack_from("<Q", d, o + 0x50)[0]
        fc, blend = struct.unpack_from("<ff", d, o + 0x58)
        bc, bcc = struct.unpack_from("<HH", d, o + 0x68)
        fr = struct.unpack_from("<H", d, o + 0x6E)[0]
        mname = u16s(d, o + namesoff) if o + namesoff < len(d) else "?"
        seen_ver.add((mver, mmagic))
        if verbose:
            print("  [%2d] off=0x%06x mver=%d %r name=%-28s frames=%-7.1f blend=%-5.1f bones=%d clips=%d fps=%d" % (i, o, mver, mmagic, mname, fc, blend, bc, bcc, fr))
    print("  entry versions seen: %s" % sorted(seen_ver))

for p in sys.argv[1:]:
    peek(p)
    print()
