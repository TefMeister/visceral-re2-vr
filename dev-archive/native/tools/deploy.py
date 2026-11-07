"""deploy.py -- put exactly the install list's files for one layer into the game folder (the clean rebuild, 2026-11-07).
Shared by RE2 and RE3 (RE2's repo owns it); the game, its backup and its removed-files folder come from game.conf beside it.

    python -I deploy.py --layer N [--game DIR] [--backup DIR] [--removed DIR] [--skip-missing] [--dry]

Every file of OURS in the game folder that the chosen layer does not list is MOVED to --removed/<time>/ (never deleted).
Ours = anything under natives/ (only our loose files live there), anything under reframework/ named visceral_* or inside
reframework/data/visceral/ or data/custom_sfx/, any game path the install list names on any layer, and every gen-1 path of
the 2026-11-07 inventory except the tools', the logs' and the left-alone ones. Tools (the REFramework MCP plugin, Hand
Poser's files, Enhanced Model Viewer) install themselves and are never touched.
A missing source stops the deploy before anything changes, unless --skip-missing.
"""
import argparse, filecmp, os, shutil, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)                                     # dev-archive/native
DEV = os.path.dirname(NATIVE)                                      # dev-archive
INVENTORY = os.path.join(DEV, "rebuild", "2026-11-07-inventory.tsv")


def read_conf(path=os.path.join(HERE, "game.conf")):
    """this repo's game: game_dir, backup, removed ... (the backup and removed folders stay outside every git repo)"""
    conf = {}
    for line in open(path, encoding="utf-8"):
        line = line.split("#", 1)[0].strip()
        if "=" in line:
            k, v = line.split("=", 1)
            conf[k.strip()] = v.strip()
    return conf


CONF = read_conf()
GAME, BACKUP, REMOVED = CONF["game_dir"], CONF["backup"], CONF["removed"]
NOT_OURS_VERDICTS = ("TOOL", "LEAVE", "LOG")


def read_list(path):
    lines = []
    for n, raw in enumerate(open(path, encoding="utf-8"), 1):
        p = raw.split("#", 1)[0].split() if not raw.lstrip().startswith("#") else []
        if not p or p[0].startswith("#"):
            continue
        layer, mx = int(p[0]), None
        rest = p[1:]
        if rest[0].startswith("max="):
            mx, rest = int(rest[0][4:]), rest[1:]
        if len(rest) != 2:
            sys.exit("install-list.txt line %d: want <layer> [max=N] <source> <game path>" % n)
        lines.append((layer, mx, rest[0], rest[1].replace("\\", "/")))
    return lines


def source_path(src, backup):
    if src.startswith("repo:"):
        return os.path.normpath(os.path.join(NATIVE, src[5:]))
    if src.startswith("backup:"):
        return os.path.normpath(os.path.join(backup, src[7:]))
    sys.exit("unknown source kind: " + src)


def expand(src_abs, game_rel):
    """(source file, game path) pairs; a folder source gives one pair per file inside it."""
    if os.path.isdir(src_abs):
        out = []
        for dp, _, fs in os.walk(src_abs):
            for f in fs:
                rel = os.path.relpath(os.path.join(dp, f), src_abs).replace("\\", "/")
                out.append((os.path.join(dp, f), game_rel.rstrip("/") + "/" + rel))
        return out
    return [(src_abs, game_rel)]


def is_ours(rel, listed_anywhere, gen1):
    r = rel.replace("\\", "/")
    name = r.rsplit("/", 1)[-1]
    if r.startswith("natives/"):
        return True
    if r.startswith("reframework/") and (name.startswith("visceral_") or r.startswith("reframework/data/visceral/")
                                          or r.startswith("reframework/data/custom_sfx/")):
        return True
    if r in listed_anywhere or r in gen1:
        return True
    return any(r.startswith(d + "/") for d in listed_anywhere if not os.path.splitext(d)[1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--layer", type=int, required=True)
    ap.add_argument("--game", default=GAME)
    ap.add_argument("--backup", default=BACKUP)
    ap.add_argument("--removed", default=REMOVED)
    ap.add_argument("--skip-missing", action="store_true")
    ap.add_argument("--dry", action="store_true")
    ap.add_argument("--list", default=os.path.join(HERE, "install-list.txt"))
    a = ap.parse_args()

    lines = read_list(a.list)
    listed_anywhere = {g for _, _, _, g in lines}
    gen1 = set()
    if os.path.exists(INVENTORY):
        for row in list(open(INVENTORY, encoding="utf-8"))[1:]:
            c = row.rstrip("\n").split("\t")
            if len(c) == 4 and not c[3].startswith(NOT_OURS_VERDICTS):
                gen1.add(c[0])

    wanted, missing = {}, []
    for layer, mx, src, game_rel in lines:
        if layer > a.layer or (mx is not None and a.layer > mx):
            continue
        s = source_path(src, a.backup)
        if not os.path.exists(s):
            missing.append(src)
            continue
        for sf, gr in expand(s, game_rel):
            wanted[gr] = sf
    if missing and not a.skip_missing:
        print("STOP, nothing changed: missing source(s):\n  " + "\n  ".join(missing))
        return 2
    for m in missing:
        print("skipped (missing source):", m)

    stamp = time.strftime("%Y-%m-%d_%H-%M-%S")
    moved = 0
    for dp, _, fs in os.walk(a.game):
        for f in fs:
            full = os.path.join(dp, f)
            rel = os.path.relpath(full, a.game).replace("\\", "/")
            if rel in wanted or not is_ours(rel, listed_anywhere, gen1):
                continue
            dest = os.path.join(a.removed, stamp, rel)
            print("out:", rel)
            if not a.dry:
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                shutil.move(full, dest)
            moved += 1
    copied = same = 0
    for gr, sf in sorted(wanted.items()):
        dest = os.path.join(a.game, gr)
        if os.path.exists(dest) and filecmp.cmp(sf, dest, shallow=False):
            same += 1
            continue
        print("in: ", gr)
        if not a.dry:
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(sf, dest)
            if not filecmp.cmp(sf, dest, shallow=False):
                print("COPY FAILED:", gr)
                return 3
        copied += 1
    for dp, ds, fs in sorted(os.walk(a.game), key=lambda t: -len(t[0])):   # deepest first: empty folders we emptied
        r = os.path.relpath(dp, a.game).replace("\\", "/")
        if (r.startswith(("reframework/", "natives")) and not os.listdir(dp)) and not a.dry:
            os.rmdir(dp)
    print("layer %d: %d in, %d already right, %d moved out%s" % (a.layer, copied, same, moved, " (dry run)" if a.dry else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
