"""Preview Visceral's forearm bracelets on Claire's arms, without launching anything.

  blender -b --python render_bracelets.py -- <plXXXX folder> <bracelet .blend> <out_dir> [--side l|r] [--size 1500]

Imports the skin mesh, appends the two bracelet objects, gives the leather and metal stand-in shading (the real
material comes later), and renders each forearm from the back and from the side. Read-only on game data.
"""
import bpy, sys, os, ctypes, argparse, math
import numpy as np
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index("--") + 1:]
ap = argparse.ArgumentParser()
ap.add_argument("src"); ap.add_argument("blend"); ap.add_argument("out")
ap.add_argument("--size", type=int, default=1500)
ap.add_argument("--tex", default=None, help="folder with <base>_ALBM.png for the skin")
ap.add_argument("--tex-base", default="pl1000_Jacket")
ap.add_argument("--character", default="pl1000")
a = ap.parse_args(argv)
os.makedirs(a.out, exist_ok=True)
ctypes.windll.ole32.CoInitializeEx(None, 0)

mesh_path = os.path.join(a.src, a.character + ".mesh.2109108288")
bpy.ops.re_mesh.importfile(filepath=mesh_path, directory=a.src, files=[{"name": os.path.basename(mesh_path)}],
                           clearScene=True, loadMaterials=False, loadMDFData=False)
arm_obj = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
skin = [o for o in bpy.data.objects if o.type == "MESH" and o.data.materials and "Body_Mat" in o.data.materials[0].name][0]
jacket = None
for o in bpy.data.objects:
    if o.type != "MESH" or o is skin: continue
    if o.data.materials and o.data.materials[0] and "Jacket_Mat" in o.data.materials[0].name:
        jacket = o                       # holds Claire's WATCH at the left wrist and the rolled sleeve: keep it, or the
        continue                         # preview cannot show whether our bands crowd the watch (they could not, before)
    o.hide_render = True

with bpy.data.libraries.load(a.blend) as (src, dst):
    dst.objects = [n for n in src.objects if n.startswith("visceral_bracelet")]
brac = []
for o in dst.objects:
    if o is not None:
        bpy.context.scene.collection.objects.link(o); brac.append(o)
print("appended", [o.name for o in brac])

def mat(name, base, rough, metal):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = base
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    return m
leather = mat("leather", (0.055, 0.032, 0.022, 1), 0.62, 0.0)          # right arm: plain brown leather
metal = mat("metal", (0.62, 0.60, 0.56, 1), 0.28, 1.0)
dark_red = mat("leather_dark_red", (0.115, 0.016, 0.020, 1), 0.55, 0.0)      # left, lower band (Tefa)
red_purple = mat("leather_red_purple", (0.105, 0.020, 0.062, 1), 0.52, 0.0)  # left, upper band, nearest the jacket
jacket_m = mat("jacket", (0.045, 0.040, 0.042, 1), 0.70, 0.0)
skin_m = mat("skin", (0.52, 0.36, 0.32, 1), 0.55, 0.0)
if a.tex:
    nt = skin_m.node_tree; bsdf = nt.nodes["Principled BSDF"]
    img = nt.nodes.new("ShaderNodeTexImage"); img.image = bpy.data.images.load(os.path.join(a.tex, a.tex_base + "_ALBM.png")); img.image.alpha_mode = "NONE"
    nt.links.new(img.outputs["Color"], bsdf.inputs["Base Color"])
skin.data.materials.clear(); skin.data.materials.append(skin_m)
if jacket is not None:
    for i in range(max(len(jacket.data.materials), 1)):
        if i < len(jacket.data.materials): jacket.data.materials[i] = jacket_m
        else: jacket.data.materials.append(jacket_m)

# the builder tags every face leather (slot 0) or metal (slot 1), so the preview just swaps the two materials in.
for o in brac:
    # replace the two slots IN PLACE. materials.clear() resets every polygon's material_index to 0, which silently
    # threw the build-time leather/metal split away (2026-09-06: the first preview rendered 0 metal faces).
    while len(o.data.materials) < 3: o.data.materials.append(None)
    if o.name.endswith("_l"):
        o.data.materials[0] = dark_red; o.data.materials[1] = metal; o.data.materials[2] = red_purple
    else:
        o.data.materials[0] = leather; o.data.materials[1] = metal; o.data.materials[2] = leather
    nmet = sum(1 for p in o.data.polygons if p.material_index == 1)
    print("  %s: %d leather, %d metal faces (tagged at build time)" % (o.name, len(o.data.polygons) - nmet, nmet))

def unit(v): return v / (np.linalg.norm(v) + 1e-9)
def jpos(n):
    b = arm_obj.data.bones.get(n); return np.array(arm_obj.matrix_world @ b.head_local, np.float32) if b else None

scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = a.size
scene.render.image_settings.file_format = "PNG"
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("w"); scene.world = world; world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.20, 0.20, 0.23, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.75
cam_data = bpy.data.cameras.new("cam"); cam_data.type = "ORTHO"
cam = bpy.data.objects.new("cam", cam_data); scene.collection.objects.link(cam); scene.camera = cam

def sun(name, direction, energy, angle):
    ld = bpy.data.lights.new(name, "SUN"); ld.energy = energy; ld.angle = angle
    lo = bpy.data.objects.new(name, ld); scene.collection.objects.link(lo)
    lo.matrix_world = Matrix.Translation(cam.location) @ (-Vector(tuple(map(float, direction)))).to_track_quat("-Z", "Y").to_matrix().to_4x4()
    return lo

for side in ("l", "r"):
    w = jpos(side + "_arm_wrist"); el = jpos(side + "_arm_radius"); m0 = jpos(side + "_hand_middle_0")
    up = unit(el - w)
    krow = []
    for f in ("index", "little"):
        for k in range(4):
            pk = jpos(side + "_hand_%s_%d" % (f, k))
            if pk is not None and np.dot(pk - w, unit(m0 - w)) > 0.04: krow.append(pk); break
    lat = unit(np.cross(unit(m0 - w), unit(krow[1] - krow[0])))
    curl = 0.0
    for f in ("index", "middle", "ring", "little"):
        j0, j1, j2 = jpos(side + "_hand_%s_0" % f), jpos(side + "_hand_%s_1" % f), jpos(side + "_hand_%s_2" % f)
        curl += float(np.dot(unit(j2 - j1) - unit(j1 - j0), lat))
    back = unit(lat * (-1.0 if curl > 0 else 1.0))
    sidev = unit(np.cross(up, back))
    centre = w + up * 0.055
    for view, d in (("back", back), ("side", sidev)):
        for o in bpy.data.objects:
            if o.type == "LIGHT": bpy.data.objects.remove(o, do_unlink=True)
        radius = 0.095
        cam.location = Vector(tuple(map(float, centre + d * 0.4)))
        zax = Vector(tuple(map(float, d))); upv = Vector(tuple(map(float, up)))
        yax = (upv - zax * upv.dot(zax)).normalized(); xax = yax.cross(zax)
        cam.matrix_world = Matrix(((xax.x, yax.x, zax.x, cam.location.x), (xax.y, yax.y, zax.y, cam.location.y),
                                   (xax.z, yax.z, zax.z, cam.location.z), (0, 0, 0, 1)))
        cam_data.ortho_scale = radius * 2.4; cam_data.clip_start = 0.01; cam_data.clip_end = 1.0
        key = unit(np.array(d, np.float32) * 0.45 + np.array(tuple(xax), np.float32) * -0.75 + np.array(tuple(yax), np.float32) * 0.5)
        sun("key", key, 3.4, 0.06); sun("fill", -np.array(d, np.float32), 0.9, 0.7)
        scene.render.filepath = os.path.join(a.out, "bracelet_%s_%s.png" % (side, view))
        bpy.ops.render.render(write_still=True)
        print("rendered", scene.render.filepath)
print("DONE")
