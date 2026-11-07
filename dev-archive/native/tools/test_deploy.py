"""test_deploy.py -- deploy.py installs exactly the install list and moves every other file of ours out (plan Task 5).

    python -I test_deploy.py        exit 0 = all checks passed
"""
import os, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEPLOY = os.path.join(HERE, "deploy.py")


def make(root, rel, text="x"):
    p = os.path.join(root, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        f.write(text)


def main():
    tmp = tempfile.mkdtemp(prefix="vn-deploy-test-")
    game, backup, removed = (os.path.join(tmp, n) for n in ("game", "backup", "removed"))
    try:
        # a fake backup holding every backup: source the list names, each with its own text
        for line in open(os.path.join(HERE, "install-list.txt"), encoding="utf-8"):
            parts = line.split()
            if not parts or parts[0].startswith("#"):
                continue
            src = [p for p in parts if p.startswith(("repo:", "backup:"))][0]
            if src.startswith("backup:"):
                rel = src[len("backup:"):]
                make(backup, rel if "." in os.path.basename(rel) else rel + "/inner.lua", "backup " + rel)
        # the game folder: a game file, two gen-1 files of ours, one tool file
        make(game, "re2.exe", "the game")
        make(game, "reframework/autorun/visceral_tuning.lua", "gen-1")
        make(game, "reframework/data/visceral_slide_grip.json", "gen-1")
        make(game, "natives/stm/sectionroot/animation/player/pl00/list/hdg/base_hdg_hold.motlist.524", "hold pose")
        make(game, "reframework/plugins/REFramework.NET.dll", "the MCP's tool")

        r = subprocess.run([sys.executable, "-I", DEPLOY, "--layer", "1", "--game", game, "--backup", backup,
                            "--removed", removed], capture_output=True, text=True)
        print(r.stdout, r.stderr)
        fails = []
        if r.returncode != 0:
            fails.append("deploy.py exit %d" % r.returncode)
        for gone in ("reframework/autorun/visceral_tuning.lua", "reframework/data/visceral_slide_grip.json",
                     "natives/stm/sectionroot/animation/player/pl00/list/hdg/base_hdg_hold.motlist.524"):
            if os.path.exists(os.path.join(game, gone)):
                fails.append("still in the game folder: " + gone)
            if not any(gone.replace("/", os.sep) in os.path.join(dp, f) for dp, _, fs in os.walk(removed) for f in fs):
                fails.append("not kept in the removed folder: " + gone)
        for stays in ("re2.exe", "reframework/plugins/REFramework.NET.dll"):
            if not os.path.exists(os.path.join(game, stays)):
                fails.append("removed a file that is not ours: " + stays)
        for want in ("dinput8.dll", "openxr_loader.dll", "re2_fw_config.txt", "reframework/autorun/visceral_mute_music.lua",
                     "reframework/autorun/vr/inner.lua", "_interaction_profiles_oculus_touch_controller.json"):
            if not os.path.exists(os.path.join(game, want)):
                fails.append("layer 1 file missing: " + want)
        if open(os.path.join(game, "dinput8.dll"), encoding="utf-8").read() != "backup dinput8.dll":
            fails.append("dinput8.dll is not the backup's")
        # Review Focus 4: the plugin DLL is never in a layer below 2
        for line in open(os.path.join(HERE, "install-list.txt"), encoding="utf-8"):
            p = line.split()
            if p and not p[0].startswith("#") and p[-1].endswith("visceral_native.dll") and int(p[0]) < 2:
                fails.append("the plugin is listed below layer 2")
        # max=1: deploying layer 2 must take the layer-1-only music script out again
        r2 = subprocess.run([sys.executable, "-I", DEPLOY, "--layer", "2", "--game", game, "--backup", backup,
                             "--removed", removed, "--skip-missing"], capture_output=True, text=True)
        if os.path.exists(os.path.join(game, "reframework/autorun/visceral_mute_music.lua")):
            fails.append("max=1 file still there after a layer 2 deploy")
        print("\n".join("FAIL " + f for f in fails) or "ALL PASS")
        return 1 if fails else 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
