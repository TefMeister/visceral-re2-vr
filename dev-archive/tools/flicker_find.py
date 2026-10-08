"""flicker_find.py - find one-frame flickers in a burst capture (2026-10-09, Tefa: "lights, some meshes in front of me,
just a quick flicker"). Reads the JPEG frames burst_capture.py wrote, measures how much each frame differs from the one
before and the one after, and flags frames that differ from BOTH neighbours while the neighbours agree with each other:
that is a one-frame intrusion, not motion. The flagged frame and its two neighbours are copied side by side into
<outdir>/flicker-NN.jpg so they can be looked at, and frames.txt gets a FLICKER column.
Usage: py -I flicker_find.py <burst outdir> [--thresh 6.0] [--ratio 2.5]
  thresh: mean per-pixel difference (0-255) that counts as "different";  ratio: how much more the frame differs from
  its neighbours than they differ from each other.
"""
import argparse, os, sys
from PIL import Image, ImageChops, ImageStat


def mean_diff(a, b):
    return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir")
    ap.add_argument("--thresh", type=float, default=6.0)
    ap.add_argument("--ratio", type=float, default=2.5)
    a = ap.parse_args()
    names = sorted(n for n in os.listdir(a.outdir) if n.lower().endswith(".jpg") and not n.startswith("flicker-"))
    if len(names) < 3:
        raise SystemExit("need at least 3 frames in %s" % a.outdir)
    small = []
    for n in names:
        im = Image.open(os.path.join(a.outdir, n)).convert("RGB")
        small.append(im.resize((max(1, im.width // 4), max(1, im.height // 4))))
    prev = [0.0] + [mean_diff(small[i - 1], small[i]) for i in range(1, len(small))]
    found = []
    for i in range(1, len(small) - 1):
        d_before, d_after = prev[i], prev[i + 1]
        d_skip = mean_diff(small[i - 1], small[i + 1])            # the neighbours against each other
        if d_before >= a.thresh and d_after >= a.thresh and d_before > a.ratio * max(d_skip, 0.5) and d_after > a.ratio * max(d_skip, 0.5):
            found.append((i, d_before, d_after, d_skip))
    print("%d frames, %d one-frame flickers" % (len(names), len(found)))
    for k, (i, db, da, ds) in enumerate(found):
        print("  frame %4d %-14s differs %.1f before / %.1f after, neighbours differ %.1f" % (i, names[i], db, da, ds))
        ims = [Image.open(os.path.join(a.outdir, names[j])).convert("RGB") for j in (i - 1, i, i + 1)]
        w, h = ims[0].size
        sheet = Image.new("RGB", (w * 3 + 8, h), (255, 0, 0))
        for j, im in enumerate(ims):
            sheet.paste(im, (j * (w + 4), 0))
        sheet.save(os.path.join(a.outdir, "flicker-%02d.jpg" % k), quality=85)
    with open(os.path.join(a.outdir, "flicker.txt"), "w") as f:
        for i, n in enumerate(names):
            f.write("%s %.2f %s\n" % (n, prev[i], "FLICKER" if any(x[0] == i for x in found) else ""))


if __name__ == "__main__":
    main()
