"""Build Visceral's neck plug (our own geometry, no game data) and export it as an RE2 RT .mesh.
Usage: blender -b --python build_neckplug.py -- <out_dir> [<pl1000.mesh path for bone matrices>] [local]
Blender space here = the RE-Mesh-Editor import space: Z up, character faces -Y, metres, origin at the feet.
Measured on Claire (pl3050 face mesh, 2026-09-05): neck skin ring centre ~(0, +0.025), radius 0.063 @ z1.10,
0.057 @ 1.12, 0.045 @ 1.14; chin begins ~1.16; back of the neck y<=0.061 @ 1.16. Collar cloth ring z1.09..1.14 r0.061.
"""
import bpy, bmesh, sys, os, math
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index("--") + 1:]
out_dir = argv[0]
bone_src = argv[1] if len(argv) > 1 and argv[1] != "local" else None
LOCAL = "local" in argv
# neck_0 bind pose in RE space (row-major, rows = joint axes in world, last row = translation).
# 2026-09-06 15:05: CLAIRE is pl1000 (pl3000, measured on 2026-09-05, is SHERRY - a child: neck_0 at 1.100 vs Claire's 1.416).
# Claire's values, read from pl1000.mesh `[measured 2026-09-06]`; the Sherry ones are kept below for the record.
BIND_R = ((1.0, 0.0, 0.0), (0.0, 0.91525, 0.40289), (0.0, -0.40289, 0.91525))
BIND_T = (0.0, 1.41573, -0.03468)
# SHERRY_BIND_R = ((1.0, 0.0, 0.0), (0.0, 0.95217, 0.30558), (0.0, -0.30558, 0.95217)); SHERRY_BIND_T = (0.0, 1.10038, -0.04075)
def to_local_blender(x, y, z):
    # Blender (x,y,z) -> RE (x, z, -y); local = (w - t) . rows; back to Blender (x, -z, y)
    d = (x - BIND_T[0], z - BIND_T[1], -y - BIND_T[2])
    l = [sum(d[j] * BIND_R[i][j] for j in range(3)) for i in range(3)]
    return (l[0], -l[2], l[1])
NAME = "visceral_neckplug_neck0local" if "local" in sys.argv else "visceral_neckplug"
MAT = "pl1000_Body_Mat"            # Claire's skin material (pl1000.mdf2); the game supplies the material. Was pl3000_Skin_Mat = Sherry's
UV_C = (0.9597, 0.5125)            # a dense forearm-skin patch on pl1000_Jacket_ALBM (densest 1/40 UV cell of l_arm_radius loops, 2026-09-06)
UV_R = 0.004
CENTRE_Y = 0.012                   # Claire (Sherry's was 0.025)
# Claire's neck ring, measured on pl1050.mesh (Face_Mat, neck-weighted skin) 2026-09-06 15:00 `[measured]`: skin from z 1.45 to
# 1.54, half-width 0.043 at 1.50 and 0.040 at 1.48, axis ~0.012 forward of the bone; jaw (head-weighted) begins at 1.467.
Z_BOTTOM, Z_CYL_TOP, Z_TOP = 1.440, 1.500, 1.520
R_CYL = 0.038                      # inside Claire's 0.040-0.043 half-width
SEGS, RINGS_DOME = 32, 5

# --- optional: read neck bone bind matrices from the real body mesh (for the runtime attach math)
if bone_src:
    bpy.ops.re_mesh.importfile(filepath=bone_src, directory=os.path.dirname(bone_src), files=[{"name": os.path.basename(bone_src)}], clearScene=True, loadMaterials=False, loadMDFData=False)
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    for bn in ("spine_2", "neck_0", "neck_1", "head"):
        b = arm.data.bones.get(bn)
        if b:
            m = arm.matrix_world @ b.matrix_local
            q = m.to_quaternion(); t = m.to_translation()
            print("BIND %-8s blender t=(%.4f, %.4f, %.4f) q(w,x,y,z)=(%.4f, %.4f, %.4f, %.4f) parent=%s" % (bn, t.x, t.y, t.z, q.w, q.x, q.y, q.z, b.parent.name if b.parent else None))
            cp = {k: b.get(k) for k in b.keys()}
            if cp: print("      custom props:", cp)
    # clear everything the import made
    for c in list(bpy.data.collections): bpy.data.collections.remove(c)
    for o in list(bpy.data.objects): bpy.data.objects.remove(o)
else:
    for c in list(bpy.data.collections): bpy.data.collections.remove(c)
    for o in list(bpy.data.objects): bpy.data.objects.remove(o)

# --- geometry
bm = bmesh.new()
uv_layer = bm.loops.layers.uv.new("UVMap0")
rings = []
def ring(z, r):
    vs = []
    for i in range(SEGS):
        a = 2 * math.pi * i / SEGS
        pt = (r * math.cos(a), CENTRE_Y + r * math.sin(a), z)
        vs.append(bm.verts.new(to_local_blender(*pt) if LOCAL else pt))
    return vs
rings.append(ring(Z_BOTTOM, R_CYL))
rings.append(ring(Z_CYL_TOP, R_CYL))
for k in range(1, RINGS_DOME):
    f = k / RINGS_DOME                       # 0..1 up the dome
    z = Z_CYL_TOP + (Z_TOP - Z_CYL_TOP) * f
    r = R_CYL * math.sqrt(max(0.0, 1.0 - f * f))
    rings.append(ring(z, r))
top = bm.verts.new(to_local_blender(0.0, CENTRE_Y, Z_TOP) if LOCAL else (0.0, CENTRE_Y, Z_TOP))
bottom = bm.verts.new(to_local_blender(0.0, CENTRE_Y, Z_BOTTOM) if LOCAL else (0.0, CENTRE_Y, Z_BOTTOM))
faces = []
for a, b in zip(rings[:-1], rings[1:]):
    for i in range(SEGS):
        j = (i + 1) % SEGS
        faces.append(bm.faces.new((a[i], a[j], b[j], b[i])))
last = rings[-1]
for i in range(SEGS):
    j = (i + 1) % SEGS
    faces.append(bm.faces.new((last[i], last[j], top)))
first = rings[0]
for i in range(SEGS):
    j = (i + 1) % SEGS
    faces.append(bm.faces.new((first[j], first[i], bottom)))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
for f in bm.faces:
    for l in f.loops:
        # tiny UV footprint inside the skin patch, varied a little so no two loops share a point
        ang = math.atan2(l.vert.co.y, l.vert.co.x)
        l[uv_layer].uv = (UV_C[0] + UV_R * 0.5 * math.cos(ang), UV_C[1] + UV_R * 0.5 * math.sin(ang))
me = bpy.data.meshes.new("LOD_0_Group_0_Sub_0__" + MAT)
bm.to_mesh(me); bm.free()
mat = bpy.data.materials.new(MAT)
me.materials.append(mat)
obj = bpy.data.objects.new("LOD_0_Group_0_Sub_0__" + MAT, me)
coll = bpy.data.collections.new(NAME + ".mesh")
bpy.context.scene.collection.children.link(coll)
coll.objects.link(obj)
coll["~TYPE"] = "RE_MESH_COLLECTION"
print("PLUG verts=%d faces=%d" % (len(me.vertices), len(me.polygons)))

os.makedirs(out_dir, exist_ok=True)
out = os.path.join(out_dir, NAME + ".mesh.2109108288")
r = bpy.ops.re_mesh.exportfile(filepath=out, filename_ext=".2109108288", targetCollection=NAME + ".mesh", selectedOnly=False)
print("EXPORT", r, out, os.path.getsize(out) if os.path.exists(out) else "MISSING")
