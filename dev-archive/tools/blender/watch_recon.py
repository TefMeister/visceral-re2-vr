"""What exactly is on Claire's left wrist, and can it be moved up the arm?

  blender -b --python watch_recon.py -- <plXXXX folder> [--character pl1000]

Tefa (2026-09-06) asked whether the watch she already wears can be shifted along the forearm so our two new left-arm
bracelets do not crowd it. That depends on facts this script establishes: which submesh the watch belongs to, whether
it is a separate connected island inside that submesh (movable on its own) or welded into the sleeve (not), which
bones drive it, and how much clear arm there is either side of it.
Read-only. Prints measurements; writes nothing.
"""
import bpy, sys, os, ctypes, argparse
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
ap = argparse.ArgumentParser()
ap.add_argument("src"); ap.add_argument("--character", default="pl1000")
ap.add_argument("--side", default="l")
a = ap.parse_args(argv)
ctypes.windll.ole32.CoInitializeEx(None, 0)

mesh_path = os.path.join(a.src, a.character + ".mesh.2109108288")
bpy.ops.re_mesh.importfile(filepath=mesh_path, directory=a.src, files=[{"name": os.path.basename(mesh_path)}],
                           clearScene=True, loadMaterials=False, loadMDFData=False)
arm_obj = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
def jpos(n):
    b = arm_obj.data.bones.get(n)
    return np.array(arm_obj.matrix_world @ b.head_local, np.float32) if b else None
def unit(v): return v / (np.linalg.norm(v) + 1e-9)

w = jpos(a.side + "_arm_wrist"); el = jpos(a.side + "_arm_radius")
up = unit(el - w)

for o in bpy.data.objects:
    if o.type != "MESH" or not o.data.materials: continue
    me = o.data; M = o.matrix_world
    vco = np.array([M @ v.co for v in me.vertices], np.float32)
    d = (vco - w) @ up
    near = (np.linalg.norm(vco - w, axis=1) < 0.10) & (d > -0.03) & (d < 0.10)
    if near.sum() < 30: continue
    mat = me.materials[0].name if me.materials[0] else "?"
    print("\n==== %s (%s): %d verts within 10 cm of the %s wrist ====" % (o.name, mat, near.sum(), a.side))

    # connected components among those faces -- a separate island can be translated on its own in a mesh edit
    idx = set(int(i) for i in np.nonzero(near)[0])
    adj = {}
    for p in me.polygons:
        vs = [v for v in p.vertices if v in idx]
        if len(vs) < 2: continue
        for i in range(len(vs)):
            adj.setdefault(vs[i], set()).update(vs)
    seen = set(); comps = []
    for start in adj:
        if start in seen: continue
        stack = [start]; comp = set()
        while stack:
            v = stack.pop()
            if v in comp: continue
            comp.add(v); seen.add(v)
            stack.extend(adj.get(v, ()))
        comps.append(comp)
    comps.sort(key=len, reverse=True)
    names = [g.name for g in o.vertex_groups]
    for ci, comp in enumerate(comps[:6]):
        arr = np.array(sorted(comp))
        dd = (vco[arr] - w) @ up
        rad = vco[arr] - w - np.outer(dd, up)
        rn = np.linalg.norm(rad, axis=1)
        bones = {}
        for i in arr:
            for g in me.vertices[i].groups:
                if g.weight > 0.2: bones[names[g.group]] = bones.get(names[g.group], 0) + 1
        topb = sorted(bones.items(), key=lambda kv: -kv[1])[:4]
        # is this island welded to anything outside the 10 cm window?
        outside = 0
        for p in me.polygons:
            vs = list(p.vertices)
            if any(v in comp for v in vs) and any(v not in idx for v in vs): outside += 1
        print("   island %d: %5d verts, %5.1f .. %5.1f cm along the arm, radius %.2f-%.2f cm, %s, faces touching the rest: %d"
              % (ci, len(arr), dd.min() * 100, dd.max() * 100, rn.min() * 100, rn.max() * 100,
                 ", ".join("%s x%d" % (b, n) for b, n in topb), outside))
print("\nDONE")
