# blender -b --python split.py -- <mesh in> <mesh out> <re_up_min> <max_width> <min_verts> <group:newgroup,...>
# Moves the magazine joint's (_04) round-sized islands (lowest point above re_up_min, width <= max_width, >= min_verts
# vertices) of each named group into a new group of their own, in every detail level, and writes the mesh.
import bpy, bmesh, sys, os, re
a = sys.argv[sys.argv.index("--") + 1:]
src, dst, thr, maxw, minv = a[0], a[1], float(a[2]), float(a[3]), int(a[4])
remap = {int(k): int(v) for k, v in (p.split(":") for p in a[5].split(","))}
JOINT = a[6] if len(a) > 6 else "_04"
FWD = (float(a[7]), float(a[8])) if len(a) > 8 else (-9.0, 9.0)
bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.preferences.addon_enable(module="RE-Mesh-Editor")
d, f = os.path.split(src)
bpy.ops.re_mesh.importfile(files=[{"name": f}], directory=d, loadMaterials=False, importAllLODs=True, clearScene=True)
done = set()
for ob in [o for o in bpy.data.objects if o.type == "MESH"]:
    m = re.match(r"(LOD_\d+_)?Group_(\d+)_Sub_(\d+)__(.*)", ob.name)
    if not m or int(m.group(2)) not in remap or ob.name in done: continue
    done.add(ob.name)
    gi = {g.index: g.name for g in ob.vertex_groups}
    bm = bmesh.new(); bm.from_mesh(ob.data); bm.verts.ensure_lookup_table()
    dl = bm.verts.layers.deform.active
    def mainj(v):
        w = v[dl] if dl else None
        return gi[max(w.items(), key=lambda kv: kv[1])[0]] if w else None
    seen, keep = set(), set()
    for v in bm.verts:
        if v.index in seen: continue
        isl, stack = [], [v]; seen.add(v.index)
        while stack:
            x = stack.pop(); isl.append(x)
            for e in x.link_edges:
                y = e.other_vert(x)
                if y.index not in seen: seen.add(y.index); stack.append(y)
        if not all(mainj(x) == JOINT for x in isl): continue
        ws = [ob.matrix_world @ x.co for x in isl]
        if min(p.z for p in ws) >= thr and max(p.x for p in ws) - min(p.x for p in ws) <= maxw and len(isl) >= minv and FWD[0] <= -max(p.y for p in ws) and -min(p.y for p in ws) <= FWD[1]:
            keep.update(x.index for x in isl)
    bm.free()
    if not keep:
        print("SPLIT", ob.name, "no round islands: left as it is"); continue
    new = ob.copy(); new.data = ob.data.copy()
    newname = f"{m.group(1) or ''}Group_{remap[int(m.group(2))]}_Sub_{m.group(3)}__{m.group(4)}"
    for c in ob.users_collection: c.objects.link(new)
    for obj, drop_rounds in ((ob, True), (new, False)):
        bm = bmesh.new(); bm.from_mesh(obj.data); bm.verts.ensure_lookup_table()
        gone = [v for v in bm.verts if (v.index in keep) == drop_rounds]
        bmesh.ops.delete(bm, geom=gone, context="VERTS"); bm.to_mesh(obj.data); bm.free()
    new.name = newname
    print("SPLIT", ob.name, "->", new.name, "round verts", len(keep), "in", [c.name for c in new.users_collection])
target = next(c.name for c in bpy.data.collections if c.get("~TYPE") == "RE_MESH_COLLECTION")
os.makedirs(os.path.dirname(dst), exist_ok=True)
print(bpy.ops.re_mesh.exportfile(filepath=dst, targetCollection=target, exportAllLODs=True))
