"""lookat_patch.py -- keep the ordinary body posture while aiming (Visceral, item 22 second half).

RE2 bends the survivor's spine toward the aim point with a per-stance LookAt profile
(natives/stm/sectionroot/userdata/character/survivor/pl0000/lookat/*.user.2, type
app.ropeway.LookAtUserData). Default.user.2 lets spine_1/spine_2 pitch about -5..10 deg; Hold_HG.user.2
lets spine_2 pitch -30..75 deg plus a world-pitch pre-correction of 0..55 deg, and spine_0/spine_1 more on
top [measured 2026-09-24]. That extra bend is the "hunched" aim posture. This script copies each Hold*
profile and rewrites its spine_0 / spine_1 / spine_2 records to Default's numbers, in place (same size, so
the RSZ layout is untouched). Head and neck records are left alone.

Usage: py lookat_patch.py --game-dir "<RE2 folder>" --out <folder> [--zero]
       py lookat_patch.py --in <folder with the pulled originals> --out <folder> [--zero]
--zero writes 0..0 ranges instead of Default's (no spine bend at all while aiming).
--zero-yaw keeps Default's pitch but sets the spine YAW ranges to 0..0 (no torso twist toward the target
while aiming; Tefa 2026-09-24: "torso twisting while feet stay put when AIM is active").
Copy the output's natives/... tree into <RE2>/ with REFramework's LooseFileLoader_Enabled=true.
Only this script ships; the output is made from the player's own game data.
"""
import argparse, os, struct, subprocess, sys

LOOKAT_DIR = "natives/stm/sectionroot/userdata/character/survivor/pl0000/lookat"
HOLD_FILES = ["hold", "hold_hg", "hold_hg_hp", "hold_sg", "hold_gg", "hold_rl", "hold_hacktool", "holdgrenade", "holdknife"]

# Default.user.2 spine records [measured 2026-09-24]: pitch, yaw, preX, worldPitch, inherit
DEFAULT_SPINE = {
    "spine_2": ((-5.0, 10.0), (-7.0, 14.0), 0, (0.0, 0.0), 1),
    "spine_1": ((-5.0, 10.0), (-7.0, 14.0), 0, (-5.0, 10.0), 1),
    "spine_0": ((0.0, 0.0), (0.0, 0.0), 0, (0.0, 0.0), 1),   # Default has no spine_0 record: no bend
}
ZERO_SPINE = {k: ((0.0, 0.0), (0.0, 0.0), 0, (0.0, 0.0), 1) for k in DEFAULT_SPINE}
ZERO_YAW_SPINE = {k: (v[0], (0.0, 0.0), v[2], v[3], v[4]) for k, v in DEFAULT_SPINE.items()}

# Default.user.2 head/neck records [measured 2026-09-24]; neck_0 has none in Default -> no bend
DEFAULT_HEAD = {
    "head":   ((-5.0, 15.0), (-20.0, 30.0), 0, (-15.0, 30.0), 1),
    "neck_1": ((-5.0, 15.0), (-20.0, 30.0), 0, (0.0, 0.0), 1),
    "neck_0": ((0.0, 0.0), (0.0, 0.0), 0, (0.0, 0.0), 1),
}


def alter_records(d):
    """Yield (name, tail_offset) for every AlterJointData record: u32 len, UTF-16 name, 4-aligned, 8 dwords."""
    i = 0x100
    while i < len(d) - 40:
        n = struct.unpack_from("<I", d, i)[0]
        if 2 <= n <= 24 and i + 4 + 2 * n <= len(d):
            try:
                s = d[i + 4:i + 4 + 2 * n].decode("utf-16-le")
            except Exception:
                i += 4; continue
            if s.endswith("\0") and len(s) > 1 and all(32 <= ord(c) < 127 for c in s[:-1]):
                p = (i + 4 + 2 * n + 3) & ~3
                w = struct.unpack_from("<8I", d, p)
                flags_ok = w[4] in (0, 1) and w[7] in (0, 1)
                if flags_ok:
                    yield s[:-1], p
                i = p
                continue
        i += 4


def patch(d, table, log):
    d = bytearray(d)
    done = []
    for name, p in list(alter_records(bytes(d))):
        if name not in table:
            continue
        pitch, yaw, prex, world, inherit = table[name]
        old = struct.unpack_from("<ffffIffI", d, p)
        struct.pack_into("<ffffIffI", d, p, pitch[0], pitch[1], yaw[0], yaw[1], prex, world[0], world[1], inherit)
        log("    %-8s pitch %5.0f..%-5.0f yaw %5.0f..%-5.0f preX=%d world %5.0f..%-5.0f  ->  pitch %5.0f..%-5.0f yaw %5.0f..%-5.0f preX=%d world %5.0f..%-5.0f"
            % (name, old[0], old[1], old[2], old[3], old[4], old[5], old[6], pitch[0], pitch[1], yaw[0], yaw[1], prex, world[0], world[1]))
        done.append(name)
    return bytes(d), done


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-dir")
    ap.add_argument("--in", dest="indir", help="folder holding natives/... with the pulled originals")
    ap.add_argument("--out", required=True)
    ap.add_argument("--zero", action="store_true")
    ap.add_argument("--zero-yaw", action="store_true")
    ap.add_argument("--head-default", action="store_true",
                    help="also set the head / neck_1 / neck_0 records to Default's (2026-09-24: the VR camera hangs off the head bone)")
    a = ap.parse_args()
    here = os.path.dirname(os.path.abspath(__file__))
    src = a.indir
    if a.game_dir:
        src = os.path.join(a.out, "_originals")
        paths = ["%s/%s.user.2" % (LOOKAT_DIR, f) for f in HOLD_FILES]
        r = subprocess.run([sys.executable, os.path.join(here, "pak_pull.py"), a.game_dir, src] + paths, capture_output=True, text=True)
        if r.returncode != 0:
            raise SystemExit("pak pull failed:\n" + r.stdout + r.stderr)
    if not src:
        raise SystemExit("give --game-dir or --in")
    table = dict(ZERO_SPINE if a.zero else (ZERO_YAW_SPINE if a.zero_yaw else DEFAULT_SPINE))
    if a.head_default:
        table.update(DEFAULT_HEAD)
    for f in HOLD_FILES:
        ip = os.path.join(src, LOOKAT_DIR, f + ".user.2")
        if not os.path.exists(ip):
            print("  %s: missing, skipped" % f); continue
        d = open(ip, "rb").read()
        print("%s.user.2 (%d bytes)" % (f, len(d)))
        out, done = patch(d, table, print)
        assert len(out) == len(d)
        op = os.path.join(a.out, LOOKAT_DIR, f + ".user.2")
        os.makedirs(os.path.dirname(op), exist_ok=True)
        open(op, "wb").write(out)
        # verify: re-read and check every spine record now carries the table values
        back = dict(alter_records(out))
        for name in done:
            w = struct.unpack_from("<ffffIffI", out, back[name])
            want = table[name]
            assert (w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]) == (want[0][0], want[0][1], want[1][0], want[1][1], want[2], want[3][0], want[3][1], want[4]), name
        print("    verified %d record(s) rewritten (%s), %d bytes unchanged in size" % (len(done), ",".join(done), len(out)))
    print("install: copy %s/natives into <RE2>/ (LooseFileLoader_Enabled=true)" % a.out)


if __name__ == "__main__":
    main()
