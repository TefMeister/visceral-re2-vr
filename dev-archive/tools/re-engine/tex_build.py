"""tex_build.py -- generic PNG -> RE2 RT .tex.34 converter (any name, any output folder), for our own textures.

  py tex_build.py --out <folder> <name>=<format> [<name>=<format> ...] --in <png folder>
    e.g.  py tex_build.py --in tex --out "<game>/natives/stm/visceral" visceral_bracelet_leather_ALBM=BC7_UNORM_SRGB ^
             visceral_bracelet_leather_NRMR=BC7_UNORM visceral_bracelet_leather_ATOS=BC1_UNORM

Formats as shipped on Claire `[measured 2026-09-06]`: ALBM BC7_UNORM_SRGB (dxgi 99), NRMR BC7_UNORM (98), ATOS BC1_UNORM (71),
MSK1 BC4_UNORM (80). Mipmaps generated. BC4 and BC1 go through a child process + a header transplant when the
converter's DDSToTex will not take them (BC4 segfaulted on 2026-09-06; BC1 is tried directly first, then --ref).
Uses RE Mesh Editor's converter in place (NSACloud; DirectXTex inside). body_tex_build.py is the character-specific
version of this; this one is the same pipeline with the paths made explicit.
"""
import argparse, ctypes, os, shutil, struct, subprocess, sys
REMESH_DIR = r"D:\RE2 REFramework builds\tools\RE-Mesh-Editor"
DXGI = {"BC7_UNORM_SRGB": 99, "BC7_UNORM": 98, "BC1_UNORM": 71, "BC1_UNORM_SRGB": 72, "BC4_UNORM": 80}
ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("items", nargs="+", help="<name>=<format>")
ap.add_argument("--in", dest="inp", required=True); ap.add_argument("--out", required=True)
ap.add_argument("--remesh-dir", default=REMESH_DIR)
ap.add_argument("--ref", default=None, help="a shipped .tex.34 of the SAME size/format/mips whose header is transplanted when DDSToTex refuses a format")
a = ap.parse_args()
sys.path.insert(0, a.remesh_dir)
ctypes.windll.ole32.CoInitializeEx(None, 0)
from modules.tex import re_tex_utils as TU   # noqa: E402

work = os.path.join(a.out, "_work"); os.makedirs(a.out, exist_ok=True); os.makedirs(work, exist_ok=True)
for item in a.items:
    name, fmt = item.split("=", 1)
    want = DXGI[fmt]
    src = os.path.join(a.inp, name + ".png")
    if not os.path.exists(src): raise SystemExit("missing " + src)
    png = os.path.join(work, name + ".png"); shutil.copyfile(src, png)
    dds = os.path.join(work, name + ".dds")
    if os.path.exists(dds): os.remove(dds)
    if fmt in ("BC4_UNORM", "BC1_UNORM", "BC1_UNORM_SRGB"):
        code = ("import sys, ctypes; sys.path.insert(0, %r); ctypes.windll.ole32.CoInitializeEx(None, 0); "
                "from modules.tex import re_tex_utils as TU; TU.ImageListToDDS([(%r, %r)], outDir=%r, generateMipMaps=True)") % (a.remesh_dir, png, fmt, work)
        subprocess.run([sys.executable, "-c", code], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        TU.ImageListToDDS([(png, fmt)], outDir=work, generateMipMaps=True)
    if not os.path.exists(dds): raise SystemExit("DDS conversion failed for " + name)
    out = os.path.join(a.out, name + ".tex.34")
    ok = False
    if fmt not in ("BC4_UNORM",):
        code = ("import sys, ctypes; sys.path.insert(0, %r); ctypes.windll.ole32.CoInitializeEx(None, 0); "
                "from modules.tex import re_tex_utils as TU; TU.DDSToTex([%r], 34, %r)") % (a.remesh_dir, dds, out)
        r = subprocess.run([sys.executable, "-c", code], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        ok = os.path.exists(out) and os.path.getsize(out) > 0x100
    if not ok:
        if not a.ref: raise SystemExit("DDSToTex failed for %s (%s) and no --ref given for a header transplant" % (name, fmt))
        orig = open(a.ref, "rb").read(); payload = open(dds, "rb").read()[128:]
        hdr = len(orig) - len(payload)
        if hdr <= 0 or hdr > 4096: raise SystemExit("%s: payload %d bytes does not match the reference %d" % (name, len(payload), len(orig)))
        open(out, "wb").write(orig[:hdr] + payload)
    h = open(out, "rb").read(0x20); w, hh = struct.unpack_from("<HH", h, 8); f = struct.unpack_from("<I", h, 0x10)[0]
    print("%-36s %dx%d dxgi=%d (want %d) %d bytes" % (name, w, hh, f, want, os.path.getsize(out)))
    if f != want: raise SystemExit("format mismatch on " + name)
print("done ->", a.out)
