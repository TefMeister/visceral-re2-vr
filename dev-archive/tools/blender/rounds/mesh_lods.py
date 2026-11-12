import os, sys, importlib, tempfile
sys.path.insert(0, os.path.join(tempfile.gettempdir(), "re_mesh_reader_pkg"))
fm = importlib.import_module("remesh.modules.mesh.file_re_mesh"); pm = importlib.import_module("remesh.modules.mesh.re_mesh_parse")
for f in sys.argv[1:]:
    p = pm.ParsedREMesh(); p.ParseREMesh(fm.readREMesh(f))
    print(os.path.basename(f), "LODs", len(p.mainMeshLODList), "groups per LOD", [sorted(g.visconGroupNum for g in l.visconGroupList) for l in p.mainMeshLODList], "shadow LODs", len(getattr(p, "shadowMeshLODList", []) or []))
