"""HD hands, veins/tendons step 1: WHICH SIDE of the hand does the dorsal pick actually select?
Headless Blender 5.2 + RE Mesh Editor:
  blender -b --python hd_hands_dorsal_preview.py -- <pl3000 folder> <work_dir> <out_dir>

Background (2026-09-06, hd_hands_paint.py --anatomy): the rig-derived veins landed on an island edge as scars.
The pick there was  Dv = clip(-(normal . palm))  with  palm = the thumb's offset from the wrist->middle axis.
That vector lies IN the palm plane (it points at the thumb), so -(n . palm) selects the little-finger EDGE of
the hand, not its back. The plane's normal is  lat = axis x palm  -- but its sign flips between the left and
right hand, so the side is fixed here by where the thumb's second joint sits relative to the metacarpal
centroid (the thumb hangs on the palmar side).  [hypothesis until the renders below say so]

Writes into <out_dir>:
  <side>_albedo_{A,B}.png     the hand with the game albedo, seen from +lat (A) and -lat (B): nails = dorsal
  <side>_old_{A,B}.png        the old Dv (thumb-direction pick) as vertex colour, same two views
  <side>_new_{A,B}.png        the candidate Dv (plane-normal pick, thumb-signed), same two views
  uv_strip_old.png / uv_strip_new.png   the hand strip of the 1024 albedo with each pick tinted green
  dorsal_preview.txt          the numbers
Read-only on the game files. Outputs are derivatives: keep out of git.
"""
import bpy, sys, os, math, ctypes
import numpy as np
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index("--") + 1:]
SRC, WORK, OUT = argv[0], argv[1], argv[2]
os.makedirs(OUT, exist_ok=True)
ctypes.windll.ole32.CoInitializeEx(None, 0)
report = []
def say(*a):
    s = " ".join(str(x) for x in a); print(s); report.append(s)

mesh_path = os.path.join(SRC, "pl3000.mesh.2109108288")
bpy.ops.re_mesh.importfile(filepath=mesh_path, directory=SRC, files=[{"name": os.path.basename(mesh_path)}],
                           clearScene=True, loadMaterials=False, loadMDFData=False)
skin = [o for o in bpy.data.objects if o.type == "MESH" and any("Skin" in (m.name if m else "") for m in o.data.materials)][0]
arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
me = skin.data; names = [g.name for g in skin.vertex_groups]; uv = me.uv_layers.active
for o in bpy.data.objects:
    if o.type == "MESH" and o is not skin: o.hide_render = True
M = skin.matrix_world; R3 = M.to_3x3()
vco = np.array([M @ v.co for v in me.vertices], np.float32)
vno = np.array([(R3 @ v.normal).normalized() for v in me.vertices], np.float32)
def top(v):
    best, bw = "", 0.0
    for g in v.groups:
        if g.weight > bw: best, bw = names[g.group], g.weight
    return best
topname = np.array([top(v) for v in me.vertices])
def jpos(n):
    b = arm.data.bones.get(n); return np.array(arm.matrix_world @ b.head_local, np.float32) if b else None
def unit(v): return v / (np.linalg.norm(v) + 1e-9)

old = np.zeros(len(me.vertices), np.float32); new = np.zeros(len(me.vertices), np.float32)
views = {}
for side in ("l", "r"):
    w = jpos(side + "_arm_wrist"); m0 = jpos(side + "_hand_middle_0"); t1 = jpos(side + "_hand_thumb_1"); t2 = jpos(side + "_hand_thumb_2")
    axis = unit(m0 - w); tv = t1 - w; palm = unit(tv - axis * np.dot(tv, axis)); lat = unit(np.cross(axis, palm))
    hidx = np.where(np.char.startswith(topname, side + "_hand_") | (topname == side + "_arm_wrist"))[0]
    meta = np.array([i for i in hidx if topname[i].endswith("_0") or topname[i] == side + "_arm_wrist"])
    c = vco[meta].mean(0)
    thumb_off = float(np.dot(t2 - c, lat))
    # second opinion: fingers flex toward the PALM, so the bend of each finger chain along lat points palmward
    curl = 0.0
    for f in ("index", "middle", "ring", "little"):
        j0, j1, j2 = jpos(side + "_hand_%s_0" % f), jpos(side + "_hand_%s_1" % f), jpos(side + "_hand_%s_2" % f)
        if j0 is None or j1 is None or j2 is None: continue
        curl += float(np.dot(unit(j2 - j1) - unit(j1 - j0), lat))
    say("%s: finger-curl along lat = %+.4f (negative = palm is on -lat, so dorsal is +lat)" % (side, curl))
    s = -1.0 if thumb_off > 0 else 1.0          # thumb_2 on the palmar side -> dorsal is the other way along lat
    old[hidx] = np.clip(-(vno[hidx] @ palm), 0, 1)
    new[hidx] = np.clip(s * (vno[hidx] @ lat), 0, 1)
    npos = (vno[meta] @ lat > 0.5).sum(); nneg = (vno[meta] @ lat < -0.5).sum(); nold = (old[meta] > 0.5).sum()
    say("%s: wrist %s  axis %s  palm(thumb dir) %s  lat(plane normal) %s" % (side, np.round(w, 3), np.round(axis, 3), np.round(palm, 3), np.round(lat, 3)))
    say("%s: %d hand verts, %d metacarpal; thumb_2 offset along lat = %+.4f m -> dorsal sign %+d; metacarpal verts facing +lat %d, -lat %d; OLD pick >0.5: %d" % (
        side, len(hidx), len(meta), thumb_off, int(s), npos, nneg, nold))
    views[side] = (c, lat, axis, hidx)

# vertex colours
for nm, arr in (("Dv_old", old), ("Dv_new", new)):
    ca = me.color_attributes.new(nm, "FLOAT_COLOR", "POINT")
    for i in range(len(me.vertices)):
        ca.data[i].color = (arr[i], arr[i] * 0.2, 1.0 - arr[i], 1.0)      # blue = 0, red = 1

# material with the game albedo for the TEXTURE render
alb_png = os.path.join(WORK, "pl3000_body_albm.png")
mat = bpy.data.materials.new("preview_skin"); mat.use_nodes = True
tex = mat.node_tree.nodes.new("ShaderNodeTexImage"); tex.image = bpy.data.images.load(alb_png)
tex.image.alpha_mode = "NONE"       # ALBM alpha is metallic (~0.04): Workbench would draw the hand transparent
bsdf = mat.node_tree.nodes.get("Principled BSDF"); mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
mat.node_tree.nodes.active = tex
me.materials.clear(); me.materials.append(mat)

# camera + workbench
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.render.resolution_x = scene.render.resolution_y = 900; scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
sh = scene.display.shading; sh.light = "STUDIO"; sh.show_specular_highlight = False
cam_data = bpy.data.cameras.new("cam"); cam_data.type = "ORTHO"
cam = bpy.data.objects.new("cam", cam_data); scene.collection.objects.link(cam); scene.camera = cam

def render(side, tag, direction):
    c, lat, axis, hidx = views[side]
    d = Vector(tuple(map(float, lat * direction)))            # camera sits on +d, looks along -d
    ext = vco[hidx]; radius = float(np.linalg.norm(ext - c, axis=1).max())
    cam.location = Vector(tuple(map(float, c))) + d * (radius + 0.05)
    up = Vector(tuple(map(float, axis)))
    # roll so the fingers point up in the frame
    zaxis = d; yaxis = (up - zaxis * up.dot(zaxis)).normalized(); xaxis = yaxis.cross(zaxis)   # camera local -Z looks along -d, at the hand
    cam.matrix_world = Matrix(((xaxis.x, yaxis.x, zaxis.x, cam.location.x), (xaxis.y, yaxis.y, zaxis.y, cam.location.y),
                               (xaxis.z, yaxis.z, zaxis.z, cam.location.z), (0, 0, 0, 1)))
    cam_data.ortho_scale = radius * 2.2; cam_data.clip_start = 0.01; cam_data.clip_end = radius * 2 + 0.05 + 0.02
    scene.render.filepath = os.path.join(OUT, "%s_%s_%s.png" % (side, tag, "A" if direction > 0 else "B"))
    bpy.ops.render.render(write_still=True)

for side in ("l", "r"):
    for direction in (1.0, -1.0):
        sh.color_type = "TEXTURE"; render(side, "albedo", direction)
        sh.color_type = "VERTEX"
        for nm in ("Dv_old", "Dv_new"):
            me.color_attributes.active_color = me.color_attributes[nm]
            render(side, nm.lower(), direction)

# UV-space strip: which islands each pick lands on
W = 1024
img = bpy.data.images.load(alb_png); px = np.empty(W * W * 4, np.float32); img.pixels.foreach_get(px); alb = np.flipud(px.reshape(W, W, 4)).copy()
def raster_field(vals):
    fld = np.zeros((W, W), np.float32)
    for poly in me.polygons:
        vi = list(poly.vertices)
        if not any(topname[i].startswith(("l_hand_", "r_hand_", "l_arm_wrist", "r_arm_wrist")) for i in vi): continue
        uvs = [tuple(uv.data[li].uv) for li in poly.loop_indices]
        for k in range(1, len(uvs) - 1):
            pts = [(u * W, (1.0 - v) * W) for u, v in (uvs[0], uvs[k], uvs[k + 1])]; idx = (vi[0], vi[k], vi[k + 1])
            xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
            x0, x1 = max(int(min(xs)) - 1, 0), min(int(max(xs)) + 2, W); y0, y1 = max(int(min(ys)) - 1, 0), min(int(max(ys)) + 2, W)
            if x1 <= x0 or y1 <= y0: continue
            gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
            (ax, ay), (bx, by), (cx, cy) = pts
            det = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay)
            if abs(det) < 1e-9: continue
            l1 = ((bx - gx) * (cy - gy) - (cx - gx) * (by - gy)) / det; l2 = ((cx - gx) * (ay - gy) - (ax - gx) * (cy - gy)) / det; l3 = 1 - l1 - l2
            inside = (l1 >= -0.003) & (l2 >= -0.003) & (l3 >= -0.003)
            val = l1 * vals[idx[0]] + l2 * vals[idx[1]] + l3 * vals[idx[2]]
            sub = fld[y0:y1, x0:x1]; sub[inside] = np.maximum(sub[inside], val[inside])
    return fld
def save_png(arr, path):
    import zlib, struct
    arr8 = (np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8)
    raw = b"".join(bytes([0]) + arr8[i].tobytes() for i in range(arr8.shape[0]))
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    hdr = struct.pack(">IIBBBBB", arr8.shape[1], arr8.shape[0], 8, 6, 0, 0, 0)
    open(path, "wb").write(bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]) + chunk(b"IHDR", hdr) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
strip = slice(int(W * 0.84), W)
for nm, arr in (("old", old), ("new", new)):
    f = raster_field(arr)[strip]
    o = alb[strip].copy(); o[..., 1] = o[..., 1] * (1 - f) + f; o[..., 0] *= (1 - 0.6 * f); o[..., 2] *= (1 - 0.6 * f); o[..., 3] = 1
    save_png(np.repeat(np.repeat(o, 2, axis=0), 2, axis=1), os.path.join(OUT, "uv_strip_%s.png" % nm))
    say("%s pick: %.1f%% of hand-strip texels above 0.5" % (nm, 100 * (f > 0.5).mean()))
open(os.path.join(OUT, "dorsal_preview.txt"), "w", encoding="utf-8").write("\n".join(report) + "\n")
print("DONE")
