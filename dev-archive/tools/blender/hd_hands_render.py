"""HD hands: a close-up look at a texture set on Claire's hand mesh, without launching the game.
Headless Blender 5.2 + RE Mesh Editor:
  blender -b --python hd_hands_render.py -- <pl3000 folder> <render_out> <label>=<png folder> [<label>=<png folder> ...]
      [--side l|r] [--view dorsal|palm] [--size 1400] [--flip-green]

Each <png folder> must hold <tex-base>_ALBM.png + _NRMR.png (Claire: pl1000_Jacket_*) (hd_hands_paint.py output).
For every set: EEVEE render of the hand from the dorsal (default) or palm side, raking sun so the normal map reads,
written as <render_out>/<label>_<side>_<view>.png.  Read-only on the inputs; outputs are derivatives, keep out of git.
Dorsal side per hand is the finger-curl rule from hd_hands_dorsal_preview.py (verified on the nails 2026-09-06).
"""
import bpy, sys, os, ctypes, argparse
import numpy as np
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index("--") + 1:]
ap = argparse.ArgumentParser()
ap.add_argument("src"); ap.add_argument("out"); ap.add_argument("sets", nargs="+")
ap.add_argument("--side", default="l"); ap.add_argument("--view", default="dorsal"); ap.add_argument("--size", type=int, default=1400)
ap.add_argument("--flip-green", action="store_true", help="treat the NRMR as DirectX-style (Y down)")
ap.add_argument("--zoom", type=float, default=1.0, help=">1 = closer on the metacarpal region")
ap.add_argument("--focus", default="hand", help="hand (default) or fingers = frame the distal phalanges (nails)")
ap.add_argument("--character", default="pl1000"); ap.add_argument("--skin-mat", default="Body_Mat"); ap.add_argument("--tex-base", default="pl1000_Jacket")
a = ap.parse_args(argv)
os.makedirs(a.out, exist_ok=True)
ctypes.windll.ole32.CoInitializeEx(None, 0)

mesh_path = os.path.join(a.src, a.character + ".mesh.2109108288")
bpy.ops.re_mesh.importfile(filepath=mesh_path, directory=a.src, files=[{"name": os.path.basename(mesh_path)}],
                           clearScene=True, loadMaterials=False, loadMDFData=False)
skin = [o for o in bpy.data.objects if o.type == "MESH" and any(a.skin_mat in (m.name if m else "") for m in o.data.materials)][0]
arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
me = skin.data; names = [g.name for g in skin.vertex_groups]
for o in bpy.data.objects:
    if o.type == "MESH" and o is not skin: o.hide_render = True
M = skin.matrix_world
vco = np.array([M @ v.co for v in me.vertices], np.float32)
def top(v):
    best, bw = "", 0.0
    for g in v.groups:
        if g.weight > bw: best, bw = names[g.group], g.weight
    return best
topname = np.array([top(v) for v in me.vertices])
def jpos(n):
    b = arm.data.bones.get(n); return np.array(arm.matrix_world @ b.head_local, np.float32) if b else None
def unit(v): return v / (np.linalg.norm(v) + 1e-9)
side = a.side
w = jpos(side + "_arm_wrist"); m0 = jpos(side + "_hand_middle_0"); t1 = jpos(side + "_hand_thumb_1")
axis = unit(m0 - w); tv = t1 - w; palm = unit(tv - axis * np.dot(tv, axis))
krow = []                                                   # palm plane from the knuckle row (the thumb leaves the plane on Claire)
for f in ("index", "little"):
    for k in range(4):
        pk = jpos(side + "_hand_%s_%d" % (f, k))
        if pk is not None and np.dot(pk - w, axis) > 0.04: krow.append(pk); break
lat = unit(np.cross(axis, unit(krow[1] - krow[0]))) if len(krow) == 2 else unit(np.cross(axis, palm))
curl = 0.0
for f in ("index", "middle", "ring", "little"):
    j0, j1, j2 = jpos(side + "_hand_%s_0" % f), jpos(side + "_hand_%s_1" % f), jpos(side + "_hand_%s_2" % f)
    curl += float(np.dot(unit(j2 - j1) - unit(j1 - j0), lat))
dorsal = lat * (-1.0 if curl > 0 else 1.0)
hidx = np.where(np.char.startswith(topname, side + "_hand_") | (topname == side + "_arm_wrist"))[0]
meta = np.array([i for i in hidx if topname[i].endswith("_0") or topname[i] == side + "_arm_wrist"])
c = vco[hidx].mean(0) if a.zoom <= 1.0 else vco[meta].mean(0)
radius = float(np.linalg.norm(vco[hidx] - c, axis=1).max()) / a.zoom
if a.focus == "forearm":                                     # the bare forearm, wrist -> sleeve (the straight lines, 2026-09-06 evening)
    fa = np.array([i for i in range(len(vco)) if topname[i].startswith(side + "_arm_radius") or topname[i] == side + "_arm_wrist"])
    c = vco[fa].mean(0); radius = float(np.linalg.norm(vco[fa] - c, axis=1).max()) * 1.02
    hidx = fa
if a.focus in ("fingers", "thumb"):                          # the distal phalanges only (nails, 2026-09-06 evening)
    want = (side + "_hand_thumb_2",) if a.focus == "thumb" else (side + "_hand_index_2", side + "_hand_middle_2", side + "_hand_ring_3", side + "_hand_little_3")
    tips = np.array([i for i in hidx if topname[i] in want])
    c = vco[tips].mean(0); radius = float(np.linalg.norm(vco[tips] - c, axis=1).max()) * 1.05
    if a.focus == "thumb":                                   # look at the thumb's own back: radially (away from the little finger) + dorsally, bent perpendicular to its distal bone
        t2 = jpos(side + "_hand_thumb_2"); tax = unit(t2 - t1)
        across = unit(krow[1] - krow[0]) if len(krow) == 2 else unit(np.cross(dorsal, axis))
        tb = unit(dorsal * 0.6 - across * 0.8); tb = unit(tb - tax * np.dot(tb, tax))
        dorsal = tb; axis = tax
d = Vector(tuple(map(float, dorsal if a.view == "dorsal" else -dorsal)))

scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE_NEXT" if hasattr(bpy.types, "SceneEEVEE") and "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = a.size; scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("w"); scene.world = world; world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.18, 0.18, 0.2, 1); world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
cam_data = bpy.data.cameras.new("cam"); cam_data.type = "ORTHO"
cam = bpy.data.objects.new("cam", cam_data); scene.collection.objects.link(cam); scene.camera = cam
cam.location = Vector(tuple(map(float, c))) + d * (radius + 0.05)
up = Vector(tuple(map(float, axis)))
zaxis = d; yaxis = (up - zaxis * up.dot(zaxis)).normalized(); xaxis = yaxis.cross(zaxis)
cam.matrix_world = Matrix(((xaxis.x, yaxis.x, zaxis.x, cam.location.x), (xaxis.y, yaxis.y, zaxis.y, cam.location.y),
                           (xaxis.z, yaxis.z, zaxis.z, cam.location.z), (0, 0, 0, 1)))
cam_data.ortho_scale = radius * 2.2; cam_data.clip_start = 0.01; cam_data.clip_end = radius * 2 + 0.07
# raking key light from the upper-left of the frame, soft fill from the camera
def sun(name, direction, energy, angle):
    ld = bpy.data.lights.new(name, "SUN"); ld.energy = energy; ld.angle = angle
    lo = bpy.data.objects.new(name, ld); scene.collection.objects.link(lo)
    lo.matrix_world = Matrix.Translation(cam.location) @ (-direction).to_track_quat("-Z", "Y").to_matrix().to_4x4()
    return lo
ks = 1.0 if side == "l" else -1.0                                        # keep the key on the little-finger side for both hands
key_dir = (-(d * 0.35) - xaxis * 0.8 * ks + yaxis * 0.45).normalized()   # light travels mostly across the surface
sun("key", key_dir, 3.0, 0.05); sun("fill", -d, 0.6, 0.6)

def material(label, folder):
    mat = bpy.data.materials.new("skin_" + label); mat.use_nodes = True; nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]; bsdf.inputs["Roughness"].default_value = 0.55
    if "Subsurface Weight" in bsdf.inputs: bsdf.inputs["Subsurface Weight"].default_value = 0.15
    alb = nt.nodes.new("ShaderNodeTexImage"); alb.image = bpy.data.images.load(os.path.join(folder, a.tex_base + "_ALBM.png")); alb.image.alpha_mode = "NONE"
    nrm = nt.nodes.new("ShaderNodeTexImage"); nrm.image = bpy.data.images.load(os.path.join(folder, a.tex_base + "_NRMR.png")); nrm.image.alpha_mode = "NONE"
    nrm.image.colorspace_settings.name = "Non-Color"
    nmap = nt.nodes.new("ShaderNodeNormalMap"); nmap.inputs["Strength"].default_value = 1.0
    if a.flip_green:
        sep = nt.nodes.new("ShaderNodeSeparateColor"); inv = nt.nodes.new("ShaderNodeMath"); inv.operation = "SUBTRACT"; inv.inputs[0].default_value = 1.0
        comb = nt.nodes.new("ShaderNodeCombineColor")
        nt.links.new(nrm.outputs["Color"], sep.inputs[0]); nt.links.new(sep.outputs[1], inv.inputs[1])
        nt.links.new(sep.outputs[0], comb.inputs[0]); nt.links.new(inv.outputs[0], comb.inputs[1]); nt.links.new(sep.outputs[2], comb.inputs[2])
        nt.links.new(comb.outputs[0], nmap.inputs["Color"])
    else:
        nt.links.new(nrm.outputs["Color"], nmap.inputs["Color"])
    nt.links.new(alb.outputs["Color"], bsdf.inputs["Base Color"]); nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    return mat

for spec in a.sets:
    label, folder = spec.split("=", 1)
    me.materials.clear(); me.materials.append(material(label, folder))
    scene.render.filepath = os.path.join(a.out, "%s_%s_%s.png" % (label, side, a.view))
    bpy.ops.render.render(write_still=True)
    print("rendered", scene.render.filepath)
print("DONE")
