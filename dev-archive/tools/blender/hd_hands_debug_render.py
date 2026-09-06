"""Paint hd_hands_paint.py's anatomy masks (anat_mask.png: R = tendons, G = veins, B = dorsal weight) in loud colours over the
albedo, so hd_hands_render.py can show WHERE on the hand the lines land.
  blender -b --python hd_hands_debug_render.py -- <anatomy out folder> <debug out folder> [tex-base, default pl1000_Jacket]
"""
import bpy, sys, os
import numpy as np
argv = sys.argv[sys.argv.index("--") + 1:]; SRC, OUT = argv[0], argv[1]; TB = argv[2] if len(argv) > 2 else "pl1000_Jacket"; os.makedirs(OUT, exist_ok=True)
def load(path):
    img = bpy.data.images.load(path); w, h = img.size
    px = np.empty(w * h * 4, np.float32); img.pixels.foreach_get(px); return np.flipud(px.reshape(h, w, 4)).copy()
def save(arr, path):
    import zlib, struct
    arr8 = (np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8)
    raw = b"".join(bytes([0]) + arr8[i].tobytes() for i in range(arr8.shape[0]))
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    hdr = struct.pack(">IIBBBBB", arr8.shape[1], arr8.shape[0], 8, 6, 0, 0, 0)
    open(path, "wb").write(bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]) + chunk(b"IHDR", hdr) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
alb = load(os.path.join(SRC, TB + "_ALBM.png")); m = load(os.path.join(SRC, "anat_mask.png"))
out = alb.copy()
d = np.clip(m[..., 2], 0, 1)[..., None]; out[..., :3] = out[..., :3] * (1 - 0.35 * d) + np.array([0, 0, 1.0]) * 0.35 * d     # dorsal weight: blue tint
t = np.clip(m[..., 0] * 3, 0, 1)[..., None]; out[..., :3] = out[..., :3] * (1 - t) + np.array([1.0, 1.0, 0]) * t                 # tendons: yellow
v = np.clip(m[..., 1] * 3, 0, 1)[..., None]; out[..., :3] = out[..., :3] * (1 - v) + np.array([0, 1.0, 0]) * v                   # veins: green
out[..., 3] = 1
save(out, os.path.join(OUT, TB + "_ALBM.png"))
# flat normal so only the colours show
n = np.zeros_like(alb); n[..., 0] = 0.5; n[..., 1] = 0.5; n[..., 2] = 1.0; n[..., 3] = 1.0
save(n, os.path.join(OUT, TB + "_NRMR.png")); print("DONE")
