"""HD hands tier 1b (first, procedural pass): re-author Claire's body textures at 4K with real hand detail baked in.
Headless Blender 5.2 + RE Mesh Editor:
  blender -b --python hd_hands_paint.py -- <plXXXX folder> <work_dir> <out_dir> [--character pl1000 --skin-mat Body_Mat --tex-base pl1000_Jacket] [--size 4096] [--pores 1.0] [--crease 1.0] [--tone 1.0]

What it does, only inside the skin material's hand + forearm UV region (everything else is a clean upscale):
  1. upscales pl3000_Body_ALBM / _NRMR from 1024 to <size> (Blender's image scaler);
  2. finds every finger/wrist JOINT on the mesh as the zone where two adjacent bones share the skin weight
     (J = 2 * min(w1, w2), 1.0 at a 50/50 blend) and rasterises it into UV space -> knuckle / flexion creases
     as grooves in the normal map and a faint redness in the albedo, exactly where the skin folds;
  3. bakes skin micro-relief (our own tiling detail normal, tools/blender/make_skin_detail.py output) into the
     normal map at ~0.5 mm scale, so pores exist even where the material's detail slot is not used;
  4. adds low-frequency colour mottling to the albedo (real skin is not one colour).
Writes <out_dir>/pl3000_Body_ALBM.png and pl3000_Body_NRMR.png (RGBA, alpha channels carried over untouched:
ALBM alpha = metallic, NRMR alpha left as shipped). Conversion to .tex.34 is tools/re-engine/body_tex_build.py.
Outputs are derivatives of game textures: keep out of git; the player's copy is rebuilt by the scripts.
"""
import bpy, sys, os, importlib, ctypes, argparse
from mathutils import Vector
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
ap = argparse.ArgumentParser()
ap.add_argument("src"); ap.add_argument("work"); ap.add_argument("out")
ap.add_argument("--size", type=int, default=4096)
# 2026-09-06 13:15: pl3000 is SHERRY. Claire is pl1000, skin material pl1000_Body_Mat, textures pl1000_Jacket_* (shared atlas).
ap.add_argument("--character", default="pl1000"); ap.add_argument("--skin-mat", default="Body_Mat", help="substring of the skin material name")
ap.add_argument("--tex-base", default="pl1000_Jacket", help="texture base name: inputs <base>_albm/_nrmr(.tex.34 or .png in work), outputs <base>_ALBM/_NRMR.png")
ap.add_argument("--pores", type=float, default=0.3, help="micro-relief strength (0 = none); 1.0 was the first pass, sandpaper in game")
ap.add_argument("--veins", type=float, default=0.0, help="dorsal vein relief lifted from the original albedo. OFF by default since 14:35: its high-pass fires along UV island borders (a crisp line round the wrist, a plate outline on the thumb); the rig-drawn veins replaced it")
ap.add_argument("--wrinkles", type=float, default=1.0, help="fine wrinkle networks around the joints (0 = none)")
ap.add_argument("--anatomy", type=float, default=0.0, help="rig-derived veins/tendons drawn along the mesh surface (off by default until judged in VR). The 2026-09-06 02:50 attempts picked the little-finger edge instead of the back of the hand; fixed 12:00 via the finger-curl sign")
ap.add_argument("--crease", type=float, default=1.0, help="joint crease depth (0 = none)")
ap.add_argument("--palm-pores", type=float, default=0.3, help="pore relief on the PALM side as a fraction of the back (baked relief AND the detail-mask texture)")
ap.add_argument("--tone", type=float, default=1.0, help="albedo variation strength (0 = none)")
ap.add_argument("--detail-png", default=None, help="tiling detail normal PNG (default <work>/visceral_skin_detail_NRM.png)")
ap.add_argument("--nails", type=float, default=1.0, help="redraw the nails from the rig (0 = leave the artist's 14-px blobs as upscaled): plate, lunula, free edge, cuticle + side folds, no pores on the plate. Added 2026-09-06 evening")
ap.add_argument("--nail-length", type=float, default=0.58, help="nail plate length as a fraction of the distal phalanx (cuticle sits at 1 - this)")
ap.add_argument("--nail-face-from-art", action="store_true", help="take each nail's facing direction from the mean normal of the artist's nail pixels. OFF by default: it moves every nail 12-16 deg, and Tefa judged the pass-16 nails (this off) perfect in VR on 2026-09-06. It was written because a Blender thumb render seemed to show the plate on the thumb's flank -- but that render's CAMERA uses the same hand-wide guess, so it may have been looking at the flank rather than the plate being on it. Unverified either way")
ap.add_argument("--pores-uv", action="store_true", help="sample the micro-relief from the tiling 2D detail map by TEXEL, the pre-2026-09-06-22:30 behaviour. Off by default because that locks the pore pattern to the texture grid, so it cannot match across a UV seam -- which is the hard line Tefa found at the base of the thumb, and the same mechanism behind the straight lines on the forearms")
ap.add_argument("--edge-repair", type=int, default=4, help="texels at the OUTER rim of each UV island to re-fill from that island's interior. The 1024 source is upscaled 4x before we touch it, and at an island border that interpolation drags whatever sits outside the island into its edge texels -- one texel of contamination at 1024 becomes four at 4K, which is a bright hard-edged fringe on the surface. 0 = off")
ap.add_argument("--gutter", type=int, default=6, help="texels of padding grown past every UV island edge. Our treatment used to stop exactly at the island border, leaving an untreated strip along every seam; the strip is what was left of the thumb line after the 3D pores went in. 0 = the old behaviour")
ap.add_argument("--pore-mm", type=float, default=0.62, help="pore size in millimetres for the 3D-coherent micro-relief")
ap.add_argument("--nail-min-px", type=int, default=400, help="drop any connected nail-plate blob smaller than this (the ten real nails are 1391-3683 texels at 4K)")
ap.add_argument("--nail-tip", type=float, default=1.0, help="where the nail's FREE EDGE sits along the distal phalanx (1.0 = the fingertip). Every pass before 16 ended at 0.92 and left a bare pad beyond the nail")
ap.add_argument("--nail-lunula", type=float, default=0.6, help="strength of the lighter crescent at the base of each nail (0 = none). Tefa liked the 16:53 look, which was 0.6")
ap.add_argument("--nail-fold", type=float, default=0.022, help="height of the raised skin fold just outside the plate (0 = none). 0.08 read as a swollen rim round every nail in VR, 2026-09-06")
ap.add_argument("--nail-width", type=float, default=0.62, help="plate width as a fraction of the finger's width at the distal phalanx (0.85 read as a cap over the whole tip)")
a = ap.parse_args(argv)
N = a.size
os.makedirs(a.out, exist_ok=True)
ctypes.windll.ole32.CoInitializeEx(None, 0)

def load_np(path, size=None):
    img = bpy.data.images.load(path)
    if size and img.size[0] != size:
        img.scale(size, size)
    w, h = img.size
    px = np.empty(w * h * 4, np.float32); img.pixels.foreach_get(px)
    return np.flipud(px.reshape(h, w, 4)).copy()        # row 0 = top

def save_np(arr, path):
    """Pure-python 8-bit RGBA PNG writer: bypasses Blender's colour management entirely, so the numbers we read
    (raw encoded bytes / 255) are the numbers we write. Both textures are treated as data here."""
    import zlib, struct
    arr8 = (np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8)
    filt = bytes([0])
    raw = b"".join(filt + arr8[i].tobytes() for i in range(arr8.shape[0]))
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    hdr = struct.pack(">IIBBBBB", arr8.shape[1], arr8.shape[0], 8, 6, 0, 0, 0)
    sig = bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A])
    open(path, "wb").write(sig + chunk(b"IHDR", hdr) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))

# ---- 1. decode + upscale ---------------------------------------------------------------------------
tex_utils = importlib.import_module("RE-Mesh-Editor.modules.tex.re_tex_utils")
Texconv = importlib.import_module("RE-Mesh-Editor.modules.ddsconv.directx.texconv").Texconv
tc = Texconv()
src_png = {}
TB = a.tex_base.lower()
for base in (TB + "_albm", TB + "_nrmr"):
    png = os.path.join(a.work, base + ".png")
    if not os.path.exists(png):
        dds = os.path.join(a.work, base + ".dds")
        tex_utils.convertTexFileToDDS(os.path.join(a.src, base + ".tex.34"), dds)
        png = tc.convert_to_png(dds, out=a.work, verbose=False)
    src_png[base] = png
alb = load_np(src_png[TB + "_albm"], N)          # Blender gives sRGB PNG pixels back as linear floats
nrm = load_np(src_png[TB + "_nrmr"], N)          # normal map: treated as data (we only nudge RG)
print("upscaled to %dx%d: albedo mean %s, nrmr mean %s" % (N, N, np.round(alb.mean((0, 1)), 3), np.round(nrm.mean((0, 1)), 3)))

# ---- 2. mesh -> per-pixel fields in UV space ----------------------------------------------------------
mesh_path = os.path.join(a.src, a.character + ".mesh.2109108288")
bpy.ops.re_mesh.importfile(filepath=mesh_path, directory=a.src, files=[{"name": os.path.basename(mesh_path)}],
                           clearScene=True, loadMaterials=False, loadMDFData=False)
skin = [o for o in bpy.data.objects if o.type == "MESH" and any(a.skin_mat in (m.name if m else "") for m in o.data.materials)][0]
me = skin.data; names = [g.name for g in skin.vertex_groups]; uv = me.uv_layers.active
HAND = ("l_hand_", "r_hand_", "l_arm_wrist", "r_arm_wrist"); FORE = ("l_arm_radius", "r_arm_radius")
FINGER = ("l_hand_", "r_hand_")

def weights(v):
    ws = sorted(((g.weight, names[g.group]) for g in v.groups), reverse=True)
    return ws

J = np.zeros(len(me.vertices), np.float32)      # jointness: 1 at a 50/50 blend between two skin-driving bones
K = np.zeros(len(me.vertices), np.float32)      # 1 on hands, 0.6 on forearm skin, 0 elsewhere (paint mask)
F = np.zeros(len(me.vertices), np.float32)      # 1 on finger bones (not wrist / palm root)
for v in me.vertices:
    ws = weights(v)
    if not ws: continue
    top = ws[0][1]
    if top.startswith(HAND): K[v.index] = 1.0
    elif top.startswith(FORE): K[v.index] = 1.0
    if len(ws) >= 2 and ws[0][1].startswith(FINGER) and ws[1][1].startswith(FINGER):        # finger joints + knuckles only (14:00: the
        J[v.index] = float(np.clip(2.0 * ws[1][0] / max(ws[0][0] + ws[1][0], 1e-6), 0, 1))    # wrist/forearm blend drew a red ring "like a tight band")
    if any(n.startswith(FINGER) and n.rsplit("_", 1)[-1].isdigit() and int(n.rsplit("_", 1)[-1]) >= 1 for w, n in ws[:2]):
        F[v.index] = 1.0

fieldJ = np.zeros((N, N), np.float32); fieldK = np.zeros((N, N), np.float32); fieldF = np.zeros((N, N), np.float32)
def raster(tri_uv, tri_vals, fields):
    pts = [(u * N, (1.0 - v) * N) for u, v in tri_uv]
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    x0, x1 = max(int(min(xs)) - 1, 0), min(int(max(xs)) + 2, N); y0, y1 = max(int(min(ys)) - 1, 0), min(int(max(ys)) + 2, N)
    if x1 <= x0 or y1 <= y0: return
    gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
    (ax, ay), (bx, by), (cx, cy) = pts
    det = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay)
    if abs(det) < 1e-9: return
    l1 = ((bx - gx) * (cy - gy) - (cx - gx) * (by - gy)) / det
    l2 = ((cx - gx) * (ay - gy) - (ax - gx) * (cy - gy)) / det
    l3 = 1.0 - l1 - l2
    inside = (l1 >= -0.003) & (l2 >= -0.003) & (l3 >= -0.003)
    for fld, vals in zip(fields, tri_vals):
        val = l1 * vals[0] + l2 * vals[1] + l3 * vals[2]
        sub = fld[y0:y1, x0:x1]
        sub[inside] = np.maximum(sub[inside], val[inside])

n_faces = 0
for poly in me.polygons:
    vi = list(poly.vertices)
    if max(K[i] for i in vi) <= 0: continue
    uvs = [tuple(uv.data[li].uv) for li in poly.loop_indices]
    for k in range(1, len(uvs) - 1):
        tri = (uvs[0], uvs[k], uvs[k + 1]); idx = (vi[0], vi[k], vi[k + 1])
        raster(tri, ([J[i] for i in idx], [K[i] for i in idx], [F[i] for i in idx]), (fieldJ, fieldK, fieldF))
    n_faces += 1
print("rasterised %d skin faces: paint mask %.1f%% of texture, joint field max %.2f, mean over mask %.3f" % (
    n_faces, 100 * (fieldK > 0).mean(), fieldJ.max(), fieldJ[fieldK > 0].mean()))

# ---- 2b. the same faces again, but writing each texel's 3D POSITION (assign, not max: coordinates are signed).
# This is what lets the micro-relief below be a function of where a texel is ON THE HAND rather than where it is in the
# texture, so two islands that meet in 3D get the same pores whatever the atlas does with them.
Mw = skin.matrix_world
vco_all = np.array([Mw @ v.co for v in me.vertices], np.float32)
fieldP3 = np.zeros((N, N, 3), np.float32)
def raster_assign_xyz(tri_uv, tri_vals, fields):
    pts = [(u * N, (1.0 - v) * N) for u, v in tri_uv]
    xs = [q[0] for q in pts]; ys = [q[1] for q in pts]
    x0, x1 = max(int(min(xs)) - 1, 0), min(int(max(xs)) + 2, N); y0, y1 = max(int(min(ys)) - 1, 0), min(int(max(ys)) + 2, N)
    if x1 <= x0 or y1 <= y0: return
    gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
    (ax_, ay_), (bx, by), (cx_, cy_) = pts
    det_ = (bx - ax_) * (cy_ - ay_) - (cx_ - ax_) * (by - ay_)
    if abs(det_) < 1e-9: return
    l1 = ((bx - gx) * (cy_ - gy) - (cx_ - gx) * (by - gy)) / det_
    l2 = ((cx_ - gx) * (ay_ - gy) - (ax_ - gx) * (cy_ - gy)) / det_
    l3 = 1.0 - l1 - l2
    inside = (l1 >= -0.003) & (l2 >= -0.003) & (l3 >= -0.003)
    for fld, vals in zip(fields, tri_vals):
        val = l1 * vals[0] + l2 * vals[1] + l3 * vals[2]
        sub = fld[y0:y1, x0:x1]; sub[inside] = val[inside]
for poly in me.polygons:
    vi = list(poly.vertices)
    if max(K[i] for i in vi) <= 0: continue
    uvs = [tuple(uv.data[li].uv) for li in poly.loop_indices]
    for k in range(1, len(uvs) - 1):
        tri = (uvs[0], uvs[k], uvs[k + 1]); idx = (vi[0], vi[k], vi[k + 1])
        raster_assign_xyz(tri, ([vco_all[i][0] for i in idx], [vco_all[i][1] for i in idx], [vco_all[i][2] for i in idx]),
                          (fieldP3[..., 0], fieldP3[..., 1], fieldP3[..., 2]))
print("rasterised 3D positions over the paint mask: %.1f%% of texture carries a position" % (100 * (np.abs(fieldP3).sum(2) > 0).mean()))

def blur(img, sigma):
    if sigma <= 0: return img
    y = np.fft.fftfreq(N)[:, None]; x = np.fft.fftfreq(N)[None, :]
    g = np.exp(-2 * (np.pi ** 2) * (sigma ** 2) * (x ** 2 + y ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(img) * g)).astype(np.float32)

# ---- 2c. PAD PAST EVERY ISLAND EDGE (2026-09-06 22:50). Tefa, after the 3D pores went in: the seam "blends in a lot
# better ... but still there are these triangle areas, where the seam is really visible". Second cause, independent of the
# first: every field here is rasterised from FACES, so it stops dead at an island's border, and the masks derived from it
# fade to nothing over the last texel or two. That leaves a hairline of UNTREATED skin along every seam -- and the two
# sides of a seam are different islands, so the hairline lands somewhere different on each, which is what draws the
# triangles. The cure is the standard one for atlases: grow the painted content a few texels into the gutter so the
# fade happens outside anything the surface actually shows. Kept small (6 texels at 4K = 1.1 mm) so it cannot reach a
# neighbouring island's interior -- this atlas is shared with the jacket.
fieldK_core = (fieldK > 0).copy()          # the true island footprint, before any padding is grown onto it
if a.gutter > 0:
    _base = fieldK > 0
    _grown = _base.copy()
    for _ in range(a.gutter):
        _grown |= np.roll(_grown, 1, 0) | np.roll(_grown, -1, 0) | np.roll(_grown, 1, 1) | np.roll(_grown, -1, 1)
    _new = _grown & ~_base
    _w = _base.astype(np.float32); _den = blur(_w, float(a.gutter) * 0.6) + 1e-6
    for _c in range(3):
        _f = blur(fieldP3[..., _c] * _w, float(a.gutter) * 0.6) / _den
        fieldP3[..., _c] = np.where(_new, _f, fieldP3[..., _c])
    for _fld in (fieldJ, fieldF):
        _f = blur(_fld * _w, float(a.gutter) * 0.6) / _den
        _fld[...] = np.where(_new, _f, _fld)
    fieldK[...] = np.where(_new, 1.0, fieldK)
    print("gutter: grew the paint mask by %d texels, %d texels of padding added (%.1f%% -> %.1f%% of the texture)"
          % (a.gutter, int(_new.sum()), 100 * _base.mean(), 100 * (fieldK > 0).mean()))


maskK = blur(fieldK, 1.5)                                    # soft edge so seams do not show
edge = np.clip(maskK, 0, 1)
# smooth skinning blends almost everywhere (mean J 0.44 over the hand), so keep only the near-50/50 core of each
# joint zone: that is where the skin actually folds. Then a few texels of blur so the groove has soft walls.
sharpJ = np.clip((fieldJ - 0.62) / 0.38, 0, 1) ** 2
creaseJ = blur(sharpJ, N / 1024.0 * 1.2)

# ---- 3. normal map: creases + micro-relief ----------------------------------------------------------
def nrm_from_height(h, scale):
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * 0.5
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * 0.5
    return -dx * scale, dy * scale                            # image rows go down, tangent +Y goes up

# crease groove: height dips where the joint field peaks; the ridged shape (J^2) keeps it narrow
h_crease = -(creaseJ ** 2) * 0.9 * a.crease
# micro-relief from our own tiling detail map, sampled at ~4.3 tiles across the strip (~0.5 mm per texel at 4K)
det_png = a.detail_png or os.path.join(a.work, "visceral_skin_detail_NRM.png")
det = load_np(det_png)                                       # 1024 tile, RG = normal xy (only used by --pores-uv now)
S = 10                                                       # tiles across the texture: finer than pass 1 (6), ~0.6 mm pores
ty = (np.arange(N) * det.shape[0] * S // N) % det.shape[0]; tx = (np.arange(N) * det.shape[1] * S // N) % det.shape[1]

# ---- MICRO-RELIEF FROM 3D POSITION (2026-09-06 22:30). Tefa found a hard line at the base of the thumb and read it
# exactly right: "it looks like this is where two layers of skin meet and the line clearly separates pores on the skin".
# It is a UV seam. The pore tile was sampled by TEXEL, so its pattern, orientation and scale are properties of the atlas,
# not of the hand: two islands that touch in 3D but sit apart in the atlas get unrelated pores, and the join between them
# is a visible edge. The vanilla normal map over the hands is flat `[measured 2026-09-06]`, so every pore there is ours and
# so is the seam. Same mechanism as the straight lines on the forearms.
# The fix is to make the relief a function of the surface point. A value noise on a 3D lattice is sampled at each texel's
# rasterised position, then the tangent-space normal is taken as the gradient of that height IN TEXEL SPACE -- so it stays
# a correct tangent-space normal for whatever the UV does locally, while the height itself is continuous across every seam.
def noise3(P, cell_m, seed, L=128):
    """value noise on a periodic 3D lattice of `cell_m` metre cells, trilinear, smoothstepped"""
    g = np.random.default_rng(seed).standard_normal((L, L, L)).astype(np.float32)
    q = P / cell_m
    i0 = np.floor(q).astype(np.int64)
    fr = (q - i0).astype(np.float32); fr = fr * fr * (3.0 - 2.0 * fr)
    ix = i0[..., 0] % L; iy = i0[..., 1] % L; iz = i0[..., 2] % L
    jx = (ix + 1) % L; jy = (iy + 1) % L; jz = (iz + 1) % L
    fx, fy, fz = fr[..., 0], fr[..., 1], fr[..., 2]
    c00 = g[ix, iy, iz] * (1 - fx) + g[jx, iy, iz] * fx
    c10 = g[ix, jy, iz] * (1 - fx) + g[jx, jy, iz] * fx
    c01 = g[ix, iy, jz] * (1 - fx) + g[jx, iy, jz] * fx
    c11 = g[ix, jy, jz] * (1 - fx) + g[jx, jy, jz] * fx
    return ((c00 * (1 - fy) + c10 * fy) * (1 - fz) + (c01 * (1 - fy) + c11 * fy) * fz).astype(np.float32)

if a.pores_uv:
    det_xy = det[np.ix_(ty, tx)][..., :2] * 2.0 - 1.0
    pore_h = None
else:
    _cell = a.pore_mm / 1000.0
    pore_h = noise3(fieldP3, _cell, 11) * 0.62 + noise3(fieldP3, _cell * 0.5, 12) * 0.38
    _m = fieldK > 0
    pore_h = (pore_h - float(pore_h[_m].mean())) / (float(pore_h[_m].std()) + 1e-9)
    # ...and the slope must be measured in METRES, not texels. The gradient of a height field taken per texel makes a
    # bump look deeper on an island with fewer texels per millimetre, so two islands of different density show different
    # pore depth even when the height itself matches perfectly. Dividing by the 3D distance each texel step covers makes
    # the slope a property of the surface, which is what has to agree across a seam.
    _dPx = (np.roll(fieldP3, -1, axis=1) - np.roll(fieldP3, 1, axis=1)) * 0.5
    _dPy = (np.roll(fieldP3, -1, axis=0) - np.roll(fieldP3, 1, axis=0)) * 0.5
    _sx = np.linalg.norm(_dPx, axis=2); _sy = np.linalg.norm(_dPy, axis=2)
    _mk = fieldK > 0
    _med = float(np.median(np.concatenate([_sx[_mk], _sy[_mk]])))
    _sx = np.where((_sx > 1e-7) & _mk, _sx, _med); _sy = np.where((_sy > 1e-7) & _mk, _sy, _med)
    _gx, _gy = nrm_from_height(pore_h, 1.0)
    _gx = _gx * (_med / _sx); _gy = _gy * (_med / _sy)
    print("pores: texel size over the mask %.3f-%.3f mm (median %.3f), gradient normalised to it"
          % (1000 * float(np.percentile(_sx[_mk], 5)), 1000 * float(np.percentile(_sx[_mk], 95)), 1000 * _med))
    # match the amplitude the tiling map produced, so nothing about the look changes except that the seams go
    _ref = det[np.ix_(ty, tx)][..., :2] * 2.0 - 1.0
    _sc = float(np.sqrt((_ref[_m] ** 2).mean()) / (np.sqrt((np.stack([_gx, _gy], -1)[_m] ** 2).mean()) + 1e-9))
    det_xy = np.stack([_gx * _sc, _gy * _sc], axis=-1).astype(np.float32)
    print("pores: 3D-coherent, %.2f mm cells, gradient scaled x%.1f to match the old tile's RMS tilt" % (a.pore_mm, _sc))
rng = np.random.default_rng(3)
# the low-frequency colour mottling was UV-locked too, so it also stepped across every seam. Same cure: a 3D noise,
# ~8 mm cells (2026-09-06 22:35). With --pores-uv it falls back to the old UV noise.
if a.pores_uv:
    mottle = blur(rng.standard_normal((N, N)).astype(np.float32), N / 90.0)
else:
    mottle = noise3(fieldP3, 0.008, 21)
_mm = fieldK > 0
mottle = (mottle - float(mottle[_mm].mean())) / (float(mottle[_mm].std()) + 1e-9)
# --- veins: the original albedo already carries faint dorsal veins as darker branching lines. Lift them: luminance
# high-pass at vein scale, keep the dark side, threshold softly -> a raised ridge in the normal + a cooler tint in colour.
lum = alb[..., :3].mean(2)
cool = alb[..., 2] - alb[..., 0]                              # blue minus red: veins are COOLER than skin; hair and grime are only darker
s1, s2 = N / 1024.0 * 1.5, N / 1024.0 * 7.0                   # vein width band (1.5-7 texels at 1K)
dog_l = blur(lum, s1) - blur(lum, s2)                         # negative on darker lines
dog_c = blur(cool, s1) - blur(cool, s2)                       # positive on cooler lines
veins = np.clip((-dog_l - 0.004) / 0.02, 0, 1) * np.clip((dog_c - 0.0015) / 0.006, 0, 1)
veins = blur(veins, N / 1024.0 * 1.2) * edge * (1.0 - 0.7 * fieldF) * a.veins
veins = np.clip(veins * 1.5, 0, 1)
print("veins: %.1f%% of the hand mask above 0.3, %.1f%% above 0.6" % (100 * (veins[fieldK > 0] > 0.3).mean(), 100 * (veins[fieldK > 0] > 0.6).mean()))
h_veins = veins * 0.6                                         # veins stand proud of the skin
# --- wrinkles: fine ridged noise, only near the joints (where skin bunches), finer than the crease itself
def fbm(octaves, base_freq, persistence=0.55):
    out = np.zeros((N, N), np.float32); amp = 1.0
    for o in range(octaves):
        f = base_freq * (2 ** o); n = rng.standard_normal((N, N)).astype(np.float32)
        n = blur(n, N / (2 * np.pi * f)); n /= (n.std() + 1e-9); out += amp * n; amp *= persistence
    return out / (out.std() + 1e-9)
rng = np.random.default_rng(3)
wr = -(np.abs(fbm(2, 900.0)) ** 0.6) * 0.5                    # narrow ridged valleys ~ 4-5 texels apart at 4K
near_joint = blur(np.clip((fieldJ - 0.45) / 0.55, 0, 1), N / 1024.0 * 3.0)
h_wrinkle = wr * near_joint * 0.18 * a.wrinkles
# --- anatomy from the rig: the back of each hand gets four extensor tendons (wrist -> each knuckle) and three
# dorsal veins (wrist -> the gaps between knuckles), drawn as curves in UV space between anchors found from the
# bones, and masked to the DORSAL side, which is the side facing away from the thumb's offset from the palm plane.
h_anat = np.zeros((N, N), np.float32); vein_lines = np.zeros((N, N), np.float32)
dorsal = np.ones((N, N), np.float32)                          # 1 on the back of the hands + forearms, 0 on the palms (set below)
fieldD = np.zeros((N, N), np.float32)
nail_plate = np.zeros((N, N), np.float32); nail_h = np.zeros((N, N), np.float32); nail_clean = np.zeros((N, N), np.float32)   # filled by paint_nail() below
nail_lun = np.zeros((N, N), np.float32); nail_free = np.zeros((N, N), np.float32); nail_cut = np.zeros((N, N), np.float32)
nail_shadow = np.zeros((N, N), np.float32); nail_groove = np.zeros((N, N), np.float32); nail_base = np.zeros((N, N, 3), np.float32)
nail_wipe = np.zeros((N, N), np.float32)
try:
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    M = skin.matrix_world; R3 = M.to_3x3()
    vco = np.array([M @ v.co for v in me.vertices], np.float32); vno = np.array([R3 @ v.normal for v in me.vertices], np.float32)
    vuv = np.zeros((len(me.vertices), 2), np.float32); seen = np.zeros(len(me.vertices), bool)
    for poly in me.polygons:
        for vi, li in zip(poly.vertices, poly.loop_indices):
            if not seen[vi]: vuv[vi] = uv.data[li].uv; seen[vi] = True
    def jpos(n):
        b = arm.data.bones.get(n); return np.array(arm.matrix_world @ b.head_local, np.float32) if b else None
    def unit(v): return v / (np.linalg.norm(v) + 1e-9)
    Dv = np.zeros(len(me.vertices), np.float32)
    topname = np.array([(weights(v) or [(0, "")])[0][1] for v in me.vertices])
    is_finger = np.array([bool(F[i]) for i in range(len(me.vertices))])
    def polyline(img, uvs, vals=None):
        """draw a UV polyline (value per sample, so the ends can taper), skipping jumps across islands"""
        if vals is None: vals = np.ones(len(uvs), np.float32)
        for (q0, q1), (v0, v1) in zip(zip(uvs[:-1], uvs[1:]), zip(vals[:-1], vals[1:])):
            if np.linalg.norm(q1 - q0) > 0.03: continue
            n = max(2, int(np.linalg.norm(q1 - q0) * N))
            t = np.linspace(0, 1, n)[:, None]; pts = (1 - t) * q0 + t * q1; vv = ((1 - t) * v0 + t * v1)[:, 0]
            xs = np.clip((pts[:, 0] * N).astype(int), 0, N - 1); ys = np.clip(((1 - pts[:, 1]) * N).astype(int), 0, N - 1)
            img[ys, xs] = np.maximum(img[ys, xs], vv)
    from mathutils.bvhtree import BVHTree
    def make_surface(cand):
        """BVH over the triangles whose vertices all belong to `cand` (one side of the hand), plus each triangle's UVs, so a 3D
        point projects to a UV by barycentric interpolation INSIDE the triangle -- smooth curves, not vertex-to-vertex stairs
        (the 2026-09-06 02:50 attempts snapped to the nearest vertex and drew zigzags)"""
        cset = set(int(i) for i in cand); tris = []; tuv = []
        for poly in me.polygons:
            vi = list(poly.vertices)
            if not all(i in cset for i in vi): continue
            uvs = [tuple(uv.data[li].uv) for li in poly.loop_indices]
            for k in range(1, len(vi) - 1):
                tris.append((vi[0], vi[k], vi[k + 1])); tuv.append((uvs[0], uvs[k], uvs[k + 1]))
        if not tris: return None
        verts = [tuple(map(float, vco[i])) for i in range(len(me.vertices))]
        return BVHTree.FromPolygons(verts, tris, all_triangles=True), tris, tuv
    hits = [0, 0]                                                          # ray hits / nearest-point fallbacks, printed per side
    def project(q, surf, lift, max_dist):
        """3D point (near the bone) -> UV on the one-sided surface: ray from inside the hand outward, nearest-point fallback"""
        bvh, tris, tuv = surf
        loc = None
        if lift is not None:
            d = Vector(tuple(map(float, lift))).normalized()
            loc, nrm, idx, dist = bvh.ray_cast(Vector(tuple(map(float, q))), d, 0.05)
            if loc is not None: hits[0] += 1
        if loc is None:
            hits[1] += 1
            loc, nrm, idx, dist = bvh.find_nearest(Vector(tuple(map(float, q + (lift if lift is not None else 0)))))
        if loc is None or dist > max_dist: return None
        a, b, c = (vco[i] for i in tris[idx]); P = np.array(loc, np.float32)
        v0, v1, v2 = b - a, c - a, P - a
        d00, d01, d11, d20, d21 = v0 @ v0, v0 @ v1, v1 @ v1, v2 @ v0, v2 @ v1
        den = d00 * d11 - d01 * d01
        if abs(den) < 1e-12: return None
        l1 = (d11 * d20 - d01 * d21) / den; l2 = (d00 * d21 - d01 * d20) / den; l0 = 1 - l1 - l2
        ua, ub, uc = (np.array(x, np.float32) for x in tuv[idx])
        return l0 * ua + l1 * ub + l2 * uc
    def surface_path(img, surf, pts, lift=None, max_dist=0.015, taper=(0.15, 0.15), scale=1.0, step=0.0008):
        """draw a 3D polyline (control points near the bones) on the surface: sampled every `step` m, projected, UV polyline"""
        if surf is None or len(pts) < 2: return
        pts = [np.asarray(p, np.float32) for p in pts]
        L = sum(float(np.linalg.norm(pts[i + 1] - pts[i])) for i in range(len(pts) - 1))
        e0, e1 = max(taper[0], 1e-3), max(taper[1], 1e-3)
        uvs = []; vals = []; acc = 0.0
        for i in range(len(pts) - 1):
            seg = pts[i + 1] - pts[i]; sl = float(np.linalg.norm(seg)); n = max(2, int(sl / step))
            for k in range(n):
                t_local = k / n; q = pts[i] + seg * t_local; t = (acc + sl * t_local) / (L + 1e-9)
                u = min(t / e0, (1 - t) / e1, 1.0); r = u * u * (3 - 2 * u)
                uvp = project(q, surf, lift, max_dist)
                if uvp is None:
                    if len(uvs) >= 2: polyline(img, np.array(uvs), np.array(vals, np.float32))
                    uvs = []; vals = []; continue
                uvs.append(uvp); vals.append(r * scale)
            acc += sl
        if len(uvs) >= 2: polyline(img, np.array(uvs), np.array(vals, np.float32))
    def surface_line(img, surf, p_from, p_to, lateral, offs, wiggle_amp, seed, steps=80, taper=(0.15, 0.15), lift=None, max_dist=0.015, scale=1.0):
        """sample the 3D segment p_from->p_to shifted sideways by offs (+ a gentle sine wiggle), project each sample onto the
        one-sided surface, draw the UV polyline through the interpolated UVs"""
        if surf is None: return
        bvh, tris, tuv = surf
        r = np.random.default_rng(seed); ph = r.uniform(0, 6.28)
        uvs = []; vals = []
        from mathutils import Vector
        def ramp(t):                                                      # 0 at the ends, 1 in the middle, smooth
            e0, e1 = max(taper[0], 1e-3), max(taper[1], 1e-3)
            u = min(t / e0, (1 - t) / e1, 1.0); return u * u * (3 - 2 * u)
        for t in np.linspace(0, 1, steps):
            q = p_from + (p_to - p_from) * t + lateral * (offs + wiggle_amp * np.sin(t * 7.0 + ph) * t * (1 - t) * 4)
            loc = None
            if lift is not None:
                # bones run mid-hand: cast a ray from the sample OUT through the back of the hand (13:35: lifting by a fixed vector and
                # snapping to the nearest point drifted sideways whenever that vector was tilted, which squashed Claire's fan by half)
                d = Vector(tuple(map(float, lift))).normalized()
                loc, nrm, idx, dist = bvh.ray_cast(Vector(tuple(map(float, q))), d, 0.05)     # from INSIDE the hand (the bone) outward: first exit = the back
                if loc is not None: hits[0] += 1
            if loc is None:
                hits[1] += 1
                loc, nrm, idx, dist = bvh.find_nearest(Vector(tuple(map(float, q + (lift if lift is not None else 0)))))
            if loc is None or dist > max_dist:                                # too far off the one-sided surface = it would snap to the island edge
                if len(uvs) >= 2: polyline(img, np.array(uvs), np.array(vals, np.float32))
                uvs = []; vals = []; continue
            a, b, c = (vco[i] for i in tris[idx]); P = np.array(loc, np.float32)
            v0, v1, v2 = b - a, c - a, P - a
            d00, d01, d11, d20, d21 = v0 @ v0, v0 @ v1, v1 @ v1, v2 @ v0, v2 @ v1
            den = d00 * d11 - d01 * d01
            if abs(den) < 1e-12: continue
            l1 = (d11 * d20 - d01 * d21) / den; l2 = (d00 * d21 - d01 * d20) / den; l0 = 1 - l1 - l2
            ua, ub, uc = (np.array(x, np.float32) for x in tuv[idx])
            uvs.append(l0 * ua + l1 * ub + l2 * uc); vals.append(ramp(t) * scale)
        if len(uvs) >= 2: polyline(img, np.array(uvs), np.array(vals, np.float32))
    tend = np.zeros((N, N), np.float32); vein_lines = np.zeros((N, N), np.float32); tint_lines = np.zeros((N, N), np.float32)
    fieldD = np.zeros((N, N), np.float32)
    # ---- nails (2026-09-06 evening): per-pixel 3D POSITION over the finger islands, so each nail can be cut analytically in the
    # distal phalanx's own frame (t along the bone, angle round it) instead of upscaling the artist's 14-px blob.
    FINGERS = ("thumb", "index", "middle", "ring", "little")
    def raster_assign(tri_uv, tri_vals, fields):
        """like raster() but overwrites (coordinates are signed, so max() would clobber them)"""
        pts = [(u * N, (1.0 - v) * N) for u, v in tri_uv]
        xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
        x0, x1 = max(int(min(xs)) - 1, 0), min(int(max(xs)) + 2, N); y0, y1 = max(int(min(ys)) - 1, 0), min(int(max(ys)) + 2, N)
        if x1 <= x0 or y1 <= y0: return
        gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
        (ax_, ay_), (bx, by), (cx_, cy_) = pts
        det = (bx - ax_) * (cy_ - ay_) - (cx_ - ax_) * (by - ay_)
        if abs(det) < 1e-9: return
        l1 = ((bx - gx) * (cy_ - gy) - (cx_ - gx) * (by - gy)) / det
        l2 = ((cx_ - gx) * (ay_ - gy) - (ax_ - gx) * (cy_ - gy)) / det
        l3 = 1.0 - l1 - l2
        inside = (l1 >= -0.003) & (l2 >= -0.003) & (l3 >= -0.003)
        for fld, vals in zip(fields, tri_vals):
            val = l1 * vals[0] + l2 * vals[1] + l3 * vals[2]
            sub = fld[y0:y1, x0:x1]; sub[inside] = val[inside]
    fid = np.zeros(len(me.vertices), np.int32)                          # finger id per vertex: 1..5 left, 6..10 right, 0 = not a finger
    for i in range(len(me.vertices)):
        tn = topname[i]
        for si, sd in enumerate(("l", "r")):
            for fi, f in enumerate(FINGERS):
                if tn.startswith("%s_hand_%s_" % (sd, f)): fid[i] = si * 5 + fi + 1
    fieldP = np.zeros((N, N, 3), np.float32); fieldID = np.zeros((N, N), np.float32); fieldNm = np.zeros((N, N, 3), np.float32)
    for poly in me.polygons:
        vi = list(poly.vertices)
        if max(fid[i] for i in vi) == 0: continue
        uvs = [tuple(uv.data[li].uv) for li in poly.loop_indices]
        tid = float(max(fid[i] for i in vi))
        for k in range(1, len(uvs) - 1):
            tri = (uvs[0], uvs[k], uvs[k + 1]); idx = (vi[0], vi[k], vi[k + 1])
            raster_assign(tri, ([vco[i][0] for i in idx], [vco[i][1] for i in idx], [vco[i][2] for i in idx], [tid] * 3,
                                [vno[i][0] for i in idx], [vno[i][1] for i in idx], [vno[i][2] for i in idx]),
                          (fieldP[..., 0], fieldP[..., 1], fieldP[..., 2], fieldID, fieldNm[..., 0], fieldNm[..., 1], fieldNm[..., 2]))
    def smooth_on_pixels(vals, ys_, xs_, sigma):
        """gaussian-smooth a per-pixel field over the pixels' own bbox, normalised by the pixel mask (facets of the coarse finger mesh
        put a 2-6 texel wobble on the plate outline in pass 5)"""
        y0, x0 = ys_.min(), xs_.min(); h, w = ys_.max() - y0 + 1, xs_.max() - x0 + 1
        img = np.zeros((h, w), np.float32); msk = np.zeros((h, w), np.float32)
        img[ys_ - y0, xs_ - x0] = vals; msk[ys_ - y0, xs_ - x0] = 1.0
        r = int(3 * sigma) + 1; k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2); k /= k.sum()
        def g(m):
            m = np.apply_along_axis(lambda row: np.convolve(row, k, mode="same"), 1, m)
            return np.apply_along_axis(lambda col: np.convolve(col, k, mode="same"), 0, m)
        out = g(img) / (g(msk) + 1e-6)
        return out[ys_ - y0, xs_ - x0]
    nail_plate = np.zeros((N, N), np.float32); nail_h = np.zeros((N, N), np.float32); nail_clean = np.zeros((N, N), np.float32)
    nail_lun = np.zeros((N, N), np.float32); nail_free = np.zeros((N, N), np.float32); nail_cut = np.zeros((N, N), np.float32)
    nail_shadow = np.zeros((N, N), np.float32); nail_groove = np.zeros((N, N), np.float32); nail_base = np.zeros((N, N, 3), np.float32)
    nail_wipe = np.zeros((N, N), np.float32)
    def paint_nail(side, f, ddir0, across_h):
        """one nail: frame from the distal phalanx bone (axis = previous joint -> distal joint, back = the hand's dorsal direction bent
        perpendicular to the bone), plate = rounded rectangle in (arc length round the finger, distance along the bone), in TEXELS"""
        chain = [n for n in names if n.startswith("%s_hand_%s_" % (side, f)) and n.rsplit("_", 1)[-1].isdigit()]
        chain.sort(key=lambda n: int(n.rsplit("_", 1)[-1]))
        if len(chain) < 2: print("nails: no chain for", side, f); return
        dist, prev = chain[-1], chain[-2]
        hd, pv = jpos(dist), jpos(prev)
        if hd is None or pv is None: return
        ax = unit(hd - pv)
        base = ddir0
        if f == "thumb": base = unit(ddir0 * 0.6 - across_h * 0.8)          # same rule as the dorsal pick above: the thumb's back faces radially + dorsally
        dd = unit(base - ax * np.dot(base, ax))
        my = fid == (("l", "r").index(side) * 5 + FINGERS.index(f) + 1)
        dv = np.array([i for i in range(len(me.vertices)) if my[i] and topname[i] == dist])
        if len(dv) < 4: print("nails: %s %s: only %d distal verts" % (side, f, len(dv))); return
        # refine the back direction from THIS finger's own mesh (pass 2: the little finger sits rolled ~40 deg in Claire's pose, so the
        # hand-wide dorsal put its plate on the side): mean normal of the distal verts facing the hand's back, re-orthogonalised to the bone
        # pass 8: the mean-normal refinement moved nothing (it is biased toward its own start), and the little finger's plate stayed on
        # its side. A fingertip is wider than it is tall, so the cross-section's MAJOR axis is the lateral axis and the nail lies on the
        # face perpendicular to it: 2D PCA of the distal verts' radial offsets, sign from the hand-wide dorsal direction.
        lt0 = np.cross(ax, dd)
        dvs0 = (vco[dv] - hd) @ ax; dvr0 = (vco[dv] - hd) - np.outer(dvs0, ax)
        midv = (dvs0 > 0.25 * L0) if (L0 := float(dvs0.max())) > 0 else np.ones(len(dv), bool)
        pts2 = np.stack([dvr0[midv] @ lt0, dvr0[midv] @ dd], 1)
        if len(pts2) >= 8:
            c2 = pts2 - pts2.mean(0); w2, v2 = np.linalg.eigh(c2.T @ c2); major = v2[:, 1]
            lat_new = unit(lt0 * major[0] + dd * major[1]); dd_new = unit(np.cross(lat_new, ax))
            if np.dot(dd_new, dd) < 0: dd_new = -dd_new
            print("nails %s %-6s: cross-section %.1f x %.1f mm, back turned %.0f deg by its shape" % (side, f, 2 * np.sqrt(w2[1] / len(c2)) * 1000, 2 * np.sqrt(w2[0] / len(c2)) * 1000, np.degrees(np.arccos(np.clip(np.dot(dd_new, dd), -1, 1)))))
            dd = dd_new
        L = float(((vco[dv] - hd) @ ax).max())                                # tip of the mesh along the bone
        sel = fieldID == float(("l", "r").index(side) * 5 + FINGERS.index(f) + 1)
        ys_, xs_ = np.nonzero(sel)
        if len(ys_) == 0: return
        P = fieldP[ys_, xs_]; rel = P - hd; s = rel @ ax; t = s / max(L, 1e-6)
        rad = rel - np.outer(s, ax); rn = np.linalg.norm(rad, axis=1) + 1e-9
        nmp = fieldNm[ys_, xs_]; nml = nmp / (np.linalg.norm(nmp, axis=1, keepdims=True) + 1e-9)
        rgb = alb[ys_, xs_, :3]; lum = rgb.mean(1); sat = rgb.max(1) - rgb.min(1)
        # ---- pass 18 (Tefa: "that patch of skin on the thumb, like a little corner coming loose"): THE ARTIST'S NAIL SAYS
        # WHICH WAY THE FINGER FACES. A Blender render of the thumb showed its plate laid down the thumb's FLANK, its lower
        # corner running off the silhouette -- the cross-section PCA had turned the back direction 47 deg the wrong way on the
        # one digit whose distal segment is broadest. Guessing the orientation from the bone or the mesh shape has now been
        # wrong three times (normals in pass 3, occlusion in pass 5, PCA here). The artist painted the nail on the correct
        # face, so the MEAN SURFACE NORMAL over their nail pixels IS the back direction. Detected with no orientation gate at
        # all -- colour and distance along the bone only -- so nothing circular is assumed.
        sref0 = (t > 0.05) & (t < 0.32)
        if sref0.sum() < 20: sref0 = t < 0.35
        s_lum0 = float(np.median(lum[sref0])); s_sat0 = float(np.median(sat[sref0]))
        orig0 = (sat < s_sat0 - 0.022) & (lum > s_lum0 - 0.03) & (t > 0.30) & (t < 1.05)
        if a.nail_face_from_art and orig0.sum() >= 120:
            dm = nml[orig0].mean(0)
            if np.linalg.norm(dm) > 0.25:                                     # a coherent face, not a ring of normals cancelling out
                dm = unit(dm - ax * np.dot(dm, ax))
                turn = np.degrees(np.arccos(np.clip(np.dot(dm, dd), -1, 1)))
                print("nails %s %-6s: back direction from the artist's %d nail px, turned %.0f deg off the shape guess" % (side, f, int(orig0.sum()), turn))
                dd = dm
        lt = np.cross(ax, dd)
        xl = rad @ lt; xd = rad @ dd                                           # lateral offset from the bone's sagittal plane / height toward the back
        inplate = (t > 0.35) & (t < 0.95)
        R = float(np.median(rn[inplate])) if inplate.sum() > 20 else float(np.median(rn))
        # the finger's half-width, from the distal verts themselves (pass 3: an ANGLE round the bone put the little finger's plate on its
        # side, because that bone runs off-centre in a flattened segment; a lateral distance does not care where the bone sits)
        dvs = (vco[dv] - hd) @ ax; dvrad = (vco[dv] - hd) - np.outer(dvs, ax)
        mid = (dvs > 0.3 * L) & (dvs < 0.9 * L)
        halfw = float(np.percentile(np.abs(dvrad[mid] @ lt), 90)) if mid.sum() > 6 else R
        mm = N / 4096.0 / 0.183                                                # texels per mm (back of the hand, measured 2026-09-06)
        nd = nml @ dd                                                          # surface normal vs the back direction
        # ...and ON THE DISTAL SEGMENT. Pass 18b (Tefa circled a "loose triangular bit of skin" on the BACK OF THE HAND near the
        # thumb): a finger's pixel set is everything weighted to that finger, which for the thumb reaches down its metacarpal
        # onto the back of the hand. Nothing gated the nail by distance ALONG the bone, so where the smoothed coordinates
        # drifted near a UV island edge, a few of those far-away pixels fell inside the plate and got painted as nail.
        backside = np.clip((nd - 0.1) / 0.25, 0, 1) * np.clip((rn / max(R, 1e-6) - 0.2) / 0.15, 0, 1) * np.clip((t - 0.10) / 0.12, 0, 1)   # faces the back, not the tip cap where rad -> 0, and not the rest of the hand
        X0 = xl * 1000.0 * mm                                                  # lateral offset from the bone, texels
        Y0 = s * 1000.0 * mm                                                   # distance along the bone from the joint, texels
        skin_ref = (t > 0.05) & (t < 0.32) & (nd > 0.2)
        if skin_ref.sum() < 20: skin_ref = t < 0.35
        col = np.median(alb[ys_[skin_ref], xs_[skin_ref], :3], axis=0)
        s_lum = float(np.median(lum[skin_ref])); s_sat = float(np.median(sat[skin_ref]))
        # SATURATION is the clean signal: the painted nail is pale grey-pink on pink skin, so it desaturates by 0.03-0.06
        # while its luminance overlaps the skin's own shading `[measured 2026-09-06, n=4 nails]`.
        d_col = np.clip((s_sat - sat) / 0.045, 0, 1) * np.clip((lum - s_lum + 0.06) / 0.05, 0, 1)
        orig = (sat < s_sat - 0.022) & (lum > s_lum - 0.03) & (t > 0.30) & (backside > 0.2)
        halfw_tex = halfw * 1000.0 * mm
        # Work in TEXTURE space from here (pass 10): the artist drew the nail in UV, so measuring and drawing in UV cancels
        # the island's stretch exactly, where a 3D-projected box does not. The only thing taken from the rig is WHICH WAY the
        # finger runs: a least-squares fit of pixel coordinates against the along-bone distance gives that as a UV direction.
        Mfit = np.stack([X0, Y0, np.ones_like(X0)], 1)
        sol = np.linalg.lstsq(Mfit, np.stack([xs_.astype(np.float64), ys_.astype(np.float64)], 1), rcond=None)[0]
        uY = sol[1] / (np.linalg.norm(sol[1]) + 1e-9)                          # +1 = toward the fingertip, in texels
        uX = np.array([-uY[1], uY[0]])
        # pass 15 (Tefa, 21 screenshots): the plate SIZED from the blob's extents read as a teardrop / gel blob in VR; the 16:53
        # build's plate -- a rounded rectangle from the finger's own width and the phalanx's length, in the 3D bone frame -- was
        # "nearly everything right" apart from its grey tips. So: pass 8's frame, shape and size again, and only the CENTRE
        # taken from the artist's nail. (A UV-Jacobian rescale of the bone sizes was tried first and gave 28x14 and 34x149.)
        Xb = smooth_on_pixels(X0, ys_, xs_, 2.5); Yb = smooth_on_pixels(Y0, ys_, xs_, 2.5)
        wx = a.nail_width * (0.95 if f == "thumb" else 1.0) * halfw_tex
        wy = 0.5 * a.nail_length * (0.85 if f == "thumb" else 1.0) * L * 1000.0 * mm
        # pass 16 (Tefa: "only the fingertips are wrong"): ANCHOR THE FREE EDGE AT THE FINGERTIP. Every pass so far ended the
        # plate at 92 % of the phalanx and left a bare pad beyond it, which is what reads as wrong up close -- a real nail runs
        # out to the tip. The distal edge now sits at `--nail-tip` of the phalanx and the plate extends BACK from there; the tip
        # cap is cut by the surface-normal test (`backside`), not by this number, which is why 0.97 could not be used in pass 2.
        y_hi = a.nail_tip * L * 1000.0 * mm
        yc = y_hi - wy
        if orig.sum() >= 120:
            xc = float(np.mean(X0[orig]))                                   # across the finger: the artist's nail is the truth
            fit = "free edge at %.0f%% of the phalanx, centred across on the artist's %d px (x=%.0f)" % (100 * a.nail_tip, int(orig.sum()), xc)
        else:
            xc = 0.0
            fit = "FALLBACK centre (only %d px read as nail)" % int(orig.sum())
        # pass 17 (Tefa: "a patch of skin on the thumb, like a little corner coming loose"): `orig` is a pure colour test, so on
        # the thumb -- whose distal segment is broad and pale -- it also caught skin well away from the nail (2 461 px against the
        # index's 1 194). Those pixels were inside `clean`, so the wipe flattened them to one colour and left a hard-edged patch.
        # The plate no longer depends on `orig` for anything but the across-finger centre, so confine it to the plate's own
        # neighbourhood first, then take the centre from what is left.
        near = (np.abs(Yb - yc) < 1.5 * wy) & (np.abs(X0) < 2.0 * wx)
        n_before = int(orig.sum()); orig = orig & near
        if n_before - int(orig.sum()) > 0:
            print("nails %s %-6s: %d of %d px dropped as too far from the nail" % (side, f, n_before - int(orig.sum()), n_before))
        if orig.sum() >= 120: xc = float(np.mean(X0[orig]))
        X = Xb - xc; Y = Yb - yc
        # rounded-rectangle SDF. The DISTAL end gets the big radius: a real free edge is a strong arc bulging toward the
        # fingertip (Tefa's green curve); pass 8's flat distal edge with small corners is the red one.
        rc = wx * (0.50 + 0.25 * np.clip(-Y / wy, 0, 1))   # pass 15: pass 8's corners again (rounder at the cuticle, 0.5 at the free edge); the big distal radius made a teardrop
        qx = np.abs(X) - (wx - rc); qy = np.abs(Y) - (wy - rc)
        sdf = np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2) + np.minimum(np.maximum(qx, qy), 0) - rc
        # the plate keeps the shape it was fitted to: `backside` is what decides WHERE a nail may be drawn, and the fit already
        # happened inside it, so clamping the plate by it again only bites notches out of the outline (pass 11 renders).
        plate_side = np.clip((nd + 0.15) / 0.25, 0, 1)
        plate = np.clip(0.5 - sdf, 0, 1) * plate_side
        yn = Y / max(wy, 1e-6); xn = X / max(wx, 1e-6)
        distal = np.clip((yn - 0.45) / 0.4, 0, 1)                              # 1 toward the free edge
        groove = np.exp(-(sdf ** 2) / (2 * 1.1 ** 2)) * (1.0 - distal) * backside          # the plate's outline: cuticle + side folds
        # the skin fold outside the plate. Pass 13: at +0.08 over a 1.6-texel spread this was the biggest positive relief in the
        # whole nail -- taller than the plate itself -- and in VR it read as a swollen rim of skin round every nail, which is the
        # only fault left on Tefa's second look. Narrower, pushed further out so it does not merge with the groove, and much lower.
        fold = np.exp(-((sdf - 2.6) ** 2) / (2 * 1.1 ** 2)) * (1.0 - distal) * backside
        cut = np.exp(-((sdf - 1.5) ** 2) / (2 * 2.0 ** 2)) * np.clip((-yn - 0.55) / 0.3, 0, 1) * backside   # eponychium band, proximal only
        free = np.clip((yn - 0.55) / 0.3, 0, 1) * plate                          # free edge: RELIEF ONLY from pass 9 (see below)
        shadow = np.zeros_like(plate)                                            # hyponychium shadow retired: it read as dirt under every nail
        lun = np.clip((-yn - 0.45 - 0.35 * xn ** 2) / 0.18, 0, 1) * plate         # lunula: crescent at the base
        ridges = 0.5 + 0.5 * np.sin(X * (2 * np.pi / 5.0))                        # longitudinal ridges, 5 texels apart (~0.9 mm)
        dome = 1.0 - np.clip(xn, -1, 1) ** 2
        h = (0.05 * plate + 0.035 * dome * plate + 0.04 * free - 0.11 * groove + a.nail_fold * fold + 0.004 * ridges * plate) * a.nails   # ridges at 0.01 read as stripes in the render
        # clean-up zone: a ring round the plate PLUS every pixel the artist painted as nail, so nothing of the original can
        # survive past our edge. Never the pad; the tip cap is included (the artist's free edge sits on it).
        clean = np.maximum(np.clip((14.0 - sdf) / 3.0, 0, 1), orig.astype(np.float32)) * (t < 1.1) * (t > 0.15) * np.clip((nd + 0.3) / 0.3, 0, 1)
        # the wipe itself, per pixel: how far this pixel is from its own finger's skin (both brighter AND darker, which is the
        # whole point of pass 9), inside the clean-up zone. Section 4 just applies it.
        wipe_here = clean * d_col                                              # d_col: 1 where this pixel is nail-coloured, 0 on true skin
        for arr, v in ((nail_plate, plate), (nail_h, h), (nail_clean, clean), (nail_wipe, wipe_here), (nail_lun, lun),
                       (nail_free, free), (nail_cut, cut), (nail_shadow, shadow), (nail_groove, groove + 0.2 * fold)):
            arr[ys_, xs_] = np.maximum(arr[ys_, xs_], v.astype(np.float32))
        nail_base[ys_, xs_] = col
        # proof in the static domain: is the artist's nail entirely INSIDE our plate now? (that is the fault this pass exists
        # to fix: any original pixel outside the plate is a grey band showing past our edge)
        if plate.sum() > 0 and orig.sum() > 10:
            outside = orig & (plate < 0.5)
            pc = (np.average(xs_, weights=plate), np.average(ys_, weights=plate))
            bc = (np.average(xs_[orig]), np.average(ys_[orig]))
            print("nails %s %-6s: L %.1f mm, plate %dx%d texels (%d px), %s; artist nail %d px, centroid offset %.0f texels, "
                  "%d px (%.1f%% OF IT) still outside the plate" % (side, f, L * 1000, 2 * wx, 2 * wy, int(plate.sum()), fit,
                  int(orig.sum()), np.hypot(pc[0] - bc[0], pc[1] - bc[1]), int(outside.sum()), 100.0 * outside.sum() / max(orig.sum(), 1)))
    for side in ("l", "r"):
        w = jpos(side + "_arm_wrist"); m0 = jpos(side + "_hand_middle_0"); t1 = jpos(side + "_hand_thumb_1"); el = jpos(side + "_arm_radius")
        if w is None or m0 is None or t1 is None: print("anatomy: bones missing for", side); continue
        axis = unit(m0 - w); tv = t1 - w; palm = unit(tv - axis * np.dot(tv, axis))
        # palm plane from the KNUCKLE ROW, not the thumb (14:05): Claire's thumb is abducted forward out of the palm plane, which tilted the
        # thumb-derived normal ~45 deg sideways; rays from the bones then exited the back displaced and the fan came out 3.6 cm wide for
        # 5.4 cm of knuckles. The knuckle row (index -> little proximal phalanx heads) always lies in the palm plane.
        krow = []
        for f in ("index", "little"):
            for k in range(4):
                pk = jpos(side + "_hand_%s_%d" % (f, k))
                if pk is not None and np.dot(pk - w, axis) > 0.04: krow.append(pk); break
        if len(krow) == 2:
            lat = unit(np.cross(axis, unit(krow[1] - krow[0])))
        else:
            lat = unit(np.cross(axis, palm))
        hidx = np.array([i for i in range(len(me.vertices)) if topname[i].startswith((side + "_hand_", side + "_arm_wrist"))])
        # dorsal side (fixed 2026-09-06 12:00, hd_hands_dorsal_preview.py): `palm` points AT the thumb, in the palm plane, so
        # -(n . palm) picked the little-finger EDGE. The plane's normal is `lat`; its sign flips per hand, so it is fixed by
        # the finger curl: fingers flex toward the palm, so the bend of each finger chain along lat points palmward.
        # Verified on renders: the -sign face carries the nails (left hand: dorsal = -lat, curl +1.69; right: +lat, -1.69).
        curl = 0.0
        for f in ("index", "middle", "ring", "little"):
            j0, j1, j2 = jpos(side + "_hand_%s_0" % f), jpos(side + "_hand_%s_1" % f), jpos(side + "_hand_%s_2" % f)
            if j0 is not None and j1 is not None and j2 is not None: curl += float(np.dot(unit(j2 - j1) - unit(j1 - j0), lat))
        dsign = -1.0 if curl > 0 else 1.0
        print("anatomy %s: finger curl %+.3f along lat -> dorsal = %s lat" % (side, curl, "-" if dsign < 0 else "+"))
        # dorsal weight, geometric (13:55): a vertex is on the BACK of the hand if a short ray from it toward the back meets no more hand.
        # (The normal test dropped the domed index side of Claire's flatter hand; a per-bone mid-plane test misclassified the wrist-driven
        # back-of-hand verts because that bone sits 5 cm up the forearm.) Fingers use the back direction bent perpendicular to their own bone.
        nrm_dir = dsign * lat
        hmeta = hidx[~is_finger[hidx]]; hback = hmeta[(vno[hmeta] @ nrm_dir) > 0.3]
        ddir0 = unit(vno[hback].mean(0)) if len(hback) > 10 else nrm_dir
        kr = []
        for f in ("index", "little"):
            for k in range(4):
                pk = jpos(side + "_hand_%s_%d" % (f, k))
                if pk is not None and np.dot(pk - w, axis) > 0.04: kr.append(pk); break
        across_h = unit(kr[1] - kr[0]) if len(kr) == 2 else unit(np.cross(ddir0, axis))   # index -> little
        if a.nails > 0:
            for f in FINGERS: paint_nail(side, f, ddir0, across_h)
        hset = set(int(i) for i in hidx); htris = []
        for poly in me.polygons:
            vi = list(poly.vertices)
            if all(i in hset for i in vi):
                for k in range(1, len(vi) - 1): htris.append((vi[0], vi[k], vi[k + 1]))
        from mathutils.bvhtree import BVHTree as _BVH
        hbvh = _BVH.FromPolygons([tuple(map(float, vco[i])) for i in range(len(me.vertices))], htris, all_triangles=True)
        n_dorsal = 0
        for bn in set(topname[hidx]):
            idx = hidx[topname[hidx] == bn]; b = arm.data.bones.get(bn)
            dd = ddir0
            if b is not None and is_finger[idx[0]]:
                ax = unit(np.array(arm.matrix_world @ b.tail_local, np.float32) - np.array(arm.matrix_world @ b.head_local, np.float32))
                base = ddir0
                if "thumb" in bn:                                              # 14:20: a flat "plate" on the thumb = its back misread as palm
                    base = unit(ddir0 * 0.6 - across_h * 0.8)                # the thumb's back faces radially (away from the little finger) and dorsally
                dd = unit(base - ax * np.dot(base, ax))
            D = Vector(tuple(map(float, dd)))
            for i in idx:
                o = Vector(tuple(map(float, vco[i]))) + D * 0.0015
                hit = hbvh.ray_cast(o, D, 0.04)[0]
                Dv[i] = 0.0 if hit is not None else 1.0; n_dorsal += Dv[i] > 0.5
        print("anatomy %s: geometric dorsal pick: %d of %d hand verts on the back" % (side, n_dorsal, len(hidx)))
        dors = hidx[(Dv[hidx] > 0.5) & (~is_finger[hidx])]                       # back of the hand, metacarpal region
        dsurf = make_surface(hidx[~is_finger[hidx]])                             # the whole metacarpal surface, both faces: the lift below makes the nearest point the dorsal one
        # dorsal direction refined from the mesh: the mean normal of the metacarpal vertices on the back half (the thumb-derived `lat`
        # can be tilted by the thumb's pose; on Claire it is)
        ddir = ddir0
        print("anatomy %s: lat-vs-mesh dorsal tilt %.1f deg (%d back verts)" % (side, np.degrees(np.arccos(np.clip(np.dot(ddir, dsign * lat), -1, 1))), len(hback)))
        dlift = ddir * 0.012                                              # 1.2 cm toward the back of the hand: the bone segments run mid-hand (on Claire's flatter hand a
                                                                                 # dorsal-only triangle set lost the index side and squashed the fan toward the centre, 13:25)
        # knuckles = the head of each finger's PROXIMAL PHALANX. In this rig index_0/middle_0 start at the knuckle (6.8 cm along
        # the hand) but ring_0/little_0 are metacarpals starting 2 cm from the wrist `[measured 2026-09-06]`, so pick per finger
        # the first bone that lies more than 4 cm along the wrist->middle axis.
        kn = []
        for f in ("index", "middle", "ring", "little"):
            for k in range(4):
                p = jpos(side + "_hand_%s_%d" % (f, k))
                if p is not None and np.dot(p - w, axis) > 0.04: kn.append(p); break
        print("anatomy %s: %d hand verts, %d dorsal metacarpal verts, %d knuckles; projection so far: %d ray hits, %d fallbacks" % (side, len(hidx), len(dors), len(kn), hits[0], hits[1]))
        if len(dors) < 20 or len(kn) < 2: continue
        across = unit(kn[-1] - kn[0])                                            # index -> little, in the palm plane
        rs = np.random.default_rng(101 if side == "l" else 202)                   # the two hands differ by construction (14:00, Tefa)
        # extensor tendons: a hint on the distal half only (they show when the fingers extend; full length read as cartoon glove seams)
        for i, k in enumerate(kn):
            surface_path(tend, dsurf, [w + (k - w) * 0.55 + across * rs.uniform(-0.001, 0.001), w + (k - w) * 0.92], lift=dlift, taper=(0.5, 0.15), scale=1.0)
        # dorsal veins: 2-3 meandering main veins per hand, each with a branch; they run wrist -> the gaps between knuckles, never straight
        gaps = [(kn[i] + kn[i + 1]) / 2 for i in range(3)]
        n_main = int(rs.integers(2, 4)); mains = [int(g) for g in rs.choice(3, size=n_main, replace=False)]
        vscale = 0.85 if side == "r" else 0.75                                  # 14:20: right "nearly perfect, very slightly lower", left lower a bit more
        for gi in mains:
            v_end = w + (gaps[gi] - w) * rs.uniform(0.82, 0.9) + across * rs.uniform(-0.002, 0.002)
            v_start = w + (gaps[gi] - w) * rs.uniform(0.18, 0.3) + across * rs.uniform(-0.003, 0.003)   # stays in its own lane: no crossings (14:20)
            pts = [v_start]
            for t in np.linspace(0, 1, 6)[1:-1]:                                    # lightning-strike meander: 4 kinks of a few mm
                pts.append(v_start + (v_end - v_start) * t + across * rs.uniform(-0.0035, 0.0035) + axis * rs.uniform(-0.002, 0.002))
            pts.append(v_end)
            surface_path(vein_lines, dsurf, pts, lift=dlift, taper=(0.25, 0.2), scale=vscale * rs.uniform(0.85, 1.0))
            # one short branch, only toward a neighbouring gap that has NO main vein, so branches never meet another vein
            free = [g for g in (gi - 1, gi + 1) if 0 <= g <= 2 and g not in mains]
            if free:
                ng = gaps[int(rs.choice(free))]; j = int(rs.integers(1, 4))
                b_end = pts[j] + (ng - pts[j]) * rs.uniform(0.3, 0.45) + axis * rs.uniform(0.005, 0.015)
                surface_path(vein_lines, dsurf, [pts[j], pts[j] + (b_end - pts[j]) * 0.5 + across * rs.uniform(-0.002, 0.002), b_end], lift=dlift, taper=(0.05, 0.5), scale=vscale * 0.55)
        # the cephalic vein on the thumb side, meandering from the web toward the wrist
        web = (kn[0] + t1) / 2; c_end = w + (kn[0] - w) * 0.2 + (web - kn[0]) * 0.3
        c_pts = [w + (web - w) * 0.85]
        for t in np.linspace(0, 1, 5)[1:-1]:
            c_pts.append(c_pts[0] + (c_end - c_pts[0]) * t + across * rs.uniform(-0.004, 0.004))
        c_pts.append(c_end)
        surface_path(vein_lines, dsurf, c_pts, lift=dlift, taper=(0.2, 0.3), scale=0.6 * vscale)
        if el is not None:                                                        # forearm veins: two, along the arm, both faces
            fidx = np.array([i for i in range(len(me.vertices)) if topname[i].startswith(side + "_arm_radius")])
            if len(fidx) > 20:
                fset = set(int(i) for i in fidx); ftris = []
                for poly in me.polygons:
                    vi = list(poly.vertices)
                    if all(i in fset for i in vi):
                        for k in range(1, len(vi) - 1): ftris.append((vi[0], vi[k], vi[k + 1]))
                fbvh = _BVH.FromPolygons([tuple(map(float, vco[i])) for i in range(len(me.vertices))], ftris, all_triangles=True)
                Df = Vector(tuple(map(float, ddir0)))
                for i in fidx:                                                    # 14:20: the wrist "line" was the 30 % -> 100 % pore step
                    hit = fbvh.ray_cast(Vector(tuple(map(float, vco[i]))) + Df * 0.0015, Df, 0.06)[0]
                    Dv[i] = 0.0 if hit is not None else 1.0
                fsurf = make_surface(fidx)
                # 14:20 (Tefa): no veins on the BACK of the forearm at all; on the INSIDE of the wrist a faint lightning-branch TINT, no relief
                rf = np.random.default_rng(303 if side == "l" else 404)
                plift = -ddir0 * 0.012                                            # ray from the bone toward the palm side of the forearm
                trunk0 = w + (el - w) * 0.04 + across_h * rf.uniform(-0.004, 0.004)
                for i in range(2):                                                # two branches leaving the wrist crease, each forking once
                    a1 = w + (el - w) * rf.uniform(0.32, 0.48) + across_h * (rf.uniform(0.006, 0.014) * (1 if i == 0 else -1))
                    pts = [trunk0]
                    for t in np.linspace(0, 1, 5)[1:-1]: pts.append(trunk0 + (a1 - trunk0) * t + across_h * rf.uniform(-0.003, 0.003))
                    pts.append(a1)
                    surface_path(tint_lines, fsurf, pts, lift=plift, max_dist=0.03, taper=(0.1, 0.35), scale=1.0)
                    j = 2; fork = pts[j] + (a1 - pts[j]) * 0.6 + across_h * (rf.uniform(0.006, 0.012) * (-1 if i == 0 else 1))
                    surface_path(tint_lines, fsurf, [pts[j], fork], lift=plift, max_dist=0.03, taper=(0.05, 0.5), scale=0.7)
                print("anatomy %s forearm: %d verts, 2 veins" % (side, len(fidx)))
    # SMOOTH THE PALM/BACK SPLIT ON THE MESH, NOT IN THE TEXTURE (2026-09-06 23:00). Pore strength runs 30 % on the palm
    # side and 100 % on the back, and the transition between them used to be feathered by a blur IN UV -- so the two sides
    # of a seam, being different islands, got different amounts of feathering and the strength stepped at the join. On the
    # forearm that split runs along its length, which is exactly where Tefa's remaining line is, and a note from 14:35
    # already recorded the same mechanism producing a line at the wrist. Averaging over each vertex's neighbours instead
    # makes the transition a property of the surface, so it is identical on both sides of every seam by construction.
    _adj = [[] for _ in range(len(me.vertices))]
    for poly in me.polygons:
        vi = list(poly.vertices)
        for _a in range(len(vi)):
            _adj[vi[_a]].append(vi[(_a + 1) % len(vi)]); _adj[vi[(_a + 1) % len(vi)]].append(vi[_a])
    _sm = Dv.astype(np.float32).copy()
    # BOTH hands: this block runs once, after the per-side loop has filled Dv for each, so the mask must not be one side's
    _mask_v = np.array([1.0 if topname[i].startswith(("l_hand_", "r_hand_", "l_arm_", "r_arm_")) else 0.0 for i in range(len(me.vertices))], np.float32)
    for _ in range(14):
        _nxt = _sm.copy()
        for _i in range(len(me.vertices)):
            if _mask_v[_i] <= 0 or not _adj[_i]: continue
            _n = [_sm[j] for j in _adj[_i] if _mask_v[j] > 0]
            if _n: _nxt[_i] = 0.35 * _sm[_i] + 0.65 * (sum(_n) / len(_n))
        _sm = _nxt
    print("palm/back split smoothed over the mesh, both hands (14 passes, %d verts), range %.2f-%.2f" % (int((_mask_v > 0).sum()), float(_sm[_mask_v > 0].min()), float(_sm[_mask_v > 0].max())))
    Dv = _sm
    for poly in me.polygons:
        vi = list(poly.vertices)
        if max(K[i] for i in vi) <= 0: continue
        uvs = [tuple(uv.data[li].uv) for li in poly.loop_indices]
        for k in range(1, len(uvs) - 1):
            raster((uvs[0], uvs[k], uvs[k + 1]), ([Dv[vi[0]], Dv[vi[k]], Dv[vi[k + 1]]],), (fieldD,))
    dorsal = blur(np.clip((fieldD - 0.35) / 0.3, 0, 1), N / 1024.0 * 1.0) * edge
    # widths in millimetres: the back of the hand runs at 0.18 mm per texel at 4K `[measured 2026-09-06]`; a tendon reads ~3 mm
    # wide, a vein ~2.5 mm. Normalised per line family by a high percentile, not the max, so overlaps do not dim the rest.
    mm = N / 4096.0 / 0.183
    def line_blur(img, sigma):
        """blur a 1-texel-wide drawn line into a Gaussian ridge whose peak is 1.0 for an isolated line (analytic: a unit line
        blurred by sigma peaks at 1/(sigma*sqrt(2*pi))); overlaps are soft-clipped rather than cut into plateaus"""
        out = blur(img, sigma) * (sigma * np.sqrt(2 * np.pi))
        return 1.0 - np.exp(-1.2 * out)                                      # 0.70 for one line, eases toward 1 where lines cross (no plateau)
    tint_lines = line_blur(tint_lines, 2.0 * mm) * (1.0 - fieldF) * 0.6   # inner-wrist veins: colour only, a soft ~4 mm band (14:35: 0.9 mm read as sharp scratches)
    tend = line_blur(tend, 1.3 * mm)                                     # ~3 mm wide, faint
    vein_lines = line_blur(vein_lines, 0.75 * mm)                        # ~1.8 mm wide (14:00: "too wide" at 2.8 mm on a female hand)
    vein_lines = vein_lines * dorsal * (1.0 - fieldF)
    tend = tend * dorsal * (1.0 - fieldF)
    # amplitudes chosen for a max normal tilt of ~0.25 (veins) / ~0.12 (tendons) at nrm_from_height scale 6: A = tilt*sigma/(6*0.61)
    h_anat = (vein_lines * 0.42 + tend * 0.16) * a.anatomy               # subtle (14:00 in VR on Claire: 0.80/0.50 read as cartoon glove seams)               # line_blur peaks at 0.70, so these are ~0.39 / 0.28 effective
    if a.anatomy > 0:                                         # full-res masks for hd_hands_debug_render.py (where do the lines land in 3D?)
        dbg = np.zeros((N, N, 4), np.float32); dbg[..., 0] = tend; dbg[..., 1] = vein_lines; dbg[..., 2] = dorsal; dbg[..., 3] = 1.0
        save_np(dbg, os.path.join(a.out, "anat_mask.png"))
    print("anatomy: dorsal %.1f%% of the hand mask; veins cover %.2f%%, tendons %.2f%%, inner-wrist tint %.2f%%" % (100 * (dorsal[fieldK > 0] > 0.5).mean(),
          100 * (vein_lines[fieldK > 0] > 0.3).mean(), 100 * (tend[fieldK > 0] > 0.3).mean(), 100 * (tint_lines[fieldK > 0] > 0.3).mean()))
except Exception as e:
    import traceback; traceback.print_exc(); print("anatomy: skipped:", e)
# ---- pass 19: DROP ANY PLATE FRAGMENT TOO SMALL TO BE A NAIL. Tefa, 2026-09-06: "ONLY that patch of skin on the back of
# the hand at the base of thumb" -- a 55-74 texel speck, one per hand, mirrored, sitting well away from any fingertip. A
# finger's pixel set is every texel its vertices cover, which reaches far down the hand, and where the smoothed coordinates
# drift near a UV island edge a few of those texels can fall inside the plate. Confining the paint to the distal segment by
# `t` removed most of it; this removes the rest by the one property no real nail can have -- ten nails here run 1 391 to
# 3 683 texels, so anything under a few hundred is not one. Cause of the drift itself not identified `[hypothesis]`.
if a.nails > 0 and nail_plate.any():
    import collections
    _pl = nail_plate > 0.5
    _seen = np.zeros_like(_pl); _keep = np.zeros_like(_pl)
    _ys, _xs = np.nonzero(_pl); _H, _W = _pl.shape
    _dropped = 0
    for _y0, _x0 in zip(_ys, _xs):
        if _seen[_y0, _x0]: continue
        _q = collections.deque([(_y0, _x0)]); _seen[_y0, _x0] = True; _pix = []
        while _q:
            _y, _x = _q.popleft(); _pix.append((_y, _x))
            for _dy in (-1, 0, 1):
                for _dx in (-1, 0, 1):
                    _yy, _xx = _y + _dy, _x + _dx
                    if 0 <= _yy < _H and 0 <= _xx < _W and _pl[_yy, _xx] and not _seen[_yy, _xx]:
                        _seen[_yy, _xx] = True; _q.append((_yy, _xx))
        if len(_pix) >= a.nail_min_px:
            for _y, _x in _pix: _keep[_y, _x] = True
        else:
            _dropped += len(_pix)
    _grow = _keep.astype(np.float32)
    _grow = (blur(_grow, 3.0) > 0.02)                       # a little margin so the relief and wipe round each kept nail survive
    _m = _grow.astype(np.float32)
    for _arr in (nail_plate, nail_h, nail_clean, nail_wipe, nail_lun, nail_free, nail_cut, nail_shadow, nail_groove):
        _arr *= _m
    nail_base *= _m[..., None]
    print("nails: fragment guard dropped %d texels in specks under %d" % (_dropped, a.nail_min_px))
no_plate = 1.0 - nail_plate                                   # nails: no wrinkles, pores or grain on the plate itself
cx, cy = nrm_from_height(h_crease + h_veins + h_wrinkle * no_plate + h_anat + nail_h, 6.0)
_fs = 4.0 * (N / 4096.0 / 0.183)
# no UV blur here any more: the split is smoothed on the MESH above, so it is already gradual and already seam-safe.
dorsal_soft = np.clip((fieldD - 0.25) / 0.5, 0, 1) if fieldD.any() else dorsal
palm_pore = (a.palm_pores + (1.0 - a.palm_pores) * np.clip(dorsal_soft, 0, 1)) * no_plate   # palms: 30 % of the pore relief; nail plates: none
nx = (nrm[..., 0] * 2 - 1) + edge * (cx + det_xy[..., 0] * 0.35 * a.pores * (0.7 + 0.3 * fieldF) * palm_pore)
ny = (nrm[..., 1] * 2 - 1) + edge * (cy + det_xy[..., 1] * 0.35 * a.pores * (0.7 + 0.3 * fieldF) * palm_pore)
nz = np.sqrt(np.clip(1.0 - nx ** 2 - ny ** 2, 0.05, 1.0))
out_nrm = nrm.copy()
out_nrm[..., 0] = nx * 0.5 + 0.5; out_nrm[..., 1] = ny * 0.5 + 0.5; out_nrm[..., 2] = nz
# roughness (alpha, hypothesis): slightly lower in creases/knuckles where skin is tighter and moister; lower again on the nail plates (gloss)
out_nrm[..., 3] = np.clip(nrm[..., 3] - edge * creaseJ * 0.06 - nail_plate * 0.06 * a.nails, 0, 1)

# ---- 4. albedo: joint redness + mottling ----------------------------------------------------------------
out_alb = alb.copy()
red = edge * np.clip(creaseJ * 1.5, 0, 1) * 0.35 * a.tone
out_alb[..., 0] = alb[..., 0] * (1.0 + 0.10 * red)
out_alb[..., 1] = alb[..., 1] * (1.0 - 0.18 * red)
out_alb[..., 2] = alb[..., 2] * (1.0 - 0.22 * red)
mot = 1.0 + edge * mottle * 0.02 * a.tone
vt = np.clip(veins * 0.6 + vein_lines * 1.4 + tint_lines * 1.1, 0, 1)
out_alb[..., 0] *= (1.0 - vt * 0.07); out_alb[..., 1] *= (1.0 - vt * 0.035)                                       # veins: a cool ~5 % (14:20: still a touch visible at 7 %)
out_alb[..., :3] *= mot[..., None]
# pores slightly darker: use the detail map's alpha (AO) very gently
if pore_h is None:
    det_pit = 1.0 - det[np.ix_(ty, tx)][..., 3]
else:
    det_pit = np.clip(-pore_h * 0.5 + 0.5, 0, 1)                # pits (low height) darker, same field as the relief
out_alb[..., :3] *= (1.0 - edge * det_pit * 0.06 * a.pores * no_plate)[..., None]   # barely-there: pores read as relief, not dirt
# ---- nails in colour: wipe the artist's nail back to this finger's own skin, then lay one pale-pink plate over it.
if a.nails > 0 and nail_plate.any():
    # 1. the wipe. Pass 9: `nail_wipe` carries "differs from this finger's skin" in BOTH directions, so the artist's DARK
    #    free edge goes too. The old brightness-only test left it, and it was the grey-brown band Tefa saw past every nail.
    wipe = nail_wipe * a.nails
    fill = nail_base * mot[..., None]
    out_alb[..., :3] = out_alb[..., :3] * (1.0 - wipe)[..., None] + fill * wipe[..., None]
    # 2. the plate: ONE pale-pink tone all the way to its edge. No grey tip (Tefa, 2026-09-06: "the fix to fingernails is
    #    not painting the tips grey at all"), and no shadow beyond it; the free edge reads by relief alone.
    bed = (fill + (1.0 - fill) * 0.30) * np.array([1.0, 0.94, 0.95], np.float32)                     # pale pink: skin lifted a third toward white, a touch pinker (0.20 barely read as a nail at all)
    white = fill + (1.0 - fill) * 0.50                                                                # lunula: skin lifted halfway to white
    # lunula back UP (pass 14): Tefa's reference is the 16:53 build, whose faint lighter crescent at the base of each nail they
    # want kept, with the tips left as they are now (no grey). It was 0.6 then, 0.35 since pass 12; a.nail_lunula tunes it.
    nail_col = bed * (1.0 - nail_lun[..., None] * a.nail_lunula) + white * (nail_lun[..., None] * a.nail_lunula)
    pl = nail_plate * a.nails
    out_alb[..., :3] = out_alb[..., :3] * (1.0 - pl)[..., None] + nail_col * pl[..., None]
    out_alb[..., :3] *= (1.0 - (0.06 * nail_cut + 0.05 * nail_groove) * a.nails)[..., None]           # cuticle band + the outline, faint
    dbg = np.zeros((N, N, 4), np.float32); dbg[..., 0] = nail_plate; dbg[..., 1] = nail_clean; dbg[..., 2] = nail_wipe; dbg[..., 3] = 1.0
    save_np(dbg, os.path.join(a.out, "nail_mask.png"))
    print("nails: plate covers %d texels, clean-up zone %d, wiped %.0f texel-equivalents" % ((nail_plate > 0.5).sum(), (nail_clean > 0.5).sum(), wipe.sum()))
out_alb[..., 3] = alb[..., 3]                                 # metallic untouched

# detail mask (MSK1, BC4 2048, white over the hands): scale it down on the palms so the material's TILED pores follow the same rule
msk_png = os.path.join(a.work, TB + "_msk1.png")
if os.path.exists(msk_png):
    msk = load_np(msk_png); Wm = msk.shape[0]
    step = N // Wm
    pp = palm_pore[::step, ::step] if step > 1 else palm_pore              # palm_pore already carries (1 - nail plate): no tiled pores on the nails
    ed = edge[::step, ::step] if step > 1 else edge
    m = msk[..., 0] * (1.0 - ed) + msk[..., 0] * pp * ed
    msk_out = np.repeat(m[..., None], 4, axis=2); msk_out[..., 3] = 1.0
    save_np(msk_out, os.path.join(a.out, a.tex_base + "_MSK1.png"))
    print("detail mask: hand mean %.2f (was %.2f)" % (m[ed > 0.5].mean(), msk[..., 0][ed > 0.5].mean()))
# ---- REPAIR THE OUTER RIM OF EVERY ISLAND (2026-09-06 23:15). Tefa's last two screenshots show a small BRIGHT
# hard-edged sliver that survived all three seam fixes, and the same shape appears in the ARTIST'S OWN 1024 texture at
# island borders. Cause: we upscale that 1024 to 4K before painting anything, and at a border the interpolation drags
# whatever lies outside the island into its edge texels -- one texel of contamination at 1024 becomes four at 4K, which
# is exactly a bright fringe with straight edges. Neither the 3D pores, the padding nor the palm/back split could touch
# it, because it is in the colour we started from. Cure: re-fill the outermost few texels of each island from that
# island's own interior, ramped so nothing steps. `[hypothesis]` -- built at 23:15, unseen.
if a.edge_repair > 0 and fieldK_core.any():
    _core = fieldK_core.copy()
    for _ in range(a.edge_repair):
        _core &= np.roll(_core, 1, 0) & np.roll(_core, -1, 0) & np.roll(_core, 1, 1) & np.roll(_core, -1, 1)
    _ring = fieldK_core & ~_core
    if _ring.any():
        _w = _core.astype(np.float32); _sig = float(a.edge_repair) * 1.1
        _den = blur(_w, _sig) + 1e-6
        # ramp: 0 where the core still reaches, 1 at the island's outer edge, so the repair never steps
        _t = np.clip(1.0 - blur(_w, float(a.edge_repair) * 0.7) / (blur(np.ones_like(_w), float(a.edge_repair) * 0.7) + 1e-6) * 1.6, 0, 1)
        _t = np.where(_ring, _t, 0.0).astype(np.float32)
        for _c in range(3):
            _f = blur(out_alb[..., _c] * _w, _sig) / _den
            out_alb[..., _c] = out_alb[..., _c] * (1 - _t) + _f * _t
        for _c in range(4):
            _f = blur(out_nrm[..., _c] * _w, _sig) / _den
            out_nrm[..., _c] = out_nrm[..., _c] * (1 - _t) + _f * _t
        print("edge repair: %d texels of island rim re-filled from the interior (%d texels, mean blend %.2f)"
              % (a.edge_repair, int(_ring.sum()), float(_t[_ring].mean())))
save_np(out_alb, os.path.join(a.out, a.tex_base + "_ALBM.png"))
save_np(out_nrm, os.path.join(a.out, a.tex_base + "_NRMR.png"))
# preview crops of the hand strip (bottom 15%), original vs new, for eyes
def crop(img): return img[int(N * 0.85):, :, :]
save_np(np.concatenate([crop(alb), crop(out_alb)], axis=0), os.path.join(a.out, "preview_albedo_strip.png"))
save_np(np.concatenate([crop(nrm), crop(out_nrm)], axis=0), os.path.join(a.out, "preview_normal_strip.png"))
vv = np.repeat(np.clip(veins + vein_lines + (h_anat > 0.05) * 0.3, 0, 1)[int(N * 0.85):, :, None], 4, axis=2); vv[..., 3] = 1.0
save_np(vv, os.path.join(a.out, "preview_veins_strip.png"))
print("wrote 4K ALBM/NRMR + previews to", a.out)
print("DONE")
