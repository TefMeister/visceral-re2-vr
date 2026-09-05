"""Targeted RE Engine PAK puller (v4.0 unencrypted paks, as RE2 RT uses).
Format knowledge from Ekey's REE.PAK.Tool (MIT-style public source) - reimplemented here.
Usage: py pak_pull.py <game_dir> <out_dir> <path> [<path> ...]
Paths are the internal natives/... paths (case as in the file list). Patch paks win over chunk_000.
"""
import sys, os, struct, glob, zlib
import mmh3, zstandard

SEED = 0xFFFFFFFF

def h32(s):
    return mmh3.hash(s.encode("utf-16-le"), SEED, signed=False)

def load_table(pak):
    with open(pak, "rb") as f:
        hdr = f.read(16)
        if len(hdr) < 16:
            return None
        magic, major, minor, feature, total, fp = struct.unpack("<IBBHII", hdr)
        assert magic == 0x414B504B, "not a KPKA pak: %s" % pak
        assert major == 4, "unsupported pak major %d" % major
        assert feature == 0, "pak has features 0x%x (encrypted/chunked) - not handled" % feature
        raw = f.read(total * 48)
    table = {}
    for i in range(total):
        lo, hi, off, csz, dsz, attr, chk = struct.unpack_from("<IIqqqqQ", raw, i * 48)
        table[(lo, hi)] = (off, csz, dsz, attr)
    return table

def pull(pak, entry, out_path):
    off, csz, dsz, attr = entry
    comp = attr & 0xF
    enc = (attr >> 16) & 0xFF
    assert enc == 0, "encrypted resource (type %d) - not handled" % enc
    with open(pak, "rb") as f:
        f.seek(off)
        src = f.read(csz)
    if comp == 0:
        data = src
    elif comp == 1:
        data = zlib.decompress(src, -15)
    elif comp == 2:
        data = zstandard.ZstdDecompressor().decompressobj().decompress(src)
    else:
        raise RuntimeError("unknown compression %d" % comp)
    assert len(data) == dsz, "size mismatch %d != %d" % (len(data), dsz)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(data)
    return len(data), comp

def main():
    game, out, paths = sys.argv[1], sys.argv[2], sys.argv[3:]
    paks = sorted(glob.glob(os.path.join(game, "re_chunk_000.pak.patch_*.pak")), reverse=True)
    paks.append(os.path.join(game, "re_chunk_000.pak"))
    tables = []
    for p in paks:
        if os.path.getsize(p) < 16:
            continue
        t = load_table(p)
        tables.append((p, t))
        print("pak %-40s entries=%d" % (os.path.basename(p), len(t)))
    for path in paths:
        key = (h32(path.lower()), h32(path.upper()))
        for p, t in tables:
            if key in t:
                n, comp = pull(p, t[key], os.path.join(out, path.replace("/", os.sep)))
                print("OK   %-90s %9d bytes  comp=%d  from %s" % (path, n, comp, os.path.basename(p)))
                break
        else:
            print("MISS %s" % path)

main()
