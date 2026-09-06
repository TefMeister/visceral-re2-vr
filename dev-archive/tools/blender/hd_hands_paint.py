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
ap.add_argument("--veins", type=float, default=0.35, help="dorsal vein relief lifted from the original albedo (0 = none); the lift finds blobs, not lines, so it is a hint by default")
ap.add_argument("--wrinkles", type=float, default=1.0, help="fine wrinkle networks around the joints (0 = none)")
ap.add_argument("--anatomy", type=float, default=0.0, help="rig-derived veins/tendons drawn along the mesh surface (off by default until judged in VR). The 2026-09-06 02:50 attempts picked the little-finger edge instead of the back of the hand; fixed 12:00 via the finger-curl sign")
ap.add_argument("--crease", type=float, default=1.0, help="joint crease depth (0 = none)")
ap.add_argument("--palm-pores", type=float, default=0.3, help="pore relief on the PALM side as a fraction of the back (baked relief AND the detail-mask texture)")
ap.add_argument("--tone", type=float, default=1.0, help="albedo variation strength (0 = none)")
ap.add_argument("--detail-png", default=None, help="tiling detail normal PNG (default <work>/visceral_skin_detail_NRM.png)")
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
    if len(ws) >= 2 and ws[0][1].startswith(HAND + FORE) and ws[1][1].startswith(HAND + FORE):
        J[v.index] = float(np.clip(2.0 * ws[1][0] / max(ws[0][0] + ws[1][0], 1e-6), 0, 1))
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

def blur(img, sigma):
    if sigma <= 0: return img
    y = np.fft.fftfreq(N)[:, None]; x = np.fft.fftfreq(N)[None, :]
    g = np.exp(-2 * (np.pi ** 2) * (sigma ** 2) * (x ** 2 + y ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(img) * g)).astype(np.float32)

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
det = load_np(det_png)                                       # 1024 tile, RG = normal xy
S = 10                                                       # tiles across the texture: finer than pass 1 (6), ~0.6 mm pores
ty = (np.arange(N) * det.shape[0] * S // N) % det.shape[0]; tx = (np.arange(N) * det.shape[1] * S // N) % det.shape[1]
det_xy = det[np.ix_(ty, tx)][..., :2] * 2.0 - 1.0
rng = np.random.default_rng(3)
mottle = blur(rng.standard_normal((N, N)).astype(np.float32), N / 90.0); mottle /= (mottle.std() + 1e-9)
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
    tend = np.zeros((N, N), np.float32); vein_lines = np.zeros((N, N), np.float32)
    fieldD = np.zeros((N, N), np.float32)
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
                dd = unit(ddir0 - ax * np.dot(ddir0, ax))
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
        for i, k in enumerate(kn):                                               # extensor tendons: from under the wrist retinaculum -> each knuckle
            surface_line(tend, dsurf, w + (k - w) * 0.15, w + (k - w) * 0.93, lat, 0.0, 0.0, 11 + i, lift=dlift)
        for i in range(len(kn) - 1):                                              # dorsal veins: the gaps between knuckles -> down between the tendons
            gap = (kn[i] + kn[i + 1]) / 2
            surface_line(vein_lines, dsurf, w + (gap - w) * 0.22, w + (gap - w) * 0.9, lat, 0.0, 0.004 * (1 if i % 2 else -1), 21 + i, lift=dlift, taper=(0.25, 0.15))
        if len(kn) >= 4:                                                          # the dorsal venous arch, joining them a third of the way up
            arch = [w + (k - w) * 0.36 for k in kn]
            for i in range(3): surface_line(vein_lines, dsurf, arch[i], arch[i + 1], lat, 0.0, 0.003, 41 + i, steps=30, taper=(0.3, 0.3), lift=dlift, scale=0.45)   # the arch is fainter than the veins it joins
            web = (kn[0] + t1) / 2                                                # the cephalic vein: from the thumb/index web toward the wrist, thumb side
            surface_line(vein_lines, dsurf, w + (web - w) * 0.85, arch[0], lat, 0.0, 0.004, 51, steps=60, taper=(0.2, 0.3), lift=dlift, scale=0.8)
        if el is not None:                                                        # forearm veins: two, along the arm, both faces
            fidx = np.array([i for i in range(len(me.vertices)) if topname[i].startswith(side + "_arm_radius")])
            if len(fidx) > 20:
                Dv[fidx] = 1.0
                fsurf = make_surface(fidx)
                for i, off in enumerate((-0.012, 0.011)):
                    surface_line(vein_lines, fsurf, w + (el - w) * 0.05, w + (el - w) * 0.85, lat, off, 0.005, 31 + i, steps=120, max_dist=0.035)
                print("anatomy %s forearm: %d verts, 2 veins" % (side, len(fidx)))
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
    tend = line_blur(tend, 1.7 * mm)                                     # ~4 mm wide ridge
    vein_lines = line_blur(vein_lines, 1.2 * mm)                         # ~2.8 mm wide
    vein_lines = vein_lines * dorsal * (1.0 - fieldF)
    tend = tend * dorsal * (1.0 - fieldF)
    # amplitudes chosen for a max normal tilt of ~0.25 (veins) / ~0.12 (tendons) at nrm_from_height scale 6: A = tilt*sigma/(6*0.61)
    h_anat = (vein_lines * 0.80 + tend * 0.50) * a.anatomy               # raised ×1.4 after VR 12:07 (invisible at 0.55/0.40 in the game's shading)               # line_blur peaks at 0.70, so these are ~0.39 / 0.28 effective
    if a.anatomy > 0:                                         # full-res masks for hd_hands_debug_render.py (where do the lines land in 3D?)
        dbg = np.zeros((N, N, 4), np.float32); dbg[..., 0] = tend; dbg[..., 1] = vein_lines; dbg[..., 2] = dorsal; dbg[..., 3] = 1.0
        save_np(dbg, os.path.join(a.out, "anat_mask.png"))
    print("anatomy: dorsal %.1f%% of the hand mask; veins cover %.2f%%, tendons %.2f%%" % (100 * (dorsal[fieldK > 0] > 0.5).mean(),
          100 * (vein_lines[fieldK > 0] > 0.3).mean(), 100 * (tend[fieldK > 0] > 0.3).mean()))
except Exception as e:
    import traceback; traceback.print_exc(); print("anatomy: skipped:", e)
cx, cy = nrm_from_height(h_crease + h_veins + h_wrinkle + h_anat, 6.0)
palm_pore = a.palm_pores + (1.0 - a.palm_pores) * dorsal        # palms: 30 % of the pore relief (VR 12:07: "palms very rough compared to the back")
nx = (nrm[..., 0] * 2 - 1) + edge * (cx + det_xy[..., 0] * 0.35 * a.pores * (0.7 + 0.3 * fieldF) * palm_pore)
ny = (nrm[..., 1] * 2 - 1) + edge * (cy + det_xy[..., 1] * 0.35 * a.pores * (0.7 + 0.3 * fieldF) * palm_pore)
nz = np.sqrt(np.clip(1.0 - nx ** 2 - ny ** 2, 0.05, 1.0))
out_nrm = nrm.copy()
out_nrm[..., 0] = nx * 0.5 + 0.5; out_nrm[..., 1] = ny * 0.5 + 0.5; out_nrm[..., 2] = nz
# roughness (alpha, hypothesis): slightly lower in creases/knuckles where skin is tighter and moister
out_nrm[..., 3] = np.clip(nrm[..., 3] - edge * creaseJ * 0.06, 0, 1)

# ---- 4. albedo: joint redness + mottling ----------------------------------------------------------------
out_alb = alb.copy()
red = edge * np.clip(creaseJ * 1.5, 0, 1) * 0.35 * a.tone
out_alb[..., 0] = alb[..., 0] * (1.0 + 0.10 * red)
out_alb[..., 1] = alb[..., 1] * (1.0 - 0.18 * red)
out_alb[..., 2] = alb[..., 2] * (1.0 - 0.22 * red)
mot = 1.0 + edge * mottle * 0.02 * a.tone
vt = np.clip(veins * 0.6 + vein_lines * 1.4, 0, 1)
out_alb[..., 0] *= (1.0 - vt * 0.16); out_alb[..., 1] *= (1.0 - vt * 0.08); out_alb[..., 2] *= (1.0 - vt * 0.02)   # veins: cooler and ~12 % darker (VR 12:07 could not see them at 0.07)
out_alb[..., :3] *= mot[..., None]
# pores slightly darker: use the detail map's alpha (AO) very gently
det_ao = det[np.ix_(ty, tx)][..., 3]
out_alb[..., :3] *= (1.0 - edge * (1.0 - det_ao) * 0.06 * a.pores)[..., None]   # barely-there: pores read as relief, not dirt
out_alb[..., 3] = alb[..., 3]                                 # metallic untouched

# detail mask (MSK1, BC4 2048, white over the hands): scale it down on the palms so the material's TILED pores follow the same rule
msk_png = os.path.join(a.work, TB + "_msk1.png")
if os.path.exists(msk_png):
    msk = load_np(msk_png); Wm = msk.shape[0]
    step = N // Wm
    pp = palm_pore[::step, ::step] if step > 1 else palm_pore
    ed = edge[::step, ::step] if step > 1 else edge
    m = msk[..., 0] * (1.0 - ed) + msk[..., 0] * pp * ed
    msk_out = np.repeat(m[..., None], 4, axis=2); msk_out[..., 3] = 1.0
    save_np(msk_out, os.path.join(a.out, a.tex_base + "_MSK1.png"))
    print("detail mask: hand mean %.2f (was %.2f)" % (m[ed > 0.5].mean(), msk[..., 0][ed > 0.5].mean()))
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
