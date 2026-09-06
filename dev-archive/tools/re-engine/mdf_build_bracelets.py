"""Build Visceral's own material file for the forearm bracelets: visceral_bracelets.mdf2.21.

  py mdf_build_bracelets.py --template <pl1000.mdf2.21> --out <folder>

Starts from one of Claire's own Record_Player.mmtr materials (the holster: the plainest opaque one) so every
record-system slot the shader expects is present and points where the game's own materials point, then makes FOUR
materials from it with our texture paths and a BaseColor tint each. The mesh names its materials the same way:

  visceral_bracelet_leather         brown         right cuff
  visceral_bracelet_leather_red     dark red      left, lower band          (Tefa, 2026-09-06)
  visceral_bracelet_leather_purple  red-purple    left, upper band, nearest the jacket
  visceral_bracelet_metal           brushed steel plates, rivets, rings, clasp

Texture paths in an MDF are natives/stm-relative, ".tex" with no version number (the loose-file loader resolves
visceral/x.tex to natives/stm/visceral/x.tex.34 -- proven by the skin-detail tile, 2026-09-06). The template's material
name hashes are recomputed by the writer from the new names. Nothing of the game is copied out: the template is read,
the output holds only our four materials.
"""
import argparse, copy, os, sys
REMESH_DIR = r"D:\RE2 REFramework builds\tools\RE-Mesh-Editor"
ap = argparse.ArgumentParser()
ap.add_argument("--template", required=True); ap.add_argument("--out", required=True)
ap.add_argument("--remesh-dir", default=REMESH_DIR)
ap.add_argument("--name", default="visceral_bracelets")
a = ap.parse_args()
sys.path.insert(0, a.remesh_dir)
from modules.mdf import file_re_mdf as M   # noqa: E402

mdf = M.readMDF(a.template)
tmpl = [m for m in mdf.materialList if m.materialName == "pl1000_Holster_Mat"]
if not tmpl: raise SystemExit("template has no pl1000_Holster_Mat: " + str([m.materialName for m in mdf.materialList]))
tmpl = tmpl[0]

def make(name, tex_base, base_color, roughness=None, metallic=None):
    m = copy.deepcopy(tmpl)
    m.materialName = name
    for t in m.textureList:
        if t.textureType == "BaseMetalMap": t.texturePath = "visceral/%s_ALBM.tex" % tex_base
        elif t.textureType == "NormalRoughnessMap": t.texturePath = "visceral/%s_NRMR.tex" % tex_base
        elif t.textureType == "AlphaTranslucentOcclusionSSSMap": t.texturePath = "visceral/%s_ATOS.tex" % tex_base
        elif t.textureType == "Rec_Rain_WetMask": t.texturePath = "systems/rendering/NullBlack.tex"   # no wet mask of our own
    for p in m.propertyList:
        if p.propName == "BaseColor": p.propValue = list(base_color) + [1.0]
        elif p.propName == "Roughness" and roughness is not None: p.propValue = [roughness]
        elif p.propName == "Metallic" and metallic is not None: p.propValue = [metallic]
        elif p.propName == "Rec_Cloth": p.propValue = [0.0]          # leather and steel are not cloth to the record system
    return m

mats = [
    make("visceral_bracelet_leather",        "visceral_bracelet_leather", (0.84, 0.52, 0.32)),   # grey tile x this = brown leather
    make("visceral_bracelet_leather_red",    "visceral_bracelet_leather", (0.70, 0.12, 0.14)),   # dark red
    make("visceral_bracelet_leather_purple", "visceral_bracelet_leather", (0.72, 0.16, 0.52)),   # between red and purple
    make("visceral_bracelet_metal",          "visceral_bracelet_metal",   (1.00, 1.00, 1.00)),
]
# The writer mis-places the string table when the material COUNT changes (probed 2026-09-06: with one material kept, the
# names landed 14 KB past where the entries said; with all twelve, every rename and path change round-trips -- which is
# also how skin_detail_build.py has always used it). So keep the template's count: overwrite four slots, leave the rest.
victims = ("pl1000_Boots_Mat", "pl1000_Holster_Mat", "pl1000_Sling_Mat", "pl1000_Radio_Mat")
for victim, ours in zip(victims, mats):
    i = [k for k, m in enumerate(mdf.materialList) if m.materialName == victim][0]
    mdf.materialList[i] = ours
os.makedirs(a.out, exist_ok=True)
out = os.path.join(a.out, a.name + ".mdf2.21")
M.writeMDF(mdf, out)
back = M.readMDF(out)
print("wrote %s: %d materials (%d ours + the template's unused rest), %d bytes" % (out, len(back.materialList), len(mats), os.path.getsize(out)))
for m in back.materialList:
    if not m.materialName.startswith("visceral_"): continue
    texs = {t.textureType: t.texturePath for t in m.textureList}
    bc = [p.propValue for p in m.propertyList if p.propName == "BaseColor"][0]
    print("   %-34s BaseColor=%s ALBM=%s" % (m.materialName, [round(v, 2) for v in bc], texs.get("BaseMetalMap")))
