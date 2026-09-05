import bpy, bmesh, sys, traceback
from mathutils import Vector
path = sys.argv[sys.argv.index("--") + 1]
print("IMPORT", path)
try:
    import os
    r = bpy.ops.re_mesh.importfile(filepath=path, directory=os.path.dirname(path), files=[{"name": os.path.basename(path)}], clearScene=True, loadMaterials=False, loadMDFData=False)
    print("import result", r)
except Exception:
    traceback.print_exc()
    sys.exit(1)

arm = [o for o in bpy.data.objects if o.type == "ARMATURE"]
print("ARMATURES", [a.name for a in arm])
for a in arm:
    names = [b.name for b in a.data.bones]
    print("  bones", len(names))
    print("  neck/head/spine bones:", [n for n in names if any(k in n.lower() for k in ("neck", "head", "spine", "clavicle"))])
    for n in names:
        if any(k in n.lower() for k in ("neck", "head")):
            b = a.data.bones[n]
            print("    %-24s head=%s tail=%s parent=%s" % (n, tuple(round(v, 3) for v in b.head_local), tuple(round(v, 3) for v in b.tail_local), b.parent.name if b.parent else None))

print("MESH OBJECTS")
for o in bpy.data.objects:
    if o.type != "MESH":
        continue
    me = o.data
    mats = [m.name for m in me.materials if m]
    bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
    zs = [v.z for v in bb]; ys = [v.y for v in bb]
    bm = bmesh.new(); bm.from_mesh(me)
    bedges = [e for e in bm.edges if e.is_boundary]
    # group boundary edges into loops
    loops = []
    seen = set()
    for e in bedges:
        if e.index in seen: continue
        stack = [e]; loop = []
        while stack:
            x = stack.pop()
            if x.index in seen: continue
            seen.add(x.index); loop.append(x)
            for v in x.verts:
                for le in v.link_edges:
                    if le.is_boundary and le.index not in seen:
                        stack.append(le)
        loops.append(loop)
    print("- %-32s verts=%6d faces=%6d vgroups=%3d mats=%s parent=%s" % (o.name, len(me.vertices), len(me.polygons), len(o.vertex_groups), mats, o.parent.name if o.parent else None))
    print("    bbox y[%.3f..%.3f] z[%.3f..%.3f]  boundary edges=%d loops=%d" % (min(ys), max(ys), min(zs), max(zs), len(bedges), len(loops)))
    for lp in sorted(loops, key=lambda l: -len(l))[:6]:
        vs = set(v for e in lp for v in e.verts)
        c = sum((o.matrix_world @ v.co for v in vs), Vector()) / len(vs)
        # dominant vertex group at the loop
        vg = {}
        for v in vs:
            for g in me.vertices[v.index].groups:
                if g.weight > 0.3:
                    nm = o.vertex_groups[g.group].name
                    vg[nm] = vg.get(nm, 0) + 1
        top = sorted(vg.items(), key=lambda kv: -kv[1])[:3]
        print("      loop edges=%4d centroid=(%.3f, %.3f, %.3f) groups=%s" % (len(lp), c.x, c.y, c.z, top))
    bm.free()

print("DATA meshes=%d objects=%d collections=%s scenes=%s" % (len(bpy.data.meshes), len(bpy.data.objects), [c.name for c in bpy.data.collections], [s.name for s in bpy.data.scenes]))
print("mesh datablocks:", [m.name for m in bpy.data.meshes][:20])
print("armature datablocks:", [a.name for a in bpy.data.armatures][:5])

print("LOOPS NEAREST THE COLLAR (centroid z > 0.9), per object")
for o in bpy.data.objects:
    if o.type != "MESH": continue
    me = o.data
    bm = bmesh.new(); bm.from_mesh(me)
    seen = set(); loops = []
    for e in bm.edges:
        if not e.is_boundary or e.index in seen: continue
        stack = [e]; loop = []
        while stack:
            x = stack.pop()
            if x.index in seen: continue
            seen.add(x.index); loop.append(x)
            for v in x.verts:
                for le in v.link_edges:
                    if le.is_boundary and le.index not in seen: stack.append(le)
        loops.append(loop)
    rows = []
    for lp in loops:
        vs = set(v for e in lp for v in e.verts)
        c = sum((o.matrix_world @ v.co for v in vs), Vector()) / len(vs)
        if c.z > 0.9 and len(lp) >= 12:
            xs = [ (o.matrix_world @ v.co).x for v in vs]; ys = [(o.matrix_world @ v.co).y for v in vs]
            vg = {}
            for v in vs:
                for g in me.vertices[v.index].groups:
                    if g.weight > 0.3:
                        nm = o.vertex_groups[g.group].name; vg[nm] = vg.get(nm, 0) + 1
            rows.append((c.z, len(lp), c, max(xs)-min(xs), max(ys)-min(ys), sorted(vg.items(), key=lambda kv: -kv[1])[:3]))
    for z, n, c, w, d, top in sorted(rows, key=lambda r: -r[0])[:8]:
        print("  %-34s edges=%4d centroid=(%.3f, %.3f, %.3f) width=%.3f depth=%.3f groups=%s" % (o.name[:34], n, c.x, c.y, c.z, w, d, top))
    bm.free()
