"""make_faces.py -- build the ZOMBIE VARIETY face pack: N new zombie faces + the tables that use them.

  py make_faces.py --game "<RE2 folder>" --work <scratch> --out <overlay> [--limit N] [--deploy] [--undeploy]

For every entry in face_roster.ROSTER it writes, under <out>/natives/STM/SectionRoot/:
  Character/Enemy/em0000/Face/<Key>/em0050_<Key>.pfb.17              source head's prefab, paths re-pointed
  Character/Enemy/em0000/Face/<Key>/em0050_<Key>.mesh.2109108288     source head's mesh, copied
  Character/Enemy/em0000/Face/<Key>/em0050_<Key>.mdf2.21             source material, its three face
                                                                     texture slots re-pointed at ours
  Character/Enemy/em0000/Face/<Key>/em0050_<Key>_{ALBM,NRMR,ATOS}.tex.34
and rewrites the three em0000 montage tables so the new faces are in the pool and the everyday
outfits are dealt evenly across ALL of them.

Everything is derived at build time from the player's own installed game. No game content is shipped.

⚠️ The material is edited with RE Mesh Editor's own MDF reader/writer rather than by patching bytes,
because a source head's albedo is not always in its own folder (Face06 ships none) and a same-length
string swap cannot add a path that was never there.
"""
import argparse, glob, os, shutil, subprocess, sys, json
from collections import Counter
from PIL import Image
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import montage_rsz as R                                                  # noqa: E402
from face_roster import ROSTER, all_face_keys, SHIPPED_EVERYDAY          # noqa: E402
import head_shapes                                                      # noqa: E402

RE_ENGINE_TOOLS = os.path.normpath(os.path.join(HERE, "..", "re-engine"))
BLENDER_TOOLS = os.path.normpath(os.path.join(HERE, "..", "blender"))
BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
REMESH_PARENT = r"D:\RE2 REFramework builds\tools"
REMESH_DIR = os.path.join(REMESH_PARENT, "RE-Mesh-Editor")

MONTAGE_DIR = "natives/STM/SectionRoot/UserData/Character/Enemy/em0000/Montage"
FACE_DIR = "natives/STM/SectionRoot/Character/Enemy/em0000/Face"
MDF_PREFIX = "SectionRoot/Character/Enemy/em0000/Face"
TABLES = ["em0000PartsContainer", "em0000MontageTableData", "em0000CombinationRule"]
SPECIAL_IDS = {"ID070", "ID071", "ID072", "ID073", "ID270", "ID271", "ID900", "ID901", "ID902", "ID903"}
BODY_FOR_FACE = {"FACE03": "BODY00_00_01"}
DEFAULT_BODY = "BODY00_00_00"
FACE_SLOTS = ("BaseMetalMap", "NormalRoughnessMap", "AlphaTranslucentOcclusionSSSMap")
SUFFIX = {"BaseMetalMap": "ALBM", "NormalRoughnessMap": "NRMR", "AlphaTranslucentOcclusionSSSMap": "ATOS"}

# printable ASCII as UTF-16LE, 6+ chars: how paths appear inside a .pfb
import re as _re_mod
UTF16_STR = _re_mod.compile(b"(?:[\x20-\x7e]\x00){6,}")


# ---------------------------------------------------------------- archive
def pak_tables(game):
    src = open(os.path.join(RE_ENGINE_TOOLS, "pak_pull.py"), encoding="utf-8").read().replace("\nmain()\n", "\n")
    ns = {}
    exec(src, ns)
    paks = sorted(glob.glob(os.path.join(game, "re_chunk_000.pak.patch_*.pak")), reverse=True)
    paks.append(os.path.join(game, "re_chunk_000.pak"))
    return ns, [(p, ns["load_table"](p)) for p in paks if os.path.getsize(p) >= 16]


def pull(ns, tables, work, path, required=True):
    dst = os.path.join(work, "pulled", path.replace("/", os.sep))
    if os.path.exists(dst):
        return dst
    key = (ns["h32"](path.lower()), ns["h32"](path.upper()))
    for p, t in tables:
        if key in t:
            ns["pull"](p, t[key], dst)
            return dst
    if required:
        raise SystemExit("not in any pak: " + path)
    return None


# ---------------------------------------------------------------- textures
def decode_tex(tex, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    png = os.path.join(out_dir, os.path.basename(tex).split(".tex")[0] + ".png")
    if not os.path.exists(png):
        subprocess.run([BLENDER, "-b", "--python", os.path.join(BLENDER_TOOLS, "tex2png.py"), "--", out_dir, tex],
                       cwd=REMESH_PARENT, check=True, capture_output=True)
    if not os.path.exists(png):
        raise SystemExit("decode failed: " + tex)
    return png


def build_tex(png_dir, out_dir, name, fmt):
    r = subprocess.run([sys.executable, os.path.join(RE_ENGINE_TOOLS, "tex_build.py"),
                        "--in", png_dir, "--out", out_dir, "%s=%s" % (name, fmt)],
                       capture_output=True, text=True)
    made = os.path.join(out_dir, name + ".tex.34")
    if r.returncode or not os.path.exists(made):
        raise SystemExit("tex_build failed for %s:\n%s%s" % (name, r.stdout, r.stderr))
    shutil.rmtree(os.path.join(out_dir, "_work"), ignore_errors=True)
    return made


def retint(src_png, dst_png, hue=0.0, sat=1.0, value=1.0, grey=0.0, ruddy=0.0):
    """Tint a head's albedo. Hue/sat/value act on the whole skin; `grey` lifts dark low-saturation
    pixels (hair) toward grey; `ruddy` reddens mid-tones only, for a kill that is still flushed."""
    im = Image.open(src_png).convert("RGBA")
    a = np.asarray(im).astype(np.float32) / 255.0
    rgb, alpha = a[..., :3], a[..., 3:]
    hsv = np.asarray(Image.fromarray((rgb * 255).astype(np.uint8)).convert("HSV")).astype(np.float32) / 255.0
    h, s, v = hsv[..., 0], hsv[..., 1], hsv[..., 2]
    hair = (v < 0.22) & (s < 0.45)
    h = (h + hue) % 1.0
    s = np.clip(s * sat, 0, 1)
    v = np.clip(v * value, 0, 1)
    if grey:
        v = np.where(hair, np.clip(v + grey, 0, 1), v)
        s = np.where(hair, s * 0.25, s)
    if ruddy:
        mid = (v > 0.25) & (v < 0.85) & ~hair
        h = np.where(mid, (h * (1 - ruddy)) % 1.0, h)      # pull hue toward red
        s = np.where(mid, np.clip(s * (1 + ruddy), 0, 1), s)
    out = Image.fromarray((np.stack([h, s, v], -1) * 255).astype(np.uint8), "HSV").convert("RGB")
    out = np.concatenate([np.asarray(out).astype(np.float32) / 255.0, alpha], -1)
    Image.fromarray((out * 255).astype(np.uint8), "RGBA").save(dst_png)


# ---------------------------------------------------------------- head shape
def shaped_mesh(src_mesh, src_name, shape, work):
    """Return a path to `src_mesh` deformed by the named recipe, or the source itself for "stock".

    The deformation runs in headless Blender through RE Mesh Editor (`blender/head_roundtrip.py`).
    A no-op round trip was proven lossless on all eight usable heads 2026-09-12 — vertex count, every
    bone weight, both UV layers and the whole skeleton come back identical — and the recipes move
    nothing at or below the neck bone, so a head cannot detach from its body. Results are cached per
    (head, recipe) because one Blender launch per face would be wasteful when 25 faces share 8 heads.
    """
    if not shape or shape == "stock":
        return src_mesh
    out_dir = os.path.join(work, "shaped", "%s__%s" % (src_name, shape))
    made = os.path.join(out_dir, os.path.basename(src_mesh))
    if os.path.exists(made):
        return made
    spec = json.dumps(head_shapes.recipe(shape))
    r = subprocess.run([BLENDER, "-b", "--python", os.path.join(BLENDER_TOOLS, "head_roundtrip.py"),
                        "--", src_mesh, out_dir, spec],
                       cwd=REMESH_PARENT, capture_output=True, text=True)
    if not os.path.exists(made):
        raise SystemExit("shape %s on %s produced nothing:" % (shape, src_name) + r.stdout[-2000:] + r.stderr[-1000:])
    # The driver prints a JSON report. Check it against the REAL key names — an earlier version read keys
    # that do not exist, so every check quietly passed on a `None`. A safety gate that cannot fail is worse
    # than none, because it is believed.
    try:
        rep = json.loads(r.stdout.split("<<<REPORT>>>")[1].split("<<<END>>>")[0])
        bad = []
        before, after = rep.get("before", {}), rep.get("after", {})
        b_objs = {o.get("name"): o for o in before.get("objects", [])}
        a_objs = {o.get("name"): o for o in after.get("objects", [])}
        if set(b_objs) != set(a_objs):
            bad.append("object set changed")
        for nm, bo in b_objs.items():
            ao = a_objs.get(nm, {})
            for field, label in (("verts", "vertex count"), ("weights", "bone weights"),
                                 ("uv_hash", "UVs"), ("uv_layers", "UV layers")):
                if field in bo and bo.get(field) != ao.get(field):
                    bad.append("%s: %s changed" % (nm, label))
        if before.get("armature") != after.get("armature"):
            bad.append("armature changed")
        if not rep.get("export_ok"):
            bad.append("export reported failure")
        seam = rep.get("seam", {})
        worst_seam = max((seam.get(k, {}).get("max_move_mm", 0.0) or 0.0) for k in ("ring", "neck")) if seam else None
        if worst_seam is not None and worst_seam > 0.001:
            bad.append("seam moved %.4f mm" % worst_seam)
        moved = 0.0
        for v in (rep.get("deform") or {}).values():
            if isinstance(v, dict) and "max_mm" in v:
                moved = max(moved, float(v["max_mm"]))          # the driver already reports millimetres
        if moved < 1.0:
            bad.append("deformation moved almost nothing (%.3f mm) — the recipe did not take" % moved)
        if bad:
            raise SystemExit("shape %s on %s is unsafe: %s" % (shape, src_name, "; ".join(bad)))
        print("    shaped %-10s on %-8s max move %.1f mm, seam %.4f mm" %
              (shape, src_name, moved, worst_seam if worst_seam is not None else -1))
    except (IndexError, ValueError) as e:
        raise SystemExit("shape %s on %s: could not read the round-trip report (%s)" % (shape, src_name, e))
    return made


# ---------------------------------------------------------------- material
def load_mdf_module():
    sys.path.insert(0, REMESH_DIR)
    from modules.mdf import file_re_mdf as M
    return M


def pick_face_material(mdf):
    """Which material is the SKIN? Not simply material 0: Face01 lists 'Hair' first, and every head
    also carries 'Teeth' and 'insidehead_Mat'. Prefer a material whose name says Face, then one whose
    albedo path is a FaceNN texture that is not hair/teeth/inside-head, then material 0.
    `[measured 2026-09-12: Face00/02/03/04/05/06/07 name it 'Face', Face01 puts 'Hair' first]`"""
    def albedo(m):
        for t in m.textureList:
            if t.textureType == "BaseMetalMap":
                return t.texturePath or ""
        return ""
    bad = ("hair", "teeth", "inside", "eye")
    for m in mdf.materialList:
        if "face" in m.materialName.lower() and not any(b in m.materialName.lower() for b in bad):
            return m
    for m in mdf.materialList:
        a = albedo(m).lower()
        if "_albm" in a and not any(b in a for b in bad):
            return m
    return mdf.materialList[0]


def source_texture_paths(M, mdf_path):
    mdf = M.readMDF(mdf_path)
    mat = pick_face_material(mdf)
    out = {}
    for t in mat.textureList:
        if t.textureType in FACE_SLOTS:
            out[t.textureType] = t.texturePath
    return mdf, mat.materialName, out


def folder_of(key):
    """Shipped convention: container key FACE20, folder and file name Face20."""
    return key[0] + key[1:].lower()


def write_face_mdf(M, src_mdf_path, folder, dst):
    mdf = M.readMDF(src_mdf_path)
    mat = pick_face_material(mdf)
    for t in mat.textureList:
        if t.textureType in FACE_SLOTS:
            t.texturePath = "%s/%s/em0050_%s_%s.tex" % (MDF_PREFIX, folder, folder, SUFFIX[t.textureType])
    M.writeMDF(mdf, dst)
    back = M.readMDF(dst)
    got = {t.textureType: t.texturePath for t in pick_face_material(back).textureList if t.textureType in FACE_SLOTS}
    for slot in FACE_SLOTS:
        if slot in got and folder not in got[slot]:
            raise SystemExit("mdf re-point did not stick for %s %s" % (folder, slot))
    return dst


# ---------------------------------------------------------------- tables
def rebuild_tables(docs, new_keys, deal_pool):
    cont = docs["em0000PartsContainer"]
    root_id = cont["objects"][0]
    root0 = cont["instances"][root_id]
    have = {cont["instances"][i]["KeyName"] for i in root0["FacePrefabs"]}
    # Anything the deal draws from must be IN the base container. That includes shipped heads the
    # base table never used -- FACE10 exists on disk but only the AfterChapter2 container lists it,
    # so dealing it without this line would hand the picker a key it cannot resolve.
    add = [k for k in deal_pool if k not in have and k not in new_keys] + list(new_keys)
    pre, ent = R.template(cont, "PrefabObj"), R.template(cont, "PartsPrefabData")
    new = []
    for key in add:
        f = key[0] + key[1:].lower()
        a = dict(pre); a.update({"_flag": 0, "Path": "%s/%s/em0050_%s.pfb" % (MDF_PREFIX, f, f)})
        b = dict(ent); b.update({"KeyName": key, "Prefab": 0})
        new += [a, b]
    at = R.insert_instances(cont, root_id, new)
    root = cont["instances"][cont["objects"][0]]
    added_keys = list(add)
    for i in range(0, len(new), 2):
        cont["instances"][at + i + 1]["Prefab"] = at + i
        root["FacePrefabs"].append(at + i + 1)

    tbl = docs["em0000MontageTableData"]
    rows = [i for i in tbl["instances"][1:]
            if i["_type"] == "MontageData" and i["MontageID"] not in SPECIAL_IDS and i["FaceKeyName"]]
    rows.sort(key=lambda r: r["MontageID"])
    n = len(deal_pool)
    stride = next(s for s in (7, 5, 11, 13, 3) if n % s)
    before = {}
    for k, r in enumerate(rows):
        before[r["MontageID"]] = r["FaceKeyName"]
        f = deal_pool[(k * stride) % n]
        r["FaceKeyName"] = f
        if r["BodyKeyName"].startswith("BODY00_00_"):
            r["BodyKeyName"] = BODY_FOR_FACE.get(f, DEFAULT_BODY)

    rule = docs["em0000CombinationRule"]
    rules = [i for i in rule["instances"][1:] if i["_type"] == "CombinationRule"]
    target = [r for r in rules if r["_RuleName"] == "em0000_Body00_00_00"][0]
    rid = rule["instances"].index(target)
    item = R.template(rule, "MontagePartsItem")
    have = set()
    for i in target["Combination"]:
        have.add(rule["instances"][i]["RegisterName"])
    want = [folder_of(k) for k in deal_pool if k != "FACE03"]      # FACE20 -> Face20
    missing = [w for w in want if w not in have]
    if missing:
        items = []
        for nm in missing:
            it = dict(item); it["RegisterName"] = nm
            items.append(it)
        at2 = R.insert_instances(rule, rid, items)
        target["Combination"] += list(range(at2, at2 + len(items)))
    return rows, before, missing, added_keys


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--limit", type=int, default=None, help="build only the first N roster entries")
    ap.add_argument("--no-shapes", action="store_true", help="skip the mesh deformation — the control for a shapes-on/off comparison")
    ap.add_argument("--deploy", action="store_true")
    ap.add_argument("--undeploy", action="store_true")
    a = ap.parse_args()
    manifest = os.path.join(a.game, "visceral_zombie_pack.manifest")

    if a.undeploy:
        if not os.path.exists(manifest):
            raise SystemExit("no manifest at " + manifest)
        for line in open(manifest, encoding="utf-8"):
            p = os.path.join(a.game, line.strip())
            if os.path.exists(p):
                os.remove(p)
                if os.path.exists(p + ".pre-zombie"):
                    os.rename(p + ".pre-zombie", p)
        os.remove(manifest)
        print("undeployed")
        return

    roster = ROSTER if a.limit is None else ROSTER[:a.limit]
    ns, tables = pak_tables(a.game)
    M = load_mdf_module()

    # tables
    table_paths = {n: pull(ns, tables, a.work, "%s/%s.user.2" % (MONTAGE_DIR, n)) for n in TABLES}
    R.learn(list(table_paths.values()))
    docs = {n: R.parse(open(p, "rb").read()) for n, p in table_paths.items()}
    for n, p in table_paths.items():
        assert R.build(docs[n]) == open(p, "rb").read(), "writer is not byte-faithful on " + n

    # source heads
    srcs = sorted({s for _k, s, _t, _sh, _n in roster})
    src_files, src_albm_png = {}, {}
    for s in srcs:
        base = "%s/%s/em0050_%s" % (FACE_DIR, s, s)
        f = {"pfb.17": pull(ns, tables, a.work, base + ".pfb.17"),
             "mdf2.21": pull(ns, tables, a.work, base + ".mdf2.21")}
        # Read the mesh path out of the prefab rather than assuming em0050_<Face>.mesh: Face11 ships
        # no mesh of that name `[measured 2026-09-12]`.

        strs = [m.group(0).decode("utf-16-le") for m in
                UTF16_STR.finditer( open(f["pfb.17"], "rb").read())]
        meshes = [x for x in strs if x.endswith(".mesh")]
        if not meshes:
            raise SystemExit("no .mesh path inside %s's prefab" % s)
        f["mesh_rel"] = meshes[0]
        f["mesh.2109108288"] = pull(ns, tables, a.work, "natives/STM/" + meshes[0] + ".2109108288")
        _mdf, matname, texpaths = source_texture_paths(M, f["mdf2.21"])
        pulled_tex = {}
        for slot, rel in texpaths.items():
            pulled_tex[slot] = pull(ns, tables, a.work, "natives/STM/" + rel + ".34")
        f["tex"] = pulled_tex
        src_files[s] = f
        src_albm_png[s] = decode_tex(pulled_tex["BaseMetalMap"], os.path.join(a.work, "png"))
        print("source %-8s material '%s' albedo %s" % (s, matname, os.path.basename(texpaths["BaseMetalMap"])))

    # faces
    written = []
    tint_dir = os.path.join(a.work, "tint")
    os.makedirs(tint_dir, exist_ok=True)
    for key, src, tint, shape, note in roster:
        folder = folder_of(key)
        d = os.path.join(a.out, FACE_DIR.replace("/", os.sep), folder)
        os.makedirs(d, exist_ok=True)
        # The prefab is nothing but its own name: Face00's and Face10's are byte-identical after a
        # UTF-16 rename `[verified-numerically 2026-09-12]`.
        pfb = open(src_files[src]["pfb.17"], "rb").read()
        renamed = pfb.replace(src.encode("utf-16-le"), folder.encode("utf-16-le"))
        assert len(renamed) == len(pfb) and renamed != pfb, "prefab rename failed for " + key
        open(os.path.join(d, "em0050_%s.pfb.17" % folder), "wb").write(renamed)
        # name our copy exactly what the RENAMED prefab now asks for
        shaped = shaped_mesh(src_files[src]["mesh.2109108288"], src, None if a.no_shapes else shape, a.work)
        mesh_name = os.path.basename(src_files[src]["mesh_rel"].replace(src, folder)) + ".2109108288"
        mesh_dir = os.path.dirname(src_files[src]["mesh_rel"].replace(src, folder))
        if not mesh_dir.endswith("/" + folder):
            # the prefab points at a mesh in another head's folder; the rename left it there, so ours
            # is unreferenced -- put it where the prefab actually looks instead of guessing
            print("    note: %s's prefab points at %s, mesh left where it is" % (src, src_files[src]["mesh_rel"]))
        shutil.copyfile(shaped, os.path.join(d, mesh_name))
        write_face_mdf(M, src_files[src]["mdf2.21"], folder, os.path.join(d, "em0050_%s.mdf2.21" % folder))
        for slot in ("NormalRoughnessMap", "AlphaTranslucentOcclusionSSSMap"):
            if slot in src_files[src]["tex"]:
                shutil.copyfile(src_files[src]["tex"][slot], os.path.join(d, "em0050_%s_%s.tex.34" % (folder, SUFFIX[slot])))
        png = os.path.join(tint_dir, "em0050_%s_ALBM.png" % folder)
        retint(src_albm_png[src], png, **tint)
        build_tex(tint_dir, d, "em0050_%s_ALBM" % folder, "BC7_UNORM_SRGB")
        written += [os.path.join(d, f) for f in sorted(os.listdir(d)) if os.path.isfile(os.path.join(d, f))]
        print("  built %-8s from %-8s %-11s %s" % (key, src, shape, note))

    pool = SHIPPED_EVERYDAY + [k for k, _s, _t, _sh, _n in roster]
    rows, before, added_rules, added_keys = rebuild_tables(docs, [k for k, _s, _t, _sh, _n in roster], pool)

    out_m = os.path.join(a.out, MONTAGE_DIR.replace("/", os.sep))
    os.makedirs(out_m, exist_ok=True)
    for n in TABLES:
        data = R.build(docs[n])
        assert R.build(R.parse(data)) == data, "our own output does not round-trip: " + n
        p = os.path.join(out_m, n + ".user.2")
        open(p, "wb").write(data)
        written.append(p)

    c = Counter(r["FaceKeyName"] for r in rows)
    print("\nface pool now %d; %d outfits dealt over %d faces (%d changed)" %
          (len(pool), len(rows), len(c), sum(1 for r in rows if before[r["MontageID"]] != r["FaceKeyName"])))
    print("per-face use:", dict(sorted(c.items())))
    print("container keys added: %d %s" % (len(added_keys), added_keys))
    print("combination-rule names added:", added_rules or "none")
    print("files: %d, %.1f MB" % (len(written), sum(os.path.getsize(p) for p in written) / 1e6))
    json.dump({r["MontageID"]: [before[r["MontageID"]], r["FaceKeyName"]] for r in rows},
              open(os.path.join(a.out, "face-deal.json"), "w"), indent=0)

    if a.deploy:
        lines = []
        for p in written:
            rel = os.path.relpath(p, a.out)
            dst = os.path.join(a.game, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            if os.path.exists(dst) and not os.path.exists(dst + ".pre-zombie"):
                os.rename(dst, dst + ".pre-zombie")
            shutil.copyfile(p, dst)
            lines.append(rel.replace(os.sep, "/"))
        open(manifest, "w", encoding="utf-8").write("\n".join(lines) + "\n")
        print("DEPLOYED %d files (manifest: %s)" % (len(lines), manifest))


if __name__ == "__main__":
    main()
