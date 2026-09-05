#!/usr/bin/env python3
"""Independent numeric check of the v0.6 reach clamp's measurement.

Ground truth is the joint dump the game itself produced on 2026-09-05 run 12
(dev-archive/recon/2026-09-05-dock-v04/run12-dock-v05-final-space-mapping.txt).
Nothing here reads the plugin's own arithmetic - it recomputes the same quantity
from the logged world positions and compares the two arms, which are independent
measurements of the same skeleton.
"""
import re, sys, math

LOG = r"C:\Users\TD3KX\github-backups-pd\visceral-re2-vr\dev-archive\recon\2026-09-05-dock-v04\run12-dock-v05-final-space-mapping.txt"
FRAC = 0.98            # DOCK_REACH_FRAC in Plugin.cpp
BAND = (0.05, 1.0)     # the per-segment acceptance band in Plugin.cpp

pat = re.compile(r"joint\[\d+\]\s+([lr]_arm_\w+)\s+pos=\(([-\d.]+) ([-\d.]+) ([-\d.]+)\)")
pos = {}
with open(LOG, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        m = pat.search(line)
        if m and m.group(1) not in pos:
            pos[m.group(1)] = tuple(float(m.group(i)) for i in (2, 3, 4))

def d(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(pos[a], pos[b])))

need = ["l_arm_clavicle", "l_arm_humerus", "l_arm_radius", "l_arm_wrist",
        "r_arm_clavicle", "r_arm_humerus", "r_arm_radius", "r_arm_wrist"]
missing = [n for n in need if n not in pos]
if missing:
    sys.exit("missing joints in the log: %s" % missing)

print("segment lengths from the game's own joint dump (metres)\n")
arms = {}
for side in ("l", "r"):
    cla = d("%s_arm_clavicle" % side, "%s_arm_humerus" % side)
    up = d("%s_arm_humerus" % side, "%s_arm_radius" % side)
    fo = d("%s_arm_radius" % side, "%s_arm_wrist" % side)
    straight = d("%s_arm_humerus" % side, "%s_arm_wrist" % side)
    arms[side] = (cla, up, fo, straight)
    print("  %s  clavicle->humerus %.4f  humerus->radius %.4f  radius->wrist %.4f" % (side, cla, up, fo))
    print("     arm length (upper+fore) = %.4f   clamp radius (x%.2f) = %.4f" % (up + fo, FRAC, (up + fo) * FRAC))
    print("     shoulder->wrist AT REST  = %.4f  = %.1f%% of the arm length" % (straight, 100.0 * straight / (up + fo)))
    ok = all(BAND[0] < x < BAND[1] for x in (up, fo))
    print("     both segments inside the plugin's acceptance band %s: %s\n" % (BAND, "yes" if ok else "NO"))

la = arms["l"][1] + arms["l"][2]
ra = arms["r"][1] + arms["r"][2]
print("cross-check, the two arms as independent measurements of one skeleton:")
print("  left  %.4f" % la)
print("  right %.4f" % ra)
print("  difference %.5f m (%.3f %%)" % (abs(la - ra), 100.0 * abs(la - ra) / la))

print("\nwhat the clamp ignores:")
print("  the clavicle segment is %.4f m and is NOT counted, so the measured reach is a" % arms["l"][0])
print("  LOWER bound on true reach - the shoulder girdle can carry the pivot further out.")
print("  Combined with x%.2f the clamp is conservative twice over: it can fire on a target" % FRAC)
print("  the real arm could have held. clamp= in the 1 Hz summary is what settles whether it does.")

print("\nbound on run 12 (the flat runs already verified):")
print("  at full weight the wrist reached the target exactly (|Lw-tgt| = 0.000, 14 samples).")
print("  So |target - humerus| = |wrist - humerus| <= %.4f, i.e. inside the RAW reach." % la)
print("  Whether it was inside %.4f (the clamped radius) is NOT established - the humerus" % (la * FRAC))
print("  position was never logged at dock time. That is why the clamp has a NUM2 toggle.")
