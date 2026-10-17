# re2launch_pid.py -- launch RE2 and print the settled process id in hex (x64dbg attach wants hex). 2026-10-17.
# launch RE2 from its own folder, wait for the Steam re-spawn to settle, print the final PID in hex (what x64dbg's attach wants)
import subprocess, time, os, re
GAME = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2"
def pids():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq re2.exe", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout
    return [int(m) for m in re.findall(r'"re2.exe","(\d+)"', out)]
subprocess.Popen([os.path.join(GAME, "re2.exe")], cwd=GAME, creationflags=0x00000008)
t0 = time.time(); last = None; stable = 0
while time.time() - t0 < 40:
    p = pids()
    if p and p == last:
        stable += 1
        if stable >= 6: break   # same single pid for ~3 s
    else:
        stable = 0
    last = p
    time.sleep(0.5)
print("pids", last, "hex", [hex(x) for x in (last or [])], "after %.1f s" % (time.time() - t0))
