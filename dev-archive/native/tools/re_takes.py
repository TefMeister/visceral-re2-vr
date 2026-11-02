"""re_takes.py GAME -- finger takes for every gun in RE2 / RE3 (flat), restarting the game when the inventory is full.
Uses the visceral_finger_rec.lua command file. Prints one line per gun."""
import ctypes, os, re, subprocess, sys, time

GAME = sys.argv[1]
CFG = {
    "re2": dict(app="883710", title="RESIDENT EVIL 2", exe="re2.exe",
                folder=r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2",
                menu=["insert", "w1", "enter", "w4", "enter", "w3", "space"]),
    "re3": dict(app="952060", title="RESIDENT EVIL 3", exe="re3.exe", folder=r"C:\Steam\steamapps\common\RE3",
                menu=["insert", "w1", "enter", "w5", "space", "w4", "space", "w3", "space"]),
}[GAME]
GUNS = [g.split(":") for g in sys.argv[2].split(",")]   # WPxxxx:ammo1/ammo2
D = os.path.join(CFG["folder"], "reframework", "data", "finger_rec")
LOG = os.path.join(CFG["folder"], "re2_framework_log.txt")
MOM = os.path.expanduser("~/github-backups/_lanes-next-wt/plugins/lanes/tools/menu-o-matic/menu_o_matic.py")
u = ctypes.windll.user32
sys.path.insert(0, os.path.dirname(MOM))
import mom_window as MW                      # its focus() gets past Windows' foreground lock


def front():
    h = u.FindWindowW(None, CFG["title"])
    if h and u.GetForegroundWindow() != h:
        try:
            MW.focus(h)
        except Exception:
            u.SetForegroundWindow(h)


def key(k):
    front()
    subprocess.run([sys.executable, MOM, "press", CFG["title"], k], capture_output=True)


def cmd(c, wait=1.2):
    front()
    open(os.path.join(D, "command.txt"), "w").write(c)
    time.sleep(wait)
    try:
        return open(os.path.join(D, "status.txt")).read().strip()[9:]
    except OSError:
        return ""


def gun():
    try:
        os.remove(os.path.join(D, "probe.txt"))
    except OSError:
        pass
    cmd("probe", 1.5)
    try:
        line = open(os.path.join(D, "probe.txt")).readline()
    except OSError:
        return None
    return line.split("gun: ")[1].split(",")[0] if "gun: " in line else None


def running():
    return CFG["exe"] in subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {CFG['exe']}"], capture_output=True, text=True).stdout.lower()


def close():
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    f"$p=Get-Process {CFG['exe'][:-4]} -ErrorAction SilentlyContinue; if($p){{[void]$p.CloseMainWindow()}}"])
    for _ in range(60):
        if not running():
            return
        time.sleep(1)


def boot():
    t0 = time.time()
    os.startfile(f"steam://rungameid/{CFG['app']}")
    for _ in range(180):
        if os.path.exists(LOG) and os.path.getmtime(LOG) > t0 and b"finger-rec] ready" in open(LOG, "rb").read():
            break
        time.sleep(1)
    time.sleep(20)
    for step in CFG["menu"]:
        if step.startswith("w"):
            time.sleep(float(step[1:]))
        else:
            key(step)
    for k in range(60):
        if gun() is not None:
            return True
        if k in (12, 24):         # an extra screen (RE3's "Completed Challenges"): Story and Continue are the first items
            key("space")
        time.sleep(1.5)
    return False


done = []
fresh = False                     # True right after a boot, before anything was given: a failure then means "no such gun"
if not running():
    print("boot:", boot(), flush=True)
    fresh = True
for wp, ammo in GUNS:
    ok = False
    for attempt in range(2):
        r = cmd(f"give {wp}", 3.5)
        g = gun()
        if g and g != "none" and g.lower() == wp.lower():
            ok = True
            break
        if fresh:
            break                 # a fresh game could not give it either: not a gun in this game, no restart
        print(f"{wp}: no model ({r}); the inventory may be full: restarting the game once", flush=True)
        close()
        boot()
        fresh = True
    if not ok:
        print(f"{wp}: SKIPPED (no such gun in this game)", flush=True)
        continue
    fresh = False
    cmd("start draw_idle", 0.3)
    time.sleep(4)
    cmd("stop", 0.5)
    # a gifted gun's slot names no ammo (bullet id 0) and the game will not reload it: name one, add some, until the
    # game says rounds are ready (RE2 2026-11-02: set_BulletID 15 on the Matilda -> reloadable 82, and R reloaded)
    sp = "no ammo found"
    r0 = cmd("ammo", 1.0)
    m0 = re.search(r"reloadable (\d+)", r0)
    tries = [] if (m0 and int(m0.group(1)) > 0) else [x for x in ammo.split("/") if x] + [str(i) for i in range(15, 41)]
    if not tries:
        sp = f"its own ammo ({r0})"
    for a in tries:
        cmd(f"slotcall set_BulletID {a}", 0.8)
        cmd(f"additem {a} 60", 0.8)
        r = cmd("ammo", 1.0)
        m = re.search(r"reloadable (\d+)", r)
        if m and int(m.group(1)) > 0:
            sp = f"ammo item {a} ({r})"
            break
    e = cmd("empty", 1.0)
    cmd("start reload_full", 0.3)
    key("r")
    for _ in range(11):           # keep the window in front: these games pause the moment it is not
        front()
        time.sleep(1)
    s = cmd("stop", 0.5)
    print(f"{wp}: {g} recorded; {sp}; {e}; {s}", flush=True)
    done.append(wp)
print("done:", ",".join(done), flush=True)
