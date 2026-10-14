"""motlist_splice_cp.py -- the walk/idle/raise splice for a CUSTOM-PART override list (hdg_hold_cpA_01 and friends).

Why (board row, Tefa 2026-10-04 "we need to alter those at some point"): a handgun with a custom part (the Matilda's
stock, Claire's parts) aims with its OWN list, hdg_hold_cpA_01 / cpAC_01 (each also as stLIGHT_ / stWATER_), whose
slots override our spliced base_hdg_hold. Those lists carry the stock's strafes, idle, raise and shots (HG2_ / SMG1_
clips), so the hunched aim-walk came back with the stock on. The stop-gap since b068 replaced them with the game's
EMPTY list, which also threw away the stock's own shooting clips. This tool keeps those and splices the rest.

The override-list layout, measured on the originals 2026-10-14 (pl00 / pl10 / pl20, cpA / cpAC, plain / stLIGHT / stWATER):
  header    : as the base list, PLUS u64 at 0x28 = offset of a second UTF-16 string, the path of the base list this one
              overrides ("SectionRoot/Animation/Player/pl00/list/HDG/BASE_HDG_HOLD.motlist"); the pointer table follows
              both strings (0xe0 here, 0xf0 for the longer stLIGHT_/stWATER_ names). Copied verbatim.
  pointers  : 30 slots as the base list, most EMPTY (0); the same slot numbering as the base list (slot 6 = StrafeL_F ...).
  entries   : standard v492 mot blobs, movable whole (motlist_splice.py's finding).
  collection: 72 bytes per slot (as the base list) FOLLOWED BY A TAIL: one extra block per shooting slot (three of
              1880 bytes, "CLIP+" inside), whose rows point at them with an ABSOLUTE file offset at +0 and a flag byte
              at +0x17. The blocks themselves hold absolute file offsets into the tail. So the collection cannot be
              copied verbatim once the entries change size: every 8-aligned u64 inside the rows and the tail whose
              value lies inside the old tail is moved by the same delta. verify() proves the relocation is exactly that
              and nothing else changed (moving the output back by -delta gives the original bytes).

What it changes: the slots whose clip name is a strafe, the hold idle, or a hold start (HG2_Strafe*, HG2_Hold_Idle_Loop,
SMG1_Hold_Start_*) take the clip the DONOR list has in the SAME slot. The donor is the character's installed, spliced
base_hdg_hold (so the stock gets exactly the walks, idle and 20/22/24-frame raise Tefa approved there). Everything else
(wheel turns, the SMG1 shots, the quick reload) is kept byte for byte.

Usage:
    py motlist_splice_cp.py --cp <hdg_hold_cpA_01.motlist.524> --donor <installed base_hdg_hold.motlist.524> --out <file> [--dry-run]
    py motlist_splice_cp.py --batch <folder with the cp originals> --donor ... --out-dir <folder>   (every hdg_hold_*cp*_01 in it)

Legitimacy: reads the player's own game files, writes only the output; no game data is in this script.
"""
import argparse, glob, os, struct, sys

REPLACE_WHEN = ("_Strafe", "_Hold_Idle_Loop", "_Hold_Start_")   # name parts of the slots that take the donor's clip
ROW = 72


def u16s(d, off, maxlen=256):
    end = off
    while end + 1 < len(d) and not (d[end] == 0 and d[end + 1] == 0):
        end += 2
        if end - off > maxlen * 2:
            break
    return d[off:end].decode("utf-16-le", errors="replace")


def align16(n):
    return (n + 15) & ~15


class List:
    """Any v524 motlist: base (collection = 72 x slots) or override (collection = rows + tail)."""
    def __init__(self, path):
        self.path = path
        d = self.data = open(path, "rb").read()
        self.version, magic = struct.unpack_from("<I4s", d, 0)
        if magic != b"mlst":
            raise SystemExit("%s: not a motlist (magic %r)" % (path, magic))
        self.ptrs, self.col, self.nameoff = struct.unpack_from("<QQQ", d, 0x10)
        self.num = struct.unpack_from("<I", d, 0x30)[0]
        self.name = u16s(d, self.nameoff)
        self.slots = list(struct.unpack_from("<%dQ" % self.num, d, self.ptrs))
        starts = sorted(set(self.slots) - {0})
        ends = starts[1:] + [self.col]
        self.blob = {o: d[o:e] for o, e in zip(starts, ends)}
        self.entry_name = {o: self.mot_name(o) for o in starts}
        self.entry_name[0] = None
        self.header = d[:self.ptrs]
        self.rows = d[self.col:self.col + ROW * self.num]
        self.tail = d[self.col + ROW * self.num:]
        self.tail_start = self.col + ROW * self.num

    def mot_name(self, o):
        ver, magic = struct.unpack_from("<I4s", self.data, o)
        if magic != b"mot ":
            raise SystemExit("%s: entry at 0x%x has magic %r" % (self.path, o, magic))
        return u16s(self.data, o + struct.unpack_from("<Q", self.data, o + 0x58)[0])

    def slot_name(self, i):
        return self.entry_name[self.slots[i]]

    def frames(self, blob):
        return struct.unpack_from("<f", blob, 0x60)[0]


def relocate(block, lo, hi, delta):
    """Move every 8-aligned u64 in `block` whose value lies in [lo, hi) by delta. Returns (bytes, count)."""
    b = bytearray(block)
    n = 0
    for off in range(0, len(b) - 7, 8):
        v = struct.unpack_from("<Q", b, off)[0]
        if lo <= v < hi:
            struct.pack_into("<Q", b, off, v + delta)
            n += 1
    return bytes(b), n


def build(cp, donor, log):
    if cp.num != donor.num:
        raise SystemExit("slot counts differ: %s has %d, donor %d" % (cp.path, cp.num, donor.num))
    src = {}        # key -> blob;  key = ("cp", off) or ("donor", off)
    slot_key = []
    replaced = []
    for i, o in enumerate(cp.slots):
        if o == 0:
            slot_key.append(None)
            continue
        name = cp.entry_name[o]
        if any(part in name for part in REPLACE_WHEN):
            do = donor.slots[i]
            if do == 0:
                raise SystemExit("donor has no clip in slot %d (%s)" % (i, name))
            key = ("donor", do)
            src[key] = donor.blob[do]
            replaced.append((i, name, donor.entry_name[do]))
        else:
            key = ("cp", o)
            src[key] = cp.blob[o]
        slot_key.append(key)
    # header + pointer table + entries, as motlist_splice.py lays them out
    cursor = align16(cp.ptrs + 8 * cp.num)
    placed, body = {}, bytearray()
    for key in slot_key:
        if key is None or key in placed:
            continue
        blob = src[key]
        pad = align16(len(blob)) - len(blob)
        placed[key] = cursor
        body += blob + b"\0" * pad
        cursor += len(blob) + pad
    out = bytearray(cp.header)
    out += b"\0" * (cp.ptrs - len(out))
    for key in slot_key:
        out += struct.pack("<Q", placed[key] if key is not None else 0)
    out += b"\0" * (align16(len(out)) - len(out))
    col = len(out) + len(body)
    out += body
    # collection: rows and tail, with the tail's absolute offsets moved by the same delta as the collection
    delta = col - cp.col
    lo, hi = cp.tail_start, len(cp.data)
    rows, n_rows = relocate(cp.rows, lo, hi, delta)
    tail, n_tail = relocate(cp.tail, lo, hi, delta)
    out += rows + tail
    struct.pack_into("<Q", out, 0x18, col)
    log("slot  motion (after)                               frames  <- ")
    for i, key in enumerate(slot_key):
        if key is None:
            continue
        blob = src[key]
        n = (donor if key[0] == "donor" else cp).entry_name[key[1]]
        tag = "REPLACED (was %s)" % cp.slot_name(i) if key[0] == "donor" else "kept"
        log("[%2d]  %-46s %5.0f  %s" % (i, n, cp.frames(blob), tag))
    log("collection moved by %+d bytes: %d offsets moved in the rows, %d in the %d-byte tail" % (delta, n_rows, n_tail, len(cp.tail)))
    return bytes(out), replaced, delta, (n_rows, n_tail)


def verify(out_bytes, cp, donor, replaced, delta, counts, log):
    import tempfile
    tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".motlist.524")
    tmp.write(out_bytes); tmp.close()
    o = List(tmp.name)
    os.unlink(tmp.name)
    problems = []
    if o.num != cp.num: problems.append("slot count changed")
    if o.name != cp.name: problems.append("container name changed")
    ho, hc = bytearray(o.header), bytearray(cp.header)
    ho[0x18:0x20] = hc[0x18:0x20] = b"\0" * 8   # the collection offset is the one header word that moves
    if ho != hc: problems.append("header changed")
    rep = {i: new for i, _old, new in replaced}
    for i in range(cp.num):
        if cp.slots[i] == 0:
            if o.slots[i] != 0: problems.append("slot %d was empty and is not any more" % i)
            continue
        if o.slots[i] == 0: problems.append("slot %d lost its clip" % i); continue
        want = rep.get(i, cp.slot_name(i))
        if o.slot_name(i) != want: problems.append("slot %d is %s, expected %s" % (i, o.slot_name(i), want))
        srcblob = donor.blob[donor.slots[i]] if i in rep else cp.blob[cp.slots[i]]
        if o.blob[o.slots[i]][:len(srcblob)] != srcblob: problems.append("slot %d blob differs from its source" % i)
        if o.slots[i] % 16: problems.append("slot %d entry not 16-aligned" % i)
    # sharing: slots that shared an entry in the original still do (unless replaced), and vice versa
    for a in range(cp.num):
        for b in range(a + 1, cp.num):
            if cp.slots[a] == 0 or cp.slots[b] == 0 or a in rep or b in rep: continue
            if (cp.slots[a] == cp.slots[b]) != (o.slots[a] == o.slots[b]): problems.append("slots %d and %d changed their sharing" % (a, b))
    # the collection: moving the output's rows and tail back by -delta must give the original bytes exactly
    lo, hi = o.tail_start, len(o.data)
    rows_back, nr = relocate(o.rows, lo, hi, -delta)
    tail_back, nt = relocate(o.tail, lo, hi, -delta)
    if rows_back != cp.rows: problems.append("collection rows changed beyond the relocation")
    if tail_back != cp.tail: problems.append("collection tail changed beyond the relocation")
    if (nr, nt) != counts: problems.append("relocation count differs on the way back (%d,%d vs %d,%d)" % (nr, nt, counts[0], counts[1]))
    # every moved offset must point INSIDE the new tail, 8-aligned
    for off in range(0, len(o.tail) - 7, 8):
        v = struct.unpack_from("<Q", o.tail, off)[0]
        if lo <= v < hi and v % 8: problems.append("tail offset at +0x%x not 8-aligned after the move" % off)
    for p in problems: log("VERIFY FAIL: " + p)
    if not problems:
        log("verify: %d slots, %d replaced, kept clips byte-identical, collection = original moved by %+d (%d offsets) -- OK"
            % (o.num, len(rep), delta, counts[0] + counts[1]))
    return not problems


def build_null(cp, log):
    """The lighter treatment (what the 2026-10-05 Claire / Ada job did by hand): the strafe, idle and raise slots are
    simply EMPTIED in the pointer table, so the game falls back to our spliced base list for them, slot by slot, the
    same way the original already leaves slots 0-5 and 25-29 to the base list. Nothing else moves: the file is byte
    for byte the original except those pointers (the unreferenced clips stay in the file, as dead weight)."""
    out = bytearray(cp.data)
    nulled = []
    for i, o in enumerate(cp.slots):
        if o == 0: continue
        name = cp.entry_name[o]
        if any(part in name for part in REPLACE_WHEN):
            struct.pack_into("<Q", out, cp.ptrs + 8 * i, 0)
            nulled.append((i, name))
    for i, name in nulled: log("[%2d]  %-46s EMPTIED (the base list's clip shows through)" % (i, name))
    for i, o in enumerate(cp.slots):
        if o and not any(i == n for n, _ in nulled): log("[%2d]  %-46s kept" % (i, cp.entry_name[o]))
    return bytes(out), nulled


def verify_null(out_bytes, cp, nulled, log):
    problems = []
    if len(out_bytes) != len(cp.data): problems.append("size changed")
    want = bytearray(cp.data)
    for i, _ in nulled: struct.pack_into("<Q", want, cp.ptrs + 8 * i, 0)
    if bytes(want) != out_bytes: problems.append("bytes changed beyond the emptied pointers")
    for p in problems: log("VERIFY FAIL: " + p)
    if not problems: log("verify: %d slots emptied, every other byte identical to the original -- OK" % len(nulled))
    return not problems


def one(cp_path, donor_path, out_path, dry, log=print, null=False):
    cp = List(cp_path)
    if null:
        log("== %s (%d slots, %d empty): EMPTY the strafe / idle / raise slots" % (os.path.basename(cp_path), cp.num, cp.slots.count(0)))
        out, nulled = build_null(cp, log)
        if not verify_null(out, cp, nulled, log):
            raise SystemExit(2)
    else:
        donor = List(donor_path)
        log("== %s (%d slots, %d empty) <- donor %s" % (os.path.basename(cp_path), cp.num, cp.slots.count(0), donor.name))
        out, replaced, delta, counts = build(cp, donor, log)
        if not verify(out, cp, donor, replaced, delta, counts, log):
            raise SystemExit(2)
    if dry:
        log("dry run: would write %d bytes to %s" % (len(out), out_path))
        return
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(out)
    log("wrote %d bytes -> %s" % (len(out), out_path))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cp", help="one override list (hdg_hold_cpA_01.motlist.524 ...)")
    ap.add_argument("--batch", help="a folder: every hdg_hold_*cp*_01.motlist.524 in it")
    ap.add_argument("--donor", help="the character's installed, spliced base_hdg_hold.motlist.524 (not needed with --null)")
    ap.add_argument("--null", action="store_true", help="empty the strafe / idle / raise slots instead of filling them (the 2026-10-05 Claire treatment)")
    ap.add_argument("--out", help="output file (with --cp)")
    ap.add_argument("--out-dir", help="output folder (with --batch); same file names")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    if not a.null and not a.donor:
        ap.error("--donor is needed unless --null")
    if a.cp:
        one(a.cp, a.donor, a.out or a.cp + ".spliced", a.dry_run, null=a.null)
    elif a.batch:
        files = sorted(glob.glob(os.path.join(a.batch, "hdg_hold_*cp*_01.motlist.524")))
        if not files: raise SystemExit("no hdg_hold_*cp*_01.motlist.524 in " + a.batch)
        for f in files:
            one(f, a.donor, os.path.join(a.out_dir or a.batch + "-spliced", os.path.basename(f)), a.dry_run, null=a.null)
            print()
    else:
        ap.error("give --cp or --batch")


if __name__ == "__main__":
    main()
