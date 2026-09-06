"""mip_diag_build.py -- which mip levels of a loose 4K texture does RE2 RT actually sample on the hands?

Builds a diagnostic pl3000_Body_ALBM.tex.34 whose mip chain is spliced from two sources:
  mips 0-1 (4096, 2048)  : the real albedo with LOUD GREEN BANDS painted over the hand + forearm UV region
  mips 2-12 (1024 .. 1)  : the real albedo's own mips, untouched (the reference tex is copied and only mips 0-1 overwritten)
In the headset, on the hands:
  green bands            -> the engine samples the 4K/2K mips of a loose texture; 4K authoring is worth it
  no bands, normal skin  -> the engine caps a loose texture at the shipped 1024 chain; author at 1K or find the cap
  bands only up close    -> distance-based streaming; the 4K detail exists but only within that range

  py mip_diag_build.py --albm <4K pl3000_Body_ALBM.png> --hand-mask <hand_mask_4096.png> --ref-tex <a 4K pl3000_Body_ALBM.tex.34> --out <folder>

The output is a derivative of a game texture: built locally, never shipped. Restore the real ALBM afterwards.
"""
import argparse, ctypes, os, struct, subprocess, sys, zlib
import numpy as np
REMESH_DIR = r"D:\RE2 REFramework builds\tools\RE-Mesh-Editor"
ap = argparse.ArgumentParser()
ap.add_argument("--albm", required=True); ap.add_argument("--hand-mask", required=True); ap.add_argument("--ref-tex", required=True); ap.add_argument("--out", required=True)
ap.add_argument("--remesh-dir", default=REMESH_DIR); ap.add_argument("--blender", default=r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe")
a = ap.parse_args()
work = os.path.join(a.out, "_work"); os.makedirs(work, exist_ok=True)

# ---- 1. the two source PNGs, made in headless Blender (its image loader/scaler; a pure-python PNG writer) --------------
blend_code = r'''
import bpy, sys, numpy as np, zlib, struct
albm, mask, out = sys.argv[sys.argv.index("--") + 1:]
def load(p, size=None):
    img = bpy.data.images.load(p)
    if size and img.size[0] != size: img.scale(size, size)
    w, h = img.size; px = np.empty(w * h * 4, np.float32); img.pixels.foreach_get(px); return np.flipud(px.reshape(h, w, 4)).copy()
def save(arr, path):
    arr8 = (np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8); raw = b"".join(bytes([0]) + arr8[i].tobytes() for i in range(arr8.shape[0]))
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    open(path, "wb").write(bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]) + chunk(b"IHDR", struct.pack(">IIBBBBB", arr8.shape[1], arr8.shape[0], 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
A = load(albm); N = A.shape[0]; M = load(mask, N)[..., 0] > 0.5
striped = A.copy()
band = ((np.arange(N) // 48) % 2 == 0)[:, None] & M            # horizontal bands 48 texels tall at 4K (~9 mm on the hand)
striped[band, 0] = 0.05; striped[band, 1] = 0.9; striped[band, 2] = 0.05
save(striped, out + "/striped_4096.png")
print("PNGS DONE")
'''
open(os.path.join(work, "_mk.py"), "w").write(blend_code)
r = subprocess.run([a.blender, "-b", "--python", os.path.join(work, "_mk.py"), "--", a.albm, a.hand_mask, work], capture_output=True, text=True)
if "PNGS DONE" not in r.stdout: raise SystemExit("blender step failed:\n" + r.stdout[-2000:] + r.stderr[-2000:])

# ---- 2. both to BC7 sRGB DDS with mips (converter in a child process; it has crashed on teardown before) ----------------
def to_dds(png):
    code = ("import sys, ctypes; sys.path.insert(0, %r); ctypes.windll.ole32.CoInitializeEx(None, 0); "
            "from modules.tex import re_tex_utils as TU; TU.ImageListToDDS([(%r, 'BC7_UNORM_SRGB')], outDir=%r, generateMipMaps=True)") % (a.remesh_dir, png, work)
    subprocess.run([sys.executable, "-c", code], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    dds = png[:-4] + ".dds"
    if not os.path.exists(dds): raise SystemExit("no DDS for " + png)
    return open(dds, "rb").read()
S = to_dds(os.path.join(work, "striped_4096.png"))
def hdr_len(d): return 148 if d[84:88] == b"DX10" else 128
def mip_sizes(w, n):
    out = []; s = w
    for _ in range(n): out.append(max(1, s // 4) * max(1, s // 4) * 16); s = max(1, s // 2)
    return out
sm = mip_sizes(4096, 13)
Spay = S[hdr_len(S):]
assert len(Spay) == sum(sm), (len(Spay), sum(sm))

# ---- 3. splice INTO a copy of the reference tex: its header (248 bytes) carries a table of absolute mip offsets
#         (u32 at 0x20 + 16*k + 8), small mips are row-padded to 256 bytes, so only mips 0 and 1 are overwritten in place
ref = bytearray(open(a.ref_tex, "rb").read())
offs = [struct.unpack_from("<I", ref, 0x20 + 16 * k + 8)[0] for k in range(13)]
assert offs[0] == 248 and offs[1] == 248 + sm[0], offs[:3]
ref[offs[0]:offs[0] + sm[0]] = Spay[:sm[0]]
ref[offs[1]:offs[1] + sm[1]] = Spay[sm[0]:sm[0] + sm[1]]
dest = os.path.join(a.out, "natives", "stm", "sectionroot", "character", "player", "pl3000", "pl3000"); os.makedirs(dest, exist_ok=True)
out = os.path.join(dest, "pl3000_Body_ALBM.tex.34"); open(out, "wb").write(ref)
w, hh = struct.unpack_from("<HH", ref, 8); f = struct.unpack_from("<I", ref, 0x10)[0]; h = 248
print("wrote %s: %dx%d dxgi=%d, header %d bytes, mips 0-1 striped (green bands over the hands), 2-12 plain" % (out, w, hh, f, h))
