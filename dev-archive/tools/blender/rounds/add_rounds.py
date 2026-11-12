# blender -b --python add_round.py -- <spec.json>
# Puts the WP0400's two modelled rounds (our split model, part 41, material wp0400_Glock_Mat) into another handgun's magazine
# as a part of their own, lowers that magazine's follower under them, writes the gun's mesh, and adds the WP0400's
# material to the gun's material file. Game axes: RE (x, up, forward) = Blender (x, -y... ) -> world z is up, -y forward.
import bpy, bmesh, sys, os, re, json
from mathutils import Vector
spec = json.load(open(sys.argv[sys.argv.index("--") + 1]))
bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.preferences.addon_enable(module="RE-Mesh-Editor")
def imp(path, clear):
    d, f = os.path.split(path)
    r = bpy.ops.re_mesh.importfile(files=[{"name": f}], directory=d, loadMaterials=False, importAllLODs=True, clearScene=clear)
    assert r == {"FINISHED"}, r
imp(spec["gun_mesh"], True)
gun_coll = next(c for c in bpy.data.collections if c.get("~TYPE") == "RE_MESH_COLLECTION")
gun_objs = [o for o in gun_coll.all_objects if o.type == "MESH"]
imp(spec["rounds_mesh"], False)
src = next(o for o in bpy.data.objects if o.type == "MESH" and o.name.startswith("LOD_0_Group_41_"))
src_world = [src.matrix_world @ v.co for v in src.data.vertices]
lo = Vector([min(p[i] for p in src_world) for i in range(3)]); hi = Vector([max(p[i] for p in src_world) for i in range(3)])
# where the stack goes: its top at spec top (game up), its middle along the gun at spec fwd (game forward = world -y)
sc_ = spec.get("scale", 1.0)
mid = (lo + hi) / 2
def place(p):
    q = mid + (p - mid) * sc_
    return Vector((q.x - mid.x + spec.get("x", 0.0), q.y - mid.y + (-spec["fwd"]), q.z - (mid.z + (hi.z - mid.z) * sc_) + spec["top"]))
def mainj(ob, bm, v, dl, gi):
    w = v[dl] if dl else None
    return gi[max(w.items(), key=lambda kv: kv[1])[0]] if w else None
for t in spec["parts"]:
    base = next(o for o in gun_objs if re.match(rf"LOD_0_Group_{t['group']}_Sub_{t.get('sub', 0)}__", o.name))
    new = base.copy(); new.data = src.data.copy()
    for c in base.users_collection: c.objects.link(new)
    inv = new.matrix_world.inverted()
    for v, p in zip(new.data.vertices, src_world): v.co = inv @ place(p)
    allv = list(range(len(new.data.vertices)))
    for g in new.vertex_groups: g.remove(allv)
    new.vertex_groups["_04"].add(allv, 1.0, "REPLACE")
    new.name = f"LOD_0_Group_{t['new']}_Sub_0__wp0400_Glock_Mat"
    print("ADDED", new.name, "in", [c.name for c in new.users_collection])
    # the follower under the rounds: its islands moved down
    f = t.get("follower")
    if f:
        gi = {g.index: g.name for g in base.vertex_groups}
        bm = bmesh.new(); bm.from_mesh(base.data); bm.verts.ensure_lookup_table(); dl = bm.verts.layers.deform.active
        seen, moved = set(), 0
        for v in bm.verts:
            if v.index in seen: continue
            isl, st = [], [v]; seen.add(v.index)
            while st:
                x = st.pop(); isl.append(x)
                for e in x.link_edges:
                    y = e.other_vert(x)
                    if y.index not in seen: seen.add(y.index); st.append(y)
            if not all(mainj(base, bm, x, dl, gi) == "_04" for x in isl): continue
            ws = [base.matrix_world @ x.co for x in isl]
            if min(p.z for p in ws) >= f["ymin"] and max(p.x for p in ws) - min(p.x for p in ws) <= f["maxw"] and len(isl) >= f.get("minv", 0):
                binv = base.matrix_world.inverted()
                for x, p in zip(isl, ws): x.co = binv @ (p - Vector((0, 0, f["drop"])))
                moved += len(isl)
        bm.to_mesh(base.data); bm.free()
        print("FOLLOWER", base.name, "moved down", f["drop"], "verts", moved)
for c in [c for c in bpy.data.collections if c.get("~TYPE") == "RE_MESH_COLLECTION" and c != gun_coll]:
    for o in list(c.all_objects): bpy.data.objects.remove(o)
os.makedirs(os.path.dirname(spec["out_mesh"]), exist_ok=True)
print("MESH", bpy.ops.re_mesh.exportfile(filepath=spec["out_mesh"], targetCollection=gun_coll.name, exportAllLODs=True))
# the material file: the WP0400's material added
bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.preferences.addon_enable(module="RE-Mesh-Editor")
for p in (spec["gun_mdf"], spec["rounds_mdf"]):
    d, f = os.path.split(p); assert bpy.ops.re_mdf.importfile(files=[{"name": f}], directory=d) == {"FINISHED"}
cols = [c for c in bpy.data.collections if c.get("~TYPE") == "RE_MDF_COLLECTION"]
gmdf = next(c for c in cols if os.path.basename(spec["gun_mdf"]).split(".mdf2")[0] in c.name)
rmdf = next(c for c in cols if c != gmdf)
srcmat = next(o for o in rmdf.objects if o.re_mdf_material.materialName == "wp0400_Glock_Mat")
n = len(gmdf.objects)
cp = srcmat.copy(); gmdf.objects.link(cp); cp.name = f"Material {n:02d} (wp0400_Glock_Mat)"
print("MDF materials", [o.re_mdf_material.materialName for o in gmdf.objects])
for o in list(rmdf.objects): bpy.data.objects.remove(o)
for out in spec["out_mdf"]:
    os.makedirs(os.path.dirname(out), exist_ok=True)
    print("MDF", out, bpy.ops.re_mdf.exportfile(filepath=out, targetCollection=gmdf.name))
