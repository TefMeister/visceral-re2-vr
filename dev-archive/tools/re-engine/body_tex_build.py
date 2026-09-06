"""body_tex_build.py -- convert re-authored body textures (PNG) back into RE2 RT .tex.34 files, in place of
<base>_ALBM / <base>_NRMR (Claire: pl1000_Jacket_*; Sherry: pl3000_Body_*), ready for the loose-file loader.

  py body_tex_build.py --in <folder with pl3000_Body_ALBM.png / pl3000_Body_NRMR.png> --out <folder> [--character pl3000]

ALBM  -> BC7_UNORM_SRGB (dxgi 99, as shipped), mipmaps generated
MSK1  -> BC4_UNORM      (dxgi 80, as shipped), only if pl3000_Body_MSK1.png is present (palm detail-mask, 2026-09-06)
NRMR  -> BC7_UNORM      (dxgi 98, as shipped), mipmaps generated
Output: <out>/natives/stm/sectionroot/character/player/<pl>/<pl>/<name>.tex.34  -- copy that tree into the game folder.
These outputs are derived from the game's own textures: they are built on the player's machine, never shipped.
Uses RE Mesh Editor's converter in place (NSACloud; DirectXTex inside).
"""
import argparse, ctypes, os, shutil, struct, sys
REMESH_DIR = r"D:\RE2 REFramework builds\tools\RE-Mesh-Editor"
ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("--in", dest="inp", required=True); ap.add_argument("--out", required=True)
ap.add_argument("--character", default="pl3000"); ap.add_argument("--remesh-dir", default=REMESH_DIR)
ap.add_argument("--tex-base", default=None, help="texture base name, default <character>_Body; Claire (pl1000) is pl1000_Jacket (her skin shares the jacket atlas)")
ap.add_argument("--src", default=None, help="extracted <pl> folder holding the shipped .tex.34 files (needed for MSK1: its header is reused)")
a = ap.parse_args()
sys.path.insert(0, a.remesh_dir)
ctypes.windll.ole32.CoInitializeEx(None, 0)
from modules.tex import re_tex_utils as TU   # noqa: E402

dest = os.path.join(a.out, "natives", "stm", "sectionroot", "character", "player", a.character, a.character)
work = os.path.join(a.out, "_work"); os.makedirs(dest, exist_ok=True); os.makedirs(work, exist_ok=True)
base = a.tex_base or ("%s_Body" % a.character)
plan = [(base + "_ALBM", "BC7_UNORM_SRGB", 99), (base + "_NRMR", "BC7_UNORM", 98), (base + "_MSK1", "BC4_UNORM", 80)]          # MSK1 (detail mask) is optional: only built when its PNG is present
for name, fmt, dxgi in plan:
    src = os.path.join(a.inp, name + ".png")
    if not os.path.exists(src):
        if name.endswith("_MSK1"): print("no MSK1 png, skipping"); continue
        raise SystemExit("missing " + src)
    png = os.path.join(work, name + ".png"); shutil.copyfile(src, png)
    dds = os.path.join(work, name + ".dds")
    if fmt == "BC4_UNORM":
        # the converter writes a correct BC4 DDS and then segfaults on teardown (2026-09-06): run it in a child process
        import subprocess
        if os.path.exists(dds): os.remove(dds)
        code = ("import sys, ctypes; sys.path.insert(0, %r); ctypes.windll.ole32.CoInitializeEx(None, 0); "
                "from modules.tex import re_tex_utils as TU; TU.ImageListToDDS([(%r, %r)], outDir=%r, generateMipMaps=True)") % (a.remesh_dir, png, fmt, work)
        subprocess.run([sys.executable, "-c", code], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        TU.ImageListToDDS([(png, fmt)], outDir=work, generateMipMaps=True)
    if not os.path.exists(dds): raise SystemExit("DDS conversion failed for " + name)
    out = os.path.join(dest, name + ".tex.34")
    if fmt == "BC4_UNORM" and a.src:
        # DDSToTex segfaults on BC4 (2026-09-06). The shipped MSK1 has the same size/format/mip chain, so transplant its
        # 160-byte tex header onto the new DDS payload (header sizes verified equal: 2796376 - 2796216).
        orig = open(os.path.join(a.src, name.lower() + ".tex.34"), "rb").read(); payload = open(dds, "rb").read()[128:]
        hdr = len(orig) - len(payload)
        if hdr <= 0 or hdr > 4096: raise SystemExit("MSK1: payload size does not match the shipped texture (%d vs %d)" % (len(payload), len(orig)))
        open(out, "wb").write(orig[:hdr] + payload)
    else:
        TU.DDSToTex([dds], 34, out)
    h = open(out, "rb").read(0x20); w, hh = struct.unpack_from("<HH", h, 8); f = struct.unpack_from("<I", h, 0x10)[0]
    print("%-20s %dx%d dxgi=%d (want %d) %d bytes -> %s" % (name, w, hh, f, dxgi, os.path.getsize(out), out))
    if f != dxgi: raise SystemExit("format mismatch on " + name)
print("install: copy %s into the game folder (loose loader on)" % os.path.join(a.out, "natives"))
