"""Measure the ALBEDO STEP across every UV-island boundary on Claire's hands and forearms.

  blender -b --python seam_measure.py -- <plXXXX folder> <work_dir> <out_dir>
        [--character pl1000 --skin-mat Body_Mat --tex-base pl1000_Jacket]
        [--alb <png>] [--compare <png> ...] [--offsets 1,2,4] [--samples 7]

Why this exists (2026-09-06 night): after four seam causes were found and fixed -- all four OURS -- Tefa still
sees a soft tonal seam: a faint band across the right wrist and a faint diagonal at the base of the left thumb.
No hard edge, no lifted corner, just two areas of skin at slightly different LIGHTNESS. The remaining candidate
is that neighbouring UV islands carry slightly different AVERAGE COLOUR in the artist's own 1024 texture, which
no amount of relief work can touch. Inference has been wrong three times on this seam, so: measure first.

What it found (2026-09-07), since the guess above was half wrong: whole islands do NOT carry different average
colour -- their means sit within 4 units of each other. The step is real but LOCAL, along the seams themselves:
median 2.7 and p90 8.6 in 0-255 units on the 23:31 build, against 1.7 / 6.2 in the artist's own 1024, so about
40 % of it was added by our own pipeline. The normal map is not the cause (systematic tilt across a seam matches
the artist's, 1.5-4 deg either way) and neither is tangent handedness (no flips anywhere). hd_hands_paint.py's
--seam-blend cancels what was measured here; re-run this to check it.

What it does, and nothing else -- it writes no texture the game will see:
  1. loads the skin mesh and labels every UV island (faces joined across an edge only when their UVs agree);
  2. finds every edge that is ONE edge in 3D but TWO edges in the atlas -- the texels either side of it are
     3D neighbours on the skin, so whatever the shader does to one it does to the other, and any colour
     difference between them is a visible line;
  3. samples the albedo a few texels INSIDE each island either side of that edge (at several offsets, so an
     edge artefact can be told apart from a genuine difference in the islands' bulk colour);
  4. prints the step in sRGB 0-255 units -- the numbers a paint program would show -- per island pair, with the
     seam's length in millimetres, its position on the hand named by the nearest bone, and the ratio of the
     mean step to its variation along the seam (high = a constant offset between two islands, which a per-island
     correction can cancel exactly; low = the step wanders, and a constant will not fix it).

Every texture it reads is a derivative of a game texture; nothing here is written back into the game folder.
"""
import bpy, sys, os, importlib, ctypes, argparse
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uv_seams
from collections import defaultdict
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
ap = argparse.ArgumentParser()
ap.add_argument("src"); ap.add_argument("work"); ap.add_argument("out")
ap.add_argument("--character", default="pl1000")
ap.add_argument("--skin-mat", default="Body_Mat")
ap.add_argument("--tex-base", default="pl1000_Jacket")
ap.add_argument("--alb", default=None, help="albedo PNG to measure (default: the artist's own, <work>/<base>_albm.png)")
ap.add_argument("--compare", action="append", default=[], help="further albedo PNGs to measure the same way (repeatable)")
ap.add_argument("--offsets", default="0.05,0.1,0.2,0.4", help="how far INSIDE each island to sample, in MILLIMETRES of skin (comma separated). Millimetres, not texels, so a 1K and a 4K texture are compared at the same place on the hand -- and so the step can be extrapolated back to the seam itself")
ap.add_argument("--nrm", default=None, help="normal map to measure (default <work>/<base>_nrmr.png)")
ap.add_argument("--compare-nrm", action="append", default=[], help="further normal maps, paired with --compare in order")
ap.add_argument("--samples", type=int, default=7, help="sample points along each seam edge")
ap.add_argument("--min-mm", type=float, default=1.0, help="ignore island pairs whose shared seam is shorter than this")
a = ap.parse_args(argv)
os.makedirs(a.out, exist_ok=True)
ctypes.windll.ole32.CoInitializeEx(None, 0)
TB = a.tex_base.lower()


def load_np(path):
    img = bpy.data.images.load(path)
    w, h = img.size
    px = np.empty(w * h * 4, np.float32); img.pixels.foreach_get(px)
    return np.flipud(px.reshape(h, w, 4)).copy()          # row 0 = top; values are the stored bytes / 255 (see to_srgb255)


def to_srgb255(v):
    """-> the 0-255 numbers a paint program shows, which is the scale on which a difference is or is not visible.

    Checked by hand 2026-09-07 rather than assumed: for these 8-bit PNGs Blender's `pixels` hands back the STORED
    bytes divided by 255, with no colour management at all (mean |blender - raw bytes| = 0.00000 over 200k texels;
    against a linearised copy it was 0.222). So the conversion is a multiply, and an earlier version of this file
    that applied an sRGB encode here was inflating every number it printed. The comment in hd_hands_paint.py
    saying Blender returns linear floats is wrong in the same way; its save_np docstring is right."""
    return np.clip(v, 0.0, 1.0) * 255.0


def save_png(arr, path):
    import zlib, struct
    arr8 = (np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8)
    raw = b"".join(bytes([0]) + arr8[i].tobytes() for i in range(arr8.shape[0]))
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    hdr = struct.pack(">IIBBBBB", arr8.shape[1], arr8.shape[0], 8, 6, 0, 0, 0)
    sig = bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A])
    open(path, "wb").write(sig + chunk(b"IHDR", hdr) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def bilinear(img, uv):
    """uv (M,2) -> (M,3) linear RGB. Same convention as the rasteriser: x = u*W, y = (1-v)*H, pixel centres at +0.5"""
    h, w = img.shape[0], img.shape[1]
    x = uv[:, 0] * w - 0.5
    y = (1.0 - uv[:, 1]) * h - 0.5
    x0 = np.floor(x).astype(np.int64); y0 = np.floor(y).astype(np.int64)
    fx = (x - x0)[:, None]; fy = (y - y0)[:, None]
    x0 = np.clip(x0, 0, w - 1); y0 = np.clip(y0, 0, h - 1)
    x1 = np.clip(x0 + 1, 0, w - 1); y1 = np.clip(y0 + 1, 0, h - 1)
    c = img[..., :3]
    return ((c[y0, x0] * (1 - fx) + c[y0, x1] * fx) * (1 - fy) +
            (c[y1, x0] * (1 - fx) + c[y1, x1] * fx) * fy)


# ---- mesh ---------------------------------------------------------------------------------------------
mesh_path = os.path.join(a.src, a.character + ".mesh.2109108288")
bpy.ops.re_mesh.importfile(filepath=mesh_path, directory=a.src, files=[{"name": os.path.basename(mesh_path)}],
                           clearScene=True, loadMaterials=False, loadMDFData=False)
skin = [o for o in bpy.data.objects if o.type == "MESH" and any(a.skin_mat in (m.name if m else "") for m in o.data.materials)][0]
me = skin.data; names = [g.name for g in skin.vertex_groups]; uvd = me.uv_layers.active.data
Mw = skin.matrix_world
vco = np.array([Mw @ v.co for v in me.vertices], np.float32)

HAND = ("l_hand_", "r_hand_", "l_arm_wrist", "r_arm_wrist"); FORE = ("l_arm_radius", "r_arm_radius")
K = np.zeros(len(me.vertices), np.float32)
for v in me.vertices:
    ws = sorted(((g.weight, names[g.group]) for g in v.groups), reverse=True)
    if ws and (ws[0][1].startswith(HAND) or ws[0][1].startswith(FORE)):
        K[v.index] = 1.0
in_paint = np.array([max(K[i] for i in p.vertices) > 0 for p in me.polygons])
print("skin object %r: %d verts, %d faces, %d of them on hands/forearms"
      % (skin.name, len(me.vertices), len(me.polygons), int(in_paint.sum())))

# bone heads, so a seam can be named by where it sits on the hand rather than by a UV coordinate
bones = []
for o in bpy.data.objects:
    if o.type == "ARMATURE":
        for b in o.data.bones:
            bones.append((b.name, np.array(o.matrix_world @ b.head_local, np.float32)))
bone_pos = np.array([p for _, p in bones], np.float32) if bones else np.zeros((1, 3), np.float32)
bone_name = [n for n, _ in bones] or ["<no armature>"]
def nearest_bone(p):
    d = np.linalg.norm(bone_pos - p[None, :], axis=1)
    i = int(np.argmin(d))
    return bone_name[i], float(d[i])

# ---- islands + seams (shared with hd_hands_paint.py, so there is one implementation) -----------------------
island, seam_list, n_open = uv_seams.label_islands(me, uvd, vco, in_paint)
print("boundary edges with no partner anywhere in space (a real open edge): %d" % n_open)
seam_edges = [((v0, v1), fa, ua0, ua1, fb, ub0, ub1, (w0, w1))
              for (fa, ua0, ua1, fb, ub0, ub1, v0, v1, w0, w1) in seam_list]
uniq = {r: i for i, r in enumerate(sorted(set(island[in_paint].tolist())))}
print("UV islands touching the hand/forearm mask: %d; seam edges inside the mask: %d"
      % (len(uniq), len(seam_edges)))

# ---- per-face UV frame: how the texture is laid onto the skin there ---------------------------------------
# T = dP/du, B = dP/dv (metres per unit UV), N = the face normal. A tangent-space normal map means EVERYTHING in the
# map is interpreted in this frame, so if the frame turns or flips between two islands, identical map content shades
# differently either side of the join -- a lightness difference with no hard edge, which is what is left to explain.
face_frame = uv_seams.face_frames(me, uvd, vco)
face_uv = {}; face_dens = {}
for poly in me.polygons:
    face_uv[poly.index] = {int(v): np.array(uvd[l].uv, np.float64) for v, l in zip(poly.vertices, poly.loop_indices)}
    vi = list(poly.vertices)
    p0, p1, p2 = (vco[vi[0]].astype(np.float64), vco[vi[1]].astype(np.float64), vco[vi[2]].astype(np.float64))
    w0, w1, w2 = (face_uv[poly.index][vi[0]], face_uv[poly.index][vi[1]], face_uv[poly.index][vi[2]])
    d1 = w1 - w0; d2 = w2 - w0
    det = d1[0] * d2[1] - d2[0] * d1[1]
    an = np.linalg.norm(np.cross(p1 - p0, p2 - p0))
    # texel density: how much of the atlas covers how much skin
    face_dens[poly.index] = (abs(det) / (0.5 * an)) if (an > 1e-14 and face_frame[poly.index] is not None) else None

face_uv_centroid = {p.index: np.mean(list(face_uv[p.index].values()), axis=0) for p in me.polygons}

ts = (np.arange(a.samples) + 0.5) / a.samples
OFFS = [float(x) for x in a.offsets.split(",")]

rows = []
for key, fa, ua0, ua1, fb, ub0, ub1, wa in seam_edges:
    if face_frame[fa] is None or face_frame[fb] is None: continue
    p0, p1 = vco[key[0]].astype(np.float64), vco[key[1]].astype(np.float64)
    elen = float(np.linalg.norm(p1 - p0))
    ca, cb = face_uv_centroid[fa], face_uv_centroid[fb]
    ia, ib = uniq[island[fa]], uniq[island[fb]]
    for t in ts:
        pa = (1 - t) * ua0 + t * ua1
        pb = (1 - t) * ub0 + t * ub1
        da = ca - pa; db = cb - pb
        na = np.linalg.norm(da); nb = np.linalg.norm(db)
        if na < 1e-9 or nb < 1e-9: continue
        rows.append((ia, ib, pa, da / na, pb, db / nb, (1 - t) * p0 + t * p1, elen / a.samples, fa, fb,
                     vco[wa[1]].astype(np.float64) - vco[wa[0]].astype(np.float64)))

if not rows:
    print("no seam edges found inside the mask -- nothing to measure"); sys.exit(0)

IA = np.array([r[0] for r in rows]); IB = np.array([r[1] for r in rows])
UA = np.array([r[2] for r in rows]); DA = np.array([r[3] for r in rows])
UB = np.array([r[4] for r in rows]); DB = np.array([r[5] for r in rows])
P3 = np.array([r[6] for r in rows], np.float32); LEN = np.array([r[7] for r in rows], np.float32)
FA = np.array([r[8] for r in rows]); FB = np.array([r[9] for r in rows])
EDIR = np.array([r[10] for r in rows]); EDIR /= (np.linalg.norm(EDIR, axis=1, keepdims=True) + 1e-12)

def frame_arrays(faces):
    T = np.array([face_frame[f][0] for f in faces]); B = np.array([face_frame[f][1] for f in faces])
    N = np.array([face_frame[f][2] for f in faces]); Tn = np.array([face_frame[f][3] for f in faces])
    Bn = np.array([face_frame[f][4] for f in faces]); h = np.array([face_frame[f][5] for f in faces])
    return T, B, N, Tn, Bn, h
TA, BA, NA, TnA, BnA, HA = frame_arrays(FA)
TB_, BB, NB, TnB, BnB, HB = frame_arrays(FB)

# metres of skin per unit UV, along the direction we step inward: turns a millimetre into a UV offset
def uv_per_mm(T, B, D):
    step = T * D[:, 0:1] + B * D[:, 1:2]
    return 1.0 / (np.linalg.norm(step, axis=1) + 1e-12)      # unit-UV lengths per metre, along D
SA = uv_per_mm(TA, BA, DA); SB = uv_per_mm(TB_, BB, DB)
print("%d seam samples over %.1f mm of island boundary" % (len(rows), 1000 * LEN.sum()))

# Two faces can share an edge in space and still face away from each other -- a fold, or two coincident rings where
# the arm continues under a sleeve. The eye never reads those as one continuous surface, and averaging across them
# produces nonsense (the first run reported a 110 degree normal difference at the left wrist, unchanged at every
# sampling distance and present in the ARTIST'S map too, which is the signature of exactly this).
geo_ang = np.degrees(np.arccos(np.clip(np.sum(NA * NB, axis=1), -1, 1)))
fold = geo_ang > 80.0
if fold.any():
    print("  of that, %.1f mm folds back on itself (faces >80 deg apart) and is dropped:" % (1000 * LEN[fold].sum()))
    for (ia, ib) in sorted(set(zip(IA[fold].tolist(), IB[fold].tolist()))):
        mm_ = (IA == ia) & (IB == ib) & fold
        cen = P3[mm_].mean(0); bn, _ = nearest_bone(cen)
        print("      %3d/%-4d %7.1f mm  mean %.0f deg   %s" % (ia, ib, 1000 * LEN[mm_].sum(), geo_ang[mm_].mean(), bn))
    keep = ~fold
    IA, IB = IA[keep], IB[keep]; UA, DA, UB, DB = UA[keep], DA[keep], UB[keep], DB[keep]
    P3, LEN = P3[keep], LEN[keep]; FA, FB = FA[keep], FB[keep]
    TA, BA, NA, TnA, BnA, HA = TA[keep], BA[keep], NA[keep], TnA[keep], BnA[keep], HA[keep]
    TB_, BB, NB, TnB, BnB, HB = TB_[keep], BB[keep], NB[keep], TnB[keep], BnB[keep], HB[keep]
    SA, SB = SA[keep], SB[keep]; EDIR = EDIR[keep]
    print("  %d samples over %.1f mm left" % (len(IA), 1000 * LEN.sum()))

# ---- does the frame turn or flip across the seam? ---------------------------------------------------------
Nm = NA + NB; Nm /= (np.linalg.norm(Nm, axis=1, keepdims=True) + 1e-12)
def proj(V):
    P = V - Nm * np.sum(V * Nm, axis=1, keepdims=True)
    return P / (np.linalg.norm(P, axis=1, keepdims=True) + 1e-12)
ta, tb = proj(TnA), proj(TnB)
ang = np.degrees(np.arctan2(np.sum(np.cross(ta, tb) * Nm, axis=1), np.sum(ta * tb, axis=1)))
flip = HA != HB
print("tangent frame across the seam: |turn| mean %.1f deg, median %.1f, p90 %.1f, max %.1f; handedness flips on %.1f%% of the seam length"
      % (np.average(np.abs(ang), weights=LEN), np.median(np.abs(ang)), np.percentile(np.abs(ang), 90),
         np.abs(ang).max(), 100 * float(LEN[flip].sum() / LEN.sum())))


def measure(path, label):
    img = load_np(path)
    W = img.shape[1]
    print("\n=== %s  (%s, %dx%d) ===" % (label, os.path.basename(path), W, img.shape[0]))
    out = {}
    for off in OFFS:
        A = to_srgb255(bilinear(img, UA + DA * (off * 1e-3 * SA)[:, None]))
        B = to_srgb255(bilinear(img, UB + DB * (off * 1e-3 * SB)[:, None]))
        dRGB = A - B
        # perceived lightness: the sRGB luma the eye weights, on the same 0-255 scale
        L = lambda c: 0.2126 * c[:, 0] + 0.7152 * c[:, 1] + 0.0722 * c[:, 2]
        dL = L(A) - L(B)
        w = LEN
        mean_abs = float(np.average(np.abs(dL), weights=w))
        p = np.percentile(np.abs(dL), [50, 90, 99])
        print("  %.2f mm inside: |dL| mean %.2f, median %.2f, p90 %.2f, p99 %.2f, max %.2f  (sRGB 0-255 units)"
              % (off, mean_abs, p[0], p[1], p[2], float(np.abs(dL).max())))
        out[off] = (A, B, dRGB, dL)
    return img, out


def per_pair_table(dL, dRGB, title):
    print("\n  %s" % title)
    print("  %-13s %8s %8s %8s %7s  %s" % ("islands", "seam mm", "mean dL", "sd dL", "|m|/sd", "where (nearest bone)"))
    stats = []
    for (ia, ib) in sorted(set(zip(IA.tolist(), IB.tolist()))):
        m = (IA == ia) & (IB == ib)
        mm = 1000 * float(LEN[m].sum())
        if mm < a.min_mm: continue
        v = dL[m]; mean = float(np.average(v, weights=LEN[m]))
        sd = float(np.sqrt(np.average((v - mean) ** 2, weights=LEN[m])))
        cen = P3[m].mean(0); bn, bd = nearest_bone(cen)
        rgb = np.average(dRGB[m], axis=0, weights=LEN[m])
        stats.append((abs(mean) * mm, ia, ib, mm, mean, sd, bn, bd, rgb))
    stats.sort(reverse=True)
    for _, ia, ib, mm, mean, sd, bn, bd, rgb in stats[:18]:
        print("  %5d/%-7d %8.1f %+8.2f %8.2f %7s  %s (%.0f mm)  dRGB %+.1f %+.1f %+.1f"
              % (ia, ib, mm, mean, sd, ("%.1f" % (abs(mean) / sd)) if sd > 1e-6 else "inf", bn, 1000 * bd,
                 rgb[0], rgb[1], rgb[2]))
    return stats


paths = [(a.alb or os.path.join(a.work, TB + "_albm.png"), "the artist's own albedo")]
paths += [(p, "comparison") for p in a.compare]
first_stats = None
for path, label in paths:
    if not os.path.exists(path):
        print("\n!! missing: %s" % path); continue
    img, out = measure(path, label)
    off_mid = OFFS[min(1, len(OFFS) - 1)]
    A, B, dRGB, dL = out[off_mid]
    st = per_pair_table(dL, dRGB, "island pairs by how much they differ, at offset %.1f texel "
                                  "(mean dL is island A minus island B)" % off_mid)
    if first_stats is None:
        first_stats = st
        # a seam map, so the numbers can be looked at: brighter = bigger step
        W = img.shape[1]
        vis = np.zeros((W, W, 4), np.float32); vis[..., 3] = 1.0
        sc = np.clip(np.abs(dL) / 4.0, 0, 1)
        for (uv, s) in ((UA, sc), (UB, sc)):
            xs = np.clip((uv[:, 0] * W).astype(int), 0, W - 1)
            ys = np.clip(((1 - uv[:, 1]) * W).astype(int), 0, W - 1)
            vis[ys, xs, 0] = np.maximum(vis[ys, xs, 0], s)
            vis[ys, xs, 1] = np.maximum(vis[ys, xs, 1], s * 0.4)
        save_png(vis, os.path.join(a.out, "seam_step_map.png"))
        print("\n  wrote %s (red = the step across that seam, full red at 4/255)"
              % os.path.join(a.out, "seam_step_map.png"))

# ---- the normal map, taken all the way through to SHADING -------------------------------------------------
# A tangent-space normal is only half a direction: the other half is the frame it is read in. So sample the map
# either side of the seam, turn both into WORLD normals through their own island's frame, and compare. The pore
# detail differs texel to texel and averages out; what draws a band is a SYSTEMATIC difference, so average the
# world normal along each seam first and compare the averages. Then light both with a hemisphere of directions
# and report the difference as a step in sRGB 0-255 units on skin -- the same scale as the colour numbers above.
LIGHTS = None
def hemi_lights(n=64, seed=7):
    r = np.random.default_rng(seed); v = r.standard_normal((n, 3))
    return v / np.linalg.norm(v, axis=1, keepdims=True)

def world_normal(img, uv, Tn, Bn, N):
    c = bilinear(img, uv)
    nx = c[:, 0] * 2 - 1; ny = c[:, 1] * 2 - 1
    nz = np.sqrt(np.maximum(1.0 - nx * nx - ny * ny, 1e-6))
    w = Tn * nx[:, None] + Bn * ny[:, None] + N * nz[:, None]
    return w / (np.linalg.norm(w, axis=1, keepdims=True) + 1e-12)

def rotate_onto(v, frm, to):
    """rotate each v by the shortest rotation carrying frm onto to (Rodrigues). Two faces meeting at a crease have
    genuinely different normals; that turn is the MESH's, not the seam's, and comparing normals without removing it
    reports every knuckle as a seam fault (which is what the first run of this did)."""
    k = np.cross(frm, to); s = np.linalg.norm(k, axis=1, keepdims=True)
    c = np.sum(frm * to, axis=1, keepdims=True)
    k = k / (s + 1e-12)
    out = v * c + np.cross(k, v) * s + k * np.sum(k * v, axis=1, keepdims=True) * (1 - c)
    return np.where(s < 1e-9, v, out)


def measure_normals(path, label, off=0.15):
    global LIGHTS
    if LIGHTS is None: LIGHTS = hemi_lights()
    img = load_np(path)
    print("\n=== normal map: %s  (%s, %dx%d), sampled %.2f mm inside each island ===" % (label, os.path.basename(path), img.shape[1], img.shape[0], off))
    wA = world_normal(img, UA + DA * (off * 1e-3 * SA)[:, None], TnA, BnA, NA)
    wB = world_normal(img, UB + DB * (off * 1e-3 * SB)[:, None], TnB, BnB, NB)
    Gm = NA + NB; Gm /= (np.linalg.norm(Gm, axis=1, keepdims=True) + 1e-12)
    rA = rotate_onto(wA, NA, Gm); rB = rotate_onto(wB, NB, Gm)      # both sides now judged on the same ground
    tilt = np.degrees(np.arccos(np.clip(np.sum(rA * rB, axis=1), -1, 1)))
    print("  texel-to-texel difference once the mesh's own turn is removed: mean %.1f deg (this is the pores, and it averages out)"
          % np.average(tilt, weights=LEN))
    print("\n  %-13s %8s %9s %9s %9s  %s" % ("islands", "seam mm", "frame turn", "syst tilt", "shade dL", "where (nearest bone)"))
    rowsout = []
    for (ia, ib) in sorted(set(zip(IA.tolist(), IB.tolist()))):
        m = (IA == ia) & (IB == ib)
        mm = 1000 * float(LEN[m].sum())
        if mm < a.min_mm: continue
        # The systematic part. Averaging world normals along the seam does NOT work: a seam that runs right round
        # the wrist has its normals pointing every way, so the average is nearly zero and its direction is noise
        # (that is what produced a "110 degree" left wrist, unchanged with distance and present in the artist's
        # own flat map). Instead measure each sample in ITS OWN frame -- along the seam, and across it -- and
        # average those two numbers. The frame follows the face winding, which runs consistently round a loop.
        e1 = EDIR[m] - Gm[m] * np.sum(EDIR[m] * Gm[m], axis=1, keepdims=True)
        e1 /= (np.linalg.norm(e1, axis=1, keepdims=True) + 1e-12)
        e2 = np.cross(Gm[m], e1)
        dv = rA[m] - rB[m]
        w = LEN[m] / LEN[m].sum()
        syst = np.array([float(np.sum(np.sum(dv * e1, axis=1) * w)), float(np.sum(np.sum(dv * e2, axis=1) * w))])
        syst_deg = np.degrees(np.arcsin(np.clip(np.linalg.norm(syst) / 2.0, 0, 1))) * 2.0
        # light the two leans against each other in a canonical frame to turn that angle into a lightness step
        z = np.array([0.0, 0.0, 1.0])
        nA = z + np.array([syst[0], syst[1], 0.0]) * 0.5; nA /= np.linalg.norm(nA)
        nB = z - np.array([syst[0], syst[1], 0.0]) * 0.5; nB /= np.linalg.norm(nB)
        lit = LIGHTS[LIGHTS @ z > 0.15]
        if len(lit) == 0: lit = LIGHTS
        dsh = float(np.mean(np.abs(np.maximum(lit @ nA, 0) - np.maximum(lit @ nB, 0))))
        dL255 = dsh * 222.0        # a step in reflected light, put back on the 0-255 scale of skin near 222
        turn = float(np.average(np.abs(ang[m]), weights=LEN[m]))
        cen = P3[m].mean(0); bn, _ = nearest_bone(cen)
        rowsout.append((dL255, ia, ib, mm, turn, syst_deg, dL255, bn))
    rowsout.sort(reverse=True)
    for _, ia, ib, mm, turn, syst_deg, dL255, bn in rowsout[:14]:
        print("  %5d/%-7d %8.1f %9.1f %9.2f %9.2f  %s" % (ia, ib, mm, turn, syst_deg, dL255, bn))
    return rowsout


nrm_paths = [(a.nrm or os.path.join(a.work, TB + "_nrmr.png"), "the artist's own (flat over the hands)")]
nrm_paths += [(p, "ours") for p in a.compare_nrm]
for path, label in nrm_paths:
    if os.path.exists(path):
        # several distances: a value that collapses as we step away from the seam was bleed from the neighbouring
        # atlas content in the boundary texel, not a difference between the two islands
        for off in (0.15, 0.6, 1.5):
            measure_normals(path, label, off)
    else:
        print("\n!! missing normal map: %s" % path)

# ---- texel density: does the atlas give one island finer skin than its neighbour? -------------------------
# The shipped material still runs the engine's own Detail_Skin tile in UV space, so its grain is as fine as the
# atlas is dense. Two islands at different densities show that tile at different physical sizes -- "grainy one
# side, smooth the other" -- and nothing in our textures can change it.
print("\n=== texel density per island (atlas texels per millimetre of skin, at 1024) ===")
dens_isl = defaultdict(list)
for poly in me.polygons:
    if not in_paint[poly.index] or face_dens[poly.index] is None: continue
    dens_isl[uniq[island[poly.index]]].append(np.sqrt(face_dens[poly.index]) * 1024 / 1000.0)
print("  %-8s %10s %s" % ("island", "texels/mm", "spread (p10-p90)"))
for i in sorted(dens_isl):
    d = np.array(dens_isl[i])
    print("  %-8d %10.2f  %.2f - %.2f" % (i, np.median(d), np.percentile(d, 10), np.percentile(d, 90)))
print("\n  step in density across each seam (ratio of the two islands medians, >1.15 is a visible grain change):")
dmed = {i: float(np.median(dens_isl[i])) for i in dens_isl}
for (ia, ib) in sorted(set(zip(IA.tolist(), IB.tolist()))):
    m = (IA == ia) & (IB == ib); mm = 1000 * float(LEN[m].sum())
    if mm < a.min_mm or ia not in dmed or ib not in dmed: continue
    r = max(dmed[ia], dmed[ib]) / max(min(dmed[ia], dmed[ib]), 1e-9)
    if r > 1.10:
        cen = P3[m].mean(0); bn, _ = nearest_bone(cen)
        print("    %3d/%-4d %7.1f mm  x%.2f   %s" % (ia, ib, mm, r, bn))

# ---- per-island bulk colour: is a whole island offset from its neighbours? --------------------------------
print("\n=== each island's own average colour over the mask (artist's albedo) ===")
img = load_np(paths[0][0]); W = img.shape[1]
isl_px = defaultdict(list)
for poly in me.polygons:
    if not in_paint[poly.index]: continue
    c = face_uv_centroid[poly.index]
    isl_px[uniq[island[poly.index]]].append(c)
print("  %-8s %7s %-22s %s" % ("island", "faces", "mean sRGB (R G B)", "nearest bone to its centre"))
for i in sorted(isl_px):
    uvs = np.array(isl_px[i])
    cols = to_srgb255(bilinear(img, uvs))
    fids = [p.index for p in me.polygons if in_paint[p.index] and uniq[island[p.index]] == i]
    cen = np.mean([vco[list(me.polygons[f].vertices)].mean(0) for f in fids], axis=0)
    bn, _ = nearest_bone(cen)
    print("  %-8d %7d %6.1f %6.1f %6.1f      %s" % (i, len(fids), cols[:, 0].mean(), cols[:, 1].mean(), cols[:, 2].mean(), bn))
print("\ndone.")
