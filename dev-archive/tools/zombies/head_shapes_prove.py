"""head_shapes_prove.py -- prove the head-shape recipes numerically, without looking at anything.

  py head_shapes_prove.py --game "<RE2 folder>" --work <scratch> [--face Face00] [--shape broad] [--noop]

For each recipe it imports the source head with RE Mesh Editor, deforms it, exports it back to the
RE2 RT mesh version, re-imports the export and prints one row:

  max move        largest distance any vertex travelled  (is the change big enough to read?)
  seam ring       largest distance any vertex ON THE BODY JOIN travelled   (must be 0.000000 mm)
  neck            largest distance any vertex at or below `neck_1` travelled (must be 0.000000 mm)
  verts/weights/uv/bones  source vs re-imported export -- all must be unchanged

Nothing is deployed and nothing is launched; everything lands under --work. The heads are pulled
from the player's own archive with `make_faces.py`'s helpers, so no game content is stored.
"""
import argparse, json, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_faces as MF                    # noqa: E402  (pak_tables / pull / BLENDER paths)
from head_shapes import SHAPES             # noqa: E402

DRIVER = os.path.join(MF.BLENDER_TOOLS, "head_roundtrip.py")
USABLE = ["Face%02d" % n for n in range(8)]      # only these own a mesh -- dossier 7e


def head_mesh(ns, tables, work, face):
    base = "%s/%s/em0050_%s" % (MF.FACE_DIR, face, face)
    return MF.pull(ns, tables, work, base + ".mesh.2109108288")


def run(src, out_dir, shape):
    cmd = [MF.BLENDER, "-b", "--python", DRIVER, "--", src, out_dir]
    if shape is not None:
        cmd.append(json.dumps(shape))
    r = subprocess.run(cmd, cwd=MF.REMESH_PARENT, capture_output=True, text=True)
    if "<<<REPORT>>>" not in r.stdout:
        raise SystemExit("blender produced no report:\n%s\n%s" % (r.stdout[-3000:], r.stderr[-2000:]))
    return json.loads(r.stdout.split("<<<REPORT>>>")[1].split("<<<END>>>")[0])


def structure_identical(rep):
    b, a = rep["before"], rep.get("after")
    if not a or len(a["objects"]) != len(b["objects"]):
        return False
    if b["armature"]["bone_names"] != a["armature"]["bone_names"]:
        return False
    for x, y in zip(b["objects"], a["objects"]):
        if (x["name"], x["verts"], x["faces"], x["weights"], x["vgroups_used"],
                x["uv_layers"], x["uv_hash"], x["shapekeys"]) != \
           (y["name"], y["verts"], y["faces"], y["weights"], y["vgroups_used"],
                y["uv_layers"], y["uv_hash"], y["shapekeys"]):
            return False
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--face", default="Face00", help="source head, or 'all' for Face00..Face07")
    ap.add_argument("--shape", default=None, help="one recipe name; default is every recipe")
    ap.add_argument("--noop", action="store_true", help="only the no-change round trip, per head")
    a = ap.parse_args()

    ns, tables = MF.pak_tables(a.game)
    faces = USABLE if a.face == "all" else [a.face]
    names = [None] if a.noop else ([a.shape] if a.shape else sorted(SHAPES))

    hdr = ("%-8s %-12s %8s %8s %11s %11s %8s %7s %7s %6s  %s" %
           ("head", "shape", "maxmove", "meanmov", "seam ring", "neck", "moved", "verts", "weights", "bones", "same"))
    print(hdr)
    print("-" * len(hdr))
    bad = 0
    for face in faces:
        src = head_mesh(ns, tables, a.work, face)
        for nm in names:
            spec = None if nm is None else {"name": nm}
            out = os.path.join(a.work, "shapes", face, nm or "_noop")
            rep = run(src, out, spec)
            same = structure_identical(rep)
            b = rep["before"]
            verts = sum(o["verts"] for o in b["objects"])
            wts = sum(o["weights"] for o in b["objects"])
            if nm is None:
                mx = max(v.get("max_mm", 0) for v in rep["roundtrip_error"].values())
                mean = max(v.get("mean_mm", 0) for v in rep["roundtrip_error"].values())
                ring = neck = 0.0
                moved = 0
            else:
                mx = max(v.get("max_mm", 0) for v in rep["deform"].values())
                mean = sum(v.get("mean_mm", 0) for v in rep["deform"].values()) / len(rep["deform"])
                ring = rep["seam"]["ring"]["max_move_mm"]
                neck = rep["seam"]["neck"]["max_move_mm"]
                moved = rep["shape"]["moved_verts"]
            ok = same and ring == 0.0 and neck == 0.0 and rep["export_ok"]
            bad += 0 if ok else 1
            print("%-8s %-12s %8.3f %8.3f %11.6f %11.6f %8d %7d %7d %6d  %s" %
                  (face, nm or "(no change)", mx, mean, ring, neck, moved, verts, wts,
                   b["armature"]["bones"], "yes" if same else "NO"))
    print("\n%d row(s) failed" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
