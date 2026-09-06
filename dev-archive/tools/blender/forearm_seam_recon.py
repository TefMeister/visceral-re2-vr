"""Where exactly does the forearm's texture treatment stop, and what does Claire already wear on her wrists?

  blender -b --python forearm_seam_recon.py -- <plXXXX folder> [--character pl1000]

Answers three things, for the bracelet idea (Tefa, 2026-09-06): (1) the 3D position, along each forearm, of the
boundary between the SKIN submesh (which our 4K repaint covers) and the JACKET submesh (which it does not) -- that
boundary is the straight band across the arm in Tefa's screenshots; (2) the arm's radius there, so a band can be
built to fit; (3) every submesh that already sits on a wrist, with its material, as a reference for what a
bracelet on this character is allowed to look like.
Read-only. Prints measurements; writes nothing.
"""
import bpy, sys, os, ctypes, argparse
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
ap = argparse.ArgumentParser()
ap.add_argument("src"); ap.add_argument("--character", default="pl1000")
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

ARM_BONES = ("arm_radius", "arm_wrist", "arm_elbow", "arm_ulna", "arm_upper")
print("\n==== bones on the arm ====")
for b in sorted(arm_obj.data.bones.keys()):
    if any(k in b for k in ("arm_", "hand_")) and not b.startswith(("l_hand_", "r_hand_")):
        p = jpos(b); print("   %-22s at (%.3f, %.3f, %.3f)" % (b, p[0], p[1], p[2]))

print("\n==== which submesh owns which part of the forearm ====")
per_obj = {}
for o in bpy.data.objects:
    if o.type != "MESH" or not o.data.materials: continue
    me = o.data; names = [g.name for g in o.vertex_groups]
    if not names: continue
    M = o.matrix_world
    vco = np.array([M @ v.co for v in me.vertices], np.float32)
    top = []
    for v in me.vertices:
        best, bw = "", 0.0
        for g in v.groups:
            if g.weight > bw: best, bw = names[g.group], g.weight
        top.append(best)
    top = np.array(top)
    per_obj[o.name] = (o, vco, top, o.data.materials[0].name if o.data.materials[0] else "?")

for side in ("l", "r"):
    w = jpos(side + "_arm_wrist"); el = jpos(side + "_arm_radius")
    if w is None or el is None: continue
    # the forearm axis runs wrist -> radius bone head (which sits up the arm)
    up = unit(el - w)
    print("\n--- %s forearm: wrist joint (%.3f, %.3f, %.3f), radius joint %.1f cm up that axis" % (
        side, w[0], w[1], w[2], float(np.linalg.norm(el - w)) * 100))
    rows = []
    for oname, (o, vco, top, mat) in per_obj.items():
        sel = np.array([t.startswith(side + "_arm_radius") or t == side + "_arm_wrist" for t in top])   # FOREARM only: the first cut included humerus + clavicle and read 46 cm "along the arm"
        if sel.sum() < 8: continue
        d = (vco[sel] - w) @ up                      # cm along the arm from the wrist, +ve = toward the elbow
        rad = vco[sel] - w - np.outer(d, up)
        rn = np.linalg.norm(rad, axis=1)
        rows.append((float(np.percentile(d, 2)), float(np.percentile(d, 98)), int(sel.sum()), mat, oname, rn))
    rows.sort()
    for lo, hi, n, mat, oname, rn in rows:
        print("   %-42s %-26s %5d verts, along the arm %6.1f .. %6.1f cm, radius %.1f-%.1f cm" % (
            oname[:42], mat, n, lo * 100, hi * 100, float(np.percentile(rn, 10)) * 100, float(np.percentile(rn, 90)) * 100))
    print("   per-centimetre profile along the forearm (verts per submesh; the seam is where SKIN stops and JACKET keeps going):")
    print("       cm:  " + " ".join("%4d" % c for c in range(0, 23)))
    for lo, hi, n, mat, oname, rn in rows:
        o2, vco2, top2, _ = per_obj[oname]
        sel2 = np.array([t.startswith(side + "_arm_radius") or t == side + "_arm_wrist" for t in top2])
        d2 = (vco2[sel2] - w) @ up
        hist = [int(((d2 >= c / 100.0) & (d2 < (c + 1) / 100.0)).sum()) for c in range(0, 23)]
        print("   %-18s " % mat.replace("pl1000_", "")[:18] + " ".join("%4d" % h for h in hist))
    # THE SEAM: where the skin submesh stops
    skin = [r for r in rows if "Body_Mat" in r[3]]
    if skin:
        lo, hi, n, mat, oname, rn = skin[0]
        band = np.abs(((per_obj[oname][1][np.array([t.startswith(side + "_arm_radius") or t == side + "_arm_wrist" for t in per_obj[oname][2]])] - w) @ up) - hi) < 0.010
        allr = rn[band]
        print("   >>> SKIN SUBMESH ENDS %.1f cm up the arm from the wrist joint; arm radius there %.2f-%.2f cm"
              % (hi * 100, float(np.percentile(allr, 5)) * 100 if allr.size else -1, float(np.percentile(allr, 95)) * 100 if allr.size else -1))

print("\n==== anything already on a wrist (bracelet reference) ====")
for oname, (o, vco, top, mat) in per_obj.items():
    for side in ("l", "r"):
        w = jpos(side + "_arm_wrist")
        if w is None: continue
        near = np.linalg.norm(vco - w, axis=1) < 0.09
        if near.sum() >= 20 and "Body_Mat" not in mat:
            el = jpos(side + "_arm_radius"); up = unit(el - w)
            d = (vco[near] - w) @ up
            rad = vco[near] - w - np.outer(d, up); rn = np.linalg.norm(rad, axis=1)
            print("   %-42s %-26s %s wrist: %4d verts, %5.1f .. %5.1f cm along the arm, radius %.2f-%.2f cm" % (
                oname[:42], mat, side, near.sum(), float(d.min()) * 100, float(d.max()) * 100,
                float(np.percentile(rn, 10)) * 100, float(np.percentile(rn, 90)) * 100))
print("\nDONE")
