"""lookat_read.py <file.user.2> ... : list the per-joint records of an RE2 survivor LookAt profile.

Record shape (app.ropeway.LookAtUserData.AlterJointData, il2cpp dump): u32 len, UTF-16 name (len chars incl.
NUL, 4-aligned), PitchDeg Range (2 f32), YawDeg Range (2 f32), IsEnableWorldXPreCorrect u32, WorldPitchDeg
Range (2 f32), IsEnableInherit u32. The LimitJoint record before them has the same name prefix but a
different tail (kind + 4 limits), so it is printed separately by its shape."""
import struct, sys, os

def records(d):
    out = []
    i = 0x100
    while i < len(d) - 12:
        n = struct.unpack_from("<I", d, i)[0]
        if 2 <= n <= 24 and i + 4 + 2 * n <= len(d):
            raw = d[i + 4:i + 4 + 2 * n]
            try:
                s = raw.decode("utf-16-le")
            except Exception:
                i += 4; continue
            if s.endswith("\0") and all(32 <= ord(c) < 127 for c in s[:-1]) and len(s) > 1:
                name = s[:-1]
                p = i + 4 + 2 * n
                p = (p + 3) & ~3
                vals = struct.unpack_from("<8I", d, p) if p + 32 <= len(d) else None
                out.append((i, name, p))
                i = p
                continue
        i += 4
    return out

def f(x):
    return struct.unpack("<f", struct.pack("<I", x))[0]

for path in sys.argv[1:]:
    d = open(path, "rb").read()
    print("=== %s (%d bytes)" % (os.path.basename(path), len(d)))
    for off, name, p in records(d):
        w = struct.unpack_from("<9I", d, p)
        # AlterJointData tail: pitch(2) yaw(2) precorrect world(2) inherit  -> flags are 0/1
        if w[4] in (0, 1) and w[7] in (0, 1) and all(abs(f(x)) < 400 for x in (w[0], w[1], w[2], w[3], w[5], w[6])):
            print("  %-8s @%04x  pitch %6.1f..%6.1f  yaw %6.1f..%6.1f  preX=%d  worldPitch %6.1f..%6.1f  inherit=%d"
                  % (name, p, f(w[0]), f(w[1]), f(w[2]), f(w[3]), w[4], f(w[5]), f(w[6]), w[7]))
        else:
            print("  %-8s @%04x  (other record) %s" % (name, p, " ".join("%.1f" % f(x) if 0 < x < 0xC8000000 and x not in (1, 2, 3) else str(x) for x in w[:9])))
