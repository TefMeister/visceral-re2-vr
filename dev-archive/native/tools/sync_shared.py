"""sync_shared.py -- copy the files RE2 and RE3 share from this repo (the owner) into visceral-re3-vr (2026-11-07, the clean
rebuild's decision D1: one copy of the gun code, a fix lands in both).

    python -I sync_shared.py [--to PATH_TO_RE3_NATIVE] [--dry] [--force]

What is shared: SHARED below (folders copied whole, files one by one). What is NOT: src/body/ (each game's body), src/game/
except game.h (each game's answers and gun table, its plugin.cpp and weapons.cpp), lua/ except the bridge and jitterlog,
tools/game.conf and tools/install-list.txt (each game's own).

A hash list (.shared-manifest.txt in the target) records what was copied. If a shared file in the target no longer matches
it, someone edited RE3's copy: the sync STOPS and names the file (edit it here instead, or --force to overwrite).
"""
import argparse, hashlib, os, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
DEFAULT_TO = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(NATIVE))), "visceral-re3-vr", "dev-archive", "native")
SHARED = [
    "CMakeLists.txt", "include", "third_party",
    "src/core", "src/guns", "src/hands", "src/tools", "src/game/game.h",
    "lua/visceral_bridge.lua", "lua/jitterlog.lua", "lua/visceral_hand_smooth.lua",
    "tools/deploy.py", "tools/build.sh", "tools/test_deploy.py", "tools/sim_boot.py", "tools/sync_shared.py", "tools/clips",
]
MANIFEST = ".shared-manifest.txt"
SKIP_DIRS = {"__pycache__"}


def files_of(root, rel):
    p = os.path.join(root, rel)
    if os.path.isfile(p):
        return [rel]
    out = []
    for dp, ds, fs in os.walk(p):
        ds[:] = [d for d in ds if d not in SKIP_DIRS]
        for f in fs:
            out.append(os.path.relpath(os.path.join(dp, f), root).replace("\\", "/"))
    return out


def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--to", default=DEFAULT_TO)
    ap.add_argument("--dry", action="store_true")
    ap.add_argument("--force", action="store_true")
    a = ap.parse_args()
    man_path = os.path.join(a.to, MANIFEST)
    old = {}
    if os.path.exists(man_path):
        for line in open(man_path, encoding="utf-8"):
            h, rel = line.rstrip("\n").split("  ", 1)
            old[rel] = h
    edited = [rel for rel, h in old.items() if os.path.exists(os.path.join(a.to, rel)) and sha(os.path.join(a.to, rel)) != h]
    if edited and not a.force:
        print("STOP: these shared files were edited in the target (edit them in RE2's repo instead, or --force):")
        for rel in edited:
            print("  " + rel)
        return 2
    rels = sorted({r for item in SHARED for r in files_of(NATIVE, item)})
    copied = 0
    for rel in rels:
        src, dst = os.path.join(NATIVE, rel), os.path.join(a.to, rel)
        if os.path.exists(dst) and sha(dst) == sha(src):
            continue
        print("copy", rel)
        copied += 1
        if not a.dry:
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
    gone = [rel for rel in old if rel not in rels and os.path.exists(os.path.join(a.to, rel))]
    for rel in gone:   # a file the owner deleted goes from the target too
        print("remove", rel)
        if not a.dry:
            os.remove(os.path.join(a.to, rel))
    if not a.dry:
        with open(man_path, "w", encoding="utf-8", newline="\n") as f:
            for rel in rels:
                f.write(sha(os.path.join(NATIVE, rel)) + "  " + rel + "\n")
    print("%d shared files, %d copied, %d removed%s" % (len(rels), copied, len(gone), " (dry run)" if a.dry else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
