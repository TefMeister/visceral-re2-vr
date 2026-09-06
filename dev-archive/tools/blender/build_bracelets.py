"""Build Visceral's forearm bracelets -- our own geometry, no game data -- to sit over the straight seam lines on
Claire's forearms (Tefa's idea, 2026-09-06: "cover over these exact straight lines so they can stay under the
bracelets ... something that looks like metal and leather combined, different on both hands").

  blender -b --python build_bracelets.py -- <plXXXX folder> <out_dir> [--character pl1000]
        [--left-at 6.0 --left-width 3.0] [--right-at 6.0 --right-width 4.0] [--preview]

Two different designs, both wrapped onto the arm's REAL cross-section (sampled from the skin mesh, not a cylinder,
so they never pinch or float):
  RIGHT arm -- "strap and plate": one wide leather cuff, a brushed metal plate across the back of the arm, two
      rivets either side of it, and a raised leather edge-welt top and bottom.
  LEFT arm -- "cord and rings": two narrow leather cords with a gap between them, three small metal rings threaded
      on the upper cord, sitting above the red beaded band Claire already wears at her wrist (0.1-2.2 cm).

Measured on Claire `[measured 2026-09-06]`: bare forearm skin runs from the wrist joint to 12.6 cm (right) /
13.7 cm (left); the rolled jacket sleeve starts at 10-11 cm; the existing left wrist band occupies 0.1-2.2 cm at
radius 1.84-2.99 cm; the arm's radius over the free span is about 2.3-3.6 cm. So a band belongs between roughly
3 and 10 cm from the wrist joint, which is where the defaults sit.

Exports one .mesh per side, in the attach joint's LOCAL frame, the same runtime path the neck plug already uses
(Plugin.cpp: createComponent(via.render.Mesh) + MeshResourceHolder on a loose file under natives/stm/visceral/).
Geometry is ours; nothing from the game is copied. --preview also writes an OBJ so the shape can be eyeballed.
"""
import bpy, bmesh, sys, os, ctypes, argparse, math
import numpy as np
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index("--") + 1:]
ap = argparse.ArgumentParser()
ap.add_argument("src"); ap.add_argument("out")
ap.add_argument("--character", default="pl1000")
ap.add_argument("--left-at", type=float, default=6.0, help="centre of the left band, cm from the wrist joint")
ap.add_argument("--left-width", type=float, default=3.4, help="total span of the left design, cm")
ap.add_argument("--right-at", type=float, default=6.0, help="centre of the right band, cm from the wrist joint")
ap.add_argument("--right-width", type=float, default=4.0, help="total span of the right design, cm")
ap.add_argument("--lift", type=float, default=0.0018, help="clearance above the skin, m (leather sits proud)")
ap.add_argument("--segs", type=int, default=48, help="segments round the arm")
ap.add_argument("--preview", action="store_true", help="also write .obj + keep the scene for rendering")
a = ap.parse_args(argv)
os.makedirs(a.out, exist_ok=True)
ctypes.windll.ole32.CoInitializeEx(None, 0)

mesh_path = os.path.join(a.src, a.character + ".mesh.2109108288")
bpy.ops.re_mesh.importfile(filepath=mesh_path, directory=a.src, files=[{"name": os.path.basename(mesh_path)}],
                           clearScene=True, loadMaterials=False, loadMDFData=False)
arm_obj = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
skin = [o for o in bpy.data.objects if o.type == "MESH" and o.data.materials and "Body_Mat" in o.data.materials[0].name][0]
def jpos(n):
    b = arm_obj.data.bones.get(n)
    return np.array(arm_obj.matrix_world @ b.head_local, np.float32) if b else None
def unit(v): return v / (np.linalg.norm(v) + 1e-9)

me = skin.data; names = [g.name for g in skin.vertex_groups]; M = skin.matrix_world
vco = np.array([M @ v.co for v in me.vertices], np.float32)
topname = []
for v in me.vertices:
    best, bw = "", 0.0
    for g in v.groups:
        if g.weight > bw: best, bw = names[g.group], g.weight
    topname.append(best)
topname = np.array(topname)


def arm_frame(side):
    """wrist origin, axis up the forearm, and two perpendicular axes: `back` faces the back of the hand."""
    w = jpos(side + "_arm_wrist"); el = jpos(side + "_arm_radius")
    up = unit(el - w)
    # the back of the forearm is the side the back of the hand is on: use the hand's knuckle row to define the palm
    # plane, exactly as the nail pass does, so `back` means the same thing everywhere in this toolkit.
    m0 = jpos(side + "_hand_middle_0")
    krow = []
    for f in ("index", "little"):
        for k in range(4):
            pk = jpos(side + "_hand_%s_%d" % (f, k))
            if pk is not None and np.dot(pk - w, unit(m0 - w)) > 0.04: krow.append(pk); break
    lat = unit(np.cross(unit(m0 - w), unit(krow[1] - krow[0]))) if len(krow) == 2 else unit(np.cross(up, (1, 0, 0)))
    curl = 0.0
    for f in ("index", "middle", "ring", "little"):
        j0, j1, j2 = jpos(side + "_hand_%s_0" % f), jpos(side + "_hand_%s_1" % f), jpos(side + "_hand_%s_2" % f)
        if j0 is not None and j1 is not None and j2 is not None: curl += float(np.dot(unit(j2 - j1) - unit(j1 - j0), lat))
    back = lat * (-1.0 if curl > 0 else 1.0)
    back = unit(back - up * np.dot(back, up))
    side_v = unit(np.cross(up, back))
    return w, up, back, side_v


def arm_radius_profile(side, w, up, back, side_v, d_lo, d_hi, segs):
    """the arm's REAL outline at this height: for each angle round the arm, the furthest skin vertex, smoothed.
    A cylinder pinches on a forearm -- it is a flattened oval that rotates with the radius twist."""
    sel = np.array([t.startswith(side + "_arm_radius") or t == side + "_arm_wrist" for t in topname])
    p = vco[sel] - w
    d = p @ up
    keep = (d > d_lo - 0.02) & (d < d_hi + 0.02)
    if keep.sum() < 24: keep = (d > d_lo - 0.05) & (d < d_hi + 0.05)
    q = p[keep] - np.outer(d[keep], up)
    ang = np.arctan2(q @ side_v, q @ back)
    rad = np.linalg.norm(q, axis=1)
    prof = np.zeros(segs, np.float64)
    grid = np.linspace(-math.pi, math.pi, segs, endpoint=False)
    for i, g in enumerate(grid):
        dif = np.abs((ang - g + math.pi) % (2 * math.pi) - math.pi)
        near = dif < (2.2 * math.pi / segs)
        prof[i] = np.percentile(rad[near], 88) if near.sum() >= 3 else np.nan
    # fill gaps, then wrap-around smooth so the band is a clean closed loop
    idx = np.arange(segs); good = ~np.isnan(prof)
    prof = np.interp(idx, idx[good], prof[good], period=segs)
    k = np.array([0.06, 0.24, 0.40, 0.24, 0.06])
    prof = np.convolve(np.concatenate([prof[-2:], prof, prof[:2]]), k, mode="same")[2:-2]
    return grid, prof


def ring(w, up, back, side_v, grid, prof, d, extra):
    """world points of one closed loop at height d, `extra` metres outside the skin"""
    r = prof + extra
    return np.array([w + up * d + (back * math.cos(g) + side_v * math.sin(g)) * rr for g, rr in zip(grid, r)])


def add_band(bm, uv_layer, w, up, back, side_v, grid, prof, d0, d1, thick, u_scale=(0.0, 1.0), rings=3, taper=0.0, slot=0):
    """a closed band from d0 to d1, standing `thick` off the skin, with a rounded outer edge (chamfered rings)"""
    segs = len(grid)
    profiles = []
    for j in range(rings + 1):
        t = j / rings
        dd = d0 + (d1 - d0) * t
        # chamfer: thinner at the two edges so the band reads as a strap with a rolled edge, not a tube slice
        e = min(t, 1 - t) * 2.0
        th = thick * (0.45 + 0.55 * min(e * 2.2, 1.0)) - taper * abs(t - 0.5) * 2.0
        profiles.append(ring(w, up, back, side_v, grid, prof, dd, a.lift + max(th, 0.0004)))
    verts = [[bm.verts.new(tuple(map(float, p))) for p in row] for row in profiles]
    for j in range(rings):
        for i in range(segs):
            i2 = (i + 1) % segs
            f = bm.faces.new((verts[j][i], verts[j][i2], verts[j + 1][i2], verts[j + 1][i])); f.material_index = slot
            u0, u1 = u_scale
            for lp, (uu, vv) in zip(f.loops, ((i / segs, u0), ((i + 1) / segs, u0), ((i + 1) / segs, u1), (i / segs, u1))):
                lp[uv_layer].uv = (uu * 4.0 % 1.0, vv)
    # cap the two open edges back onto the skin so no hole shows from a grazing angle
    for j, dd in ((0, d0), (rings, d1)):
        inner = [bm.verts.new(tuple(map(float, p))) for p in ring(w, up, back, side_v, grid, prof, dd, a.lift * 0.2)]
        for i in range(segs):
            i2 = (i + 1) % segs
            cf = bm.faces.new((inner[i], inner[i2], verts[0][i2], verts[0][i])) if j == 0 else bm.faces.new((verts[rings][i], verts[rings][i2], inner[i2], inner[i]))
            cf.material_index = slot
    return verts


def add_plate(bm, uv_layer, w, up, back, side_v, grid, prof, d_c, half_d, half_ang, thick, slot=1):
    """a metal plate lying on the band, centred on the BACK of the arm, spanning +-half_ang radians"""
    nu, nv = 10, 5
    rows = []
    for j in range(nv + 1):
        t = j / nv
        dd = d_c - half_d + 2 * half_d * t
        row = []
        for i in range(nu + 1):
            s = i / nu
            g = -half_ang + 2 * half_ang * s
            # a shallow dome: thickest in the middle, feathered at all four edges
            fe = min(min(s, 1 - s) * 2.6, 1.0) * min(min(t, 1 - t) * 2.6, 1.0)
            r = np.interp((g + math.pi) % (2 * math.pi) - math.pi, grid, prof, period=2 * math.pi)
            p = w + up * dd + (back * math.cos(g) + side_v * math.sin(g)) * (r + a.lift + thick * (0.35 + 0.65 * fe))
            row.append(bm.verts.new(tuple(map(float, p))))
        rows.append(row)
    for j in range(nv):
        for i in range(nu):
            f = bm.faces.new((rows[j][i], rows[j][i + 1], rows[j + 1][i + 1], rows[j + 1][i])); f.material_index = slot
            for lp, (uu, vv) in zip(f.loops, ((i / nu, j / nv), ((i + 1) / nu, j / nv), ((i + 1) / nu, (j + 1) / nv), (i / nu, (j + 1) / nv))):
                lp[uv_layer].uv = (uu, vv)


def add_stud(bm, uv_layer, w, up, back, side_v, grid, prof, d_c, ang, r_stud, h, slot=1):
    """a domed metal rivet sitting on the band"""
    r = float(np.interp((ang + math.pi) % (2 * math.pi) - math.pi, grid, prof, period=2 * math.pi))
    n = back * math.cos(ang) + side_v * math.sin(ang)
    centre = w + up * d_c + n * (r + a.lift)
    t1 = unit(np.cross(n, up)); t2 = unit(np.cross(n, t1))
    seg, rings_ = 12, 3
    prev = None
    apex = bm.verts.new(tuple(map(float, centre + n * h)))
    for j in range(rings_, 0, -1):
        rr = r_stud * math.sin(math.pi / 2 * j / rings_)
        hh = h * math.cos(math.pi / 2 * j / rings_) * 0.9
        row = [bm.verts.new(tuple(map(float, centre + t1 * (rr * math.cos(2 * math.pi * i / seg)) + t2 * (rr * math.sin(2 * math.pi * i / seg)) + n * hh))) for i in range(seg)]
        if prev is not None:
            for i in range(seg):
                i2 = (i + 1) % seg
                f = bm.faces.new((prev[i], prev[i2], row[i2], row[i])); f.material_index = slot
                for lp in f.loops: lp[uv_layer].uv = (0.5, 0.5)
        prev = row
    for i in range(seg):
        f = bm.faces.new((prev[i], prev[(i + 1) % seg], apex)); f.material_index = slot
        for lp in f.loops: lp[uv_layer].uv = (0.5, 0.5)


def add_ring(bm, uv_layer, w, up, back, side_v, grid, prof, d_c, ang, half_len, tube, slot=1):
    """a metal ring lying across the cord at this angle: a short torus whose axis runs round the arm"""
    r = float(np.interp((ang + math.pi) % (2 * math.pi) - math.pi, grid, prof, period=2 * math.pi))
    n = back * math.cos(ang) + side_v * math.sin(ang)
    ringR = half_len
    major, minor = 14, 8
    rows = []
    for i in range(major):
        th = 2 * math.pi * i / major
        # centre of the tube: a circle in the (up, n) plane, sitting on the band
        c = w + up * (d_c + ringR * math.sin(th)) + n * (r + a.lift + tube + ringR * 0.30 * (1 + math.cos(th)))
        t1 = unit(np.cross(n, up)); t2 = unit(up * math.cos(th) - n * math.sin(th))
        rows.append([bm.verts.new(tuple(map(float, c + t1 * (tube * math.cos(ph)) + t2 * (tube * math.sin(ph)))))
                     for ph in [2 * math.pi * j / minor for j in range(minor)]])
    for i in range(major):
        i2 = (i + 1) % major
        for j in range(minor):
            j2 = (j + 1) % minor
            f = bm.faces.new((rows[i][j], rows[i][j2], rows[i2][j2], rows[i2][j])); f.material_index = slot
            for lp in f.loops: lp[uv_layer].uv = (0.5, 0.5)


def build(side, centre_cm, width_cm, design):
    w, up, back, side_v = arm_frame(side)
    d_c = centre_cm / 100.0; half = width_cm / 200.0
    grid, prof = arm_radius_profile(side, w, up, back, side_v, d_c - half, d_c + half, a.segs)
    print("   %s arm: band centred %.1f cm up, span %.1f cm; arm radius round it %.2f-%.2f cm (mean %.2f)"
          % (side, centre_cm, width_cm, prof.min() * 100, prof.max() * 100, prof.mean() * 100))
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new("UVMap")
    if design == "strap_and_plate":
        # one wide leather cuff, a metal plate across the back of the arm, a rivet either side of it
        add_band(bm, uvl, w, up, back, side_v, grid, prof, d_c - half, d_c + half, 0.0030, (0.0, 1.0), rings=4, slot=0)
        # a raised welt along each edge of the cuff, so it reads as stitched leather and not a painted stripe
        for e in (-1, 1):
            add_band(bm, uvl, w, up, back, side_v, grid, prof, d_c + e * half * 0.80, d_c + e * half * 0.98, 0.0044, (0.0, 1.0), rings=2, slot=0)
        add_plate(bm, uvl, w, up, back, side_v, grid, prof, d_c, half * 0.50, 0.75, 0.0052, slot=1)
        for s in (-1, 1):
            for dz in (-0.55, 0.55):
                add_stud(bm, uvl, w, up, back, side_v, grid, prof, d_c + half * dz, s * 1.05, 0.0034, 0.0032, slot=1)
    else:
        # two narrow cords with a gap, three small rings threaded on the upper one
        gap = width_cm / 100.0 * 0.22
        wcord = (2 * half - gap) / 2.0
        add_band(bm, uvl, w, up, back, side_v, grid, prof, d_c - half, d_c - half + wcord, 0.0030, (0.0, 1.0), rings=3, slot=0)
        add_band(bm, uvl, w, up, back, side_v, grid, prof, d_c + half - wcord, d_c + half, 0.0028, (0.0, 1.0), rings=3, slot=0)
        # three metal rings threaded on the upper cord, and one small tag on the lower
        for ang in (-0.70, 0.0, 0.70):
            add_ring(bm, uvl, w, up, back, side_v, grid, prof, d_c - half + wcord * 0.5, ang, wcord * 0.62, 0.0026, slot=1)
        add_plate(bm, uvl, w, up, back, side_v, grid, prof, d_c + half - wcord * 0.5, wcord * 0.40, 0.34, 0.0040, slot=1)
    bm.normal_update()
    m = bpy.data.meshes.new("visceral_bracelet_" + side)
    for mn in ("visceral_bracelet_leather", "visceral_bracelet_metal"):
        m.materials.append(bpy.data.materials.get(mn) or bpy.data.materials.new(mn))
    bm.to_mesh(m); bm.free()
    ob = bpy.data.objects.new("visceral_bracelet_" + side, m)
    bpy.context.scene.collection.objects.link(ob)
    nmet = sum(1 for p in m.polygons if p.material_index == 1)
    print("      -> %d verts, %d faces (%d leather, %d metal)" % (len(m.vertices), len(m.polygons), len(m.polygons) - nmet, nmet))
    return ob


print("\n==== building ====")
objs = [build("r", a.right_at, a.right_width, "strap_and_plate"),
        build("l", a.left_at, a.left_width, "cord_and_rings")]
if a.preview:
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    path = os.path.join(a.out, "visceral_bracelets_preview.obj")
    bpy.ops.wm.obj_export(filepath=path, export_selected_objects=True, export_materials=False)
    print("preview OBJ ->", path)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(a.out, "visceral_bracelets.blend"))
print("DONE")
