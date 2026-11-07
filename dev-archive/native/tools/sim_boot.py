"""sim_boot.py -- the game on the OpenXR Simulator with VR Bench, straight into the last save (shared by RE2 and RE3, 2026-11-07;
was RE2's re2_sim_boot.py). The game comes from game.conf beside this file: exe, app_id, a_presses (RE2 3, RE3 4), a_gap_s.
VR Bench's layer is switched on for the exe before the start (a close-out switches it off).

    python sim_boot.py [--wait 45]
"""
import argparse, os, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from deploy import read_conf   # noqa: E402  (game.conf)

VB = os.path.expanduser("~/github-backups/_lanes-next-wt/plugins/lanes/tools/vrbench")
sys.path.insert(0, VB)
import vb_input as I, vb_settings as S   # noqa: E402

SIM = r"D:\claude video game stuff\tools\OpenXR-Simulator\openxr_simulator.json"


def press(name, hold=0.25):
    I.write(I.Input().mode(poses="pass", buttons="set").hand(S.HAND_RIGHT, **{name: True}))
    time.sleep(hold)
    I.write(I.Input().mode(poses="pass", buttons="set").hand(S.HAND_RIGHT, **{name: False}))
    print(time.strftime("%H:%M:%S"), "pressed", name, flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wait", type=float, default=float(os.environ.get("BOOT_WAIT", "40")))
    a = ap.parse_args()
    c = read_conf()
    game, exe = c["game_dir"], os.path.join(c["game_dir"], c["exe"])
    subprocess.run([sys.executable, os.path.join(VB, "vrbench.py"), "start", exe], check=False)
    env = dict(os.environ, XR_RUNTIME_JSON=SIM, SteamAppId=c["app_id"])
    p = subprocess.Popen([exe], cwd=game, env=env)   # cwd = the game folder (its config lives there)
    print(time.strftime("%H:%M:%S"), "launched pid", p.pid, flush=True)
    time.sleep(a.wait)
    for _ in range(int(c.get("a_presses", "3"))):
        press("a")
        time.sleep(float(c.get("a_gap_s", "5")))
    time.sleep(20)
    print(time.strftime("%H:%M:%S"), "boot done", flush=True)


if __name__ == "__main__":
    main()
