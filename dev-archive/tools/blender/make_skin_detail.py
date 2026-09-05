"""Generate Visceral's own tiling skin-detail normal map (pores + fine creases) — no game data, no third-party
textures. Plain Python + numpy; writes a 16-bit-safe 8-bit RGBA PNG through Blender if run inside it, or a
raw .npy + PPM otherwise (the PNG step is done by the caller when numpy alone is available).

  python make_skin_detail.py <out.png> [--size 1024] [--pores 220] [--seed 7] [--strength 1.0]

Channel convention = the game's own detail normals (Detail_Niron_NRM measured 2026-09-06): RGB tangent-space
normal, R = +X right, G = +Y up (mean 0.5/0.5, B ~0.9), alpha = ambient occlusion (1 = open; NullDetail is 1).
The map tiles seamlessly (everything is built with wrap-around arithmetic), so Detail_UVScale can be anything.
"""
import sys, argparse
import numpy as np

ap = argparse.ArgumentParser()
ap.add_argument("out")
ap.add_argument("--size", type=int, default=1024)
ap.add_argument("--pores", type=int, default=220, help="pores across one tile edge (spacing = size/pores px)")
ap.add_argument("--seed", type=int, default=7)
ap.add_argument("--strength", type=float, default=1.0, help="overall height scale before normal derivation")
a = ap.parse_args()
N = a.size
rng = np.random.default_rng(a.seed)

def wrap_blur(img, sigma):
    """Gaussian blur with wrap-around, via FFT (keeps the tile seamless)."""
    if sigma <= 0: return img
    y = np.fft.fftfreq(N)[:, None]; x = np.fft.fftfreq(N)[None, :]
    g = np.exp(-2 * (np.pi ** 2) * (sigma ** 2) * (x ** 2 + y ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(img) * g))

def fbm(octaves, base_freq, persistence=0.55, aniso=(1.0, 1.0)):
    """Tiling value noise: band-limited white noise in frequency space, summed over octaves."""
    out = np.zeros((N, N)); amp = 1.0
    for o in range(octaves):
        f = base_freq * (2 ** o)
        n = rng.standard_normal((N, N))
        sig = N / (2 * np.pi * f)
        n = wrap_blur(n, sig * aniso[0])
        n /= (n.std() + 1e-9)
        out += amp * n; amp *= persistence
    return out / out.std()

# --- 1. pores: jittered grid of soft dimples ---------------------------------------------------
height = np.zeros((N, N))
spacing = N / a.pores
gy, gx = np.mgrid[0:N, 0:N].astype(np.float64)
yy, xx = np.meshgrid(np.arange(a.pores), np.arange(a.pores), indexing="ij")
cy = (yy + 0.5) * spacing + rng.uniform(-0.45, 0.45, yy.shape) * spacing
cx = (xx + 0.5) * spacing + rng.uniform(-0.45, 0.45, xx.shape) * spacing
radius = spacing * rng.uniform(0.18, 0.34, yy.shape)
depth = rng.uniform(0.5, 1.0, yy.shape) * (rng.uniform(0, 1, yy.shape) > 0.12)   # 12% of pores missing
# stamp each pore into a small window (wrap-around indices)
r_px = int(np.ceil(spacing * 0.5)) + 1
win = np.arange(-r_px, r_px + 1)
for j in range(a.pores):
    for i in range(a.pores):
        if depth[j, i] == 0: continue
        y0, x0 = cy[j, i], cx[j, i]
        iy = (np.round(y0).astype(int) + win) % N
        ix = (np.round(x0).astype(int) + win) % N
        dy = (np.round(y0) + win - y0)[:, None]; dx = (np.round(x0) + win - x0)[None, :]
        d2 = dy ** 2 + dx ** 2
        r = radius[j, i]
        prof = -depth[j, i] * np.exp(-d2 / (2 * (r * 0.55) ** 2))          # soft dimple
        height[np.ix_(iy, ix)] += prof
# --- 2. fine crease network: two anisotropic noise fields, ridged -------------------------------
c1 = fbm(3, a.pores / 3.0, aniso=(1.0, 1.0))
c2 = fbm(3, a.pores / 4.5, aniso=(1.0, 1.0))
creases = -(np.abs(c1) ** 0.7) * 0.55 - (np.abs(c2) ** 0.8) * 0.35   # ridged noise reads as skin lines
# --- 3. broad micro-undulation so it is not uniform --------------------------------------------
broad = fbm(2, a.pores / 12.0) * 0.25
h = height * 0.9 + creases * 0.6 + broad
h = (h - h.mean()) / (h.std() + 1e-9) * 0.5 * a.strength   # unit-ish amplitude, mean 0, then the user's strength
# --- 4. height -> tangent normal (wrap-around gradients) ---------------------------------------
dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * 0.5
dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * 0.5
scale = 1.6                                                  # bump slope; tuned so B stays ~0.9 like the game's own detail maps
nx, ny, nz = -dx * scale, -dy * scale, np.ones_like(h)
l = np.sqrt(nx ** 2 + ny ** 2 + nz ** 2); nx /= l; ny /= l; nz /= l
ny = -ny                                                     # image row 0 is the top; +Y up in tangent space  -> flip
rgba = np.empty((N, N, 4), np.float32)
rgba[..., 0] = nx * 0.5 + 0.5; rgba[..., 1] = ny * 0.5 + 0.5; rgba[..., 2] = nz
ao = 1.0 - np.clip(-h, 0, None) * 0.6                        # pores/creases slightly occluded; open skin = 1
rgba[..., 3] = np.clip(ao, 0, 1)
print("normal map %dx%d: mean RGBA = %s, std = %s" % (N, N, np.round(rgba.mean((0, 1)), 3), np.round(rgba.std((0, 1)), 3)))

# --- 5. write --------------------------------------------------------------------------------
try:
    import bpy
    img = bpy.data.images.new("skin_detail", N, N, alpha=True)
    img.pixels.foreach_set(np.flipud(rgba).ravel())          # Blender rows start at the bottom
    img.filepath_raw = a.out; img.file_format = "PNG"; img.save()
    print("wrote", a.out)
except ImportError:
    import zlib, struct
    def png_write(path, arr8):
        raw = b"".join(b"\x00" + arr8[i].tobytes() for i in range(arr8.shape[0]))
        def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
        hdr = struct.pack(">IIBBBBB", arr8.shape[1], arr8.shape[0], 8, 6, 0, 0, 0)
        open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", hdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    png_write(a.out, (np.clip(rgba, 0, 1) * 255 + 0.5).astype(np.uint8))
    print("wrote", a.out, "(pure-python PNG)")
