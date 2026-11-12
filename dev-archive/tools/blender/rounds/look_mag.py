# blender -b --python look.py -- <mesh> <out_prefix> <re_y_threshold>
# Imports a gun (LOD0), colours the magazine joint's (_04) islands whose lowest point (game's up axis) is above the threshold
# red, the rest of the magazine blue, the gun grey; renders the magazine top from three sides.
import bpy, bmesh, sys, os, math
from mathutils import Vector
a = sys.argv[sys.argv.index("--") + 1:]
mesh, out, thr = a[0], a[1], float(a[2]); maxw = float(a[3]) if len(a) > 3 else 1.0; minv = int(a[4]) if len(a) > 4 else 0
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.preferences.addon_enable(module="RE-Mesh-Editor")
d, f = os.path.split(mesh)
bpy.ops.re_mesh.importfile(files=[{"name": f}], directory=d, loadMaterials=False, importAllLODs=False, importShadowMeshes=False, clearScene=True)
mats = {}
def mat(name, rgb):
    m = bpy.data.materials.new(name); m.diffuse_color = (*rgb, 1); return m
grey, blue, red = mat("g", (.5, .5, .5)), mat("b", (.2, .4, 1)), mat("r", (1, .1, .1))
redbox = None
for ob in [o for o in bpy.context.scene.objects if o.type == "MESH"]:
    gi = {g.index: g.name for g in ob.vertex_groups}
    me = ob.data
    me.materials.clear(); me.materials.append(grey); me.materials.append(blue); me.materials.append(red)
    bm = bmesh.new(); bm.from_mesh(me); bm.verts.ensure_lookup_table()
    dl = bm.verts.layers.deform.active
    def mainj(v):
        if dl is None: return None
        w = v[dl]
        return gi[max(w.items(), key=lambda kv: kv[1])[0]] if w else None
    seen = set(); n_red = 0; drop = []
    for v in bm.verts:
        if v.index in seen: continue
        isl = []; stack = [v]; seen.add(v.index)
        while stack:
            x = stack.pop(); isl.append(x)
            for e in x.link_edges:
                y = e.other_vert(x)
                if y.index not in seen: seen.add(y.index); stack.append(y)
        if not all(mainj(x) == "_04" for x in isl):
            drop.extend(isl); continue
        zs = [(ob.matrix_world @ x.co).z for x in isl]
        xs = [(ob.matrix_world @ x.co).x for x in isl]
        col = 2 if min(zs) >= thr and max(xs) - min(xs) <= maxw and len(isl) >= minv else 1
        idx = {x.index for x in isl}
        for fc in bm.faces:
            if all(vv.index in idx for vv in fc.verts): fc.material_index = col
        if col == 2:
            n_red += 1
            for x in isl:
                p = ob.matrix_world @ x.co
                redbox = [p.copy(), p.copy()] if redbox is None else [Vector(map(min, redbox[0], p)), Vector(map(max, redbox[1], p))]
    bmesh.ops.delete(bm, geom=list({id(x): x for x in drop}.values()), context='VERTS')
    bm.to_mesh(me); bm.free()
    print("OBJ", ob.name, "red islands", n_red)
print("REDBOX", redbox)
sc = bpy.context.scene
sc.render.engine = "BLENDER_WORKBENCH"; sc.display.shading.light = "STUDIO"; sc.display.shading.color_type = "MATERIAL"
sc.display.shading.show_cavity = True
sc.render.resolution_x = sc.render.resolution_y = 700
w = bpy.data.worlds.new("w"); sc.world = w
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam
c = (redbox[0] + redbox[1]) / 2 if redbox else Vector((0, 0, 0))
for name, dirv in (("side", (1, 0, 0)), ("top", (0.02, 0.3, 1)), ("q", (1, -1, 0.7))):
    dv = Vector(dirv).normalized()
    cam.location = c + dv * 0.16
    cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 50; cam.data.clip_start = 0.001
    sc.render.filepath = f"{out}_{name}.png"
    bpy.ops.render.render(write_still=True)
