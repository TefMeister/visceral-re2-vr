import bpy, sys, os, math
from mathutils import Vector
path = sys.argv[sys.argv.index("--") + 1]
bpy.ops.re_mesh.importfile(filepath=path, directory=os.path.dirname(path), files=[{"name": os.path.basename(path)}], clearScene=True, loadMaterials=False, loadMDFData=False)
o = [o for o in bpy.data.objects if o.type == "MESH" and "Face_Mat" in o.name][0]
me = o.data
pts = [(o.matrix_world @ v.co, v) for v in me.vertices]
print("NECK SLICES of", o.name, "(world: x=left/right, y=front(-)/back(+), z=up)")
z = 1.08
while z < 1.24:
    sl = [(p, v) for p, v in pts if z <= p.z < z + 0.02]
    if sl:
        c = sum((p for p, v in sl), Vector()) / len(sl)
        r = [math.hypot(p.x - c.x, p.y - c.y) for p, v in sl]
        xs = [p.x for p, v in sl]; ys = [p.y for p, v in sl]
        vg = {}
        for p, v in sl:
            for g in v.groups:
                if g.weight > 0.3:
                    nm = o.vertex_groups[g.group].name; vg[nm] = vg.get(nm, 0) + 1
        print("  z=%.2f n=%4d center=(%.3f, %.3f) r_mean=%.3f r_max=%.3f x[%.3f..%.3f] y[%.3f..%.3f] groups=%s" % (z, len(sl), c.x, c.y, sum(r)/len(r), max(r), min(xs), max(xs), min(ys), max(ys), sorted(vg.items(), key=lambda kv: -kv[1])[:3]))
    z += 0.02
# UV of the neck skin (for texturing a plug from the same albedo)
uv = me.uv_layers.active.data if me.uv_layers.active else None
if uv:
    lows = set(v.index for p, v in pts if p.z < 1.15)
    us = []; vs = []
    for poly in me.polygons:
        for li in poly.loop_indices:
            if me.loops[li].vertex_index in lows:
                us.append(uv[li].uv.x); vs.append(uv[li].uv.y)
    print("  neck skin UV region: u[%.3f..%.3f] v[%.3f..%.3f]" % (min(us), max(us), min(vs), max(vs)))
