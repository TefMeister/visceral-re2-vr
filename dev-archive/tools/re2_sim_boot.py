"""re2_sim_boot.py -- RE2 on the OpenXR Simulator with VR Bench (2026-11-05): launch with the simulator as the headset,
A (START GAME), A (Story), A (Continue): straight into the last save (KNOWN.md, 2026-11-01). Prints what it did.
The VR Bench layer must be on for re2.exe before the game starts (a close-out switches it off)."""
import os, subprocess, sys, time
VB = os.path.expanduser("~/github-backups/_lanes-next-wt/plugins/lanes/tools/vrbench")
sys.path.insert(0, VB)
import vb_input as I, vb_settings as S
G = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2"
SIM = r"D:\claude video game stuff\tools\OpenXR-Simulator\openxr_simulator.json"
def press(name, hold=0.25):
    I.write(I.Input().mode(poses="pass", buttons="set").hand(S.HAND_RIGHT, **{name: True})); time.sleep(hold)
    I.write(I.Input().mode(poses="pass", buttons="set").hand(S.HAND_RIGHT, **{name: False}))
    print(time.strftime("%H:%M:%S"), "pressed", name, flush=True)
subprocess.run([sys.executable, os.path.join(VB, "vrbench.py"), "start", os.path.join(G, "re2.exe")], check=False)
env = dict(os.environ, XR_RUNTIME_JSON=SIM, SteamAppId="883710")
p = subprocess.Popen([os.path.join(G, "re2.exe")], cwd=G, env=env)   # cwd = the game folder (re2_config.ini lives there)
print(time.strftime("%H:%M:%S"), "launched pid", p.pid, flush=True)
time.sleep(float(os.environ.get("BOOT_WAIT", "40")))
for k in range(3):
    press("a"); time.sleep(5)
time.sleep(20)
print(time.strftime("%H:%M:%S"), "boot done", flush=True)
