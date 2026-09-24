"""re2focus.py : bring the RE2 window to the foreground (the plugin's numpad hotkeys are ignored unless the game
is the foreground window -- poll_hotkeys() checks game_is_foreground()). Use before re2drive.py num N."""
import ctypes, time
u = ctypes.windll.user32
hwnd = u.FindWindowW(None, "RESIDENT EVIL 2")
if not hwnd:
    raise SystemExit("no RESIDENT EVIL 2 window")
u.ShowWindow(hwnd, 9)          # SW_RESTORE
u.SetForegroundWindow(hwnd)
time.sleep(0.3)
print("foreground:", u.GetForegroundWindow() == hwnd)
