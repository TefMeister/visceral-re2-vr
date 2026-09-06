"""Procedural textures for Visceral's forearm bracelets -- ours, nothing from the game. Two 512x512 tiling sets:

  leather : ALBM = neutral mid-grey leather grain (RGB), alpha = metallic 0    -> tinted per material by BaseColor in
            the MDF (brown / dark red / red-purple), which is why it is grey here
            NRMR = leather grain normal (RG), B = 1, alpha = roughness ~0.62
            ATOS = (1, 1, occlusion, 1) in the jacket's own convention `[measured 2026-09-06: jacket ATOS mean 1/1/0.78/1]`
  metal   : ALBM = brushed steel (RGB), alpha = metallic 1
            NRMR = fine brushing lines, alpha = roughness ~0.32
            ATOS = (1, 1, 1, 1)

  py make_bracelet_textures.py <out_dir> [--size 512] [--seed 7]
Writes visceral_bracelet_{leather,metal}_{ALBM,NRMR,ATOS}.png. Convert with tex_build.py.
"""
import argparse, os
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("out"); ap.add_argument("--size", type=int, default=512); ap.add_argument("--seed", type=int, default=7)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
N = a.size
rng = np.random.default_rng(a.seed)

def blur_wrap(img, sigma):
    """gaussian blur that wraps, so every tile is seamless"""
    if sigma <= 0: return img
    fy = np.fft.fftfreq(img.shape[0])[:, None]; fx = np.fft.fftfreq(img.shape[1])[None, :]
    g = np.exp(-2 * (np.pi ** 2) * (sigma ** 2) * (fx ** 2 + fy ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(img) * g)).astype(np.float32)

def fbm(octaves, base_sigma, persistence=0.55):
    out = np.zeros((N, N), np.float32); amp = 1.0; s = base_sigma
    for _ in range(octaves):
        n = blur_wrap(rng.standard_normal((N, N)).astype(np.float32), s); n /= (n.std() + 1e-9)
        out += amp * n; amp *= persistence; s *= 0.5
    return out / (out.std() + 1e-9)

def normal_from_height(h, strength):
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * 0.5
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * 0.5
    nx = -dx * strength; ny = dy * strength
    nz = np.sqrt(np.clip(1 - nx ** 2 - ny ** 2, 0.05, 1))
    return nx, ny, nz

def save(name, rgb, alpha):
    arr = np.concatenate([np.clip(rgb, 0, 1), np.clip(alpha, 0, 1)[..., None]], axis=2)
    Image.fromarray((arr * 255 + 0.5).astype(np.uint8), "RGBA").save(os.path.join(a.out, name + ".png"))

# ---- leather: pebbled grain (cells) + fine pores + a soft mottle -------------------------------------------
cells = fbm(3, N / 40.0)                                   # ~40 pebbles across the tile
cells = np.tanh(cells * 1.4)                               # flatten the tops, deepen the creases
pores = fbm(2, N / 220.0) * 0.35
mottle = fbm(2, N / 12.0) * 0.5
h_leather = cells * 0.7 + pores * 0.5
grey = 0.50 + 0.045 * mottle - 0.05 * np.clip(-cells, 0, 1)          # creases a little darker
rgb = np.repeat(grey[..., None], 3, axis=2) * np.array([1.0, 0.985, 0.97])   # a hair warm so tints do not go grey-cold
save("visceral_bracelet_leather_ALBM", rgb, np.zeros((N, N), np.float32))
nx, ny, nz = normal_from_height(h_leather, 3.2)
rough = 0.62 + 0.06 * mottle - 0.08 * np.clip(cells, 0, 1)          # pebble tops a touch glossier
save("visceral_bracelet_leather_NRMR", np.stack([nx * 0.5 + 0.5, ny * 0.5 + 0.5, np.ones_like(nz)], 2), rough)
occ = 1.0 - 0.18 * np.clip(-cells, 0, 1)
save("visceral_bracelet_leather_ATOS", np.stack([np.ones((N, N)), np.ones((N, N)), occ], 2).astype(np.float32), np.ones((N, N), np.float32))

# ---- metal: brushed steel, brushing along U -------------------------------------------------------------
lines = blur_wrap(rng.standard_normal((N, N)).astype(np.float32), 0.0)
lines = blur_wrap(lines, 0.6)                                        # tiny cross-section
lines = np.cumsum(lines, axis=1); lines -= lines.mean(1, keepdims=True); lines /= (lines.std() + 1e-9)   # streaks along U
lines = blur_wrap(lines, 0.0) * 0.6 + fbm(2, N / 6.0) * 0.4
lines = np.repeat(blur_wrap(rng.standard_normal((N, 1)).astype(np.float32) @ np.ones((1, N), np.float32), 1.0), 1, axis=0) * 0.0 + lines
scratches = np.clip(fbm(2, N / 300.0) - 2.2, 0, 1) * 4.0
steel = 0.60 + 0.03 * lines - 0.10 * scratches
rgb = np.stack([steel * 1.0, steel * 0.985, steel * 0.955], 2)
save("visceral_bracelet_metal_ALBM", rgb, np.ones((N, N), np.float32))
nx, ny, nz = normal_from_height(lines * 0.12 - scratches * 0.6, 2.0)
rough = 0.32 + 0.05 * np.abs(lines) * 0.5 + 0.25 * scratches
save("visceral_bracelet_metal_NRMR", np.stack([nx * 0.5 + 0.5, ny * 0.5 + 0.5, np.ones_like(nz)], 2), rough)
save("visceral_bracelet_metal_ATOS", np.ones((N, N, 3), np.float32), np.ones((N, N), np.float32))
print("wrote 6 PNGs to", a.out)
