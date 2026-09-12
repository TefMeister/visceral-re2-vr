"""mouse_look.py -- turn the RE2 camera from outside the game (2026-09-12, /lm lane).

The automation profile carried "character+camera: not exercised" from 2026-09-04 until this script,
which closes it. RE2 takes RELATIVE mouse motion through SendInput and turns the view with it, so a
session can navigate on its own instead of being stuck facing wherever the save happens to point.

  python mouse_look.py <dx> <dy> <steps>

dx/dy are per-step deltas and `steps` is how many are sent, ~15 ms apart -- many small steps, the
way a real mouse arrives. A single large jump is NOT equivalent and is often swallowed.
Measured on this machine `[verified-live 2026-09-12]`: 25 x 40 steps = a clear turn (mean pixel
delta 10.7 against a static scene); 25 x 160 steps = roughly turning around.

Focus is taken first through the shared harness, because synthetic input follows focus.
"""
import ctypes, ctypes.wintypes as w, time, sys, importlib.util

TOOLKIT = r"C:\Users\TD3KX\github-backups\flat-to-vr-RE-toolkit\tools\game-harness.py"
spec = importlib.util.spec_from_file_location("harness", TOOLKIT)
H = importlib.util.module_from_spec(spec)
spec.loader.exec_module(H)

u = ctypes.windll.user32
try:
    H.focus()
except Exception:
    pass


class MI(ctypes.Structure):
    _fields_ = [("dx", w.LONG), ("dy", w.LONG), ("mouseData", w.DWORD),
                ("dwFlags", w.DWORD), ("time", w.DWORD),
                ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong))]


class _I(ctypes.Union):
    _fields_ = [("mi", MI)]


class INPUT(ctypes.Structure):
    _fields_ = [("type", w.DWORD), ("u", _I)]


MOUSEEVENTF_MOVE = 0x0001


def look(dx, dy, steps, gap=0.015):
    for _ in range(steps):
        inp = INPUT(type=0, u=_I(mi=MI(dx, dy, 0, MOUSEEVENTF_MOVE, 0, None)))
        u.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))
        time.sleep(gap)


if __name__ == "__main__":
    dx, dy, n = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    look(dx, dy, n)
    print("moved %d,%d over %d steps" % (dx * n, dy * n, n))
