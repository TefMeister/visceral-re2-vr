"""
plot_trace.py - turn the probe's [visceral_trace] lines (one per frame after each shot) into numbers and a picture.
Usage:  python plot_trace.py <trace.txt> <out.png>
Each line: shotN f=<frame> hold=<0/1> gun=(r u f) rwrist=(r u f) lwrist=(r u f) ctl=... layer0=<motion> mf=<frame>
(r u f = cm relative to the head in the camera's right / up / forward axes; forward reads negative on this build).
"""
import re, sys, collections
LINE = re.compile(r"(shot\d+) f=(\d+) hold=(\d) gun=\(([^)]*)\) rwrist=\(([^)]*)\) lwrist=\(([^)]*)\).*layer0=(\S+) mf=([\d.-]+)")


def vec(s):
    return [float(x) for x in s.split()]


def main():
    src, out = sys.argv[1], sys.argv[2]
    shots = collections.OrderedDict()
    for line in open(src, encoding="utf-8", errors="replace"):
        m = LINE.search(line)
        if not m:
            continue
        shot, f, hold, g, rw, lw, motion, mf = m.groups()
        shots.setdefault(shot, []).append((int(f), int(hold), vec(g), vec(rw), vec(lw), motion, float(mf)))
    for shot, rows in shots.items():
        base = rows[0][2]
        peak, peak_f, peak_v = 0.0, 0, None
        lw_base = rows[0][4]
        dist_lw = []
        for f, hold, g, rw, lw, motion, mf in rows:
            d = [g[i] - base[i] for i in range(3)]
            mag = sum(x * x for x in d) ** 0.5
            if mag > peak:
                peak, peak_f, peak_v = mag, f, d
            gl = sum((g[i] - lw[i]) ** 2 for i in range(3)) ** 0.5
            dist_lw.append((f, gl))
        motions = []
        for f, hold, g, rw, lw, motion, mf in rows:
            if not motions or motions[-1][1] != motion:
                motions.append((f, motion))
        holds = sorted(set(r[1] for r in rows))
        print("%s: %d frames; gun moved at most %.1f cm (right %+.1f, up %+.1f, fwd %+.1f) at frame %d; hold values %s; left wrist to gun %.1f -> %.1f cm (min %.1f, max %.1f)" % (
            shot, len(rows), peak, peak_v[0], peak_v[1], peak_v[2], peak_f, holds, dist_lw[0][1], dist_lw[-1][1], min(d for _, d in dist_lw), max(d for _, d in dist_lw)))
        print("   layer0: " + " -> ".join("f%d %s" % (f, m.replace("pl00_", "")) for f, m in motions))
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception as e:
        print("no matplotlib:", e); return
    n = len(shots)
    fig, axes = plt.subplots(n, 2, figsize=(14, 3.6 * n), squeeze=False)
    for row, (shot, rows) in enumerate(shots.items()):
        fs = [r[0] for r in rows]
        base = rows[0][2]
        ax = axes[row][0]
        for i, (name, col) in enumerate((("right", "tab:red"), ("up", "tab:green"), ("forward", "tab:blue"))):
            ax.plot(fs, [r[2][i] - base[i] for r in rows], color=col, label="gun " + name)
            ax.plot(fs, [r[4][i] - rows[0][4][i] for r in rows], color=col, linestyle=":", label="left wrist " + name)
        ax.axhline(0, color="k", linewidth=0.5)
        ax.set_title("%s: gun and left wrist, change from the shot frame (cm, camera axes)" % shot)
        ax.set_xlabel("frames after the shot"); ax.legend(fontsize=7, ncol=2)
        ax2 = axes[row][1]
        ax2.plot(fs, [sum((r[2][i] - r[4][i]) ** 2 for i in range(3)) ** 0.5 for r in rows], color="tab:purple", label="left wrist to gun")
        ax2.plot(fs, [sum((r[2][i] - r[3][i]) ** 2 for i in range(3)) ** 0.5 for r in rows], color="tab:orange", label="right wrist to gun")
        ax2.plot(fs, [r[1] * 10 for r in rows], color="k", linestyle="--", label="aim state x10")
        last = None
        for r in rows:
            if r[5] != last:
                ax2.axvline(r[0], color="grey", linewidth=0.5)
                ax2.text(r[0], ax2.get_ylim()[1] * 0.95 if ax2.get_ylim()[1] else 1, r[5].replace("pl00_", ""), rotation=90, fontsize=6, va="top")
                last = r[5]
        ax2.set_title("%s: hand-to-gun distances (cm) and the animation on layer 0" % shot)
        ax2.set_xlabel("frames after the shot"); ax2.legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    print("picture:", out)


if __name__ == "__main__":
    main()
