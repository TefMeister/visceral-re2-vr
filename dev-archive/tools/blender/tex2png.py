"""Decode RE Engine .tex files to PNG with the RE Mesh Editor's own converter (headless Blender).
  blender -b --python tex2png.py -- <out_dir> <file.tex.NN> [<file.tex.NN> ...]
Writes <out_dir>/<basename>.dds and .png. Read-only on the inputs; outputs are game-data derivatives, keep out of git.
"""
import sys, os, importlib, ctypes
argv = sys.argv[sys.argv.index("--") + 1:]
out, files = argv[0], argv[1:]
os.makedirs(out, exist_ok=True)
ctypes.windll.ole32.CoInitializeEx(None, 0)   # WIC (PNG writer inside texconv.dll) needs COM on this thread
tex_utils = importlib.import_module("RE-Mesh-Editor.modules.tex.re_tex_utils")
Texconv = importlib.import_module("RE-Mesh-Editor.modules.ddsconv.directx.texconv").Texconv
tc = Texconv()
for f in files:
    base = os.path.basename(f).split(".tex")[0]
    dds = os.path.join(out, base + ".dds")
    tex_utils.convertTexFileToDDS(f, dds)
    png = tc.convert_to_png(dds, out=out, verbose=False)
    print("OK", base, "->", png)
