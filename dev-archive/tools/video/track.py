"""track.py <video> : left-eye picture, every frame at 240x240; the red jacket's area, its top edge (rows from the top)
and its centroid, plus a mean frame-to-frame change so jumps stand out. Prints one line per 1/10 s and marks big steps."""
import subprocess, sys, numpy as np
F = r"C:\Users\TD3KX\AppData\Local\Programs\Python\Python312\Lib\site-packages\imageio_ffmpeg\binaries\ffmpeg-win-x86_64-v7.1.exe"
W = H = 240
cmd = [F, "-hide_banner", "-loglevel", "error", "-i", sys.argv[1], "-vf", f"crop=iw/2:ih:0:0,scale={W}:{H}", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"]
p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
fps = 30.0
rows = []
i = 0
while True:
    buf = p.stdout.read(W * H * 3)
    if len(buf) < W * H * 3: break
    a = np.frombuffer(buf, np.uint8).reshape(H, W, 3).astype(np.int16)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    red = (r > 90) & (r > g * 1.6) & (r > b * 1.6)
    ys, xs = np.nonzero(red)
    area = red.mean()
    if len(ys) > 50:
        top = np.percentile(ys, 5); cy = ys.mean(); cx = xs.mean()
    else:
        top = cy = cx = float("nan")
    rows.append((i / fps, area, top, cy, cx))
    i += 1
rows = np.array(rows)
print(f"{len(rows)} frames")
d = np.abs(np.diff(rows[:, 3]))
for k in range(0, len(rows), 3):
    t, area, top, cy, cx = rows[k]
    step = d[k - 1] if k > 0 else 0
    print(f"t={t:5.2f} red={area*100:4.1f}% top={top:5.1f} cy={cy:5.1f} cx={cx:5.1f}")
np.save(sys.argv[1].rsplit("/", 1)[-1] + ".npy", rows)
