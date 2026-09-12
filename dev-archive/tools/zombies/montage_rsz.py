"""Minimal RSZ (.user.2) reader/writer for RE2's zombie montage tables.

Covers exactly the classes those nine files use, with the field order read off the
il2cpp dump (offset order) and the alignment rules of the RE_RSZ template
(alphazolam, public): u32-aligned strings = u32 char count (incl. NUL) + UTF-16LE,
object arrays = u32 count + u32 instance ids, bool = 1 byte, align 1.

Verified by round-trip: parse -> build must reproduce every shipped file byte for byte
(see roundtrip()). Anything outside these classes raises rather than guessing.

Usage:
  py montage_rsz.py learn      <shipped .user.2 files>   learn type hashes, then round-trip
  py montage_rsz.py roundtrip  <files>
  py montage_rsz.py dump       <files>                   JSON of the parsed instances
"""
import struct, sys, os, json

# field specs: S string, R resource path (serialised like a string), B bool,
#              OA object-ref array, SA string array
LAYOUTS = {
    # via.Prefab is its own instance (a u32 flag, 0 in every shipped file, then the path);
    # PartsPrefabData refers to it by instance id.
    "PrefabObj":         [("_flag", "U32"), ("Path", "R")],
    "PartsPrefabData":   [("KeyName", "S"), ("Prefab", "O")],
    "PartsContainer":    [("FacePrefabs", "OA"), ("BodyPrefabs", "OA"), ("ShirtPrefabs", "OA"),
                          ("PantsPrefabs", "OA"), ("AccessoryPrefabs", "OA")],
    "MontageData":       [("MontageID", "S"), ("FaceKeyName", "S"), ("BodyKeyName", "S"),
                          ("ShirtKeyName", "S"), ("PantsKeyName", "S"), ("AccessoryKeyNames", "SA"),
                          ("SpecifiedCombination", "B")],
    "MontageTableData":  [("MontageDataTable", "OA")],
    "MontagePartsItem":  [("RegisterName", "S")],
    "CombinationRule":   [("IsUnAuthorizedCombination", "B"), ("Combination", "OA"), ("_RuleName", "S")],
    "CombinationRuleUD": [("CombinationRuleList", "OA")],
}

HASH_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "montage_hashes.json")
KNOWN_HASHES = {}   # type_id -> layout name, learned from the shipped files
if os.path.exists(HASH_FILE):
    KNOWN_HASHES = {int(k): v for k, v in json.load(open(HASH_FILE)).items()}


def align(n, a):
    return (n + a - 1) // a * a


class Reader:
    def __init__(self, data, pos):
        self.d = data
        self.p = pos

    def u32(self):
        self.p = align(self.p, 4)
        v = struct.unpack_from("<I", self.d, self.p)[0]
        self.p += 4
        return v

    def u8(self):
        v = self.d[self.p]
        self.p += 1
        return v

    def string(self):
        n = self.u32()
        s = self.d[self.p:self.p + 2 * n].decode("utf-16-le")
        self.p += 2 * n
        assert n >= 1 and s.endswith("\0"), "string not NUL-terminated (count %d)" % n
        return s[:-1]

    def read(self, spec):
        out = {}
        for name, t in spec:
            if t in ("S", "R"):
                out[name] = self.string()
            elif t in ("U32", "O"):
                out[name] = self.u32()
            elif t == "B":
                out[name] = bool(self.u8())
            elif t == "OA":
                n = self.u32()
                out[name] = [self.u32() for _ in range(n)]
            elif t == "SA":
                n = self.u32()
                out[name] = [self.string() for _ in range(n)]
            else:
                raise ValueError(t)
        return out


class Writer:
    def __init__(self):
        self.b = bytearray()

    def pad(self, a):
        while len(self.b) % a:
            self.b.append(0)

    def u32(self, v):
        self.pad(4)
        self.b += struct.pack("<I", v)

    def u8(self, v):
        self.b.append(v & 0xFF)

    def string(self, s):
        # an empty string is shipped as count 1 + NUL, never as count 0
        enc = (s + "\0").encode("utf-16-le")
        self.u32(len(enc) // 2)
        self.b += enc

    def write(self, spec, obj):
        for name, t in spec:
            v = obj[name]
            if t in ("S", "R"):
                self.string(v)
            elif t in ("U32", "O"):
                self.u32(v)
            elif t == "B":
                self.u8(1 if v else 0)
            elif t == "OA":
                self.u32(len(v))
                for x in v:
                    self.u32(x)
            elif t == "SA":
                self.u32(len(v))
                for x in v:
                    self.string(x)


def parse(data):
    assert data[:4] == b"USR\0", "not a .user file"
    res_n, ud_n, info_n = struct.unpack_from("<III", data, 4)
    res_tbl, ud_tbl, data_off = struct.unpack_from("<QQQ", data, 0x10)
    assert res_n == 0 and ud_n == 0, "resource/userdata tables present - not handled"
    rsz = data_off
    assert data[rsz:rsz + 4] == b"RSZ\0"
    ver, obj_n, inst_n, udata_n = struct.unpack_from("<IIII", data, rsz + 4)
    inst_off, dat_off, udata_off = struct.unpack_from("<QQQ", data, rsz + 0x18)
    assert udata_n == 0, "embedded userdata - not handled"
    objs = [struct.unpack_from("<I", data, rsz + 0x30 + 4 * i)[0] for i in range(obj_n)]
    infos = [struct.unpack_from("<II", data, rsz + inst_off + 8 * i) for i in range(inst_n)]
    assert infos[0] == (0, 0)
    r = Reader(data, rsz + dat_off)
    insts = [None]
    for i in range(1, inst_n):
        tid, crc = infos[i]
        lay = KNOWN_HASHES.get(tid)
        if lay is None:
            raise KeyError("unknown type id 0x%08x at instance %d - run learn first" % (tid, i))
        inst = {"_type": lay, "_tid": tid, "_crc": crc}
        inst.update(r.read(LAYOUTS[lay]))
        insts.append(inst)
    assert r.p == len(data), "trailing bytes: parsed to %d of %d" % (r.p, len(data))
    return {"version": ver, "objects": objs, "instances": insts}


def build(doc):
    insts = doc["instances"]
    w = Writer()
    for inst in insts[1:]:
        w.write(LAYOUTS[inst["_type"]], inst)
    body = bytes(w.b)
    inst_n = len(insts)
    obj_n = len(doc["objects"])
    inst_off = align(0x30 + 4 * obj_n, 4)
    dat_off = align(inst_off + 8 * inst_n, 16)
    out = bytearray()
    out += b"USR\0" + struct.pack("<III", 0, 0, 0) + struct.pack("<QQQ", 0x30, 0x30, 0x30) + b"\0" * 8
    rsz = bytearray(b"RSZ\0" + struct.pack("<IIII", doc["version"], obj_n, inst_n, 0) + b"\0" * 4)
    rsz += struct.pack("<QQQ", inst_off, dat_off, dat_off)
    for o in doc["objects"]:
        rsz += struct.pack("<I", o)
    while len(rsz) < inst_off:
        rsz.append(0)
    rsz += struct.pack("<II", 0, 0)
    for inst in insts[1:]:
        rsz += struct.pack("<II", inst["_tid"], inst["_crc"])
    while len(rsz) < dat_off:
        rsz.append(0)
    rsz += body
    out += rsz
    return bytes(out)


def insert_instances(doc, at, new_insts):
    """Insert instances before index `at` (1-based instance id) and renumber every object
    reference >= at in the whole document, including the object table."""
    n = len(new_insts)
    insts = doc["instances"]

    def shift(v):
        return v + n if v >= at else v
    for inst in insts[1:]:
        for name, t in LAYOUTS[inst["_type"]]:
            if t == "O":
                inst[name] = shift(inst[name])
            elif t == "OA":
                inst[name] = [shift(x) for x in inst[name]]
    doc["objects"] = [shift(o) for o in doc["objects"]]
    doc["instances"] = insts[:at] + list(new_insts) + insts[at:]
    return at


def template(doc, layout):
    """A copy of the first instance of `layout` in doc, to reuse its type id and crc."""
    for inst in doc["instances"][1:]:
        if inst["_type"] == layout:
            return dict(inst)
    raise KeyError(layout)


def learn(files):
    """Assign layouts to type hashes by structural position in the shipped files, then persist."""
    for f in files:
        data = open(f, "rb").read()
        rsz = struct.unpack_from("<Q", data, 0x20)[0]
        obj_n, inst_n = struct.unpack_from("<II", data, rsz + 8)
        inst_off = struct.unpack_from("<Q", data, rsz + 0x18)[0]
        infos = [struct.unpack_from("<II", data, rsz + inst_off + 8 * i) for i in range(inst_n)]
        tids = [t for t, c in infos[1:]]
        root = tids[-1]
        base = os.path.basename(f)
        if "PartsContainer" in base:
            KNOWN_HASHES[root] = "PartsContainer"
            distinct = []
            for t in tids[:-1]:
                if t not in distinct:
                    distinct.append(t)
            assert len(distinct) == 2, distinct   # via.Prefab first, then the entry that refers to it
            KNOWN_HASHES[distinct[0]] = "PrefabObj"
            KNOWN_HASHES[distinct[1]] = "PartsPrefabData"
        elif "MontageTableData" in base:
            KNOWN_HASHES[root] = "MontageTableData"
            for t in tids[:-1]:
                KNOWN_HASHES[t] = "MontageData"
        elif "CombinationRule" in base:
            KNOWN_HASHES[root] = "CombinationRuleUD"
            distinct = []
            for t in tids[:-1]:
                if t not in distinct:
                    distinct.append(t)
            assert len(distinct) == 2, distinct   # items first, then rules
            KNOWN_HASHES[distinct[0]] = "MontagePartsItem"
            KNOWN_HASHES[distinct[1]] = "CombinationRule"
    json.dump({str(k): v for k, v in KNOWN_HASHES.items()}, open(HASH_FILE, "w"), indent=1)
    return KNOWN_HASHES


def first_diff(a, b):
    n = min(len(a), len(b))
    for i in range(n):
        if a[i] != b[i]:
            return i
    return n


def roundtrip(files):
    ok = True
    for f in files:
        data = open(f, "rb").read()
        doc = parse(data)
        out = build(doc)
        same = out == data
        ok &= same
        verdict = "IDENTICAL" if same else "DIFFERS at byte %d (out %d B)" % (first_diff(out, data), len(out))
        print("%-52s %6d B  instances=%3d  roundtrip=%s" % (os.path.basename(f), len(data), len(doc["instances"]), verdict))
    return ok


if __name__ == "__main__":
    cmd, files = sys.argv[1], sys.argv[2:]
    if cmd == "learn":
        learn(files)
        print("all identical:", roundtrip(files))
    elif cmd == "roundtrip":
        print("all identical:", roundtrip(files))
    elif cmd == "dump":
        for f in files:
            print(json.dumps(parse(open(f, "rb").read()), indent=1))
