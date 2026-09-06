"""HD hands (roadmap H2), step 1: where are Claire's hands in her body texture, and what do the textures hold?
Runs headless in Blender 5.2 with RE Mesh Editor installed:
  blender -b --python hd_hands_recon.py -- <plXXXX folder with .mesh/.tex.34> <work_dir> [pl1000 Body_Mat pl1000_jacket]
Writes into <work_dir> (game-data derivatives: keep OUT of git):
  <tex>_albm.png / <tex>_nrmr.png               the two textures, decoded via the add-on's own converter
  hand_mask_1024.png, hand_mask_4096.png        white where hand+forearm UV faces land (skin submesh only)
  hd_hands_recon.txt                            the numbers printed below
Nothing here modifies the game. Read-only on the inputs.
"""
import bpy, sys, os, importlib, time
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
SRC, WORK = argv[0], argv[1]
# character prefix, skin material substring, texture base name. 2026-09-06 13:15: pl3000 is SHERRY (chain + pendant), Claire is pl1000
# with skin material pl1000_Body_Mat sampling the pl1000_Jacket_* atlas. Defaults are Claire now.
CHAR = argv[2] if len(argv) > 2 else "pl1000"; SKIN_MAT = argv[3] if len(argv) > 3 else "Body_Mat"; TEX = argv[4] if len(argv) > 4 else "pl1000_jacket"
os.makedirs(WORK, exist_ok=True)
report = []
def say(*a):
    s = " ".join(str(x) for x in a); print(s); report.append(s)

# ---- 1. textures -> PNG through the add-on's converter -----------------------------------
import ctypes
ctypes.windll.ole32.CoInitializeEx(None, 0)   # texconv.dll writes PNG through WIC, which needs COM up on this thread (found 2026-09-06)
tex_utils = importlib.import_module("RE-Mesh-Editor.modules.tex.re_tex_utils")
Texconv = importlib.import_module("RE-Mesh-Editor.modules.ddsconv.directx.texconv").Texconv
tc = Texconv()
for base in (TEX + "_albm", TEX + "_nrmr"):
    tex = os.path.join(SRC, base + ".tex.34")
    dds = os.path.join(WORK, base + ".dds")
    tex_utils.convertTexFileToDDS(tex, dds)
    png = tc.convert_to_png(dds, out=WORK, verbose=False)
    say("converted", base, "->", png, os.path.getsize(dds), "bytes dds")

# ---- 2. the mesh: which faces are hands, and where they sit in UV space --------------------
mesh_path = os.path.join(SRC, CHAR + ".mesh.2109108288")
bpy.ops.re_mesh.importfile(filepath=mesh_path, directory=SRC, files=[{"name": os.path.basename(mesh_path)}],
                           clearScene=True, loadMaterials=False, loadMDFData=False)
HAND = ("l_hand_", "r_hand_", "l_arm_wrist", "r_arm_wrist")
FORE = ("l_arm_radius", "r_arm_radius")

def top_group(v, names):
    best, bw = None, 0.0
    for g in v.groups:
        if g.weight > bw: best, bw = names[g.group], g.weight
    return best or ""

W1, W4 = 1024, 4096
mask1 = np.zeros((W1, W1), np.uint8); mask4 = np.zeros((W4, W4), np.uint8)

def raster(tri, mask):
    W = mask.shape[0]
    pts = [(u * W, (1.0 - v) * W) for u, v in tri]          # image row 0 = top = v 1
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    x0, x1 = max(int(min(xs)) - 1, 0), min(int(max(xs)) + 2, W)
    y0, y1 = max(int(min(ys)) - 1, 0), min(int(max(ys)) + 2, W)
    if x1 <= x0 or y1 <= y0: return
    gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
    (ax, ay), (bx, by), (cx, cy) = pts
    det = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay)
    if abs(det) < 1e-9: return
    l1 = ((bx - gx) * (cy - gy) - (cx - gx) * (by - gy)) / det
    l2 = ((cx - gx) * (ay - gy) - (ax - gx) * (cy - gy)) / det
    l3 = 1.0 - l1 - l2
    eps = -0.002
    inside = (l1 >= eps) & (l2 >= eps) & (l3 >= eps)
    sub = mask[y0:y1, x0:x1]; sub[inside] = 255

for ob in [o for o in bpy.data.objects if o.type == "MESH"]:
    me = ob.data
    names = [g.name for g in ob.vertex_groups]
    mats = [m.name if m else "?" for m in me.materials]
    uv = me.uv_layers.active
    if uv is None: say("mesh", ob.name, "has no UVs"); continue
    skin = any(SKIN_MAT in m for m in mats)          # only the skin material is repainted; sleeves/props are other materials
    vcls = [top_group(v, names) for v in me.vertices]
    counts = {"hand": 0, "forearm": 0, "other": 0}
    bbox = {"hand": [1, 1, 0, 0], "forearm": [1, 1, 0, 0]}
    for poly in me.polygons:
        cls = [vcls[i] for i in poly.vertices]
        n_hand = sum(1 for c in cls if c.startswith(HAND))
        n_fore = sum(1 for c in cls if c.startswith(FORE))
        kind = "hand" if n_hand * 2 >= len(cls) else ("forearm" if (n_fore + n_hand) * 2 >= len(cls) else "other")
        counts[kind] += 1
        if kind == "other": continue
        uvs = [tuple(uv.data[li].uv) for li in poly.loop_indices]
        b = bbox[kind]
        for u, v in uvs:
            b[0], b[1], b[2], b[3] = min(b[0], u), min(b[1], v), max(b[2], u), max(b[3], v)
        for k in range(1, len(uvs) - 1):
            tri = (uvs[0], uvs[k], uvs[k + 1])
            if skin: raster(tri, mask1); raster(tri, mask4)
    say("mesh %-28s verts=%6d faces=%6d mats=%s  hand faces=%d forearm faces=%d other=%d" % (
        ob.name, len(me.vertices), len(me.polygons), mats, counts["hand"], counts["forearm"], counts["other"]))
    for k in ("hand", "forearm"):
        if counts[k]:
            b = bbox[k]
            say("   %-8s UV bbox u=[%.4f, %.4f] v=[%.4f, %.4f]  -> px x=[%d,%d] y=[%d,%d] on 1024 (y from top)" % (
                k, b[0], b[2], b[1], b[3], b[0] * W1, b[2] * W1, (1 - b[3]) * W1, (1 - b[1]) * W1) + ("" if skin else "  (other material)"))
    hand_groups = sorted({names[g.group] for v in me.vertices for g in v.groups if names[g.group].startswith(HAND)})
    if hand_groups: say("   hand vertex groups:", ", ".join(hand_groups))

say("hand+forearm mask covers %.1f%% of the 1024 texture (%d px)" % (100.0 * (mask1 > 0).mean(), int((mask1 > 0).sum())))

def save_mask(mask, path):
    W = mask.shape[0]
    img = bpy.data.images.new(os.path.basename(path), W, W, alpha=False)
    m = np.flipud(mask).astype(np.float32) / 255.0          # Blender pixel row 0 = bottom
    px = np.empty((W, W, 4), np.float32); px[..., 0] = m; px[..., 1] = m; px[..., 2] = m; px[..., 3] = 1.0
    img.pixels.foreach_set(px.ravel())
    img.filepath_raw = path; img.file_format = "PNG"; img.save()
save_mask(mask1, os.path.join(WORK, "hand_mask_1024.png"))
save_mask(mask4, os.path.join(WORK, "hand_mask_4096.png"))

# ---- 3. what the albedo looks like under the mask (skin tone to match later) ----------------
alb = bpy.data.images.load(os.path.join(WORK, TEX + "_albm.png"))
W = alb.size[0]; px = np.empty(W * W * 4, np.float32); alb.pixels.foreach_get(px); px = np.flipud(px.reshape(W, W, 4))
say("albedo image %dx%d, channels %d" % (alb.size[0], alb.size[1], alb.channels))
if W == W1:
    sel = px[mask1 > 0]
    if len(sel): say("albedo under hand mask: mean RGBA = (%.3f %.3f %.3f %.3f), std = (%.3f %.3f %.3f)" % (*sel[:, :3].mean(0), sel[:, 3].mean(), *sel[:, :3].std(0)))
nrm = bpy.data.images.load(os.path.join(WORK, TEX + "_nrmr.png"))
px2 = np.empty(W * W * 4, np.float32); nrm.pixels.foreach_get(px2); px2 = np.flipud(px2.reshape(W, W, 4))
if W == W1:
    sel = px2[mask1 > 0]
    if len(sel): say("nrmr under hand mask: mean RGBA = (%.3f %.3f %.3f %.3f), std = (%.3f %.3f %.3f %.3f)  [RE Engine NRMR: normal xy + roughness in one of B/A]" % (*sel.mean(0), *sel.std(0)))

# ---- 4. overlay for eyes: hands/forearms tinted red on the albedo --------------------------
ov = px.copy(); m = mask1 > 0
ov[m, 0] = ov[m, 0] * 0.4 + 0.6; ov[m, 1] *= 0.4; ov[m, 2] *= 0.4
img = bpy.data.images.new("overlay", W, W, alpha=False); img.pixels.foreach_set(np.flipud(ov).ravel())
img.filepath_raw = os.path.join(WORK, "albedo_hand_overlay.png"); img.file_format = "PNG"; img.save()
open(os.path.join(WORK, "hd_hands_recon.txt"), "w", encoding="utf-8").write("\n".join(report) + "\n")
print("DONE")
