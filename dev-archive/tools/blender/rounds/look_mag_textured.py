# blender -b --python looktex.py -- <mesh> <albm tex> <out prefix>: only the magazine (_04), with the gun's colour texture
import bpy, bmesh, sys, os, importlib
from mathutils import Vector
a = sys.argv[sys.argv.index("--") + 1:]
mesh, tex, out = a
bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.preferences.addon_enable(module="RE-Mesh-Editor")
tu = importlib.import_module("RE-Mesh-Editor.modules.tex.re_tex_utils")
dds = out + "_albm.dds"; tu.convertTexFileToDDS(tex, dds)
d, f = os.path.split(mesh)
bpy.ops.re_mesh.importfile(files=[{"name": f}], directory=d, loadMaterials=False, importAllLODs=False, clearScene=True)
img = bpy.data.images.load(dds)
m = bpy.data.materials.new("t"); m.use_nodes = True
nt = m.node_tree; bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
tn = nt.nodes.new("ShaderNodeTexImage"); tn.image = img; nt.links.new(tn.outputs[0], bsdf.inputs[0]); nt.nodes.active = tn
top = None
for ob in [o for o in bpy.context.scene.objects if o.type == "MESH"]:
    gi = {g.index: g.name for g in ob.vertex_groups}
    ob.data.materials.clear(); ob.data.materials.append(m)
    bm = bmesh.new(); bm.from_mesh(ob.data); dl = bm.verts.layers.deform.active
    drop = [v for v in bm.verts if not (dl and v[dl] and gi[max(v[dl].items(), key=lambda kv: kv[1])[0]] == "_04")]
    bmesh.ops.delete(bm, geom=drop, context="VERTS")
    for v in bm.verts:
        p = ob.matrix_world @ v.co
        if top is None or p.z > top.z: top = p.copy()
    bm.to_mesh(ob.data); bm.free()
sc = bpy.context.scene
sc.render.engine = "BLENDER_WORKBENCH"; sc.display.shading.light = "FLAT"; sc.display.shading.color_type = "TEXTURE"
sc.render.resolution_x = sc.render.resolution_y = 700
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam
c = Vector((0, top.y, top.z - 0.008))
for name, dirv in (("ttop", (0.02, 0.3, 1)), ("tq", (1, -1, 0.7))):
    cam.location = c + Vector(dirv).normalized() * 0.09
    cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler(); cam.data.clip_start = 0.001
    sc.render.filepath = f"{out}_{name}.png"; bpy.ops.render.render(write_still=True)
