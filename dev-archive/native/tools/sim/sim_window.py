"""sim_window.py [w h] -- put the OpenXR Simulator's preview window back to a useful size (default 1300x760) without
clicking it (a click starts its mouse-look). It shrinks to a strip by itself, and screenshots come out tiny. 2026-11-19."""
import ctypes, ctypes.wintypes as w, sys
u = ctypes.windll.user32
W, H = (int(sys.argv[1]), int(sys.argv[2])) if len(sys.argv) >= 3 else (1300, 760)
hits = []
@ctypes.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
def cb(h, _):
    t = ctypes.create_unicode_buffer(200); u.GetWindowTextW(h, t, 200)
    if u.IsWindowVisible(h) and "openxr" in t.value.lower() and "simulator" in t.value.lower(): hits.append((h, t.value))
    return True
u.EnumWindows(cb, 0)
for h, t in hits:
    r = w.RECT(); u.GetWindowRect(h, ctypes.byref(r))
    u.MoveWindow(h, r.left, r.top, W, H, True)
    print("resized '%s' from %dx%d to %dx%d" % (t, r.right - r.left, r.bottom - r.top, W, H))
if not hits: print("no simulator window found")
