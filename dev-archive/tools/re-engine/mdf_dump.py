"""Dump an RE2 RT .mdf2 material file: every material, its shader (.mmtr), its texture slots and float params.
  py mdf_dump.py <file.mdf2.21> [--params]
Read-only. Uses RE Mesh Editor's MDF reader in place (NSACloud).
"""
import argparse, sys
REMESH_DIR = r"D:\RE2 REFramework builds\tools\RE-Mesh-Editor"
ap = argparse.ArgumentParser(); ap.add_argument("mdf"); ap.add_argument("--params", action="store_true"); ap.add_argument("--remesh-dir", default=REMESH_DIR)
a = ap.parse_args()
sys.path.insert(0, a.remesh_dir)
from modules.mdf import file_re_mdf as M   # noqa: E402
mdf = M.readMDF(a.mdf)
print("%s: %d materials" % (a.mdf, len(mdf.materialList)))
for m in mdf.materialList:
    print("\n== %s   shader=%s   shaderType=%s" % (m.materialName, getattr(m, "mmtrPath", "?"), m.shaderType))
    for t in m.textureList:
        print("   tex %-24s %s" % (t.textureType, t.texturePath))
    if a.params:
        for p in getattr(m, "propertyList", []):
            print("   prm %-28s %s" % (p.propName, getattr(p, "propValue", "?")))
