"""tw_test.py -- the two-in-hand reloads on the OpenXR Simulator through VR Bench (2026-11-02).
Plays the controllers, reads the REFramework log's new [visceral-re8] lines after each step."""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vb

T = "RESIDENT EVIL 2"
LOG = "C:/Steam/steamapps/common/RESIDENT EVIL 2  BIOHAZARD RE2/re2_framework_log.txt"
P = vb.I.pose_from_ypr
GUN = P((0.20, 1.40, -0.35), 0, 45)          # the right hand: a long gun held level, pointing ahead
POUCH = P((-0.20, 1.08, -0.02), 0, 0)
pos = [os.path.getsize(LOG)]


def news(filt=("reload:", "shellclip:", "holster")):
    with open(LOG, "rb") as f:
        f.seek(pos[0]); data = f.read(); pos[0] += len(data)
    out = [l.decode(errors="replace")[60:220] for l in data.splitlines() if any(k.encode() in l for k in filt) and b"skipped (#" not in l]
    return out


def hold(left, secs, lg=0.0, lt=0.0, rg=1.0):
    t0 = time.time()
    while time.time() - t0 < secs:
        vb.front(T)
        inp = vb.I.Input().mode(poses="set", buttons="set")
        inp.hand(vb.VS.HAND_LEFT, grip=left, aim=left, squeeze=lg, trigger=lt)
        inp.hand(vb.VS.HAND_RIGHT, grip=GUN, aim=GUN, squeeze=rg)
        vb.I.write(inp)
        time.sleep(1 / 45)


def step(name, *a, **k):
    hold(*a, **k)
    for l in news():
        print(f"  [{name}] {l}")


if __name__ == "__main__":
    mode = sys.argv[1]
    if mode == "shotgun":
        step("rest", P((-0.25, 1.25, -0.25), 0, 0), 1.0)
        step("to pouch", POUCH, 1.0)
        step("LG at pouch", POUCH, 1.0, lg=1.0)
        # with LG held, look for the shell port around the gun: ahead of and below the right hand
        docked = False
        for dz in (-0.10, -0.18, -0.26, -0.34, -0.42):
            for dy in (-0.08, -0.03, 0.02):
                for dx in (0.0, -0.06):
                    hold(P((0.20 + dx, 1.40 + dy, -0.35 + dz), 0, 0), 0.25, lg=1.0)
                    got = news()
                    for l in got:
                        print(f"  [search {dx:+.2f},{dy:+.2f},{dz:+.2f}] {l}")
                    if any("the hand docks" in l for l in got):
                        docked = (dx, dy, dz); break
                if docked: break
            if docked: break
        print("docked at", docked)
        if docked:
            here = P((0.20 + docked[0], 1.40 + docked[1], -0.35 + docked[2]), 0, 0)
            step("hold", here, 0.5, lg=1.0)
            step("LT", here, 0.3, lg=1.0, lt=1.0)
            step("after LT", here, 2.0, lg=1.0)
            step("LG off", here, 1.0)
    vb.write()


def search(text, lg, center, lt=0.0, dzs=(0.0, -0.08, -0.16, -0.24, -0.32, -0.40), dys=(-0.10, -0.04, 0.02), dxs=(0.0, -0.06, 0.06), dwell=0.25, fresh_lg=False):
    """move the left hand over a grid ahead of `center` until a log line holds `text`; returns the pose or None"""
    for dz in dzs:
        for dy in dys:
            for dx in dxs:
                p = P((center[0] + dx, center[1] + dy, center[2] + dz), 0, 0)
                if fresh_lg:
                    hold(p, 0.12, lg=0.0)
                hold(p, dwell, lg=lg, lt=lt)
                got = news()
                for l in got:
                    print(f"  [search {dx:+.2f},{dy:+.2f},{dz:+.2f}] {l}")
                if any(text in l for l in got):
                    return p
    return None


def gm79():
    c = (0.20, 1.40, -0.35)
    step("rest", P((-0.25, 1.25, -0.25), 0, 0), 1.0)
    news()
    # fire it (RT) so a spent case sits in the barrel
    hold(P((-0.25, 1.25, -0.25), 0, 0), 0.4)
    t0 = time.time()
    while time.time() - t0 < 0.4:
        vb.front(T); inp = vb.I.Input().mode(poses="set", buttons="set")
        inp.hand(vb.VS.HAND_LEFT, grip=P((-0.25, 1.25, -0.25), 0, 0)); inp.hand(vb.VS.HAND_RIGHT, grip=GUN, aim=GUN, squeeze=1.0, trigger=1.0)
        vb.I.write(inp); time.sleep(1 / 45)
    step("after RT", P((-0.25, 1.25, -0.25), 0, 0), 2.0)
    # open: the left hand on the front grip with LG, then LT
    grip = search("", 1.0, c, dzs=(-0.20, -0.28, -0.36), dwell=0.15)
    p = search("opens", 1.0, c, lt=1.0, dzs=(-0.12, -0.20, -0.28, -0.36), dwell=0.3, fresh_lg=True)
    print("opened at", p is not None)
    step("LG off", P((-0.25, 1.25, -0.25), 0, 0), 0.8)
    # way 2: LG at the insert point takes the hand there; LT takes the spent case INTO the hand
    p = search("insert point: the left hand docks", 1.0, c, fresh_lg=True, dwell=0.3)
    print("docked at the insert point:", p is not None)
    if p is None:
        return
    step("LT (spent out)", p, 0.3, lg=1.0, lt=1.0)
    step("hold", p, 0.5, lg=1.0)
    step("LG held to the pouch", POUCH, 1.5, lg=1.0)
    q = search("round in", 1.0, c, dwell=0.3)
    print("went in by itself:", q is not None)
    if q is not None:
        step("held there", q, 1.0, lg=1.0)
        step("LG off", q, 1.0)


if __name__ == "__main__" and sys.argv[1] == "gm79":
    gm79()
    vb.write()
