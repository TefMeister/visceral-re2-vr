"""
burst_capture.py - grab the RE2 desktop window as fast as the screen can be read, for N seconds,
so a VR event (the gun throw) can be looked at frame by frame afterwards. 2026-10-02.
Needs the desktop mirror to be drawn: re2_fw_config.txt  VR_DesktopRecordingFixSkipPresent=false
(with it true the window is black in VR - the 2026-09-12 hazard).
Usage:
    python burst_capture.py <seconds> <outdir> [--scale 0.5] [--quality 80]
Frames are JPEGs named by their time since the start (ms), plus frames.txt with one line each.
BitBlt from the screen DC (never PrintWindow - it serves stale frames once the game stops presenting).
"""
import argparse, importlib.util, os, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("re2drive", os.path.join(HERE, "re2drive.py"))
D = importlib.util.module_from_spec(spec); spec.loader.exec_module(D)
H = D.H


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("seconds", type=float)
    ap.add_argument("outdir")
    ap.add_argument("--scale", type=float, default=0.5)
    ap.add_argument("--quality", type=int, default=80)
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    hwnd, title = H.find_window(D.WINDOW)
    frames = []
    t0 = time.perf_counter()
    print("recording %s for %.1f s -> %s" % (title, a.seconds, a.outdir), flush=True)
    while time.perf_counter() - t0 < a.seconds:
        t = time.perf_counter() - t0
        im = H.grab(hwnd)
        if a.scale != 1.0:
            im = im.resize((int(im.width * a.scale), int(im.height * a.scale)))
        frames.append((t, im))
    print("grabbed %d frames in %.1f s (%.1f fps); writing" % (len(frames), a.seconds, len(frames) / a.seconds), flush=True)
    with open(os.path.join(a.outdir, "frames.txt"), "w") as f:
        for t, im in frames:
            name = "%06d.jpg" % int(t * 1000)
            im.convert("RGB").save(os.path.join(a.outdir, name), quality=a.quality)
            f.write("%s %.4f\n" % (name, t))
    print("done: %d frames in %s" % (len(frames), a.outdir), flush=True)


if __name__ == "__main__":
    main()
