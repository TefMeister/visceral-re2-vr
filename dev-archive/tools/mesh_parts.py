"""mesh_parts.py <mesh file>: the parts (viscon groups) of an RE Engine mesh, LOD 0: per part and submesh the material,
vertex count, bounding box and the bones that carry most of its weight. Uses RE Mesh Editor's reader outside Blender."""
import os, sys, importlib
import numpy as np
# RE Mesh Editor's add-on package imports bpy at its top, so its modules/ folder is copied into a plain package in
# a temp folder and imported from there (2026-10-28). Set RE_MESH_EDITOR to the add-on folder if it is elsewhere.
import shutil, tempfile
ADDON = os.environ.get("RE_MESH_EDITOR") or os.path.join(os.path.expanduser("~"), "AppData", "Roaming", "Blender Foundation",
                                                         "Blender", "5.2", "scripts", "addons", "RE-Mesh-Editor")
TMP = os.path.join(tempfile.gettempdir(), "re_mesh_reader_pkg")
if not os.path.isdir(os.path.join(TMP, "remesh", "modules")):
    os.makedirs(os.path.join(TMP, "remesh"), exist_ok=True)
    shutil.copytree(os.path.join(ADDON, "modules"), os.path.join(TMP, "remesh", "modules"), dirs_exist_ok=True)
    open(os.path.join(TMP, "remesh", "__init__.py"), "a").close()
sys.path.insert(0, TMP)
fm = importlib.import_module("remesh.modules.mesh.file_re_mesh")
pm = importlib.import_module("remesh.modules.mesh.re_mesh_parse")
path = sys.argv[1]
re_mesh = fm.readREMesh(path)
p = pm.ParsedREMesh(); p.ParseREMesh(re_mesh)
print("file:", os.path.basename(path))
print("materials:", p.materialNameList)
bones = p.skeleton.boneList if p.skeleton else []
wb = p.skeleton.weightedBones if p.skeleton else []
print("bones:", len(bones), "weighted:", len(wb))
names = [b.boneName for b in bones]
def wname(i):
    try:
        j = wb[i]
        return names[j] if isinstance(j, int) else getattr(j, "boneName", str(j))
    except Exception:
        return names[i] if i < len(names) else f"b{i}"
lod = p.mainMeshLODList[0]
print("LOD0 parts:", len(lod.visconGroupList))
for g in lod.visconGroupList:
    for si, sm in enumerate(g.subMeshList):
        if si == 0 and g is lod.visconGroupList[0]:
            print("   submesh fields:", [k for k in vars(sm).keys()])
        pos = np.asarray(sm.vertexPosList, dtype=np.float32)
        mat = p.materialNameList[sm.materialIndex] if hasattr(sm, "materialIndex") else "?"
        tot = {}
        wl = getattr(sm, "weightList", None) or getattr(sm, "boneWeightsList", None)
        il = getattr(sm, "boneIndicesList", None)
        if il is not None and wl is not None:
            for idx, w in zip(il, wl):
                for b, ww in zip(idx, w):
                    if ww > 0: tot[b] = tot.get(b, 0.0) + ww
        top = sorted(tot.items(), key=lambda kv: -kv[1])[:6]
        print(f"part {g.visconGroupNum:2d} sub {si}: {mat:22s} verts {len(pos):5d} y {pos[:,1].min():6.3f}..{pos[:,1].max():6.3f} z {pos[:,2].min():6.3f}..{pos[:,2].max():6.3f}  bones: " +
              ", ".join(f"{wname(b)} {w/len(pos):.2f}" for b, w in top))
