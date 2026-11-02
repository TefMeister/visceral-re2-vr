import sys, os, time, glob, ctypes, math, random
VB = sorted(glob.glob(os.path.expanduser('~/.claude/plugins/cache/*/lanes/*/tools/vrbench')))[-1]
sys.path.insert(0, VB)
import vb_input as I
import vb_settings as VS
u = ctypes.windll.user32
def front(title):
    h = u.FindWindowW(None, title)
    if h: u.ShowWindow(h, 9); u.SetForegroundWindow(h)
    return h
RIGHT = I.pose_from_ypr((0.20, 1.45, -0.40), 0, 0)
LEFT = I.pose_from_ypr((-0.20, 1.20, -0.30), 0, 0)
def write(right=RIGHT, left=LEFT, **btn):
    inp = I.Input().mode(poses="set", buttons="set")
    inp.hand(VS.HAND_LEFT, grip=left, aim=left)
    inp.hand(VS.HAND_RIGHT, grip=right, aim=right, **btn)
    I.write(inp)
def press(b, title):
    front(title); write(**{b: True}); time.sleep(0.25); write(); time.sleep(0.4)
if __name__ == '__main__':
    title = sys.argv[1]; act = sys.argv[2]
    if act == 'press':
        for b in sys.argv[3:]:
            press(b, title); time.sleep(2.5)
    elif act == 'shake':   # hold the right hand still with a tremor: ~0.4 deg at 7-9 Hz, for N seconds
        secs = float(sys.argv[3]); t0 = time.time(); front(title)
        while time.time() - t0 < secs:
            t = time.time() - t0
            yaw = 0.35 * math.sin(2 * math.pi * 8.0 * t) + 0.1 * random.uniform(-1, 1)
            pit = 0.30 * math.sin(2 * math.pi * 7.0 * t + 1.0) + 0.1 * random.uniform(-1, 1)
            dx = 0.0015 * math.sin(2 * math.pi * 9.0 * t)
            write(right=I.pose_from_ypr((0.20 + dx, 1.45, -0.40), yaw, pit))
            time.sleep(1 / 90)
        write()
