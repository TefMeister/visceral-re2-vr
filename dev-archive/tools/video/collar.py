"""collar.py <video> : left-eye picture at 480x480, every frame. Collar tip = topmost row of the red jacket; hand = the
skin-coloured blob's top row (the hand is on the controller, so it is our fixed reference). Prints at 10 fps:
collar_top, hand_top, and collar relative to hand (pixels; 480 px = the full vertical view)."""
import subprocess, sys, numpy as np
F = r"C:\Users\TD3KX\AppData\Local\Programs\Python\Python312\Lib\site-packages\imageio_ffmpeg\binaries\ffmpeg-win-x86_64-v7.1.exe"
W = H = 480
p = subprocess.Popen([F, "-hide_banner", "-loglevel", "error", "-i", sys.argv[1], "-vf", f"crop=iw/2:ih:0:0,scale={W}:{H}",
                      "-f", "rawvideo", "-pix_fmt", "rgb24", "-"], stdout=subprocess.PIPE)
i = 0
out = []
while True:
    buf = p.stdout.read(W * H * 3)
    if len(buf) < W * H * 3: break
    a = np.frombuffer(buf, np.uint8).reshape(H, W, 3).astype(np.int16)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    red = (r > 80) & (r > g * 1.7) & (r > b * 1.5)
    skin = (r > 150) & (g > 100) & (b > 70) & (r > g) & (g > b) & (r - g < 70) & (r - g > 15)
    def top(m):
        rows = np.nonzero(m.sum(axis=1) > 6)[0]
        return float(rows[0]) if len(rows) else float("nan")
    out.append((i / 30.0, top(red), top(skin), float(skin.sum())))
    i += 1
o = np.array(out)
for k in range(0, len(o), int(sys.argv[2]) if len(sys.argv) > 2 else 3):
    t, c, h, s = o[k]
    print(f"t={t:5.1f} collar={c:5.0f} hand={h:5.0f} collar-hand={c-h:6.0f}")
