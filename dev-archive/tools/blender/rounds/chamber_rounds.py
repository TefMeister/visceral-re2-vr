"""chamber.py <mesh>...: islands on any joint but _04 shaped like a round (narrow, 15-35 mm long along the gun, short)"""
import os, sys, importlib, tempfile
import numpy as np
sys.path.insert(0, os.path.join(tempfile.gettempdir(), "re_mesh_reader_pkg"))
fm = importlib.import_module("remesh.modules.mesh.file_re_mesh"); pm = importlib.import_module("remesh.modules.mesh.re_mesh_parse")
for path in sys.argv[1:]:
    p = pm.ParsedREMesh(); p.ParseREMesh(fm.readREMesh(path))
    names = [b.boneName for b in p.skeleton.boneList]; wb = p.skeleton.weightedBones
    print("==", os.path.basename(path))
    for g in p.mainMeshLODList[0].visconGroupList:
        for sm in g.subMeshList:
            pos = np.asarray(sm.vertexPosList, dtype=np.float32); idx = np.asarray(sm.weightIndicesList); w = np.asarray(sm.weightList)
            main = idx[np.arange(len(idx)), w.argmax(1)]
            mn = [names[wb[m]] if not isinstance(wb[m], str) else wb[m] for m in main]
            faces = np.asarray(sm.faceList).reshape(-1, 3); par = list(range(len(pos)))
            def f(a):
                while par[a] != a: par[a] = par[par[a]]; a = par[a]
                return a
            for a, b, c in faces: par[f(b)] = f(a); par[f(c)] = f(a)
            roots = np.array([f(i) for i in range(len(pos))])
            for r in set(roots):
                sel = roots == r; q = pos[sel]; d = q.max(0) - q.min(0)
                j = set(np.array(mn)[sel])
                if "_04" in j: continue
                if d[0] <= 0.012 and d[1] <= 0.012 and 0.012 <= d[2] <= 0.036 and sel.sum() >= 20:
                    print(f"  part {g.visconGroupNum} {p.materialNameList[sm.materialIndex]} {sel.sum()} v joints {sorted(j)} x {q[:,0].min():.3f}..{q[:,0].max():.3f} up {q[:,1].min():.3f}..{q[:,1].max():.3f} fwd {q[:,2].min():.3f}..{q[:,2].max():.3f}")
