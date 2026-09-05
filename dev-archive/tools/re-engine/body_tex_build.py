"""body_tex_build.py -- convert re-authored body textures (PNG) back into RE2 RT .tex.34 files, in place of
pl3000_Body_ALBM / pl3000_Body_NRMR, ready for the loose-file loader.

  py body_tex_build.py --in <folder with pl3000_Body_ALBM.png / pl3000_Body_NRMR.png> --out <folder> [--character pl3000]

ALBM  -> BC7_UNORM_SRGB (dxgi 99, as shipped), mipmaps generated
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
a = ap.parse_args()
sys.path.insert(0, a.remesh_dir)
ctypes.windll.ole32.CoInitializeEx(None, 0)
from modules.tex import re_tex_utils as TU   # noqa: E402

dest = os.path.join(a.out, "natives", "stm", "sectionroot", "character", "player", a.character, a.character)
work = os.path.join(a.out, "_work"); os.makedirs(dest, exist_ok=True); os.makedirs(work, exist_ok=True)
plan = [("%s_Body_ALBM" % a.character, "BC7_UNORM_SRGB", 99), ("%s_Body_NRMR" % a.character, "BC7_UNORM", 98)]
for name, fmt, dxgi in plan:
    src = os.path.join(a.inp, name + ".png")
    if not os.path.exists(src): raise SystemExit("missing " + src)
    png = os.path.join(work, name + ".png"); shutil.copyfile(src, png)
    TU.ImageListToDDS([(png, fmt)], outDir=work, generateMipMaps=True)
    dds = os.path.join(work, name + ".dds")
    if not os.path.exists(dds): raise SystemExit("DDS conversion failed for " + name)
    out = os.path.join(dest, name + ".tex.34")
    TU.DDSToTex([dds], 34, out)
    h = open(out, "rb").read(0x20); w, hh = struct.unpack_from("<HH", h, 8); f = struct.unpack_from("<I", h, 0x10)[0]
    print("%-20s %dx%d dxgi=%d (want %d) %d bytes -> %s" % (name, w, hh, f, dxgi, os.path.getsize(out), out))
    if f != dxgi: raise SystemExit("format mismatch on " + name)
print("install: copy %s into the game folder (loose loader on)" % os.path.join(a.out, "natives"))
