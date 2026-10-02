"""
hold_exit_drive.py - drive the FLAT hold-exit test from outside (2026-10-02).
Pairs with reframework/autorun/visceral_hold_exit_probe.lua in the test copy:
  NUM1 = forced HOLD on/off (the game's own aim input, latched), NUM2 = one shot.
One repetition: hold W (walk) for WALK seconds, fire once, release W OFFSET seconds after the shot
(negative = release first, then fire), wait REST seconds. The 2026-09-30 throws came when the shot
was fired while STOPPING, so the offsets around 0 are the interesting ones.
Usage:
    python hold_exit_drive.py --reps 6 --offset 0.0 [--walk 1.5] [--rest 3.0] [--hold on|off|keep]
    python hold_exit_drive.py --hold off          # just turn the forced hold off
Keys go through SendInput scancodes (W) and virtual keys (numpad), as re2drive.py does.
"""
import argparse, importlib.util, time
HERE = __file__.rsplit("\\", 1)[0] if "\\" in __file__ else __file__.rsplit("/", 1)[0]
spec = importlib.util.spec_from_file_location("re2drive", HERE + "/re2drive.py")
D = importlib.util.module_from_spec(spec); spec.loader.exec_module(D)
H = D.H
W_SCAN = 0x11
S_SCAN = 0x1F


def w(down):
    H._key(W_SCAN, False, not down)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--reps", type=int, default=0)
    ap.add_argument("--offset", type=float, default=0.0, help="seconds between the shot and releasing W (negative: release first)")
    ap.add_argument("--walk", type=float, default=1.5)
    ap.add_argument("--rest", type=float, default=3.0)
    ap.add_argument("--hold", default="on", choices=["on", "off", "keep"])
    a = ap.parse_args()
    hwnd, _title = H.find_window(D.WINDOW)
    H.focus(hwnd)
    if a.hold == "on":
        D.num(1)          # NUM1 toggles; the Lua logs which way it went, the caller reads the log
        time.sleep(0.5)
    for i in range(a.reps):
        H.focus(hwnd, settle=0.2)
        # alternate forward / back: reps 5-11 of the first run walked into a wall and only flickered
        # 2-5 frames of walk, so those shots were really fired standing still
        key = W_SCAN if i % 2 == 0 else S_SCAN
        H._key(key, False, False)
        time.sleep(a.walk)
        if a.offset < 0:
            H._key(key, False, True); time.sleep(-a.offset); D.num(2)
        else:
            D.num(2); time.sleep(a.offset); H._key(key, False, True)
        print("rep %d: shot fired, %s released %+.2f s after it" % (i + 1, "W" if key == W_SCAN else "S", a.offset), flush=True)
        time.sleep(a.rest)
    if a.hold == "off":
        D.num(1)


if __name__ == "__main__":
    main()
