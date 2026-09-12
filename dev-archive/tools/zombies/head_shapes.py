"""head_shapes.py -- proportional deformations that make one zombie head read as a different person.

Step 3 of ZOMBIE VARIETY. Steps 1-2 gave the pool 25 new faces (dossier 7d-7f), but every one of
them is one of the eight shipped head MESHES (Face00..Face07) wearing a retinted skin, so the SHAPE
still repeats. This module varies the shape.

It is the same idea as `face_roster.py`'s tint recipes, one level down: a named recipe is a handful
of numbers, each a proportional change to the head's own geometry, and a face entry can later carry
an optional `shape=` field alongside its tint.

WHY THIS IS SAFE TO DO AT ALL. An RE Mesh Editor import/export round trip of all eight heads is
lossless to within the format's own position precision: object list, vertex counts, face counts,
both UV layers, every bone weight and the whole 63-77 bone armature come back identical, the file
grows by 0 or 16 bytes, and the largest vertex move is 0.0003 mm
`[verified-numerically 2026-09-12, n=8 heads]` (`tools/blender/head_roundtrip.py`). So changing only
vertex POSITIONS leaves the rig, the UVs and the submesh/material split untouched by construction --
we never touch the arrays that carry them.

THE TWO RULES EVERY RECIPE OBEYS
  1. **Nothing moves at or below the neck.** The head mesh is an open shell that meets the body at a
     ring of vertices near the bottom; if those move, the head visibly detaches. Every displacement
     is multiplied by a mask that is exactly 0.0 at and below the `neck_1` bone and rises smoothly to
     1.0 at the `head` bone, so the whole neck -- not merely the ring -- is untouched by arithmetic
     rather than by care. ⚠️ The obvious cheaper test, "freeze the bottom of the bounding box", does
     NOT work here: the skin shell's largest boundary loop runs in ONE piece from the neck cut at
     z~1.551 all the way over the crown at z~1.922 `[measured 2026-09-12, n=3 heads]`, so "boundary
     vertices low down" is just wherever you drew the line.
  2. **Everything is smooth and proportional.** Displacements are driven by where a vertex sits
     relative to landmarks the head itself provides -- its bounding box and its own facial bones
     (`nose`, `jaw`, `cheekL/R`, `eyeblowL/R_02`, `EyeL/R`, `mouth01`), all of which are present on
     all eight heads `[measured 2026-09-12, n=8]`. No absolute coordinates are baked in, so a recipe
     means the same thing on a narrow head and a broad one.

The field is computed ONCE per head and applied to every object in the file -- all LODs, plus Teeth,
Hair and insidehead. That matters: deform LOD0 alone and the head changes shape as the player walks
away, and deform the skin alone and the teeth come through the lip.

FRAME (Blender world space after the addon's Y-up -> Z-up import, metres, character space):
  +X = the character's left, -Y = forward (the face), +Z = up. A head sits at z ~1.52 .. 1.94.

KNOBS -- all default 0.0, which means "no change". Sign conventions in plain words:
  head_scale   whole head bigger (+) or smaller (-), about the neck. 0.06 = 6 % bigger.
  skull_len    back of the skull further back (+) or flatter (-). 0.15 = 15 % longer behind the ears.
  jaw_width    jaw and lower face wider (+) or narrower (-). 0.20 = 20 % wider.
  brow         brow ridge heavier, pushed forward (+) or flatter (-). 1.0 ~ 13 mm on a typical head.
  nose_len     nose longer (+) or shorter/flatter (-). 1.0 ~ 15 mm.
  cheek        cheeks fuller (+) or sunken (-). 1.0 ~ 10 mm out to each side.
  chin         chin longer and more forward (+) or receding (-). 1.0 ~ 13 mm down, 8 mm forward.

The brow/nose/cheek/chin knobs are scaled by the head's own half-width, so "1.0" is a strong but
still human change on any of the eight. Recipes below stay inside +-1.0 on those and +-0.25 on the
proportional ones, which is the range that reads as a different person rather than as a cartoon.
"""

# ------------------------------------------------------------------ recipes
# Named shapes. A face entry may later carry shape="heavy_jaw" next to its tint.
SHAPES = {
    # the identity, kept so a "no shape" entry and a "shape" entry go down the same code path
    "stock":       dict(),

    "broad":       dict(head_scale=0.045, jaw_width=0.22, cheek=0.55, brow=0.35),
    "gaunt":       dict(head_scale=-0.030, jaw_width=-0.18, cheek=-0.85, chin=0.30),
    "heavy_jaw":   dict(jaw_width=0.26, chin=0.55, brow=0.45, cheek=0.20),
    "weak_jaw":    dict(jaw_width=-0.20, chin=-0.50, brow=-0.25),
    "long_skull":  dict(skull_len=0.18, head_scale=0.020, jaw_width=-0.08),
    "flat_skull":  dict(skull_len=-0.16, head_scale=-0.015, jaw_width=0.10),
    "big_nose":    dict(nose_len=0.85, brow=0.25, jaw_width=-0.06),
    "snub_nose":   dict(nose_len=-0.70, cheek=0.30),
    "brow_ridge":  dict(brow=0.90, nose_len=0.25, jaw_width=0.10),
    "hollow":      dict(cheek=-0.95, head_scale=-0.020, chin=0.20, skull_len=0.06),
    "bloated":     dict(head_scale=0.060, cheek=0.80, jaw_width=0.14, chin=-0.20),
    "small_head":  dict(head_scale=-0.055, skull_len=-0.06),
    "big_head":    dict(head_scale=0.070, skull_len=0.08),
    "lantern":     dict(chin=0.80, jaw_width=0.16, cheek=-0.45, skull_len=0.08),
    "pug":         dict(nose_len=-0.55, brow=0.55, jaw_width=0.18, chin=-0.30),
}

# How far a knob of 1.0 moves things, as a fraction of the head's own HALF-WIDTH (~0.10 m).
# Kept here rather than inline so the numbers in the report can be traced back to one place.
GAIN = dict(brow_y=0.13, nose_y=0.15, cheek_x=0.10, cheek_y=0.05, chin_z=0.13, chin_y=0.08)

# The seam-protection band runs between two bones every head has: nothing moves at or below `neck_1`,
# everything moves fully at or above `head`. On the eight shipped heads that is a ~84 mm ramp.
SEAM_LO_BONE, SEAM_HI_BONE = "neck_1", "head"

# For the report only: a vertex is "on the seam ring" if it is an open-edge vertex within
# SEAM_RING_TOL of its object's lowest point, AND its object reaches down to within SEAM_REACH of the
# lowest point of the whole head. The second test matters -- Teeth is an open shell whose own bottom
# is 140 mm above the neck, and counting its rim as a seam makes the report say the join moved when
# it did not.
SEAM_RING_TOL = 0.015
SEAM_REACH = 0.05

# Radii of the local blobs (brow/nose/cheek/chin), as fractions of the head's half-width.
RADIUS = dict(brow=0.62, nose=0.52, cheek=0.58, chin=0.50)


def recipe(name):
    if name not in SHAPES:
        raise KeyError("no such head shape: %s (have %s)" % (name, ", ".join(sorted(SHAPES))))
    return dict(SHAPES[name])


# ------------------------------------------------------------------ maths (pure python, no deps)
def _sat(t):
    return 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)


def _smooth(t):
    """Cubic smoothstep: flat at both ends, so nothing creases where a term switches on."""
    t = _sat(t)
    return t * t * (3.0 - 2.0 * t)


def _blob(p, c, r):
    """Smooth radial falloff: 1 at the centre, 0 at and beyond radius r."""
    dx, dy, dz = p[0] - c[0], p[1] - c[1], p[2] - c[2]
    d2 = dx * dx + dy * dy + dz * dz
    if d2 >= r * r:
        return 0.0
    return _smooth(1.0 - (d2 ** 0.5) / r)


class Frame(object):
    """Everything a recipe needs to know about one head, derived from that head alone."""

    def __init__(self, bbox, bones):
        (self.x0, self.y0, self.z0, self.x1, self.y1, self.z1) = bbox
        self.bones = bones
        self.z_seam = bones[SEAM_LO_BONE][2]
        self.band = max(bones[SEAM_HI_BONE][2] - self.z_seam, 1e-6)
        self.cx = 0.5 * (self.x0 + self.x1)
        self.cy = 0.5 * (self.y0 + self.y1)
        self.hw = 0.5 * (self.x1 - self.x0)          # half width -- the unit for the local knobs
        self.hd = self.y1 - self.y0                  # depth, front of nose to back of skull
        self.hh = self.z1 - self.z0                  # height, seam region to crown
        eye_z = 0.5 * (bones["EyeL"][2] + bones["EyeR"][2])
        mouth_z = bones["mouth01"][2]
        self.eye_z, self.mouth_z = eye_z, mouth_z
        self.jaw_z = 0.5 * (bones["jaw"][2] + mouth_z)
        self.jaw_hz = 0.6 * (eye_z - mouth_z)
        self.chin = (self.cx,
                     self.y0 + 0.10 * self.hd,
                     mouth_z - 0.5 * (eye_z - mouth_z))

    def mask(self, z):
        """0 at and below the neck seam, 1 well above it. Rule 1 lives here."""
        return _smooth((z - self.z_seam) / self.band)

    def delta(self, p, k):
        """The displacement for one vertex under recipe dict `k`. Returns a 3-tuple."""
        m = self.mask(p[2])
        if m <= 0.0:
            return (0.0, 0.0, 0.0)
        dx = dy = dz = 0.0
        x, y, z = p

        s = k.get("head_scale", 0.0)
        if s:
            dx += m * s * (x - self.cx)
            dy += m * s * (y - self.cy)
            dz += m * s * (z - self.z_seam)

        s = k.get("skull_len", 0.0)
        if s:
            back = _smooth((y - self.cy) / max(self.y1 - self.cy, 1e-6))
            dy += m * s * back * (self.y1 - self.cy)

        s = k.get("jaw_width", 0.0)
        if s:
            wz = _smooth(1.0 - abs(z - self.jaw_z) / max(self.jaw_hz, 1e-6))
            wy = _smooth((self.y1 - y) / max(self.hd, 1e-6))
            dx += m * s * wz * wy * (x - self.cx)

        s = k.get("brow", 0.0)
        if s:
            r = RADIUS["brow"] * self.hw
            w = max(_blob(p, self.bones["eyeblowL_02"], r), _blob(p, self.bones["eyeblowR_02"], r))
            dy -= m * s * w * GAIN["brow_y"] * self.hw

        s = k.get("nose_len", 0.0)
        if s:
            w = _blob(p, self.bones["nose"], RADIUS["nose"] * self.hw)
            dy -= m * s * w * GAIN["nose_y"] * self.hw

        s = k.get("cheek", 0.0)
        if s:
            r = RADIUS["cheek"] * self.hw
            w = max(_blob(p, self.bones["cheekL"], r), _blob(p, self.bones["cheekR"], r))
            side = 1.0 if x >= self.cx else -1.0
            dx += m * s * w * GAIN["cheek_x"] * self.hw * side
            dy -= m * s * w * GAIN["cheek_y"] * self.hw

        s = k.get("chin", 0.0)
        if s:
            w = _blob(p, self.chin, RADIUS["chin"] * self.hw)
            dz -= m * s * w * GAIN["chin_z"] * self.hw
            dy -= m * s * w * GAIN["chin_y"] * self.hw

        return (dx, dy, dz)


# ------------------------------------------------------------------ Blender side
def _bone_map(bpy):
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    if not arms:
        raise SystemExit("head_shapes: no armature in the scene -- import with the addon first")
    return {b.name: tuple(b.head_local) for b in arms[0].data.bones}


def seam_sets(bpy, objects):
    """The vertices a deformation must leave exactly alone, measured on the SOURCE mesh.

    Two sets, because they answer two different worries:
      "ring"  -- boundary (open-edge) vertices within SEAM_RING_TOL of an object's lowest point.
                 This is the actual cut where the head shell butts onto the body.
      "neck"  -- every vertex at or below the `neck_1` bone, whatever its object. This is the whole
                 region the body also occupies, and it is the set the mask is written against.
    Call this BEFORE deforming and keep the result; recomputing it afterwards measures a moved mesh
    against a moved rule and reports nonsense.
    """
    import bmesh
    bones = _bone_map(bpy)
    z_neck = bones[SEAM_LO_BONE][2]
    z_floor = min((o.matrix_world @ v.co).z for o in objects for v in o.data.vertices)
    ring, neck = {}, {}
    for o in objects:
        zs = [(o.matrix_world @ v.co).z for v in o.data.vertices]
        lo = min(zs)
        n = [i for i, z in enumerate(zs) if z <= z_neck]
        if n:
            neck[o.name] = n
        if not o.name.startswith("LOD_0_") or lo > z_floor + SEAM_REACH:
            continue
        bm = bmesh.new()
        bm.from_mesh(o.data)
        idx = sorted({v.index for e in bm.edges if e.is_boundary for v in e.verts
                      if zs[v.index] <= lo + SEAM_RING_TOL})
        bm.free()
        if idx:
            ring[o.name] = idx
    return {"ring": ring, "neck": neck, "z_neck": z_neck}


def build_frame(bpy, objects):
    from mathutils import Vector
    pts = [o.matrix_world @ Vector(c) for o in objects for c in o.bound_box]
    bbox = (min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts),
            max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))
    return Frame(bbox, _bone_map(bpy))


def apply_to_blender(bpy, objects, spec):
    """Deform every imported object in place. `spec` is a recipe dict, or {"name": "<shape>"}.

    Only vertex positions are written. Vertex groups, weights, UV layers, materials, the object
    list and the armature are never touched, which is what keeps the rig intact.
    """
    if "name" in spec:
        k = recipe(spec["name"])
        k.update({a: b for a, b in spec.items() if a != "name"})
        name = spec["name"]
    else:
        k, name = dict(spec), "(inline)"
    f = build_frame(bpy, objects)
    moved = 0
    for o in objects:
        mw, mwi = o.matrix_world, o.matrix_world.inverted()
        for v in o.data.vertices:
            p = mw @ v.co
            d = f.delta((p.x, p.y, p.z), k)
            if d != (0.0, 0.0, 0.0):
                p.x += d[0]
                p.y += d[1]
                p.z += d[2]
                v.co = mwi @ p
                moved += 1
        o.data.update()
    return {"name": name, "knobs": k, "moved_verts": moved,
            "z_seam": round(f.z_seam, 5), "band_m": round(f.band, 5),
            "half_width_m": round(f.hw, 5), "depth_m": round(f.hd, 5), "height_m": round(f.hh, 5),
            "bbox": [round(v, 5) for v in (f.x0, f.y0, f.z0, f.x1, f.y1, f.z1)]}


def seam_report(sets, before_co, after_co):
    """Rule 1, measured: how far did the protected vertices actually move? Must be 0."""
    out = {"z_neck": round(sets["z_neck"], 5)}
    for kind in ("ring", "neck"):
        worst, n, where = 0.0, 0, None
        for name, ids in sets[kind].items():
            a, b = before_co[name], after_co[name]
            for i in ids:
                dx, dy, dz = a[i][0] - b[i][0], a[i][1] - b[i][1], a[i][2] - b[i][2]
                d = (dx * dx + dy * dy + dz * dz) ** 0.5
                if d > worst:
                    worst, where = d, name
            n += len(ids)
        out[kind] = {"verts": n, "max_move_mm": round(worst * 1000, 6), "worst_on": where}
    return out


if __name__ == "__main__":
    print("%d head shapes (plus 'stock' = no change)" % (len(SHAPES) - 1))
    knobs = ["head_scale", "skull_len", "jaw_width", "brow", "nose_len", "cheek", "chin"]
    print("%-12s %s" % ("", " ".join("%10s" % k for k in knobs)))
    for name in sorted(SHAPES):
        k = SHAPES[name]
        print("%-12s %s" % (name, " ".join("%10.3f" % k.get(x, 0.0) for x in knobs)))
