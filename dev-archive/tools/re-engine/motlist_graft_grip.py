"""motlist_graft_grip.py -- the gun's grip joint from the game's own hold clip, on the relaxed clips (2026-10-08).

Why: the gun hangs from the player's r_weapon palm joint (r_arm_wrist -> r_weapon -> setProp_A_00 -> the weapon;
dossier: AttachJoint = setProp_A_00). FirstPerson's IK drives the WRIST to the controller; the joint below it is the
clip's. The relaxed (no-weapon KFF_) clips animate r_weapon as an empty hand, so a long gun sits off to the right of
where the controller points. Tefa 2026-10-08: with the flashlight out (the game's own one-handed hold clip) the
shotgun sits straight; in the light (our relaxed clip) it sits off to the right. This copies the r_weapon track,
whole, by bone hash (motlist_graft_arms.py's method), from ONE source clip into every relaxed slot of a list.
Second reason, the bigger one (modding-notes/2026-10-02-the-swing-fix-freeze-the-grip-socket-in-reframework.md):
FirstPerson's two-handed "pistol fix" turns the gun so the ANIMATED left-wrist-relative-to-right-wrist line (the grip
socket) matches the real left hand. On a relaxed clip that socket is two hanging arms, not a forestock, so a two-handed
shotgun in the light is turned off to the right; one-handed in the dark (flashlight in the left hand) there is no fix
and the gun follows the controller. --bones arms copies both arms from the game's own hold idle, which gives the socket
the real two-handed geometry; in VR the arms are IK'd to the controllers anyway, so the body pose stays relaxed.

Usage: py motlist_graft_grip.py --list <relaxed motlist> --source <stock motlist> --clip <source clip name>
                                --out <file> [--bones r_weapon|arms|a,b,c] [--match KFF_]
Only slots whose clip name contains --match are touched (default KFF_: the relaxed walks, idle and raise).
Legitimacy: reads the player's own game files, writes only --out; no game data in this file.
"""
import argparse, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import motlist_splice as ms
from motlist_graft_arms import bone_hash, clips, ARM


def graft(target, source, hashes, log):
    """target/source: mot entry bytes. Appends the source entry to the target and repoints the target's clip headers
    for the wanted bones at the source's track headers (data offsets relocated). Returns the new entry bytes."""
    w = bytearray(target)
    base = (len(w) + 15) & ~15
    w += b"\0" * (base - len(w)) + source
    tc, sc = clips(target), clips(source)
    done = []
    for h, name in hashes.items():
        if h not in tc or h not in sc:
            log("    %s: %s" % (name, "not in the target clip" if h not in tc else "not in the source clip"))
            continue
        ks, kt = sc[h], tc[h]
        _idx, fl_s, ff_s, _h, to_s = struct.unpack_from("<HBBII", source, 0x80 + 12 * ks)
        for t in range(bin(fl_s).count("1")):          # relocate the copied track headers' data offsets
            p = base + to_s + 0x14 * t
            for f in (8, 12, 16):
                v = struct.unpack_from("<I", w, p + f)[0]
                if v:
                    struct.pack_into("<I", w, p + f, v + base)
        idx_t = struct.unpack_from("<H", w, 0x80 + 12 * kt)[0]
        struct.pack_into("<HBBII", w, 0x80 + 12 * kt, idx_t, fl_s, ff_s, h, to_s + base)
        done.append(name)
    struct.pack_into("<Q", w, 0x10, len(w))
    return bytes(w), done


def write_list(lst, blobs):
    """Re-serialise a motlist with some entries replaced (blobs: entry offset -> new bytes). Same layout rules as
    motlist_splice.build: header, pointer table, entries 16-aligned in first-use order, collection."""
    cursor = ms.align16(lst.ptrs + 8 * lst.num)
    placed = {}
    body = bytearray()
    for o in lst.slots:
        if o == 0 or o in placed:
            continue
        blob = blobs.get(o, lst.blob[o])
        pad = ms.align16(len(blob)) - len(blob)
        placed[o] = cursor
        body += blob + b"\0" * pad
        cursor += len(blob) + pad
    out = bytearray(lst.header)
    out += b"\0" * (lst.ptrs - len(out))
    for o in lst.slots:
        out += struct.pack("<Q", placed[o] if o else 0)
    out += b"\0" * (ms.align16(len(out)) - len(out))
    col = len(out) + len(body)
    out += body
    out += lst.collection
    struct.pack_into("<Q", out, 0x18, col)
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", required=True)
    ap.add_argument("--source", required=True)
    ap.add_argument("--clip", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--bones", default="r_weapon")
    ap.add_argument("--match", default="KFF_")
    a = ap.parse_args()
    lst, src = ms.Motlist(a.list), ms.Motlist(a.source)
    if a.clip not in src.by_name:
        raise SystemExit("clip %s not in %s (have: %s)" % (a.clip, a.source, ", ".join(sorted(src.by_name))))
    sblob = src.blob[src.by_name[a.clip]]
    sframes = src.mot_info(sblob)[0]
    names = sorted(ARM) if a.bones == "arms" else a.bones.split(",")   # arms = both arms, hands, fingers and palms (44)
    hashes = {bone_hash(n): n for n in names}
    print("source %s: %s, %.0f frames" % (os.path.basename(a.source), a.clip, sframes))
    blobs = {}
    for i, o in enumerate(lst.slots):
        if o == 0 or o in blobs or a.match.lower() not in lst.entry_name[o].lower():
            continue
        tframes = lst.mot_info(lst.blob[o])[0]
        print("  slot 0x%03x %-38s %4.0f frames%s" % (lst.slot_number(i), lst.entry_name[o], tframes,
              "  (longer than the source: the track's last key will have to hold)" if tframes > sframes else ""))
        blobs[o], done = graft(lst.blob[o], sblob, hashes, print)
        print("    grafted: %s" % (", ".join(done) or "nothing"))
    data = write_list(lst, blobs)
    # verify: re-parse, same slots and names, and every grafted entry's bone header now points at the source's
    chk = ms.Motlist.__new__(ms.Motlist)
    open(a.out, "wb").write(data)
    chk = ms.Motlist(a.out)
    assert chk.num == lst.num and [chk.entry_name[o] for o in chk.slots] == [lst.entry_name[o] for o in lst.slots], "names differ after write"
    ok = 0
    for i, o in enumerate(lst.slots):
        if o in blobs:
            nb = chk.blob[chk.slots[i]]
            for h, name in hashes.items():
                tc, sc = clips(nb), clips(sblob)
                if h in tc and h in sc:
                    _i, fl, ff, _h, to = struct.unpack_from("<HBBII", nb, 0x80 + 12 * tc[h])
                    _i2, fl2, ff2, _h2, to2 = struct.unpack_from("<HBBII", sblob, 0x80 + 12 * sc[h])
                    assert (fl, ff) == (fl2, ff2), "flags differ in slot %d" % i
                    n = bin(fl).count("1") * 0x14
                    want = bytearray(sblob[to2:to2 + n])
                    base = to - to2
                    for t in range(bin(fl).count("1")):
                        for f in (8, 12, 16):
                            v = struct.unpack_from("<I", want, 0x14 * t + f)[0]
                            if v:
                                struct.pack_into("<I", want, 0x14 * t + f, v + base)
                    assert nb[to:to + n] == bytes(want), "track header mismatch in slot %d" % i
                    ok += 1
    print("wrote %s (%d bytes, %d entries grafted, %d bone headers verified)" % (a.out, len(data), len(blobs), ok))


if __name__ == "__main__":
    main()
