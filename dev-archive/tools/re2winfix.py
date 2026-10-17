# re2winfix.py -- when x64dbg is open: close its "Assemble at" box, minimise it, give the game the foreground. 2026-10-17.
import ctypes, ctypes.wintypes as w, time
u = ctypes.windll.user32; k = ctypes.windll.kernel32
wins = []
@ctypes.WINFUNCTYPE(ctypes.c_bool, w.HWND, w.LPARAM)
def cb(h, l):
    if u.IsWindowVisible(h):
        n = u.GetWindowTextLengthW(h); b = ctypes.create_unicode_buffer(n + 1); u.GetWindowTextW(h, b, n + 1)
        if b.value: wins.append((h, b.value))
    return True
u.EnumWindows(cb, 0)
for h, t in wins:
    if t.startswith("Assemble at"):
        u.PostMessageW(h, 0x0010, 0, 0); print("closed dialog:", t)      # WM_CLOSE = Cancel
time.sleep(0.5)
for h, t in wins:
    if t.endswith("x64dbg"):
        u.ShowWindow(h, 6); print("minimised:", t)                       # SW_MINIMIZE
game = u.FindWindowW(None, "RESIDENT EVIL 2")
# foreground with the AttachThreadInput trick
fg = u.GetForegroundWindow(); pid = w.DWORD()
tf = u.GetWindowThreadProcessId(fg, ctypes.byref(pid)); tc = k.GetCurrentThreadId()
u.AttachThreadInput(tc, tf, True); u.ShowWindow(game, 9); u.SetForegroundWindow(game); u.BringWindowToTop(game); u.AttachThreadInput(tc, tf, False)
time.sleep(0.5)
fg = u.GetForegroundWindow(); n = u.GetWindowTextLengthW(fg); b = ctypes.create_unicode_buffer(n + 1); u.GetWindowTextW(fg, b, n + 1)
print("foreground now:", repr(b.value), "is game:", fg == game)
