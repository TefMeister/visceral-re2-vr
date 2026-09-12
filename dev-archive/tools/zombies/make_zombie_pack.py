"""make_zombie_pack.py -- build the ZOMBIE VARIETY loose-file overlay for RE2 (step 1: the proof pack).

What it produces (all under <out>/natives/STM/...):
  UserData/Character/Enemy/em0000/Montage/em0000PartsContainer.user.2   +FACE10 and +FACE08 in the face pool
  UserData/Character/Enemy/em0000/Montage/em0000MontageTableData.user.2  every everyday male outfit re-dealt over
                                                                          ALL usable faces, evenly, adjacent IDs differing
  UserData/Character/Enemy/em0000/Montage/em0000CombinationRule.user.2   FACE08/10/11 added to the Body00_00_00 group
  Character/Enemy/em0000/Face/Face08/em0050_Face08.{pfb.17,mesh.2109108288,mdf2.21}  a NEW face in an empty slot:
      Face00's prefab (same-length rename), Face00's mesh (copy), Face00's material re-pointed at Face08 textures
  Character/Enemy/em0000/Face/Face08/em0050_Face08_{ALBM,NRMR,ATOS}.tex.34   ALBM re-tinted (grey-green skin, grey hair),
      NRMR/ATOS copied from Face00

Everything is derived from the player's own game archive at build time; nothing here ships game content.
The montage writer (montage_rsz.py) is byte-faithful on all nine shipped tables `[verified-numerically 2026-09-12]`.

  py make_zombie_pack.py --game "<RE2 folder>" --work <scratch> --out <overlay folder> [--deploy] [--undeploy]

--deploy copies the overlay into <game>/natives/... and writes <game>/visceral_zombie_pack.manifest (one path per line);
--undeploy deletes exactly the files in that manifest. Existing files at those paths are backed up as <name>.pre-zombie.
"""
import argparse, glob, os, shutil, struct, subprocess, sys, json
from PIL import Image
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import montage_rsz as R                                                   # noqa: E402

RE_ENGINE_TOOLS = os.path.normpath(os.path.join(HERE, "..", "re-engine"))
BLENDER_TOOLS = os.path.normpath(os.path.join(HERE, "..", "blender"))
BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
REMESH_PARENT = r"D:\RE2 REFramework builds\tools"

MONTAGE_DIR = "natives/STM/SectionRoot/UserData/Character/Enemy/em0000/Montage"
FACE_DIR = "natives/STM/SectionRoot/Character/Enemy/em0000/Face"
NEW_FACE = "Face08"          # empty slot in EM0000_MONTAGE_PARTS_FACE (08, 09, 12, 13, 74, 75, 76 are all free)
SOURCE_FACE = "Face00"
EVERYDAY_FACES = ["FACE00", "FACE01", "FACE02", "FACE03", "FACE04", "FACE05", "FACE06", "FACE07",
                  "FACE08", "FACE10", "FACE11", "FACE14"]
# the shipped CombinationRule pairs Face03 with the second body skin, everything else with the first
BODY_FOR_FACE = {"FACE03": "BODY00_00_01"}
DEFAULT_BODY = "BODY00_00_00"
SPECIAL_IDS = {"ID070", "ID071", "ID072", "ID073", "ID270", "ID271", "ID900", "ID901", "ID902", "ID903"}


# ---------------------------------------------------------------- pak access (pak_pull.py, imported without its main())
def load_pak_tools():
    src = open(os.path.join(RE_ENGINE_TOOLS, "pak_pull.py"), encoding="utf-8").read().replace("\nmain()\n", "\n")
    ns = {}
    exec(src, ns)
    return ns


def pull_files(game, work, paths):
    ns = load_pak_tools()
    paks = sorted(glob.glob(os.path.join(game, "re_chunk_000.pak.patch_*.pak")), reverse=True) + [os.path.join(game, "re_chunk_000.pak")]
    tables = [(p, ns["load_table"](p)) for p in paks if os.path.getsize(p) >= 16]
    out = {}
    for path in paths:
        key = (ns["h32"](path.lower()), ns["h32"](path.upper()))
        dst = os.path.join(work, "pulled", path.replace("/", os.sep))
        if os.path.exists(dst):
            out[path] = dst
            continue
        for p, t in tables:
            if key in t:
                ns["pull"](p, t[key], dst)
                out[path] = dst
                break
        else:
            raise SystemExit("not in any pak: " + path)
    return out


# ---------------------------------------------------------------- table edits
def add_faces_to_container(doc, entries):
    """entries: list of (KeyName, prefab path without extension version). Inserted before the root."""
    root_id = doc["objects"][0]
    pre = R.template(doc, "PrefabObj")
    ent = R.template(doc, "PartsPrefabData")
    new = []
    for key, path in entries:
        a = dict(pre); a.update({"_flag": 0, "Path": path})
        b = dict(ent); b.update({"KeyName": key, "Prefab": 0})   # Prefab id patched below
        new += [a, b]
    at = R.insert_instances(doc, root_id, new)
    root = doc["instances"][doc["objects"][0]]
    for i in range(0, len(new), 2):
        doc["instances"][at + i + 1]["Prefab"] = at + i
        root["FacePrefabs"].append(at + i + 1)
    return [k for k, _ in entries]


def redeal_faces(doc, faces):
    """Re-deal every everyday outfit's face over `faces`, evenly, so consecutive IDs differ.
    Special / unique outfits are left alone. Body skin follows the shipped face<->body rule."""
    rows = [i for i in doc["instances"][1:] if i["_type"] == "MontageData" and i["MontageID"] not in SPECIAL_IDS and i["FaceKeyName"]]
    rows.sort(key=lambda r: r["MontageID"])
    # deal in a stride that is coprime with len(faces) so neighbours never share a face
    n = len(faces)
    stride = 5 if n % 5 else 7
    before = {}
    for k, r in enumerate(rows):
        before[r["MontageID"]] = r["FaceKeyName"]
        f = faces[(k * stride) % n]
        r["FaceKeyName"] = f
        if r["BodyKeyName"].startswith("BODY00_00_"):
            r["BodyKeyName"] = BODY_FOR_FACE.get(f, DEFAULT_BODY)
    return rows, before


def add_to_rule(doc, rule_name, register_names):
    rules = [i for i in doc["instances"][1:] if i["_type"] == "CombinationRule"]
    rule = [r for r in rules if r["_RuleName"] == rule_name][0]
    rule_id = doc["instances"].index(rule)
    item = R.template(doc, "MontagePartsItem")
    new = []
    for nm in register_names:
        it = dict(item); it["RegisterName"] = nm
        new.append(it)
    at = R.insert_instances(doc, rule_id, new)
    rule["Combination"] += list(range(at, at + len(new)))


# ---------------------------------------------------------------- face assets
def u16(s):
    return s.encode("utf-16-le")


def make_face_assets(pulled, out_face_dir, work, tint):
    os.makedirs(out_face_dir, exist_ok=True)
    src = lambda ext: pulled["%s/%s/em0050_%s.%s" % (FACE_DIR, SOURCE_FACE, SOURCE_FACE, ext)]
    # prefab: Face00 and Face10 prefabs differ ONLY in the name, so a same-length rename is the whole prefab
    pfb = open(src("pfb.17"), "rb").read()
    assert pfb.count(u16(SOURCE_FACE)) > 0
    open(os.path.join(out_face_dir, "em0050_%s.pfb.17" % NEW_FACE), "wb").write(pfb.replace(u16(SOURCE_FACE), u16(NEW_FACE)))
    # mesh: verbatim copy (materials are named "Face"/"Teeth"/"insidehead_Mat" inside, no paths)
    shutil.copyfile(src("mesh.2109108288"), os.path.join(out_face_dir, "em0050_%s.mesh.2109108288" % NEW_FACE))
    # material: re-point ONLY the three face textures; the inside-head textures stay on Face00's folder
    mdf = open(src("mdf2.21"), "rb").read()
    old = u16("%s/em0050_%s_" % (SOURCE_FACE, SOURCE_FACE))
    new = u16("%s/em0050_%s_" % (NEW_FACE, NEW_FACE))
    assert len(old) == len(new) and mdf.count(old) == 6, mdf.count(old)   # 3 slots x 2 materials (Face + a duplicate block)
    open(os.path.join(out_face_dir, "em0050_%s.mdf2.21" % NEW_FACE), "wb").write(mdf.replace(old, new))
    # textures
    for t in ("NRMR", "ATOS"):
        shutil.copyfile(pulled["%s/%s/em0050_%s_%s.tex.34" % (FACE_DIR, SOURCE_FACE, SOURCE_FACE, t)],
                        os.path.join(out_face_dir, "em0050_%s_%s.tex.34" % (NEW_FACE, t)))
    albm_png = decode_tex(pulled["%s/%s/em0050_%s_ALBM.tex.34" % (FACE_DIR, SOURCE_FACE, SOURCE_FACE)], os.path.join(work, "png"))
    tinted_dir = os.path.join(work, "tinted")
    os.makedirs(tinted_dir, exist_ok=True)
    tinted = os.path.join(tinted_dir, "em0050_%s_ALBM.png" % NEW_FACE)
    retint(albm_png, tinted, **tint)
    build_tex(tinted_dir, out_face_dir, "em0050_%s_ALBM" % NEW_FACE, "BC7_UNORM_SRGB")


def decode_tex(tex, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    png = os.path.join(out_dir, os.path.basename(tex).split(".tex")[0] + ".png")
    if not os.path.exists(png):
        subprocess.run([BLENDER, "-b", "--python", os.path.join(BLENDER_TOOLS, "tex2png.py"), "--", out_dir, tex],
                       cwd=REMESH_PARENT, check=True, capture_output=True)
    assert os.path.exists(png), png
    return png


def build_tex(png_dir, out_dir, name, fmt):
    r = subprocess.run([sys.executable, os.path.join(RE_ENGINE_TOOLS, "tex_build.py"), "--in", png_dir, "--out", out_dir, "%s=%s" % (name, fmt)],
                       capture_output=True, text=True)
    if r.returncode or not os.path.exists(os.path.join(out_dir, name + ".tex.34")):
        raise SystemExit("tex_build failed:\n" + r.stdout + r.stderr)


def retint(src_png, dst_png, hue_shift=0.0, sat=1.0, value=1.0, hair_grey=0.0):
    """A deliberately visible re-tint for the proof face: whole-skin hue/sat/value, plus hair greying
    (dark, low-saturation pixels lifted toward grey). Alpha untouched."""
    im = Image.open(src_png).convert("RGBA")
    a = np.asarray(im).astype(np.float32) / 255.0
    rgb, alpha = a[..., :3], a[..., 3:]
    hsv = np.asarray(Image.fromarray((rgb * 255).astype(np.uint8)).convert("HSV")).astype(np.float32) / 255.0
    h, s, v = hsv[..., 0], hsv[..., 1], hsv[..., 2]
    hair = (v < 0.22) & (s < 0.45)
    h = (h + hue_shift) % 1.0
    s = np.clip(s * sat, 0, 1)
    v = np.clip(v * value, 0, 1)
    if hair_grey:
        v = np.where(hair, np.clip(v + hair_grey, 0, 1), v)
        s = np.where(hair, s * 0.25, s)
    out = Image.fromarray((np.stack([h, s, v], -1) * 255).astype(np.uint8), "HSV").convert("RGB")
    out = np.concatenate([np.asarray(out).astype(np.float32) / 255.0, alpha], -1)
    Image.fromarray((out * 255).astype(np.uint8), "RGBA").save(dst_png)


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--deploy", action="store_true")
    ap.add_argument("--undeploy", action="store_true")
    # defaults chosen by eye on 2026-09-12: a sallow, paler, greener Face00 that still reads as skin.
    # (0.06 / 0.55 / 0.85 / 0.35 posterised the skin and haloed the hair - too much.)
    ap.add_argument("--hue", type=float, default=0.045, help="hue shift for the proof face (+ = toward green)")
    ap.add_argument("--sat", type=float, default=0.72)
    ap.add_argument("--value", type=float, default=0.92)
    ap.add_argument("--hair-grey", type=float, default=0.0, help="lift dark low-saturation pixels (hair) toward grey; catches skin shadows too, use sparingly")
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
                print("removed", line.strip())
        os.remove(manifest)
        return

    names = ["em0000PartsContainer", "em0000MontageTableData", "em0000CombinationRule"]
    paths = ["%s/%s.user.2" % (MONTAGE_DIR, n) for n in names]
    paths += ["%s/%s/em0050_%s.%s" % (FACE_DIR, SOURCE_FACE, SOURCE_FACE, e) for e in ("pfb.17", "mesh.2109108288", "mdf2.21")]
    paths += ["%s/%s/em0050_%s_%s.tex.34" % (FACE_DIR, SOURCE_FACE, SOURCE_FACE, t) for t in ("ALBM", "NRMR", "ATOS")]
    pulled = pull_files(a.game, a.work, paths)

    R.learn([pulled[p] for p in paths[:3]])
    docs = {n: R.parse(open(pulled["%s/%s.user.2" % (MONTAGE_DIR, n)], "rb").read()) for n in names}
    for n in names:   # the writer must reproduce the shipped bytes before it is trusted to change them
        assert R.build(docs[n]) == open(pulled["%s/%s.user.2" % (MONTAGE_DIR, n)], "rb").read(), n

    added = add_faces_to_container(docs["em0000PartsContainer"], [
        ("FACE10", "SectionRoot/Character/Enemy/em0000/Face/Face10/em0050_Face10.pfb"),
        ("FACE08", "SectionRoot/Character/Enemy/em0000/Face/%s/em0050_%s.pfb" % (NEW_FACE, NEW_FACE)),
    ])
    rows, before = redeal_faces(docs["em0000MontageTableData"], EVERYDAY_FACES)
    add_to_rule(docs["em0000CombinationRule"], "em0000_Body00_00_00", ["Face08", "Face10", "Face11"])

    out_montage = os.path.join(a.out, MONTAGE_DIR.replace("/", os.sep))
    os.makedirs(out_montage, exist_ok=True)
    written = []
    for n in names:
        p = os.path.join(out_montage, n + ".user.2")
        data = R.build(docs[n])
        open(p, "wb").write(data)
        assert R.build(R.parse(data)) == data   # our own output must round-trip too
        written.append(p)
    out_face = os.path.join(a.out, FACE_DIR.replace("/", os.sep), NEW_FACE)
    make_face_assets(pulled, out_face, a.work, dict(hue_shift=a.hue, sat=a.sat, value=a.value, hair_grey=a.hair_grey))
    shutil.rmtree(os.path.join(out_face, "_work"), ignore_errors=True)   # tex_build's scratch folder
    written += [os.path.join(out_face, f) for f in sorted(os.listdir(out_face)) if os.path.isfile(os.path.join(out_face, f))]

    # report
    from collections import Counter
    print("face pool now:", [docs["em0000PartsContainer"]["instances"][i]["KeyName"] for i in docs["em0000PartsContainer"]["instances"][docs["em0000PartsContainer"]["objects"][0]]["FacePrefabs"]])
    print("outfits re-dealt: %d  face counts: %s" % (len(rows), dict(sorted(Counter(r["FaceKeyName"] for r in rows).items()))))
    print("changed rows: %d of %d" % (sum(1 for r in rows if before[r["MontageID"]] != r["FaceKeyName"]), len(rows)))
    for p in written:
        print("  %8d  %s" % (os.path.getsize(p), os.path.relpath(p, a.out)))
    json.dump({r["MontageID"]: [before[r["MontageID"]], r["FaceKeyName"]] for r in rows}, open(os.path.join(a.out, "face-deal.json"), "w"), indent=0)

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
        print("DEPLOYED %d files into %s (manifest: %s)" % (len(lines), a.game, manifest))


if __name__ == "__main__":
    main()
