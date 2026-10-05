"""motlist_clip_swap.py -- 2026-10-05: put the game's own pistol SWITCHES back into the spliced pistol lists.

Why: the golden pose fix (b068) fills Leon's pistol slots with the no-weapon KFF_ motions. Their bones give the
straight body, but they carry none of the per-motion switches the pistol motions carry -- above all
SurvivorIkLeftArmTrack.IKBlendRatio, which keeps the left hand on the gun. Without it the left-arm IK blend is 0
(dossier 8g.2), and the left hand cannot dock (Tefa 2026-10-05; the magnum, on stock files, docks fine).

What it does: for every slot whose motion differs from the stock list's motion in the same slot, the spliced
motion keeps ALL its bone tracks and its sync clip, and its property clip is replaced by the stock motion's
property clip. Bones = no-weapon body; switches = the game's own pistol switches. Nothing is re-encoded.

Clip layout (RE2 RT mot v492, measured 2026-10-05 on pl00 base_hdg_move/hold + base_cmn_move, 129 entries):
  header +0x30 u64 = clip pointer array (entry-relative), +0x74 u8 = clip count, +0x75 u8 = sync clip count
  clip unit = 0x40-byte header {u64 0, u64 CLIP block, u64 end, u32 0, u32 track groups, u32 1, u32 kind}
              + the CLIP block; every pointer inside it is entry-relative and points inside the unit.
  kind 0 = property tracks (all of a motion's switches in ONE clip), kind 3 = via.motion.MotionSyncPoint.
  The game never has two kind-0 clips in one motion (counted: (0,3) x48, (0,) x41, (3,) x20, none x20), so the
  stock property clip REPLACES ours rather than being added beside it.
Frame times: the copied clip's f32 values equal to the stock clip's length are set to the spliced motion's length.

Usage: py motlist_clip_swap.py <spliced motlist> <stock motlist of the same path> <out file>
Legitimacy: reads the player's own game files, writes only <out file>. No game data is included here.
"""
import os, re, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import motlist_splice as ms

KIND_PROPERTY, KIND_SYNC = 0, 3
DATA_WORD = 0x10000


def units(b):
    """[(header offset, end offset, kind)] in clip-array order."""
    n = b[0x74]
    if not n:
        return []
    arr = struct.unpack_from("<Q", b, 0x30)[0]
    hdrs = [struct.unpack_from("<Q", b, arr + 8 * k)[0] for k in range(n)]
    o50 = struct.unpack_from("<Q", b, 0x50)[0]
    out = []
    for h in hdrs:
        later = [x for x in hdrs if x > h] + [o50 if o50 > h else len(b)]   # a swapped-in unit sits past o50
        out.append((h, min(later), struct.unpack_from("<I", b, h + 0x24)[0]))
    return out


def lift_unit(b, h, end, new_at):
    """Copy one clip unit, relocated to entry offset new_at. Refuses a unit that points outside itself."""
    # Every unit carries the data word 0x10000 at CLIP+0x98 (two u16s; seen in all 129 entries). It is skipped,
    # so a unit that spans entry offset 0x10000 is refused rather than guessed at.
    if h <= DATA_WORD < end:
        raise ValueError("clip unit 0x%x-0x%x spans 0x10000; cannot tell the data word from a pointer" % (h, end))
    u = bytearray(b[h:end])
    moved = 0
    for p in range(0, len(u) - 7, 8):
        v = struct.unpack_from("<Q", u, p)[0]
        if v == DATA_WORD:
            continue
        if h <= v < end:
            struct.pack_into("<Q", u, p, v - h + new_at)
            moved += 1
        elif end <= v < len(b) and v % 4 == 0:       # names/jmap live after the clips
            raise ValueError("clip unit at 0x%x has a value 0x%x at +0x%x that may point outside it" % (h, v, p))
    return u, moved


def swap(base, donor, log):
    """base: spliced motion blob, donor: stock motion blob. Returns the new base blob."""
    dprop = [x for x in units(donor) if x[2] == KIND_PROPERTY]
    bsync = [x for x in units(base) if x[2] == KIND_SYNC]
    bother = [x for x in units(base) if x[2] not in (KIND_PROPERTY, KIND_SYNC)]
    if bother:
        raise ValueError("unknown clip kind %r" % [x[2] for x in bother])
    if not dprop:
        log("    stock motion has no property clip; left as is")
        return base
    h, end, _k = dprop[0]
    w = bytearray(base)
    arr = ms.align16(len(w))
    n = 1 + len(bsync)
    at = ms.align16(arr + 8 * n)
    unit, moved = lift_unit(donor, h, end, at)
    clip = struct.unpack_from("<Q", unit, 8)[0] - at
    old_fc = struct.unpack_from("<f", unit, clip + 8)[0]
    new_fc = struct.unpack_from("<f", base, 0x60)[0]
    times = 0
    for p in range(0, len(unit) - 3, 4):
        if struct.unpack_from("<f", unit, p)[0] == old_fc:
            struct.pack_into("<f", unit, p, new_fc)
            times += 1
    w += b"\0" * (arr - len(w))
    w += struct.pack("<%dQ" % n, at, *[x[0] for x in bsync])
    w += b"\0" * (at - len(w))
    w += unit
    w += b"\0" * (ms.align16(len(w)) - len(w))
    struct.pack_into("<Q", w, 0x30, arr)
    w[0x74] = n
    w[0x75] = len(bsync)
    struct.pack_into("<Q", w, 0x10, len(w))
    log("    property clip %d bytes, %d pointers moved, %d frame times %g -> %g, clips %d (sync %d)"
        % (len(unit), moved, times, old_fc, new_fc, n, len(bsync)))
    return bytes(w)


def tracks(b):
    """The track class names in a motion's property clip (to check that shared slots want the same switches)."""
    out = set()
    for h, end, kind in units(b):
        if kind == KIND_PROPERTY:
            out |= set(m.group(1) for m in re.finditer(rb"((?:[A-Za-z_.]\x00){8,})", b[h:end]))
    return out


def write(lst, blobs):
    """Rebuild a container around new blobs (key = original entry offset); sharing and collection kept."""
    cursor = ms.align16(lst.ptrs + 8 * lst.num)
    placed, body = {}, bytearray()
    for o in lst.slots:
        if o and o not in placed:
            blob = blobs[o]
            placed[o] = cursor
            pad = ms.align16(len(blob)) - len(blob)
            body += blob + b"\0" * pad
            cursor += len(blob) + pad
    out = bytearray(lst.header) + b"\0" * (lst.ptrs - len(lst.header))
    for o in lst.slots:
        out += struct.pack("<Q", placed[o] if o else 0)
    out += b"\0" * (ms.align16(len(out)) - len(out))
    col = len(out) + len(body)
    out += body + lst.collection
    struct.pack_into("<Q", out, 0x18, col)
    return bytes(out)


def main():
    base_p, donor_p, out_p = sys.argv[1:4]
    base, donor = ms.Motlist(base_p), ms.Motlist(donor_p)
    if base.num != donor.num or base.collection != donor.collection:
        raise SystemExit("the two lists do not have the same slots")
    blobs = dict(base.blob)
    done = {}
    for i, o in enumerate(base.slots):
        if not o:
            continue
        bn, dn = base.entry_name[o], donor.entry_name[donor.slots[i]]
        if o in done:
            if done[o] != dn and tracks(donor.blob[donor.by_name[done[o]]]) != tracks(donor.blob[donor.slots[i]]):
                raise SystemExit("slot %d shares %s but its stock motion %s has other switches than %s" % (i, bn, dn, done[o]))
            continue
        done[o] = dn
        if bn == dn:
            continue
        print("[%2d] %s  <- switches of %s" % (i, bn, dn))
        blobs[o] = swap(base.blob[o], donor.blob[donor.slots[i]], print)
    data = write(base, blobs)
    os.makedirs(os.path.dirname(os.path.abspath(out_p)), exist_ok=True)
    open(out_p, "wb").write(data)
    check = ms.Motlist(out_p)       # re-parse: names, sharing, collection
    for i in range(base.num):
        if (base.slots[i] == 0) != (check.slots[i] == 0):
            raise SystemExit("VERIFY FAIL: slot %d emptiness changed" % i)
        if base.slots[i] and check.entry_name[check.slots[i]] != base.entry_name[base.slots[i]]:
            raise SystemExit("VERIFY FAIL: slot %d name changed" % i)
    print("wrote %s (%d bytes); names and empty slots re-checked" % (out_p, len(data)))


if __name__ == "__main__":
    main()
