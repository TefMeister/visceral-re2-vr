"""motlist_splice.py -- walk normally while aiming (Visceral roadmap v2 item 22), as a SPLICE.

Builds a new base_hdg_hold.motlist.524 for RE2 (ray-tracing build) in which the twelve aim-walk
motions (six HG_Interpolation_*_Loop, six HG_Strafe*) are replaced by the character's own
walk-cycle motions from base_cmn_move.motlist.524. Nothing is decompressed or re-encoded: mot
entries are opaque blobs whose internal offsets are all entry-relative (CAF motlist_format_guide,
confirmed on the RT files 2026-09-06: no header offset points outside its own blob), so an entry
can be moved between containers whole. The container is rebuilt around them.

Layout facts this relies on (measured on the RT originals, 2026-09-06):
  container : 0x00 u32 version 524 | 0x04 "mlst" | 0x10 u64 pointer-table offset (0x50) | 0x18 u64
              collection offset | 0x20 u64 name offset (0x34) | 0x30 u32 slot count | 0x34 UTF-16 name
  pointers  : u64 file offset per SLOT; two slots may share one entry (Hold_Idle_Loop, Shoot_NoAmmo do)
  entries   : contiguous, 16-byte aligned, from the first pointer up to the collection offset; the blob
              of an entry runs to the next distinct entry start (padding included)
  collection: 72 bytes per SLOT, in slot order; only u32 at +8 varies = the motion NUMBER (0x6e for
              pl10_0110, ...); byte-identical between the original and a modder's replacement file, so it
              carries no offsets or sizes and is copied verbatim -- the game asks for a slot by number
  mot entry : v492 header; name offset at +0x58 (entry-relative), frame count f32 at +0x60,
              bone count / clip count u16 at +0x70, fps u16 at +0x78

Usage (player):   py motlist_splice.py --game-dir "<RE2 folder>" --character pl10 --out <folder>
       (dev)   :   py motlist_splice.py --hold <base_hdg_hold.motlist.524> --move <base_cmn_move.motlist.524> --out <folder>
Then copy the output to  <RE2>/natives/STM/sectionroot/animation/player/<pl10|pl00>/list/hdg/base_hdg_hold.motlist.524
with REFramework's LooseFileLoader_Enabled=true. The output is built from the player's own game files and is
never redistributed; only this script ships. --map lets you change which walk motion lands in which aim slot.

Legitimacy: read-only on the game's paks; writes only into --out. No game data is included in this file.
"""
import argparse, os, struct, subprocess, sys, tempfile

HDG_PATH = "natives/stm/sectionroot/animation/player/%s/list/hdg/base_hdg_hold.motlist.524"
CMN_PATH = "natives/stm/sectionroot/animation/player/%s/list/cmn/base_cmn_move.motlist.524"

# aim slot (suffix after the pl10_/pl00_ prefix) -> walk motion (suffix) that replaces it
DEFAULT_MAP = {
    "0110_HG_Interpolation_F_Loop":      "0190_KFF_GazingWalk_F_Loop",
    "0111_HG_Interpolation_L_Loop":      "0191_KFF_GazingWalk_L_Loop",
    "0112_HG_Interpolation_R_Loop":      "0194_KFF_GazingWalk_R_Loop",
    "0113_HG_Interpolation_Back_L_Loop": "0196_KFF_GazingWalk_Back_L_Loop",
    "0114_HG_Interpolation_Back_B_Loop": "0197_KFF_GazingWalk_Back_B_Loop",
    "0115_HG_Interpolation_Back_R_Loop": "0198_KFF_GazingWalk_Back_R_Loop",
    "0120_HG_StrafeL_F": "0190_KFF_GazingWalk_F_Loop",
    "0122_HG_StrafeL_L": "0191_KFF_GazingWalk_L_Loop",
    "0124_HG_StrafeL_B": "0197_KFF_GazingWalk_Back_B_Loop",
    "0126_HG_StrafeL_R": "0194_KFF_GazingWalk_R_Loop",
    "0132_HG_StrafeR_L": "0191_KFF_GazingWalk_L_Loop",
    "0136_HG_StrafeR_R": "0194_KFF_GazingWalk_R_Loop",
}


def u16s(d, off, maxlen=256):
    end = off
    while end + 1 < len(d) and not (d[end] == 0 and d[end + 1] == 0):
        end += 2
        if end - off > maxlen * 2:
            break
    return d[off:end].decode("utf-16-le", errors="replace")


class Motlist:
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
        starts = sorted(set(self.slots))
        ends = starts[1:] + [self.col]
        self.blob = {o: d[o:e] for o, e in zip(starts, ends)}        # entry start -> bytes (padding included)
        self.entry_name = {o: self.mot_name(o) for o in starts}
        self.by_name = {}
        for o, n in self.entry_name.items():
            self.by_name.setdefault(n, o)
        self.collection = d[self.col:]
        if len(self.collection) != 72 * self.num:
            raise SystemExit("%s: collection block is %d bytes, expected 72 x %d slots -- layout differs, refusing"
                             % (path, len(self.collection), self.num))
        self.header = d[:self.ptrs]

    def mot_name(self, o):
        d = self.data
        ver, magic = struct.unpack_from("<I4s", d, o)
        if magic != b"mot ":
            raise SystemExit("%s: entry at 0x%x has magic %r" % (self.path, o, magic))
        nameoff = struct.unpack_from("<Q", d, o + 0x58)[0]
        return u16s(d, o + nameoff)

    def mot_info(self, blob):
        fc = struct.unpack_from("<f", blob, 0x60)[0]
        bc, bcc = struct.unpack_from("<HH", blob, 0x70)
        fps = struct.unpack_from("<H", blob, 0x78)[0]
        return fc, bc, bcc, fps

    def slot_number(self, i):
        return struct.unpack_from("<I", self.collection, i * 72 + 8)[0]


def align16(n):
    return (n + 15) & ~15


def build(hold, move, mapping, prefix, log):
    """Return the spliced file bytes. mapping: aim-name-suffix -> walk-name-suffix."""
    # slot -> blob to write. Slots that share an entry keep sharing it (same source key).
    src = {}           # source key ("hold", off) or ("move", off) -> blob
    slot_key = []
    replaced = []
    for i, o in enumerate(hold.slots):
        name = hold.entry_name[o]
        suffix = name[len(prefix) + 1:] if name.startswith(prefix + "_") else name
        if suffix in mapping:
            walk = "%s_%s" % (prefix, mapping[suffix])
            if walk not in move.by_name:
                raise SystemExit("walk motion %s not found in %s (have: %s)" % (walk, move.path, ", ".join(sorted(move.by_name))))
            mo = move.by_name[walk]
            key = ("move", mo)
            src[key] = move.blob[mo]
            replaced.append((i, name, walk))
        else:
            key = ("hold", o)
            src[key] = hold.blob[o]
        slot_key.append(key)
    # lay out: header, pointer table, entries (16-aligned, contiguous, first-use order), collection
    cursor = align16(hold.ptrs + 8 * hold.num)
    placed = {}
    body = bytearray()
    for key in slot_key:
        if key in placed:
            continue
        blob = src[key]
        pad = align16(len(blob)) - len(blob)
        placed[key] = cursor
        body += blob + b"\0" * pad
        cursor += len(blob) + pad
    out = bytearray(hold.header)
    out += b"\0" * (hold.ptrs - len(out))
    for key in slot_key:
        out += struct.pack("<Q", placed[key])
    out += b"\0" * (align16(len(out)) - len(out))
    assert len(out) == align16(hold.ptrs + 8 * hold.num)
    col = len(out) + len(body)
    out += body
    out += hold.collection
    struct.pack_into("<Q", out, 0x18, col)
    # report
    log("slot  number  motion (after)                                  frames bones clips fps  <- source")
    for i, key in enumerate(slot_key):
        blob = src[key]
        fc, bc, bcc, fps = hold.mot_info(blob)
        n = (move if key[0] == "move" else hold).entry_name[key[1]]
        tag = "REPLACED (was %s)" % hold.entry_name[hold.slots[i]] if key[0] == "move" else "kept"
        log("[%2d]  0x%03x   %-46s %5.0f  %3d  %3d  %2d  %s" % (i, hold.slot_number(i), n, fc, bc, bcc, fps, tag))
    return bytes(out), replaced


def verify(path_or_bytes, hold, move, replaced, log):
    """Re-parse the output independently and check every claim the build made."""
    tmp = None
    if isinstance(path_or_bytes, bytes):
        tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".motlist.524")
        tmp.write(path_or_bytes); tmp.close()
        out = Motlist(tmp.name)
    else:
        out = Motlist(path_or_bytes)
    problems = []
    if out.num != hold.num: problems.append("slot count %d != %d" % (out.num, hold.num))
    if out.collection != hold.collection: problems.append("collection block changed")
    if out.name != hold.name: problems.append("container name changed")
    rep = {i: walk for i, _, walk in replaced}
    for i, o in enumerate(out.slots):
        want = rep.get(i, hold.entry_name[hold.slots[i]])
        got = out.entry_name[o]
        if got != want: problems.append("slot %d is %s, expected %s" % (i, got, want))
        srcblob = move.blob[move.by_name[want]] if i in rep else hold.blob[hold.slots[i]]
        if out.blob[o][:len(srcblob)] != srcblob: problems.append("slot %d blob bytes differ from source" % i)
        if o % 16: problems.append("slot %d entry not 16-aligned" % i)
    # Entry sharing: the original shares one entry between two slots for its repeated motions (Hold_Idle_Loop,
    # Shoot_NoAmmo), so sharing is a format-legal pattern. Original sharing must survive; new sharing is allowed
    # only between replaced slots that received the same walk motion (the build deduplicates them the same way).
    newly_shared = 0
    for a in range(hold.num):
        for b in range(a + 1, hold.num):
            was, now = hold.slots[a] == hold.slots[b], out.slots[a] == out.slots[b]
            if was and not now: problems.append("slots %d and %d shared an entry in the original and no longer do" % (a, b))
            if now and not was:
                if a in rep and b in rep and rep[a] == rep[b]: newly_shared += 1
                else: problems.append("slots %d and %d now share an entry but were not given the same walk motion" % (a, b))
    if newly_shared: log("verify: %d slot pairs now share one entry because they received the same walk motion (the original does this for two pairs of its own)" % newly_shared)
    if tmp: os.unlink(tmp.name)
    for p in problems: log("VERIFY FAIL: " + p)
    if not problems: log("verify: %d slots, %d replaced, collection verbatim, all blobs byte-identical to their sources, 16-aligned -- OK"
                         % (out.num, len(rep)))
    return not problems


def pull(game_dir, character, out_dir):
    here = os.path.dirname(os.path.abspath(__file__))
    paths = [HDG_PATH % character, CMN_PATH % character]
    r = subprocess.run([sys.executable, os.path.join(here, "pak_pull.py"), game_dir, out_dir] + paths, capture_output=True, text=True)
    sys.stdout.write(r.stdout)
    if r.returncode != 0 or "MISS" in r.stdout:
        raise SystemExit("pak pull failed:\n" + r.stdout + r.stderr)
    return [os.path.join(out_dir, p.replace("/", os.sep)) for p in paths]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game-dir", help="RE2 install folder (pulls the two originals from its paks)")
    ap.add_argument("--character", default="pl10", choices=["pl10", "pl00"], help="pl10 = Claire, pl00 = Leon (default pl10)")
    ap.add_argument("--hold", help="original base_hdg_hold.motlist.524 (instead of --game-dir)")
    ap.add_argument("--move", help="original base_cmn_move.motlist.524 (instead of --game-dir)")
    ap.add_argument("--out", required=True, help="output folder; the file lands under its natives/... path inside it")
    ap.add_argument("--map", action="append", default=[], metavar="AIM=WALK",
                    help="override one mapping, e.g. 0120_HG_StrafeL_F=0231_KFF_Jog_Straight_Loop (name suffixes after pl10_/pl00_)")
    ap.add_argument("--dry-run", action="store_true", help="report only, write nothing")
    a = ap.parse_args()
    if a.game_dir:
        pulled = os.path.join(a.out, "_originals")
        a.hold, a.move = pull(a.game_dir, a.character, pulled)
    if not (a.hold and a.move):
        ap.error("give --game-dir, or both --hold and --move")
    mapping = dict(DEFAULT_MAP)
    for m in a.map:
        k, v = m.split("=", 1)
        mapping[k] = v
    hold, move = Motlist(a.hold), Motlist(a.move)
    print("hold: %s  %d slots, %d entries, %d bytes" % (hold.name, hold.num, len(hold.blob), len(hold.data)))
    print("move: %s  %d slots, %d entries, %d bytes" % (move.name, move.num, len(move.blob), len(move.data)))
    out, replaced = build(hold, move, mapping, a.character, print)
    ok = verify(out, hold, move, replaced, print)
    if not ok:
        raise SystemExit(2)
    dest = os.path.join(a.out, (HDG_PATH % a.character).replace("/", os.sep))
    if a.dry_run:
        print("dry run: would write %d bytes to %s" % (len(out), dest))
        return
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with open(dest, "wb") as f:
        f.write(out)
    print("wrote %d bytes -> %s" % (len(out), dest))
    print("install: copy it to <RE2>/natives/STM/sectionroot/animation/player/%s/list/hdg/ with LooseFileLoader_Enabled=true" % a.character)


if __name__ == "__main__":
    main()
