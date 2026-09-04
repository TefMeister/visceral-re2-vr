"""
re2drive.py - drive Resident Evil 2 (2019) + REFramework + the Visceral plugin from outside.

Mechanism comes from flat-to-vr-RE-toolkit/tools/game-harness.py (BitBlt capture, scancode
keys, focus first). What this file adds is RE2-specific and comes from
ai-game-control-profiles/profiles/resident-evil-2-2019.json:
  - keys the harness table lacks: 1-4 (weapon slots), R (reload), INSERT (REFramework overlay,
    an EXTENDED key)
  - numpad hotkeys by VIRTUAL KEY (wVk 0x60+n, no SCANCODE flag): the plugin polls
    GetAsyncKeyState(VK_NUMPADn), and a numpad scancode with NumLock off reports as HOME/END.
  - the REFramework log as the state oracle (PLAYER BOUND, WEAPON CHANGED, [visceral] lines)

Usage:
    python re2drive.py shot out.png
    python re2drive.py key insert|enter|space|w|s|r|1|2|3|4
    python re2drive.py num 7                  # NUMPAD7 by virtual key
    python re2drive.py wait-log "PLAYER BOUND" 60   # seconds; watches only NEW log bytes
    python re2drive.py tail 40                # last N [visceral] lines
    python re2drive.py watch                  # rendering?
    python re2drive.py close                  # WM_CLOSE to the main window, wait for exit
"""
import ctypes, ctypes.wintypes as w, importlib.util, os, sys, time

TOOLKIT = r"C:\Users\TD3KX\github-backups\flat-to-vr-RE-toolkit\tools\game-harness.py"
GAME = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2"
LOG = os.path.join(GAME, "re2_framework_log.txt")
WINDOW = "RESIDENT EVIL 2"
MARK = os.path.join(os.environ.get("TEMP", "."), "re2drive.logmark")

spec = importlib.util.spec_from_file_location("harness", TOOLKIT)
H = importlib.util.module_from_spec(spec); spec.loader.exec_module(H)

EXTRA = {"1": (0x02, False), "2": (0x03, False), "3": (0x04, False), "4": (0x05, False),
         "r": (0x13, False), "insert": (0x52, True)}
H.KEYS.update(EXTRA)
u = ctypes.windll.user32


def num(n, hold=0.07):
    vk = 0x60 + int(n)
    down = H.INPUT(type=H.INPUT_KEYBOARD, u=H._I(ki=H.KEYBDINPUT(vk, 0, 0, 0, None)))
    up = H.INPUT(type=H.INPUT_KEYBOARD, u=H._I(ki=H.KEYBDINPUT(vk, 0, H.KEYEVENTF_KEYUP, 0, None)))
    u.SendInput(1, ctypes.byref(down), ctypes.sizeof(H.INPUT)); time.sleep(hold)
    u.SendInput(1, ctypes.byref(up), ctypes.sizeof(H.INPUT)); time.sleep(0.2)


def log_size():
    try: return os.path.getsize(LOG)
    except OSError: return 0


def mark():
    with open(MARK, "w") as f: f.write(str(log_size()))


def since_mark():
    try: off = int(open(MARK).read().strip())
    except Exception: off = 0
    if off > log_size(): off = 0          # REFramework truncates the log at boot
    try:
        with open(LOG, "rb") as f:
            f.seek(off); return f.read().decode("utf-8", "replace")
    except OSError:
        return ""


def wait_log(needle, timeout):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if needle in since_mark(): return True
        time.sleep(1.0)
    return False


def tail(n):
    try: lines = open(LOG, "r", encoding="utf-8", errors="replace").read().splitlines()
    except OSError: return []
    return [l for l in lines if "[visceral]" in l][-n:]


if __name__ == "__main__":
    cmd, rest = sys.argv[1], sys.argv[2:]
    if cmd == "mark":
        mark(); print("log mark at", log_size()); sys.exit()
    if cmd == "wait-log":
        ok = wait_log(rest[0], float(rest[1]) if len(rest) > 1 else 60)
        print("FOUND" if ok else "TIMEOUT", rest[0]); sys.exit(0 if ok else 1)
    if cmd == "tail":
        print("\n".join(tail(int(rest[0]) if rest else 40))); sys.exit()
    if cmd == "since":
        print(since_mark()[-int(rest[0]) if rest else -20000:]); sys.exit()
    hwnd, title = H.find_window(WINDOW)
    if cmd == "close":
        u.PostMessageW(hwnd, 0x0010, 0, 0)
        for _ in range(30):
            time.sleep(1)
            if not u.IsWindow(hwnd): print("closed"); sys.exit()
        print("still open after 30 s"); sys.exit(1)
    if not H.focus(hwnd): print("WARNING: could not foreground", title)
    if cmd == "shot":
        H.grab(hwnd).save(rest[0]); print("saved", rest[0])
    elif cmd == "key":
        rep = int(rest[rest.index("--repeat") + 1]) if "--repeat" in rest else 1
        for _ in range(rep): H.tap(rest[0], settle=0.5)
        print("tapped", rest[0], "x", rep)
    elif cmd == "hold":
        H.hold(rest[0], float(rest[1])); print("held", rest[0], rest[1])
    elif cmd == "num":
        num(rest[0]); print("numpad", rest[0])
    elif cmd == "watch":
        ds = H.watch(hwnd, 6, 0.4)
        print("deltas:", ["%.2f" % x for x in ds], "->", "RENDERING" if max(ds) > 1.0 else "STATIC")
    else:
        raise SystemExit("unknown command " + cmd)
