"""skin_detail_build.py -- HD hands tier 1a: pores at any distance through the skin material's DETAIL slot.

Claire's skin material (pl3000_Skin_Mat, Record_Player.mmtr) ships with a detail-normal slot pointed at
MasterMaterial/Textures/NullDetail.tex and Detail_Normal_Intensity = 0 (measured 2026-09-06). This script:
  1. converts Visceral's own tiling skin-detail normal (PNG from tools/blender/make_skin_detail.py) to a
     BC7 .tex.34 with mipmaps, using the RE Mesh Editor's converter (NSACloud; DirectXTex inside);
  2. reads the player's own pl3000.mdf2.21 (pulled from the paks, or given), points the skin material's
     DetailMap at our texture and sets Detail_UVScale / Detail_Normal_Intensity / Detail_AO_Intensity;
  3. writes both under <out>/natives/stm/... ready to copy into the game folder (loose loader on).
The patched MDF is the player's own file with three numbers and one path changed; like the aim-walk splice,
the SCRIPT ships, the output does not. Our texture is ours and may ship.

Usage:  py skin_detail_build.py --png <visceral_skin_detail_NRM.png> --out <folder>
            [--game-dir <RE2>] | [--mdf <pl3000.mdf2.21>]   [--character pl3000]
            [--uv-scale 6] [--normal 1.0] [--ao 0.0] [--tex-name visceral_skin_detail_NRM]
Requires: RE Mesh Editor at REMESH_DIR (below) -- its converter DLL and MDF reader are used in place.
"""
import argparse, ctypes, os, shutil, subprocess, sys

REMESH_DIR = r"D:\RE2 REFramework builds\tools\RE-Mesh-Editor"
TEX_VERSION = 34
MDF_PATH = "natives/stm/sectionroot/character/player/%s/%s/%s.mdf2.21"

ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("--png", required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--game-dir")
ap.add_argument("--mdf")
ap.add_argument("--character", default="pl3000", help="pl3000 = Claire (default)")
ap.add_argument("--material", default=None, help="material name to patch (default <character>_Skin_Mat)")
ap.add_argument("--uv-scale", type=float, default=6.0)
ap.add_argument("--normal", type=float, default=1.0)
ap.add_argument("--ao", type=float, default=0.0)
ap.add_argument("--tex-name", default="visceral_skin_detail_NRM")
ap.add_argument("--remesh-dir", default=REMESH_DIR)
a = ap.parse_args()

sys.path.insert(0, a.remesh_dir)
ctypes.windll.ole32.CoInitializeEx(None, 0)          # DirectXTex reads PNG through WIC: COM must be up
from modules.tex import re_tex_utils as TU            # noqa: E402
from modules.mdf import file_re_mdf as M              # noqa: E402

# ---- 1. PNG -> DDS (BC7, mips) -> .tex.34 ---------------------------------------------------------
tex_dir = os.path.join(a.out, "natives", "stm", "visceral")
os.makedirs(tex_dir, exist_ok=True)
work = os.path.join(a.out, "_work"); os.makedirs(work, exist_ok=True)
png_copy = os.path.join(work, a.tex_name + ".png")
shutil.copyfile(a.png, png_copy)
TU.ImageListToDDS([(png_copy, "BC7_UNORM")], outDir=work, generateMipMaps=True)
dds = os.path.join(work, a.tex_name + ".dds")
if not os.path.exists(dds):
    raise SystemExit("DDS conversion produced nothing (expected %s)" % dds)
tex_out = os.path.join(tex_dir, a.tex_name + ".tex.%d" % TEX_VERSION)
TU.DDSToTex([dds], TEX_VERSION, tex_out)
hdr = open(tex_out, "rb").read(0x20)
import struct
w, h = struct.unpack_from("<HH", hdr, 8); fmt = struct.unpack_from("<I", hdr, 0x10)[0]
print("tex: %s  %dx%d dxgi=%d (98 = BC7_UNORM, same as the game's own detail normals) %d bytes" % (tex_out, w, h, fmt, os.path.getsize(tex_out)))
if fmt != 98: print("WARNING: format is not BC7_UNORM")

# ---- 2. the MDF ----------------------------------------------------------------------------------
rel = MDF_PATH % (a.character, a.character, a.character)
if a.mdf is None:
    if not a.game_dir: ap.error("give --game-dir or --mdf")
    pulled = os.path.join(a.out, "_originals")
    r = subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "pak_pull.py"), a.game_dir, pulled, rel],
                       capture_output=True, text=True)
    sys.stdout.write(r.stdout)
    if "MISS" in r.stdout or r.returncode != 0: raise SystemExit("pak pull failed" + r.stderr)
    a.mdf = os.path.join(pulled, rel.replace("/", os.sep))
mdf = M.readMDF(a.mdf)
mat_name = a.material or "%s_Skin_Mat" % a.character
mats = [m for m in mdf.materialList if m.materialName == mat_name]
if not mats: raise SystemExit("material %s not in %s (have %s)" % (mat_name, a.mdf, [m.materialName for m in mdf.materialList]))
mat = mats[0]
tex_path = "visceral/%s.tex" % a.tex_name           # MDF paths are natives/stm-relative, with .tex and no version number
changed = []
for tb in mat.textureList:
    if tb.textureType == "DetailMap":
        changed.append("DetailMap: %s -> %s" % (tb.texturePath, tex_path)); tb.texturePath = tex_path
for pr in mat.propertyList:
    if pr.propName == "Detail_UVScale": changed.append("Detail_UVScale: %s -> %s" % (pr.propValue, [a.uv_scale])); pr.propValue = [a.uv_scale]
    if pr.propName == "Detail_Normal_Intensity": changed.append("Detail_Normal_Intensity: %s -> %s" % (pr.propValue, [a.normal])); pr.propValue = [a.normal]
    if pr.propName == "Detail_AO_Intensity": changed.append("Detail_AO_Intensity: %s -> %s" % (pr.propValue, [a.ao])); pr.propValue = [a.ao]
if len(changed) != 4: raise SystemExit("expected to change 4 things on %s, changed %d: %s" % (mat_name, len(changed), changed))
for c in changed: print("  " + c)
mdf_out = os.path.join(a.out, rel.replace("/", os.sep))
os.makedirs(os.path.dirname(mdf_out), exist_ok=True)
M.writeMDF(mdf, mdf_out)

# ---- 3. verify by re-reading ----------------------------------------------------------------------
back = M.readMDF(mdf_out)
bm = [m for m in back.materialList if m.materialName == mat_name][0]
det = [tb.texturePath for tb in bm.textureList if tb.textureType == "DetailMap"][0]
vals = {pr.propName: pr.propValue for pr in bm.propertyList if pr.propName.startswith("Detail_")}
others = [(m.materialName, [tb.texturePath for tb in m.textureList]) for m in back.materialList if m.materialName != mat_name]
orig = M.readMDF(a.mdf)
same_others = all([tb.texturePath for tb in om.textureList] == [tb.texturePath for tb in bm2.textureList] and
                  [pr.propValue for pr in om.propertyList] == [pr.propValue for pr in bm2.propertyList]
                  for om, bm2 in zip(orig.materialList, back.materialList) if om.materialName != mat_name)
print("verify: DetailMap=%s  %s  other materials untouched=%s  sizes %d -> %d bytes" % (det, vals, same_others, os.path.getsize(a.mdf), os.path.getsize(mdf_out)))
if det != tex_path or vals.get("Detail_UVScale") != [a.uv_scale] or not same_others:
    raise SystemExit("VERIFY FAIL")
print("wrote", mdf_out)
print("install: copy <out>/natives/stm/visceral/ and <out>/%s into the game folder (loose loader on)" % os.path.dirname(rel))
