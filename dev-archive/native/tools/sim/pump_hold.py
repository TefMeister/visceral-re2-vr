"""pump_hold.py <seconds> [lx ly lz] -- RE2 / RE3 on the OpenXR Simulator: hold a pump shotgun level and forward in the right
hand (RG held, the pose from KNOWN.md) and the left controller on the fore-end with LG held (PH_LT=1: LT too, PH_LG=0: no
grip), kept alive with a 2 mm wobble. While it runs, `rv.py -f q_pump_hold.lua` says where the drawn hand sits. 2026-11-19."""
import math, os, sys, time
sys.path.insert(0, os.path.expanduser("~/github-backups/_lanes-next-wt/plugins/lanes/tools/vrbench"))
import vb_input as I, vb_settings as S

RIGHT = [0.15, 1.30, 0.05, 29.6, 77.8]       # the W-870's barrel straight ahead (0, 0, -1)
secs = float(sys.argv[1])
left = [float(v) for v in sys.argv[2:5]] if len(sys.argv) >= 5 else [0.13, 1.29, -0.36]   # within 15 cm of the pump hold (the loader docks no further)
lg = 0.0 if os.environ.get("PH_LG") == "0" else 1.0
lt = 1.0 if os.environ.get("PH_LT") == "1" else 0.0
t0 = time.time()
while time.time() - t0 < secs:
    w = 0.002 * math.sin((time.time() - t0) * 40.0)
    inp = I.Input().mode(poses="set", buttons="set")
    rp = I.pose_from_ypr([RIGHT[0] + w, RIGHT[1], RIGHT[2]], RIGHT[3], RIGHT[4])
    inp.hand(S.HAND_RIGHT, grip=rp, aim=rp, squeeze=1.0)
    lp = I.pose_from_ypr([left[0] + w, left[1], left[2]], 0, 0)
    inp.hand(S.HAND_LEFT, grip=lp, aim=lp, squeeze=lg, trigger=lt)
    I.write(inp)
    time.sleep(0.01)
print("held for %.1f s" % secs)
