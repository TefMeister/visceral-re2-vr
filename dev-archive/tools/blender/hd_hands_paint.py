"""HD hands tier 1b (first, procedural pass): re-author Claire's body textures at 4K with real hand detail baked in.
Headless Blender 5.2 + RE Mesh Editor:
  blender -b --python hd_hands_paint.py -- <pl3000 folder> <work_dir> <out_dir> [--size 4096] [--pores 1.0] [--crease 1.0] [--tone 1.0]

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
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
ap = argparse.ArgumentParser()
ap.add_argument("src"); ap.add_argument("work"); ap.add_argument("out")
ap.add_argument("--size", type=int, default=4096)
ap.add_argument("--pores", type=float, default=0.3, help="micro-relief strength (0 = none); 1.0 was the first pass, sandpaper in game")
ap.add_argument("--veins", type=float, default=0.35, help="dorsal vein relief lifted from the original albedo (0 = none); the lift finds blobs, not lines, so it is a hint by default")
ap.add_argument("--wrinkles", type=float, default=1.0, help="fine wrinkle networks around the joints (0 = none)")
ap.add_argument("--anatomy", type=float, default=0.0, help="EXPERIMENTAL, off by default: rig-derived veins/tendons drawn along the mesh surface. 2026-09-06 05:50: the dorsal-side pick is wrong (lines hug the island edge, angular), so it draws scars, not veins")
ap.add_argument("--crease", type=float, default=1.0, help="joint crease depth (0 = none)")
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
for base in ("pl3000_body_albm", "pl3000_body_nrmr"):
    png = os.path.join(a.work, base + ".png")
    if not os.path.exists(png):
        dds = os.path.join(a.work, base + ".dds")
        tex_utils.convertTexFileToDDS(os.path.join(a.src, base + ".tex.34"), dds)
        png = tc.convert_to_png(dds, out=a.work, verbose=False)
    src_png[base] = png
alb = load_np(src_png["pl3000_body_albm"], N)          # Blender gives sRGB PNG pixels back as linear floats
nrm = load_np(src_png["pl3000_body_nrmr"], N)          # normal map: treated as data (we only nudge RG)
print("upscaled to %dx%d: albedo mean %s, nrmr mean %s" % (N, N, np.round(alb.mean((0, 1)), 3), np.round(nrm.mean((0, 1)), 3)))

# ---- 2. mesh -> per-pixel fields in UV space ----------------------------------------------------------
mesh_path = os.path.join(a.src, "pl3000.mesh.2109108288")
bpy.ops.re_mesh.importfile(filepath=mesh_path, directory=a.src, files=[{"name": os.path.basename(mesh_path)}],
                           clearScene=True, loadMaterials=False, loadMDFData=False)
skin = [o for o in bpy.data.objects if o.type == "MESH" and any("Skin" in (m.name if m else "") for m in o.data.materials)][0]
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
    def polyline(img, uvs):
        """draw a UV polyline, skipping jumps across islands"""
        for q0, q1 in zip(uvs[:-1], uvs[1:]):
            if np.linalg.norm(q1 - q0) > 0.03: continue
            n = max(2, int(np.linalg.norm(q1 - q0) * N))
            t = np.linspace(0, 1, n)[:, None]; pts = (1 - t) * q0 + t * q1
            xs = np.clip((pts[:, 0] * N).astype(int), 0, N - 1); ys = np.clip(((1 - pts[:, 1]) * N).astype(int), 0, N - 1)
            img[ys, xs] = 1.0
    def surface_line(img, cand, p_from, p_to, lateral, offs, wiggle_amp, seed, steps=80):
        """sample the 3D segment p_from->p_to shifted sideways by offs (+ a gentle sine wiggle), snap each sample to the
        nearest candidate vertex, draw the UV polyline through them"""
        r = np.random.default_rng(seed); ph = r.uniform(0, 6.28)
        uvs = []
        for t in np.linspace(0, 1, steps):
            q = p_from + (p_to - p_from) * t + lateral * (offs + wiggle_amp * np.sin(t * 7.0 + ph) * t * (1 - t) * 4)
            j = cand[np.argmin(np.linalg.norm(vco[cand] - q, axis=1))]
            uvs.append(vuv[j])
        polyline(img, np.array(uvs))
    tend = np.zeros((N, N), np.float32); vein_lines = np.zeros((N, N), np.float32)
    fieldD = np.zeros((N, N), np.float32)
    for side in ("l", "r"):
        w = jpos(side + "_arm_wrist"); m0 = jpos(side + "_hand_middle_0"); t1 = jpos(side + "_hand_thumb_1"); el = jpos(side + "_arm_radius")
        if w is None or m0 is None or t1 is None: print("anatomy: bones missing for", side); continue
        axis = unit(m0 - w); tv = t1 - w; palm = unit(tv - axis * np.dot(tv, axis)); lat = unit(np.cross(axis, palm))
        hidx = np.array([i for i in range(len(me.vertices)) if topname[i].startswith((side + "_hand_", side + "_arm_wrist"))])
        Dv[hidx] = np.clip(-(vno[hidx] @ palm), 0, 1)
        dors = hidx[(Dv[hidx] > 0.5) & (~is_finger[hidx])]                       # back of the hand, metacarpal region
        kn = [jpos(side + "_hand_%s_0" % f) for f in ("index", "middle", "ring", "little")]
        kn = [k for k in kn if k is not None]
        print("anatomy %s: %d hand verts, %d dorsal metacarpal verts, %d knuckles" % (side, len(hidx), len(dors), len(kn)))
        if len(dors) < 20 or len(kn) < 2: continue
        for i, k in enumerate(kn):                                               # extensor tendons: wrist -> each knuckle
            surface_line(tend, dors, w + (k - w) * 0.2, w + (k - w) * 0.9, lat, 0.0, 0.0, 11 + i)
        for i in range(len(kn) - 1):                                              # dorsal veins: wrist -> the gaps between knuckles
            gap = (kn[i] + kn[i + 1]) / 2
            surface_line(vein_lines, dors, w + (gap - w) * 0.08, w + (gap - w) * 0.88, lat, 0.0, 0.006 * (1 if i % 2 else -1), 21 + i)
        if el is not None:                                                        # forearm veins: two, along the arm, both faces
            fidx = np.array([i for i in range(len(me.vertices)) if topname[i].startswith(side + "_arm_radius")])
            if len(fidx) > 20:
                Dv[fidx] = 1.0
                for i, off in enumerate((-0.012, 0.011)):
                    surface_line(vein_lines, fidx, w + (el - w) * 0.05, w + (el - w) * 0.85, lat, off, 0.005, 31 + i, steps=120)
                print("anatomy %s forearm: %d verts, 2 veins" % (side, len(fidx)))
    for poly in me.polygons:
        vi = list(poly.vertices)
        if max(K[i] for i in vi) <= 0: continue
        uvs = [tuple(uv.data[li].uv) for li in poly.loop_indices]
        for k in range(1, len(uvs) - 1):
            raster((uvs[0], uvs[k], uvs[k + 1]), ([Dv[vi[0]], Dv[vi[k]], Dv[vi[k + 1]]],), (fieldD,))
    dorsal = blur(np.clip((fieldD - 0.35) / 0.3, 0, 1), N / 1024.0 * 1.0) * edge
    tend = blur(tend, N / 4096.0 * 9.0); tend = tend / (tend.max() + 1e-9)
    vein_lines = blur(vein_lines, N / 4096.0 * 3.5); vein_lines = vein_lines / (vein_lines.max() + 1e-9)
    vein_lines = vein_lines * dorsal * (1.0 - fieldF)
    tend = tend * dorsal * (1.0 - fieldF)
    h_anat = (vein_lines * 0.55 + tend * 0.22) * a.anatomy
    print("anatomy: dorsal %.1f%% of the hand mask; veins cover %.2f%%, tendons %.2f%%" % (100 * (dorsal[fieldK > 0] > 0.5).mean(),
          100 * (vein_lines[fieldK > 0] > 0.3).mean(), 100 * (tend[fieldK > 0] > 0.3).mean()))
except Exception as e:
    import traceback; traceback.print_exc(); print("anatomy: skipped:", e)
cx, cy = nrm_from_height(h_crease + h_veins + h_wrinkle + h_anat, 6.0)
nx = (nrm[..., 0] * 2 - 1) + edge * (cx + det_xy[..., 0] * 0.35 * a.pores * (0.7 + 0.3 * fieldF))
ny = (nrm[..., 1] * 2 - 1) + edge * (cy + det_xy[..., 1] * 0.35 * a.pores * (0.7 + 0.3 * fieldF))
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
vt = np.clip(veins + vein_lines * 1.2, 0, 1)
out_alb[..., 0] *= (1.0 - vt * 0.07); out_alb[..., 1] *= (1.0 - vt * 0.035)          # veins: a touch cooler, hardly darker
out_alb[..., :3] *= mot[..., None]
# pores slightly darker: use the detail map's alpha (AO) very gently
det_ao = det[np.ix_(ty, tx)][..., 3]
out_alb[..., :3] *= (1.0 - edge * (1.0 - det_ao) * 0.06 * a.pores)[..., None]   # barely-there: pores read as relief, not dirt
out_alb[..., 3] = alb[..., 3]                                 # metallic untouched

save_np(out_alb, os.path.join(a.out, "pl3000_Body_ALBM.png"))
save_np(out_nrm, os.path.join(a.out, "pl3000_Body_NRMR.png"))
# preview crops of the hand strip (bottom 15%), original vs new, for eyes
def crop(img): return img[int(N * 0.85):, :, :]
save_np(np.concatenate([crop(alb), crop(out_alb)], axis=0), os.path.join(a.out, "preview_albedo_strip.png"))
save_np(np.concatenate([crop(nrm), crop(out_nrm)], axis=0), os.path.join(a.out, "preview_normal_strip.png"))
vv = np.repeat(np.clip(veins + vein_lines + (h_anat > 0.05) * 0.3, 0, 1)[int(N * 0.85):, :, None], 4, axis=2); vv[..., 3] = 1.0
save_np(vv, os.path.join(a.out, "preview_veins_strip.png"))
print("wrote 4K ALBM/NRMR + previews to", a.out)
print("DONE")
