"""motlist_relax.py -- the relaxed (no-weapon) body pose on ANY weapon (Tefa's detour step 1, 2026-10-05).

The golden Leon pistol fix (b068, hunt page GOLDEN) as one generic tool, so every weapon gets the same recipe:

  base hold  (list/<wp>/base_<wp>_hold)  : the walking/standing slots, picked by slot NUMBER, get the no-weapon
               KFF_ motion from cmn/base_cmn_move; the six raise slots get the KFF_ idle cut to the length of the
               stock raise in that slot (so the state lasts as long as before). Shots, reloads, wheels, holster: kept.
  base move  (list/<wp>/base_<wp>_move)  : every slot gets the KFF_ motion with the same slot number.
  override   (list/<wp>/<wp>_hold_*)     : the walking/standing slots are EMPTIED (pointer 0), so the game falls back
               to the relaxed base list there; the override's own shots and reloads stay. Only the pointer table
               changes; the file is otherwise byte-identical (works on every layout, cpA's included).

Reuses motlist_splice.py's build + verify for the two base modes. Checked against the golden pistol files:
  py motlist_relax.py base-hold --list <stock base_hdg_hold> --cmn <base_cmn_move> --out X  == golden kff-v1, byte for byte
Usage:
  py motlist_relax.py base-hold --list <file> --cmn <base_cmn_move> --out <file>
  py motlist_relax.py base-move --list <file> --cmn <base_cmn_move> --out <file>
  py motlist_relax.py override  --list <file> --out <file>
Legitimacy: reads the player's own game files, writes only --out; no game data in this file.
"""
import argparse, struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import motlist_splice as ms

# walking / standing slot number -> no-weapon motion suffix (the golden pistol mapping, by number)
WALK = {
    0x06e: "0190_KFF_GazingWalk_F_Loop", 0x06f: "0191_KFF_GazingWalk_L_Loop", 0x070: "0194_KFF_GazingWalk_R_Loop",
    0x071: "0196_KFF_GazingWalk_Back_L_Loop", 0x072: "0197_KFF_GazingWalk_Back_B_Loop", 0x073: "0198_KFF_GazingWalk_Back_R_Loop",
    0x078: "0190_KFF_GazingWalk_F_Loop", 0x07a: "0191_KFF_GazingWalk_L_Loop", 0x07c: "0197_KFF_GazingWalk_Back_B_Loop",
    0x07e: "0194_KFF_GazingWalk_R_Loop", 0x084: "0191_KFF_GazingWalk_L_Loop", 0x088: "0194_KFF_GazingWalk_R_Loop",
    0x0a0: "0160_KFF_Gazing_Idle_F_Loop", 0x0a6: "0160_KFF_Gazing_Idle_F_Loop",
}
RAISE = (0x08c, 0x08d, 0x08f, 0x096, 0x097, 0x099)   # Hold_Start L0/L90/L180, R0/R90/R180
IDLE = "0160_KFF_Gazing_Idle_F_Loop"
RELAXED = set(WALK) | set(RAISE)


def prefix_of(m):
    for n in m.entry_name.values():
        if n and n[:2] == "pl" and n[4] == "_":
            return n[:4]
    raise SystemExit("no plNN_ motion names in %s" % m.path)


def base_hold(lst, cmn, log):
    pre = prefix_of(lst)
    mapping, fill = {}, {}
    for i, o in enumerate(lst.slots):
        num = lst.slot_number(i)
        if num not in RELAXED:
            continue
        if o == 0:
            continue                      # empty in the stock list: leave it empty
        suffix = lst.entry_name[o][len(pre) + 1:]
        if num in RAISE:
            frames = lst.mot_info(lst.blob[o])[0]
            target = "%s@%g" % (IDLE, frames)
        else:
            target = WALK[num]
        if suffix in mapping and mapping[suffix] != target:
            # two slots share one stock motion but want different targets: the splice maps by name, so refuse
            raise SystemExit("slot 0x%x: %s already mapped to %s, wants %s" % (num, suffix, mapping[suffix], target))
        mapping[suffix] = target
    return ms.build(lst, cmn, mapping, pre, log)


def base_move(lst, cmn, log):
    pre = prefix_of(lst)
    by_num = {}
    for i, o in enumerate(cmn.slots):
        if o:
            by_num.setdefault(cmn.slot_number(i), cmn.entry_name[o][len(pre) + 1:])
    mapping, split = {}, []
    for i, o in enumerate(lst.slots):
        num = lst.slot_number(i)
        if o == 0 or num not in by_num:
            continue
        suffix = lst.entry_name[o][len(pre) + 1:]
        if suffix in mapping and mapping[suffix] != by_num[num]:
            split.append(i)     # shares a stock motion with a slot that wants another target (stg 0xa0/0xa1)
            continue
        mapping[suffix] = by_num[num]
    if not split:
        return ms.build(lst, cmn, mapping, pre, log)
    # the splice maps by name, so empty these slots in a copy and FILL them by number instead
    d = bytearray(lst.data)
    for i in split:
        struct.pack_into("<Q", d, lst.ptrs + 8 * i, 0)
    tmp = lst.path + ".split.tmp"
    open(tmp, "wb").write(d)
    try:
        cp = ms.Motlist(tmp)
        fill = {cp.slot_number(i): by_num[cp.slot_number(i)] for i in split}
        out, rep = ms.build(cp, cmn, mapping, pre, log, fill=fill)
        if not ms.verify(out, cp, cmn, rep, log):
            raise SystemExit(2)
    finally:
        os.unlink(tmp)
    return out, rep


def override(data, log):
    """Zero the pointers of the walking/standing slots. Reads the slot number from the collection block
    (u32 at +8 of each entry, whatever the entry size), so non-standard layouts (cpA) work too."""
    d = bytearray(data)
    ptrs, col = struct.unpack_from("<QQ", d, 0x10)
    num = struct.unpack_from("<I", d, 0x30)[0]
    if num == 0:
        return bytes(d), []
    # 72 bytes per slot; some lists (cpA, hdg_hold_07) carry extra data AFTER those entries (measured 2026-10-05)
    size = 72
    if len(d) - col < size * num:
        raise SystemExit("collection block is shorter than %d x 72 bytes" % num)
    cleared = []
    for i in range(num):
        n = struct.unpack_from("<I", d, col + i * size + 8)[0]
        p = struct.unpack_from("<Q", d, ptrs + 8 * i)[0]
        if n in RELAXED and p:
            struct.pack_into("<Q", d, ptrs + 8 * i, 0)
            no = struct.unpack_from("<Q", d, p + 0x58)[0]
            cleared.append((i, n, ms.u16s(d, p + no)))
    # verify: only the cleared pointers differ
    diff = [k for k in range(len(d)) if d[k] != data[k]]
    allowed = set()
    for i, _, _ in cleared:
        allowed |= set(range(ptrs + 8 * i, ptrs + 8 * i + 8))
    if not set(diff) <= allowed:
        raise SystemExit("VERIFY FAIL: bytes outside the cleared pointers changed")
    for i, n, nm in cleared:
        log("[%2d] 0x%03x emptied (was %s)" % (i, n, nm))
    log("verify: %d slots emptied, nothing else changed -- OK" % len(cleared))
    return bytes(d), cleared


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=["base-hold", "base-move", "override"])
    ap.add_argument("--list", required=True)
    ap.add_argument("--cmn", help="cmn/base_cmn_move.motlist.524 of the same character (base modes)")
    ap.add_argument("--out", required=True, help="output FILE")
    a = ap.parse_args()
    if a.mode == "override":
        out, changed = override(open(a.list, "rb").read(), print)
    else:
        if not a.cmn:
            ap.error("--cmn is needed for %s" % a.mode)
        lst, cmn = ms.Motlist(a.list), ms.Motlist(a.cmn)
        out, changed = (base_hold if a.mode == "base-hold" else base_move)(lst, cmn, print)
        if not ms.verify(out, lst, cmn, changed, print):
            raise SystemExit(2)
    if not changed:
        print("nothing to change; no file written")
        return
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    open(a.out, "wb").write(out)
    print("wrote %d bytes -> %s" % (len(out), a.out))


if __name__ == "__main__":
    main()
