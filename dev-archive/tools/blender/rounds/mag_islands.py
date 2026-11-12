"""islands.py <mesh>: for LOD0, the joint names, then per part the vertices that follow the magazine joint (_04) split into
connected islands (by faces): count, box, material."""
import os, sys, importlib, shutil, tempfile
import numpy as np
ADDON = os.path.join(os.path.expanduser("~"), "AppData", "Roaming", "Blender Foundation", "Blender", "5.2", "scripts", "addons", "RE-Mesh-Editor")
TMP = os.path.join(tempfile.gettempdir(), "re_mesh_reader_pkg")
if not os.path.isfile(os.path.join(TMP, "remesh", "modules", "mesh", "file_re_mesh.py")):
    shutil.copytree(os.path.join(ADDON, "modules"), os.path.join(TMP, "remesh", "modules"), dirs_exist_ok=True)
    open(os.path.join(TMP, "remesh", "__init__.py"), "a").close()
sys.path.insert(0, TMP)
fm = importlib.import_module("remesh.modules.mesh.file_re_mesh")
pm = importlib.import_module("remesh.modules.mesh.re_mesh_parse")
p = pm.ParsedREMesh(); p.ParseREMesh(fm.readREMesh(sys.argv[1]))
names = [b.boneName for b in p.skeleton.boneList]
print(os.path.basename(sys.argv[1]), "joints:", names)
lod = p.mainMeshLODList[0]
sm0 = lod.visconGroupList[0].subMeshList[0]
if "-v" in sys.argv: print({k: type(v).__name__ for k, v in vars(sm0).items()})
for g in lod.visconGroupList:
    for si, sm in enumerate(g.subMeshList):
        pos = np.asarray(sm.vertexPosList, dtype=np.float32)
        idx = np.asarray(sm.weightIndicesList); w = np.asarray(sm.weightList, dtype=np.float32)
        # main joint per vertex (weighted bone list -> skeleton index)
        wb = p.skeleton.weightedBones if hasattr(p.skeleton, "weightedBones") else None
        main = idx[np.arange(len(idx)), w.argmax(1)]
        mainName = np.array([names[wb[m]] if wb is not None and not isinstance(wb[m], str) else (wb[m] if wb is not None else names[m]) for m in main])
        faces = np.asarray(sm.faceList).reshape(-1, 3)
        # union-find islands
        par = np.arange(len(pos))
        def f(a):
            while par[a] != a: par[a] = par[par[a]]; a = par[a]
            return a
        for a, b, c in faces:
            ra, rb, rc = f(a), f(b), f(c); par[rb] = ra; par[f(c)] = ra
        roots = np.array([f(i) for i in range(len(pos))])
        mat = p.materialNameList[sm.materialIndex]
        print(f" part {g.visconGroupNum} sub {si} {mat} verts {len(pos)} joints: " + ", ".join(f"{n} {int((mainName==n).sum())}" for n in sorted(set(mainName))))
        for r in sorted(set(roots[mainName == "_04"]) if (mainName == "_04").any() else [], key=lambda r: -pos[roots == r][:,1].max()):
            q = pos[roots == r]
            print(f"    island {int((roots==r).sum()):5d} v  x {q[:,0].min():.3f}..{q[:,0].max():.3f}  y {q[:,1].min():.3f}..{q[:,1].max():.3f}  z {q[:,2].min():.3f}..{q[:,2].max():.3f}")
