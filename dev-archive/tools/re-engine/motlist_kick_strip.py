"""motlist_kick_strip.py -- b045 (2026-10-02): the handgun shot kick without its LEFT-arm tracks.

Why: measured in the headset 2026-10-02 (per-frame trace, 9 of 9 shots): 6-7 frames after a shot the left wrist leaves
the gun by ~7 cm for ~33 frames, and praydog's two-hand steering (right hand -> animated left-hand socket) swings the gun
after it; one-handed shots do not swing. The shot motions (1100-1102, and 1120 NoAmmo) are additive and carry 22 left
arm/hand bone tracks. This turns those tracks off, so the kick moves the right arm, spine and gun only and the support
hand stays where the pose and IK put it.

How: in each shot entry's clip-header table (12-byte headers from +0x80, count u16 at +0x72; layout in
motlist_graft_arms.py), the left-arm headers' track-flags byte (+2) is set to 0 = "no tracks for this bone". Nothing is
moved or resized; every other byte of the file is unchanged (checked). In place, so the motlist's offsets stay valid.

Usage: py motlist_kick_strip.py <in .motlist.524> <out .motlist.524>
Legitimacy: reads the player's own (already modded) list, writes only <out>. No game data is included here.
"""
import os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import motlist_splice as ms
from motlist_graft_arms import ARMH   # arm/hand/weapon bone hashes (murmur3 of the UTF-16 names)

SHOT_IDS = ("_1100_", "_1101_", "_1102_", "_1120_")
LEFT = {h: n for h, n in ARMH.items() if n.startswith("l_") and n != "l_weapon"}


def main():
    src, out = sys.argv[1], sys.argv[2]
    m = ms.Motlist(src)
    d = bytearray(m.data)
    done = 0
    for name, o in m.by_name.items():
        if not name or not any(s in name for s in SHOT_IDS):
            continue
        cc = struct.unpack_from("<H", d, o + 0x72)[0]
        n = 0
        for k in range(cc):
            p = o + 0x80 + 12 * k
            h = struct.unpack_from("<I", d, p + 4)[0]
            if h in LEFT and d[p + 2]:
                d[p + 2] = 0
                n += 1
        print("  %s: %d left-arm/hand tracks off (of %d bones)" % (name, n, cc))
        done += n
    changed = sum(1 for a, b in zip(m.data, d) if a != b)
    if changed != done or len(d) != len(m.data):
        raise SystemExit("refusing: %d bytes changed for %d tracks" % (changed, done))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, "wb").write(bytes(d))
    print("wrote %s (%d bytes changed, size unchanged)" % (out, changed))


if __name__ == "__main__":
    main()
