"""head_roundtrip.py -- does an RE2 zombie head survive an RE Mesh Editor import/export round trip?

  blender -b --python head_roundtrip.py -- <src.mesh.2109108288> <out_dir> [<shape_json>]

Imports the mesh with RE Mesh Editor, optionally applies a named shape recipe from
`tools/zombies/head_shapes.py` (passed in as JSON on argv), exports it back to the same mesh
version, re-imports the export and prints a JSON report comparing the two.

Why this exists: the ZOMBIE VARIETY pack (dossier 7d-7f) varies a head's SKIN but reuses one of
eight shipped head MESHES, so the shape repeats. Deforming the mesh is the next lift, and it is
only worth attempting if a no-op round trip preserves the rig, the UVs and the submesh split.
Nothing here is deployed; it writes only into <out_dir>.
"""
import bpy, json, os, sys, hashlib
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
SRC, OUT_DIR = argv[0], argv[1]
SHAPE = json.loads(argv[2]) if len(argv) > 2 and argv[2] else None
EXT = "." + os.path.basename(SRC).split(".mesh.")[-1]
os.makedirs(OUT_DIR, exist_ok=True)


def imp(path):
    bpy.ops.re_mesh.importfile(
        filepath=path, directory=os.path.dirname(path),
        files=[{"name": os.path.basename(path)}],
        clearScene=True, loadMaterials=False, loadMDFData=False,
        importAllLODs=True, importBlendShapes=True, importShadowMeshes=True,
        createCollections=True, rotate90=True)


def mesh_objects():
    return sorted([o for o in bpy.data.objects if o.type == "MESH"], key=lambda o: o.name)


def stats():
    """Everything a round trip could silently lose, per mesh object plus the armature."""
    out = {"objects": [], "armature": None}
    for o in mesh_objects():
        me = o.data
        nw = sum(len(v.groups) for v in me.vertices)
        used = sorted({o.vertex_groups[g.group].name for v in me.vertices for g in v.groups})
        co = b"".join(b"%.4f,%.4f,%.4f;" % tuple(v.co) for v in me.vertices)
        uv0 = me.uv_layers[0].data if me.uv_layers else []
        uvh = b"".join(b"%.4f,%.4f;" % tuple(d.uv) for d in uv0)
        bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
        out["objects"].append({
            "name": o.name,
            "verts": len(me.vertices), "faces": len(me.polygons), "loops": len(me.loops),
            "uv_layers": [l.name for l in me.uv_layers],
            "vgroups": len(o.vertex_groups), "vgroups_used": len(used),
            "weights": nw,
            "shapekeys": len(me.shape_keys.key_blocks) if me.shape_keys else 0,
            "co_hash": hashlib.sha1(co).hexdigest()[:12],
            "uv_hash": hashlib.sha1(uvh).hexdigest()[:12],
            "bbox": [round(min(v[i] for v in bb), 5) for i in range(3)] +
                    [round(max(v[i] for v in bb), 5) for i in range(3)],
        })
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    if arms:
        a = arms[0]
        out["armature"] = {"name": a.name, "bones": len(a.data.bones),
                           "bone_names": [b.name for b in a.data.bones],
                           "key_bones": {b.name: [round(v, 5) for v in b.head_local]
                                         for b in a.data.bones
                                         if b.name in ("root", "head", "neck_0", "neck_1", "nose",
                                                       "jaw", "EyeL", "EyeR", "cheekL", "cheekR",
                                                       "mouth01", "eyeblowL_02")}}
    return out


def coords():
    return {o.name: [tuple(o.matrix_world @ v.co) for v in o.data.vertices] for o in mesh_objects()}


def displacement(a, b):
    """Per-object max / mean vertex displacement between two coordinate dicts, same index order."""
    out = {}
    for k, va in a.items():
        vb = b.get(k)
        if vb is None or len(vb) != len(va):
            out[k] = {"error": "count changed"}
            continue
        d = [((x[0] - y[0]) ** 2 + (x[1] - y[1]) ** 2 + (x[2] - y[2]) ** 2) ** 0.5
             for x, y in zip(va, vb)]
        out[k] = {"max_mm": round(max(d) * 1000, 4), "mean_mm": round(sum(d) / len(d) * 1000, 4)}
    return out


def collection_name():
    for c in bpy.data.collections:
        if c.get("~TYPE") == "RE_MESH_COLLECTION" or c.name.endswith(".mesh"):
            return c.name
    return ""


report = {"src": SRC, "src_bytes": os.path.getsize(SRC)}
imp(SRC)
before = stats()
report["before"] = before
src_co = coords()

applied = None
if SHAPE:
    sys.path.insert(0, os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                     "..", "zombies")))
    import head_shapes
    # the protected sets have to be read off the UNDEFORMED mesh, or the rule moves with the mesh
    sets = head_shapes.seam_sets(bpy, mesh_objects())
    applied = head_shapes.apply_to_blender(bpy, mesh_objects(), SHAPE)
    report["shape"] = applied
    report["deform"] = displacement(src_co, coords())
    report["seam"] = head_shapes.seam_report(sets, src_co, coords())

out_path = os.path.join(OUT_DIR, os.path.basename(SRC))
coll = collection_name()
report["collection"] = coll
r = bpy.ops.re_mesh.exportfile(
    filepath=out_path, filename_ext=EXT, targetCollection=coll, selectedOnly=False,
    exportAllLODs=True, exportBlendShapes=True, rotate90=True,
    preserveBoneMatrices=True, autoSolveRepeatedUVs=True, preserveSharpEdges=True)
report["export_result"] = list(r)
report["export_ok"] = os.path.exists(out_path)
report["out_bytes"] = os.path.getsize(out_path) if report["export_ok"] else 0

if report["export_ok"]:
    pre_export_co = coords()
    imp(out_path)
    report["after"] = stats()
    # what the FILE round trip itself cost, independent of any deformation
    report["roundtrip_error"] = displacement(pre_export_co, coords())

print("<<<REPORT>>>")
print(json.dumps(report, indent=1))
print("<<<END>>>")
