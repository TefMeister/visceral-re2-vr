"""rtex_resize.py -- write a loose copy of a Record-system render-target asset (.rtex.5) with a larger target size.

The Record system paints dirt/blood/wet onto the player through a runtime render target whose size is declared in
this 64-byte asset (width at +0x10, height at +0x14; 512x512 shipped for pl1000_body / pl3000_body) `[measured
2026-09-06]`. Both hands share about a quarter of it on UVMap1, which is why the grime looks low-res up close in VR.

  py rtex_resize.py <shipped .rtex.5> <out .rtex.5> [--size 2048]

Only the two size fields change; everything else is copied verbatim. Deploy as
<RE2>\natives\STM\VFX\RecordSystem\RecordTexture\pl1000\pl1000_body.rtex.5 and restart; delete the file to revert.
What the look decides: grime/blood edges on the hands sharper than before = the size is honoured (`[hypothesis]`
until seen); unchanged = the size is decided elsewhere (the system's own config) or the asset is cached; no dirt at
all / a crash = the other fields depend on the size (mip count or a buffer) and this needs the format decoded first.
"""
import argparse, os, struct
ap = argparse.ArgumentParser()
ap.add_argument("src"); ap.add_argument("out"); ap.add_argument("--size", type=int, default=2048)
a = ap.parse_args()
b = bytearray(open(a.src, "rb").read())
assert b[:4] == b"RTEX" and len(b) == 64, "not a 64-byte RTEX asset"
w, h = struct.unpack_from("<II", b, 0x10)
struct.pack_into("<II", b, 0x10, a.size, a.size)
os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
open(a.out, "wb").write(b)
print("%s: %dx%d -> %dx%d, %d bytes, fields: %s" % (a.out, w, h, a.size, a.size, len(b), [struct.unpack_from("<I", b, i)[0] for i in range(0, 64, 4)]))
