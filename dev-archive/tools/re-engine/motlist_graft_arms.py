"""motlist_graft_arms.py -- b016 (2026-09-27): the aim-walk splice with the ARMS put back.

Why: the handgun shot motions (1100-1102) are additive (their weapon-bone offsets are all zero) and were
authored over the arms-raised aim pose. The spliced OFF_GazingWalk loops hold the arms lowered, so the same
shot kick throws the gun aside (b012-b015 bisect, Tefa 2026-09-27). This keeps the relaxed walk's legs, hips,
spine and head, and swaps each walk loop's 44 arm/hand/weapon bone tracks for the vanilla HG_Interpolation
loop's. Tracks are moved whole: the aim blob is appended to the walk blob and the copied track headers'
data offsets are shifted by where it landed. Standing aim and gun raise stay vanilla (b015 showed those fine).
[hypothesis] until Tefa's walk-and-shoot test.

Track layout used (RE2 RT mot v492, measured 2026-09-27): clip headers are 12 bytes from +0x80
(idx u16, track flags u8, 0xFF, bone hash u32, track header offset u32), clip count u16 at +0x72; one
0x14-byte track header per set flag bit: flags u32, key count u32, frame-index offset u32, key-data offset
u32, unpack-data offset u32 (all entry-relative, 0 = none). Bone hash = murmur3_32(UTF-16LE name, seed
0xFFFFFFFF), checked on "root" = 0xaba7de3c.

Usage: py motlist_graft_arms.py <folder holding the two originals under natives/...> <out folder>
       (motlist_splice.py --game-dir ... leaves the originals in <out>/_originals)
Legitimacy: reads the player's own game files, writes only into <out>. No game data is included here.
"""
import os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import motlist_splice as ms


def mm3(data, seed=0xFFFFFFFF):
    c1, c2 = 0xcc9e2d51, 0x1b873593
    h, n = seed, len(data)
    r = n & 3
    for i in range(0, n - r, 4):
        k = int.from_bytes(data[i:i + 4], "little")
        k = (k * c1) & 0xffffffff; k = ((k << 15) | (k >> 17)) & 0xffffffff; k = (k * c2) & 0xffffffff
        h ^= k; h = ((h << 13) | (h >> 19)) & 0xffffffff; h = (h * 5 + 0xe6546b64) & 0xffffffff
    k = 0
    for j in reversed(range(r)):
        k = (k << 8) | data[n - r + j]
    if r:
        k = (k * c1) & 0xffffffff; k = ((k << 15) | (k >> 17)) & 0xffffffff; k = (k * c2) & 0xffffffff; h ^= k
    h ^= n; h ^= h >> 16; h = (h * 0x85ebca6b) & 0xffffffff; h ^= h >> 13; h = (h * 0xc2b2ae35) & 0xffffffff
    return h ^ (h >> 16)


def bone_hash(name):
    return mm3(name.encode("utf-16-le"))


ARM = set()
for s in "lr":
    ARM |= {s + "_arm_clavicle", s + "_arm_humerus", s + "_arm_radius", s + "_arm_wrist", s + "_weapon"}
    for f in ["index", "middle", "ring", "little", "thumb"]:
        for i in range(4):
            ARM.add("%s_hand_%s_%d" % (s, f, i))
ARMH = {bone_hash(n): n for n in ARM}

# walk loop (legs kept) -> vanilla aim loop whose arms it takes
PAIR = {"0190_OFF_GazingWalk_F_Loop": "0110_HG_Interpolation_F_Loop",
        "0191_OFF_GazingWalk_L_Loop": "0111_HG_Interpolation_L_Loop",
        "0194_OFF_GazingWalk_R_Loop": "0112_HG_Interpolation_R_Loop",
        "0196_OFF_GazingWalk_Back_L_Loop": "0113_HG_Interpolation_Back_L_Loop",
        "0197_OFF_GazingWalk_Back_B_Loop": "0114_HG_Interpolation_Back_B_Loop",
        "0198_OFF_GazingWalk_Back_R_Loop": "0115_HG_Interpolation_Back_R_Loop"}


def clips(b):
    cc = struct.unpack_from("<H", b, 0x72)[0]
    return {struct.unpack_from("<I", b, 0x80 + 12 * k + 4)[0]: k for k in range(cc)}


def graft(walk, aim, log):
    w = bytearray(walk)
    base = (len(w) + 15) & ~15
    w += b"\0" * (base - len(w)) + aim
    wc, ac = clips(walk), clips(aim)
    n = 0
    for h in ARMH:
        if h not in wc or h not in ac:
            continue
        ka, kw = ac[h], wc[h]
        _idx, fl_a, ff_a, _h, to_a = struct.unpack_from("<HBBII", aim, 0x80 + 12 * ka)
        for t in range(bin(fl_a).count("1")):          # relocate the copied track headers' data offsets
            p = base + to_a + 0x14 * t
            for f in (8, 12, 16):
                v = struct.unpack_from("<I", w, p + f)[0]
                if v:
                    struct.pack_into("<I", w, p + f, v + base)
        idx_w = struct.unpack_from("<H", w, 0x80 + 12 * kw)[0]
        struct.pack_into("<HBBII", w, 0x80 + 12 * kw, idx_w, fl_a, ff_a, h, to_a + base)
        n += 1
    struct.pack_into("<Q", w, 0x10, len(w))
    log("  grafted %d arm/hand bones, blob %d -> %d bytes" % (n, len(walk), len(w)))
    return bytes(w)


def main():
    orig, out = sys.argv[1], sys.argv[2]
    R = "natives/stm/sectionroot/animation/player/%s/list/hdg/"
    for c in ("pl00", "pl10"):
        hold = ms.Motlist(os.path.join(orig, R % c, "base_hdg_hold.motlist.524"))
        move = ms.Motlist(os.path.join(orig, R % c, "base_hdg_move.motlist.524"))
        for wn, an in PAIR.items():
            print(c, wn, "<- arms of", an)
            o = move.by_name["%s_%s" % (c, wn)]
            move.blob[o] = graft(move.blob[o], hold.blob[hold.by_name["%s_%s" % (c, an)]], print)
        mapping = {k: v for k, v in ms.DEFAULT_MAP.items() if not k.startswith("0160_")}
        data, rep = ms.build(hold, move, mapping, c, lambda s: None)
        if not ms.verify(data, hold, move, rep, print):
            raise SystemExit(2)
        d = os.path.join(out, R % c)
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, "base_hdg_hold.motlist.524"), "wb").write(data)


if __name__ == "__main__":
    main()
