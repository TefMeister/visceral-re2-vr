"""snapshot_build.py - keep a full copy of every Visceral build that goes into the game, one folder per change.

Tefa's rule (2026-09-26): "save every session's changed files and changes in D: visceral build versions, so
that we can go back step by step ... a new folder for every smallest change made, feature added and so on,
with a new mod version number." Space is fine; being able to step back is the point.

Each snapshot is a folder `D:\\Visceral build versions\\v0.2.0-bNNN - <title>\\` holding:
  - a full copy of everything of ours in the RE2 game folder (REFramework, DLSS files, config, natives,
    scripts, plugins), never the game's own files;
  - MANIFEST.sha256, and CHANGES.md: what changed since the previous build, the note, and the test result;
and a line in INDEX.md at the top. Nothing is ever overwritten; a new change is a new number.

  py snapshot_build.py "<title>" --note "<what changed and why>" [--result "<what Tefa saw>"] [--source DIR]
  py snapshot_build.py --result-for 3 "<what Tefa saw>"     (fill in a result later)
  py snapshot_build.py --list

`--source` snapshots a folder instead of the live game (for reconstructing a build already tested).
To go back to a build: copy its files (all but MANIFEST/CHANGES) over the game after removing ours.
"""
import argparse
import hashlib
import os
import re
import shutil
import sys
import time

GAME = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2"
HOME = r"D:\Visceral build versions"
SERIES = "0.2.0"
# Everything of ours that can live in the game folder. The game's own files are never copied.
OURS = ["dinput8.dll", "openxr_loader.dll", "openvr_api.dll", "PDPerfPlugin.dll", "nvngx_dlss.dll",
        "re2_fw_config.txt", "reframework_revision.txt", "reframework", "natives"]
SKIP_NAMES = {"re2_framework_log.txt", "reframework_accessed_files.txt", "reframework_loose_files.txt"}
VERSION_RE = re.compile(r"^v" + re.escape(SERIES) + r"-b(\d{3}) - ")


def builds():
    if not os.path.isdir(HOME):
        return []
    out = [(int(m.group(1)), name) for name in os.listdir(HOME) if (m := VERSION_RE.match(name))]
    return sorted(out)


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def manifest(folder):
    out = {}
    for root, _, files in os.walk(folder):
        for n in files:
            if n in ("MANIFEST.sha256", "CHANGES.md") or n in SKIP_NAMES:
                continue
            p = os.path.join(root, n)
            out[os.path.relpath(p, folder).replace("\\", "/")] = sha(p)
    return out


def read_manifest(folder):
    out = {}
    try:
        with open(os.path.join(folder, "MANIFEST.sha256"), encoding="utf-8") as f:
            for line in f:
                h, _, p = line.rstrip("\n").partition("  ")
                out[p] = h
    except OSError:
        pass
    return out


def safe_title(t):
    return re.sub(r"[^\w .,+()-]", "", t).strip()[:70]


def snapshot(title, note, result, source):
    os.makedirs(HOME, exist_ok=True)
    existing = builds()
    num = existing[-1][0] + 1 if existing else 1
    name = f"v{SERIES}-b{num:03d} - {safe_title(title)}"
    dest = os.path.join(HOME, name)
    os.makedirs(dest)
    src = source or GAME
    for item in OURS:
        p = os.path.join(src, item)
        if os.path.isdir(p):
            shutil.copytree(p, os.path.join(dest, item), ignore=shutil.ignore_patterns(*SKIP_NAMES))
        elif os.path.isfile(p):
            shutil.copy2(p, os.path.join(dest, item))
    now = manifest(dest)
    with open(os.path.join(dest, "MANIFEST.sha256"), "w", encoding="utf-8", newline="\n") as f:
        for p in sorted(now):
            f.write(f"{now[p]}  {p}\n")
    prev = read_manifest(os.path.join(HOME, existing[-1][1])) if existing else {}
    added = sorted(set(now) - set(prev))
    removed = sorted(set(prev) - set(now))
    changed = sorted(p for p in set(now) & set(prev) if now[p] != prev[p])
    lines = [f"# {name}", "", f"- **When:** {time.strftime('%Y-%m-%d %H:%M')}",
             f"- **Copied from:** {'the RE2 game folder' if not source else source}",
             f"- **Previous build:** {existing[-1][1] if existing else '(first)'}",
             f"- **What changed and why:** {note}", f"- **Result:** {result or '(not tested yet)'}", "",
             f"## Files compared with the previous build ({len(now)} files in this build)", ""]
    for label, items in (("Added", added), ("Removed", removed), ("Changed", changed)):
        lines.append(f"**{label}:** {len(items)}")
        lines.extend(f"- `{p}`" for p in items[:200])
        lines.append("")
    with open(os.path.join(dest, "CHANGES.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    index = os.path.join(HOME, "INDEX.md")
    if not os.path.isfile(index):
        with open(index, "w", encoding="utf-8", newline="\n") as f:
            f.write("# Visceral build versions\n\nOne folder per change, oldest first. Each holds a full copy of "
                    "everything of ours that was in the RE2 game folder, plus CHANGES.md.\n\n"
                    "| Build | When | What changed | Result |\n| --- | --- | --- | --- |\n")
    with open(index, "a", encoding="utf-8", newline="\n") as f:
        f.write(f"| {name} | {time.strftime('%Y-%m-%d %H:%M')} | {note} | {result or '(not tested yet)'} |\n")
    print(f"saved {name}: {len(now)} files (+{len(added)} -{len(removed)} ~{len(changed)} vs previous)")
    return dest


def set_result(num, result):
    match = [n for b, n in builds() if b == num]
    if not match:
        sys.exit(f"no build b{num:03d}")
    name = match[0]
    path = os.path.join(HOME, name, "CHANGES.md")
    with open(path, encoding="utf-8") as f:
        text = f.read()
    text = re.sub(r"^- \*\*Result:\*\* .*$", f"- **Result:** {result}", text, count=1, flags=re.M)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    index = os.path.join(HOME, "INDEX.md")
    with open(index, encoding="utf-8") as f:
        rows = f.read().splitlines()
    rows = [re.sub(r"\| [^|]* \|$", f"| {result} |", r) if r.startswith(f"| {name} |") else r for r in rows]
    with open(index, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(rows) + "\n")
    print(f"result recorded for {name}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("title", nargs="?")
    ap.add_argument("--note", default="")
    ap.add_argument("--result", default="")
    ap.add_argument("--source")
    ap.add_argument("--result-for", type=int, metavar="N")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    if a.list:
        for b, n in builds():
            print(n)
        return
    if a.result_for:
        set_result(a.result_for, a.title or a.result)
        return
    if not a.title or not a.note:
        ap.error("a title and --note are needed")
    snapshot(a.title, a.note, a.result, a.source)


if __name__ == "__main__":
    main()
