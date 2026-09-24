"""pakfind.py : test guessed internal paths against the RE2 pak hash tables (no extraction)."""
import sys, os, struct, glob, itertools
import mmh3
GAME = r"C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2"
SEED = 0xFFFFFFFF
def h32(s): return mmh3.hash(s.encode("utf-16-le"), SEED, signed=False)
def load_table(pak):
    with open(pak, "rb") as f:
        hdr = f.read(16)
        magic, major, minor, feature, total, fp = struct.unpack("<IBBHII", hdr)
        raw = f.read(total * 48)
    t = set()
    for i in range(total):
        lo, hi = struct.unpack_from("<II", raw, i * 48)
        t.add((lo, hi))
    return t
paks = sorted(glob.glob(os.path.join(GAME, "re_chunk_000.pak.patch_*.pak")), reverse=True) + [os.path.join(GAME, "re_chunk_000.pak")]
keys = set()
for p in paks:
    if os.path.getsize(p) >= 16:
        keys |= load_table(p)
print("entries:", len(keys))
roots = ["natives/stm/sectionroot/animation/player/%s/", "natives/stm/sectionroot/character/player/%s/", "natives/stm/sectionroot/character/survivor/%s/",
         "natives/stm/sectionroot/animation/player/%s/motfsm2/", "natives/stm/sectionroot/character/player/%s/motfsm2/", "natives/stm/sectionroot/character/survivor/%s/motfsm2/",
         "natives/stm/sectionroot/animation/player/%s/fsm/", "natives/stm/sectionroot/motfsm2/player/%s/", "natives/stm/sectionroot/motfsm2/%s/",
         "natives/stm/sectionroot/animation/player/%s/motfsm/", "natives/stm/sectionroot/character/player/%s/fsm/"]
pls = ["pl00", "pl10", "pl1000", "pl0000", "pl00_1000", "pl10_1000"]
names = ["%s.motfsm2.42", "%s_motfsm2.motfsm2.42", "%s_fsm.motfsm2.42", "%s.fsmv2.40", "%s_control.fsmv2.40", "%s_main.motfsm2.42", "player.motfsm2.42", "%s_common.motfsm2.42", "%s_base.motfsm2.42"]
hits = 0
for root, pl, nm in itertools.product(roots, pls, names):
    path = (root % pl) + (nm % pl if "%s" in nm else nm)
    if (h32(path.lower()), h32(path.upper())) in keys:
        print("HIT", path); hits += 1
# also weapon fsm guesses
for root in ["natives/stm/sectionroot/animation/weapon/%s/", "natives/stm/sectionroot/character/weapon/%s/", "natives/stm/sectionroot/animation/weapon/%s/motfsm2/", "natives/stm/sectionroot/character/weapon/%s/motfsm2/", "natives/stm/sectionroot/animation/wp/%s/"]:
    for wp in ["wp0200", "wp02", "wp0800", "wp0000"]:
        for nm in ["%s.motfsm2.42", "%s_motfsm2.motfsm2.42", "%s_wep.motfsm2.42"]:
            path = (root % wp) + (nm % wp)
            if (h32(path.lower()), h32(path.upper())) in keys:
                print("HIT", path); hits += 1
# sanity: a known motlist path
for known in ["natives/stm/sectionroot/animation/player/pl10/list/hdg/base_hdg_hold.motlist.524", "natives/stm/sectionroot/userdata/character/survivor/pl0000/lookat/hold_hg.user.2"]:
    print("known", known, (h32(known.lower()), h32(known.upper())) in keys)
print("hits", hits)
