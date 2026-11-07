"""motion_table.py -- RE2's reload motions, read from the 2026-11-02 finger takes (plan Task 11 step 1, 2026-11-07).

Village's clip cutter keys on Village's motion ids (player layer 1: 2500 magazine, 2511/2512/2513 round by round). RE2's are
not known, so this reads every reload take and prints, per gun, the motions each player layer plays while the game says it
is reloading, in order, with how long each lasted, how many times its frame counter started over (a round-by-round cycle),
and which hand's fingers move in it (mean curl change of the 12 non-thumb finger joints, degrees).

A take's `motions` cell: "player.<layer>:<bank>:<id>:<frame>|..." (id 4294967295 = nothing playing).
Output: a markdown table on stdout (ids and timings only; no animation data), for dev-archive/rebuild/re2-motion-table.md.

    python -I motion_table.py [--takes DIR]
"""
import argparse, csv, glob, math, os, re

TAKES = r"D:\claude video game stuff\finger-takes\2026-11-02\re2-claire"
NONE = "4294967295"
FINGERS = ["index_0", "index_1", "index_2", "middle_0", "middle_1", "middle_2", "ring_0", "ring_1", "ring_2", "ring_3",
           "little_0", "little_1", "little_2", "little_3"]


def angle(q):
    return math.degrees(2 * math.acos(max(-1.0, min(1.0, abs(q[3])))))


def parse(cell):
    out = {}
    for m in cell.split("|"):
        p = m.split(":")
        if len(p) == 4:
            out[p[0]] = (p[1] + ":" + p[2] if p[2] != NONE else None, float(p[3]))
    return out


def curls(row, cols):
    return {s: sum(angle([float(row[k]) for k in c4]) for c4 in cols[s]) / len(cols[s]) for s in cols}


def table(path):
    rows = list(csv.reader(open(path, newline="")))
    head, rows = rows[0], rows[1:]
    cols = {s: [[head.index(f"{s}_hand_{f}.{c}") for c in "xyzw"] for f in FINGERS if f"{s}_hand_{f}.x" in head]
            for s in ("l", "r")}
    reloading = [r[2] == "true" for r in rows]
    if not any(reloading):
        return None
    first = reloading.index(True)
    last = len(reloading) - 1 - reloading[::-1].index(True)
    segs = []   # (layer, motion, t0, t1, restarts, curl range l, curl range r)
    open_seg = {}
    for i in range(max(0, first - 5), min(len(rows), last + 30)):
        t = float(rows[i][0])
        m = parse(rows[i][3])
        c = curls(rows[i], cols)
        for layer, (mid, frame) in m.items():
            if layer == "player.0":
                continue        # the whole-body locomotion layer
            cur = open_seg.get(layer)
            if cur and cur["m"] == mid:
                if frame + 0.5 < cur["frame"]:
                    cur["restarts"] += 1
                cur["frame"] = frame; cur["t1"] = t
                for s in c:
                    cur["lo"][s] = min(cur["lo"][s], c[s]); cur["hi"][s] = max(cur["hi"][s], c[s])
                continue
            if cur and cur["m"] is not None:
                segs.append(cur)
            open_seg[layer] = {"layer": layer, "m": mid, "t0": t, "t1": t, "frame": frame, "restarts": 0,
                               "lo": dict(c), "hi": dict(c)}
    segs += [s for s in open_seg.values() if s["m"] is not None]
    segs.sort(key=lambda s: s["t0"])
    t_first = float(rows[first][0])
    return [s for s in segs if s["t1"] - s["t0"] > 0.05], t_first, float(rows[last][0]) - t_first


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--takes", default=TAKES)
    a = ap.parse_args()
    print("| gun | reload (s) | layer | motion (bank:id) | from (s) | lasts (s) | restarts | left curl range | right curl range |")
    print("| --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    for path in sorted(glob.glob(os.path.join(a.takes, "re2__reload_full__*.csv"))):
        gun = re.search(r"__(wp\d+)__", path).group(1)
        res = table(path)
        if res is None:
            print(f"| {gun} | no reload in the take | | | | | | | |")
            continue
        segs, t_first, dur = res
        for s in segs:
            if s["layer"] in ("player.1", "player.2") and s["t0"] < t_first - 0.02 and s["t1"] > t_first + dur:
                continue        # an idle that runs through the whole reload: not part of it
            lr = s["hi"]["l"] - s["lo"]["l"]; rr = s["hi"]["r"] - s["lo"]["r"]
            print(f"| {gun} | {dur:.2f} | {s['layer']} | {s['m']} | {s['t0'] - t_first:+.2f} | {s['t1'] - s['t0']:.2f} | "
                  f"{s['restarts']} | {lr:.0f} | {rr:.0f} |")


if __name__ == "__main__":
    main()
