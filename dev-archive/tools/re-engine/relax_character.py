"""relax_character.py -- the relaxed (no-weapon) pose on EVERY weapon of one character, in one go (2026-10-05).

Finds the character's weapon lists in the game's own archive by name hash (nothing extracted that is not needed),
pulls them, and runs motlist_relax.py's three modes over them:
  base_<wp>_hold -> base-hold   base_<wp>_move -> base-move   <wp>_hold_* overrides -> override (walk slots emptied)
Lists with nothing to change are skipped. Output lands under --out/natives/..., ready to copy over the game folder.

  py relax_character.py --game-dir "<RE2>" --character pl10 --out <folder>
Characters found in RE2 (2026-10-05): pl00 Leon, pl10 Claire, pl20 Ada, pl64 (checked by motion names).
Legitimacy: reads the player's own paks, writes only into --out; no game data in this file.
"""
import argparse, glob, itertools, os, struct, subprocess, sys
import mmh3
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import motlist_splice as ms
import motlist_relax as mr

WEAPONS = ["hdg", "mag", "smg", "stg", "gnl", "rkl", "etc", "mle", "sup"]
LIST = "natives/stm/sectionroot/animation/player/%s/list/%s/%s.motlist.524"


def h32(s):
    return mmh3.hash(s.encode("utf-16-le"), 0xFFFFFFFF, signed=False)


def archive_keys(game):
    keys = set()
    for p in glob.glob(os.path.join(game, "re_chunk_000.pak*")):
        if os.path.getsize(p) < 16:
            continue
        with open(p, "rb") as f:
            total = struct.unpack("<IBBHII", f.read(16))[4]
            raw = f.read(total * 48)
        for i in range(total):
            keys.add(struct.unpack_from("<II", raw, i * 48))
    return keys


def find_lists(keys, pl):
    def has(path):
        return (h32(path.lower()), h32(path.upper())) in keys
    cps = [""] + ["_cp" + "".join(c) for n in (1, 2, 3, 4) for c in itertools.combinations("ABCDE", n)]
    sts = ["", "_stLIGHT", "_stWATER", "_stCOMBAT", "_stNORMAL", "_stTENSION"]
    cns = ["", "_cnFINE", "_cnCAUTION", "_cnDANGER"]
    found = []
    for wp in WEAPONS:
        for kind in ("hold", "move"):
            p = LIST % (pl, wp, "base_%s_%s" % (wp, kind))
            if has(p):
                found.append(("base-" + kind, wp, p))
        for cn, st, cp, n in itertools.product(cns, sts, cps, range(1, 10)):
            p = LIST % (pl, wp, "%s_hold%s%s%s_%02d" % (wp, cn, st, cp, n))
            if has(p):
                found.append(("override", wp, p))
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--character", required=True, help="pl00, pl10, pl20, pl64 ...")
    ap.add_argument("--out", required=True)
    ap.add_argument("--work", help="where pulled originals go (default <out>/_originals)")
    a = ap.parse_args()
    pl, work = a.character, a.work or os.path.join(a.out, "_originals")
    lists = find_lists(archive_keys(a.game_dir), pl)
    cmn = LIST % (pl, "cmn", "base_cmn_move")
    paths = [cmn] + [p for _, _, p in lists]
    r = subprocess.run([sys.executable, os.path.join(HERE, "pak_pull.py"), a.game_dir, work] + paths, capture_output=True, text=True)
    if r.returncode or "MISS" in r.stdout:
        raise SystemExit("pull failed:\n" + r.stdout + r.stderr)
    local = lambda p: os.path.join(work, p.replace("/", os.sep))
    cm = ms.Motlist(local(cmn))
    quiet = lambda *x: None
    done, skipped = [], []
    for mode, wp, p in lists:
        src, dst = local(p), os.path.join(a.out, p.replace("/", os.sep))
        name = p.split("/")[-1].replace(".motlist.524", "")
        if mode == "override":
            out, changed = mr.override(open(src, "rb").read(), quiet)
        else:
            lst = ms.Motlist(src)
            if lst.num == 0 or not any(lst.slots):   # all slots empty: the character borrows another's set
                skipped.append(name); continue
            out, changed = (mr.base_hold if mode == "base-hold" else mr.base_move)(lst, cm, quiet)
            if not ms.verify(out, lst, cm, changed, quiet):
                raise SystemExit("VERIFY FAIL on " + name)
        if not changed:
            skipped.append(name); continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, "wb").write(out)
        done.append("%-34s %2d slots" % (name, len(changed)))
    print("%s: %d lists found, %d written, %d need nothing" % (pl, len(lists), len(done), len(skipped)))
    for d in done:
        print("  wrote", d)


if __name__ == "__main__":
    main()
