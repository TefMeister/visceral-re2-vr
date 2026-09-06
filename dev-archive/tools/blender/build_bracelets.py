"""Build Visceral's forearm bracelets -- our own geometry, no game data -- to sit over the straight seam lines on
Claire's forearms (Tefa's idea, 2026-09-06: "cover over these exact straight lines so they can stay under the
bracelets ... something that looks like metal and leather combined, different on both hands").

  blender -b --python build_bracelets.py -- <plXXXX folder> <out_dir> [--character pl1000]
        [--left-at 6.0 --left-width 3.0] [--right-at 6.0 --right-width 4.0] [--preview]

Two different designs, both wrapped onto the arm's REAL cross-section (sampled from the skin mesh, not a cylinder,
so they never pinch or float):
  RIGHT arm -- "strap and plate": one wide leather cuff, a brushed metal plate across the back of the arm, two
      rivets either side of it, and a raised leather edge-welt top and bottom.
  LEFT arm -- two THIN separate bands (Tefa, 2026-09-06): a DARK RED one low with three metal rings threaded on it, and a
      RED-PURPLE one higher, nearest the jacket, with a small metal clasp. Both clear of the watch she already wears --
      that watch is the 0.3-2.2 cm of jacket-submesh geometry at her left wrist, and it is built from separate detached
      islands, so it could be moved or widened in a mesh edit if we ever want to.

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
ap.add_argument("--left-at", type=float, default=6.0, help="(unused since the two-band redesign; kept so old command lines still parse)")
ap.add_argument("--left-width", type=float, default=3.4, help="(unused since the two-band redesign)")
ap.add_argument("--left-low", type=float, default=3.6, help="centre of the LOWER left band (dark red), cm from the wrist joint")
ap.add_argument("--left-low-width", type=float, default=0.9, help="width of the lower left band, cm")
ap.add_argument("--left-high", type=float, default=5.0, help="centre of the UPPER left band (red-purple, nearest the jacket), cm")
ap.add_argument("--left-high-width", type=float, default=0.8, help="width of the upper left band, cm")
ap.add_argument("--right-at", type=float, default=3.5, help="centre of the right band, cm from the wrist joint")
ap.add_argument("--right-width", type=float, default=4.0, help="total span of the right design, cm")
ap.add_argument("--lift", type=float, default=0.0018, help="clearance above the skin, m (leather sits proud)")
ap.add_argument("--segs", type=int, default=48, help="segments round the arm")
ap.add_argument("--preview", action="store_true", help="also write .obj + keep the scene for rendering")
ap.add_argument("--export", action="store_true", help="also export one RE2 RT .mesh per side in the ARM_RADIUS joint's local frame, for the plugin (like the neck plug)")
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


# material slot -> name, per side. These names are what visceral_bracelets.mdf2 (mdf_build_bracelets.py) defines.
MAT_NAMES = {"r": ("visceral_bracelet_leather", "visceral_bracelet_metal", "visceral_bracelet_leather"),
             "l": ("visceral_bracelet_leather_red", "visceral_bracelet_metal", "visceral_bracelet_leather_purple")}


def build(side, centre_cm, width_cm, design):
    w, up, back, side_v = arm_frame(side)
    d_c = centre_cm / 100.0; half = width_cm / 200.0
    if design == "cord_and_rings":            # the two left bands sit far apart: sample the arm across the whole span
        lo = a.left_low / 100.0 - a.left_low_width / 200.0
        hi = a.left_high / 100.0 + a.left_high_width / 200.0
        grid, prof = arm_radius_profile(side, w, up, back, side_v, lo, hi, a.segs)
        d_c, half = 0.5 * (lo + hi), 0.5 * (hi - lo)
    else:
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
        # Tefa's brief (2026-09-06): "the left arm ones should be thinner and one dark red, the other one, closest to
        # the jacket, something between red and purple". Two independent thin bands rather than a paired cuff, placed to
        # leave clear skin either side of them: her watch occupies 0.3-2.2 cm and the rolled sleeve starts at 10-11 cm
        # `[measured 2026-09-06]`, so there is 7.8 cm of free arm to spread across.
        lo_c, lo_w = a.left_low / 100.0, a.left_low_width / 200.0
        hi_c, hi_w = a.left_high / 100.0, a.left_high_width / 200.0
        # lower band, closer to the hand: DARK RED (slot 0), with three small metal rings threaded on it
        add_band(bm, uvl, w, up, back, side_v, grid, prof, lo_c - lo_w, lo_c + lo_w, 0.0026, (0.0, 1.0), rings=3, slot=0)
        for ang in (-0.70, 0.0, 0.70):
            add_ring(bm, uvl, w, up, back, side_v, grid, prof, lo_c, ang, lo_w * 1.15, 0.0022, slot=1)
        # upper band, closest to the jacket: RED-PURPLE (slot 2), with one small metal clasp plate on the back
        add_band(bm, uvl, w, up, back, side_v, grid, prof, hi_c - hi_w, hi_c + hi_w, 0.0024, (0.0, 1.0), rings=3, slot=2)
        add_plate(bm, uvl, w, up, back, side_v, grid, prof, hi_c, hi_w * 0.72, 0.30, 0.0034, slot=1)
    bm.normal_update()
    m = bpy.data.meshes.new("visceral_bracelet_" + side)
    for mn in MAT_NAMES[side]:
        m.materials.append(bpy.data.materials.get(mn) or bpy.data.materials.new(mn))
    bm.to_mesh(m); bm.free()
    ob = bpy.data.objects.new("visceral_bracelet_" + side, m)
    bpy.context.scene.collection.objects.link(ob)
    counts = [sum(1 for p in m.polygons if p.material_index == k) for k in range(3)]
    print("      -> %d verts, %d faces (leather %d, metal %d, leather-2 %d)" % (len(m.vertices), len(m.polygons), *counts))
    return ob


print("\n==== building ====")
objs = [build("r", a.right_at, a.right_width, "strap_and_plate"),
        build("l", a.left_low, a.left_low_width, "cord_and_rings")]
if a.preview:
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    path = os.path.join(a.out, "visceral_bracelets_preview.obj")
    bpy.ops.wm.obj_export(filepath=path, export_selected_objects=True, export_materials=False)
    print("preview OBJ ->", path)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(a.out, "visceral_bracelets.blend"))

def export_re_mesh(ob, side):
    """Write <out>/visceral_bracelet_<side>_radiuslocal.mesh.2109108288: the object's vertices moved into the
    <side>_arm_radius joint's LOCAL bind frame, split into one object per material (LOD_0_Group_0_Sub_N__<mat>), in a
    collection tagged for RE Mesh Editor's exporter -- the same recipe as build_neckplug.py, whose hard-coded neck_0
    numbers this conversion reproduces exactly `[verified 2026-09-06]`:
      RE-space bind rows  = the Blender bone matrix's COLUMNS, each converted (x, y, z)_B -> (x, z, -y)_RE
      RE-space bind T     = the bone's translation, converted the same way
      RE local            = rows . (w_RE - T);  written back to Blender as (x, -z, y) so the exporter's own
                            Blender->RE conversion lands on the RE-local coordinates.
    The plugin then pins a GameObject to the live joint's world position + rotation every frame (plug_update())."""
    b = arm_obj.data.bones[side + "_arm_radius"]
    Mb = arm_obj.matrix_world @ b.matrix_local
    def to_re(v): return (v[0], v[2], -v[1])
    rows = [to_re((Mb[0][c], Mb[1][c], Mb[2][c])) for c in range(3)]
    T = to_re((Mb[0][3], Mb[1][3], Mb[2][3]))
    def to_local_blender(p):
        w = to_re(p); d = (w[0] - T[0], w[1] - T[1], w[2] - T[2])
        l = [sum(d[j] * rows[i][j] for j in range(3)) for i in range(3)]
        return (l[0], -l[2], l[1])
    name = "visceral_bracelet_%s_radiuslocal" % side
    coll = bpy.data.collections.new(name + ".mesh"); bpy.context.scene.collection.children.link(coll); coll["~TYPE"] = "RE_MESH_COLLECTION"
    me = ob.data
    for slot, mat_name in enumerate(MAT_NAMES[side]):
        polys = [p for p in me.polygons if p.material_index == slot]
        if not polys: continue
        bm = bmesh.new(); uvl = bm.loops.layers.uv.new("UVMap0")
        src_uv = me.uv_layers.active.data
        vmap = {}
        for p in polys:
            vs = []
            for vi in p.vertices:
                if vi not in vmap: vmap[vi] = bm.verts.new(to_local_blender(ob.matrix_world @ me.vertices[vi].co))
                vs.append(vmap[vi])
            try: f = bm.faces.new(vs)
            except ValueError: continue
            for lp, li in zip(f.loops, p.loop_indices): lp[uvl].uv = tuple(src_uv[li].uv)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        sub = bpy.data.meshes.new("LOD_0_Group_0_Sub_%d__%s" % (slot, mat_name)); bm.to_mesh(sub); bm.free()
        sub.materials.append(bpy.data.materials.get(mat_name) or bpy.data.materials.new(mat_name))
        o = bpy.data.objects.new("LOD_0_Group_0_Sub_%d__%s" % (slot, mat_name), sub); coll.objects.link(o)
        print("   %s: sub %d %-34s %d verts %d faces" % (side, slot, mat_name, len(sub.vertices), len(sub.polygons)))
    out = os.path.join(a.out, name + ".mesh.2109108288")
    r = bpy.ops.re_mesh.exportfile(filepath=out, filename_ext=".2109108288", targetCollection=name + ".mesh", selectedOnly=False)
    print("   EXPORT %s -> %s (%s bytes)" % (r, out, os.path.getsize(out) if os.path.exists(out) else "MISSING"))
    print("   bind T (RE) = (%.4f, %.4f, %.4f)" % T)

if a.export:
    for ob in objs: export_re_mesh(ob, "l" if ob.name.endswith("_l") else "r")
print("DONE")
