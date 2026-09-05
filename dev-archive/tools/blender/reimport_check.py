import bpy, sys, os
from mathutils import Vector
path = sys.argv[sys.argv.index("--") + 1]
r = bpy.ops.re_mesh.importfile(filepath=path, directory=os.path.dirname(path), files=[{"name": os.path.basename(path)}], clearScene=True, loadMaterials=False, loadMDFData=False)
print("REIMPORT", r)
for o in bpy.data.objects:
    if o.type != "MESH": continue
    me = o.data
    bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
    print("  obj=%s verts=%d faces=%d mats=%s uv=%s" % (o.name, len(me.vertices), len(me.polygons), [m.name for m in me.materials if m], [l.name for l in me.uv_layers]))
    print("  bbox x[%.3f..%.3f] y[%.3f..%.3f] z[%.3f..%.3f]" % (min(v.x for v in bb), max(v.x for v in bb), min(v.y for v in bb), max(v.y for v in bb), min(v.z for v in bb), max(v.z for v in bb)))
print("  armatures:", [a.name for a in bpy.data.armatures])
