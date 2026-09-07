# The `.rtex.5` descriptor is already fully decoded — in our own sibling RE Engine project

**Status:** 🆕 new · **Priority:** medium-high — it narrows the grime row's reading table before the
launch that reads it, and it upgrades one `[hypothesis]` on the board using evidence we already own.

## Why this was looked up

The board's `[VR]` **grime resolution test** row is deployed and unseen:

> "a loose `pl1000_body.rtex.5` declaring a 2048×2048 record target is DEPLOYED … `[hypothesis]`.
> … Sharper edges than before = the size is honoured; unchanged = decided elsewhere or cached;
> **no dirt at all or a crash = the other fields depend on the size, decode the format first.**"

and dossier §(Record system) records the lever as **"untested"**:

> "A loose rtex with a larger size is the untested lever `[hypothesis]`."

Our tool `dev-archive/tools/re-engine/rtex_resize.py` accordingly changes **only** the two size
fields at `+0x10`/`+0x14` and copies the other 56 bytes verbatim, because the rest of the
descriptor is unknown to this project.

**It is not unknown to the account.** `re-village-scope-vr` decoded the same file version on
2026-09-06 and verified the decode numerically.

## The sibling's decode, which is the same format

From `re-village-scope-vr/dev-archive/tools/rtex_author.py`, read off **five shipped RE8
`movie/rtex` targets plus `mirror_env.rtex`** `[measured 2026-09-06, n=6 files]`. Both projects'
assets are **version 5** (`.rtex.5`) and both are 64 bytes with an `RTEX` magic, so this is the same
descriptor:

| offset | field | value in the shipped RE8 set |
| --- | --- | --- |
| `0x00` | magic | `RTEX` |
| `0x04` | version | `5` — the `.5` path suffix |
| `0x08` | type / dimension | **constant `4`** across all six (2D texture is the reading) |
| `0x0C` | DXGI format | `29` = `R8G8B8A8_UNORM_SRGB` (movie targets); `26` = `R11G11B10_FLOAT` (`mirror_env`) |
| `0x10` | **width** | varies |
| `0x14` | **height** | varies |
| `0x18` | depth / array size | `1` |
| `0x1C`, `0x20` | — | `0`, `0` |
| `0x24` | **mip count** | **`1`** |
| `0x28`, `0x2C`, `0x30` | — | `0`, `0`, `0` |
| `0x34`, `0x38` | two floats | `1.0`, `1.0` |

**The decode is byte-exact, not inferred:** authoring 1920×1088 and 1280×728 from those field
definitions **reproduces the shipped files byte for byte** `[verified-numerically 2026-09-06]`.

## ⭐ What this changes for the grime row, before it is ever launched

**1. The "crash / no dirt" branch can be narrowed now, statically.** That branch reads *"the other
fields depend on the size (mip count or a buffer) and this needs the format decoded first."* The
format **is** decoded, and there is **no size-dependent field in it**: the mip count at `+0x24` is a
flat `1` in all six shipped RE8 files regardless of their size (which range 1024² to 1920×1088), the
depth/array is `1`, and the remaining dwords are zeros and two `1.0` floats. There is no stride, no
byte-size and no offset table to keep consistent. So if the launch does produce a crash or no dirt,
**"we edited a field that another field depended on" is the wrong first suspect** — look at the
Record system's own config, or at the asset being cached, which are the row's other two branches.
`[inferred-static 2026-09-07]` — inferred from the sibling's six-file read, not measured on RE2R's
own file.

**2. The engine is proven to accept an authored, non-shipped target size — in RE8.** re-village
authored **2560×1448** and **3840×2168** `.rtex` files that exist in no game archive, deployed them
as loose files with `LooseFileLoader_Enabled`, and the engine **allocated and latched the 2560 one**;
Tefa in the headset on 2026-09-06 reported the picture as clearly better
`[verified-live 2026-09-06, n=1]`. That is the general capability the grime row's `[hypothesis]`
rests on — "a loose `.rtex` declaring a bigger size is honoured" — demonstrated on the same engine,
the same file version and the same loose-file mechanism. It does **not** prove the *Record system*
honours it, which is a different consumer, so the row stays a real test; but the lever itself is no
longer untested at the account level.

**3. Both size fields are being written, and only one may be wanted.** `rtex_resize.py` sets width
**and** height to `--size` (square). The RE2R Record target is square already (512×512), so that is
right here — but note the sibling's finding that **the name is not a contract**: RE8's
`movie_1920_1080` is really 1920×**1088**, `movie_1280_720` is 1280×**728**, `movie_1144_1048` is
1144×**808**, and `mirror_env` is a true 1024×1024. If a future RE2R Record asset is ever
non-square, do not assume the name.

**4. The format field is readable, and worth printing.** `rtex_resize.py` already dumps all sixteen
dwords; with the map above, that dump now *means* something — in particular whether the Record
target is an SRGB 8-bit surface (`29`) like the movie targets or something else, which bears on how
the grime is composited.

## Honest limits

- The map comes from **RE8's** shipped files. Version 5 and the 64-byte size match RE2R's, and the
  two known fields (`+0x10`/`+0x14`) agree, but no one has checked an RE2R `.rtex.5` against the
  full map. **That check is free** — `rtex_resize.py` already prints all sixteen dwords of the
  shipped `pl1000_body.rtex.5`; comparing them to the table above is a one-minute static job and
  would upgrade point 1 from `[inferred-static]` to `[measured]`.
- Nothing here says the Record system reads the size from this asset at all. That is exactly what
  the launch is for.

## The concrete next step this unlocks

A free static check before the queued launch: run `rtex_resize.py` on the **shipped**
`pl1000_body.rtex.5` and compare its sixteen dwords against the table above. If `+0x08` is `4`,
`+0x18` is `1`, `+0x24` is `1` and `+0x34`/`+0x38` are `1.0`, the format is confirmed shared across
RE2R and RE8 and the grime row's reading table can drop its "decode the format first" clause
outright. Any disagreement is itself a finding, and a more interesting one.

## Sources

Both are our own account; no public source was needed or found for this.

- `re-village-scope-vr/dev-archive/tools/rtex_author.py` — the field map and the byte-for-byte
  reproduction `[verified-numerically 2026-09-06]`.
- `claude-memory/status/re-village-scope-vr.md`, entry 2026-09-06 (`/pd`, second pass) and the
  2026-09-06 23:25 OPEN block — the authored 2560/3840 targets, `LooseFileLoader_Enabled`, and
  Tefa's live verdict on the 2560 picture.
- `visceral-re2-vr/dev-archive/tools/re-engine/rtex_resize.py` and
  `engine-research/ENGINE-DOSSIER.md` (Record system) — this project's side.
- **Ekey — REE.PAK.Tool** (<https://github.com/Ekey/REE.PAK.Tool>): the published file lists the
  sibling used to pull the shipped RE8 `.rtex` set by name hash without unpacking. Credited here
  because the decode rests on it.
