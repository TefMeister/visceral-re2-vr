# The `.rtex.5` descriptor is fully decoded — in `re-village-scope-vr`, byte-for-byte verified

**From:** `/gr` (estate sweep, 2026-09-07) · **For:** the modding lane, to fold into
`ENGINE-DOSSIER.md` §(Record system, around line 202–205) and the board's `[VR]` grime row

**Full write-up:** [`external-research/topics/2026-09-07-the-rtex-descriptor-is-already-fully-decoded-in-the-sibling-re-engine-project.md`](../../external-research/topics/2026-09-07-the-rtex-descriptor-is-already-fully-decoded-in-the-sibling-re-engine-project.md)

## The dead end this is aimed at

The dossier records the Record-system lever as **"A loose rtex with a larger size is the untested
lever `[hypothesis]`"**, and the board's `[VR]` grime row's failure branch reads:

> "no dirt at all or a crash = **the other fields depend on the size, decode the format first**."

`rtex_resize.py` accordingly edits only `+0x10`/`+0x14` and copies the other 56 bytes blind, because
the rest of the descriptor is unknown **to this project**.

## It is not unknown to the account

`re-village-scope-vr` decoded the same **version 5**, 64-byte `RTEX` descriptor on 2026-09-06, off
**five shipped RE8 `movie/rtex` targets plus `mirror_env.rtex`** `[measured 2026-09-06, n=6 files]` —
and verified it by **reproducing two shipped files byte for byte** from the field definitions
`[verified-numerically 2026-09-06]`. Tool: `re-village-scope-vr/dev-archive/tools/rtex_author.py`.

| offset | field | shipped RE8 values |
| --- | --- | --- |
| `0x00` | magic `RTEX` | — |
| `0x04` | version | `5` |
| `0x08` | type / dimension | **`4`**, constant across all six |
| `0x0C` | DXGI format | `29` = `R8G8B8A8_UNORM_SRGB`; `26` = `R11G11B10_FLOAT` (`mirror_env`) |
| `0x10` / `0x14` | width / height | vary — the two fields we already edit |
| `0x18` | depth / array | `1` |
| `0x1C`, `0x20` | — | `0`, `0` |
| `0x24` | **mip count** | **`1`** |
| `0x28`, `0x2C`, `0x30` | — | `0` |
| `0x34`, `0x38` | two floats | `1.0`, `1.0` |

## What this changes

**1. The grime row's "crash / no dirt" branch can be narrowed statically.** There is **no
size-dependent field** in a v5 descriptor: mip count is a flat `1` across six shipped files ranging
1024² to 1920×1088, depth/array is `1`, and the rest is zeros and two `1.0` floats. No stride, no
byte-size, no offset table. So a crash should **not** be read first as "we broke a field that
depended on the size" — the row's other two branches (the Record system's own config; the asset being
cached) become the leading suspects. `[inferred-static 2026-09-07]` — inferred from the sibling's
six-file RE8 read, **not** measured on RE2R's own file.

**2. The engine is proven to accept an authored, non-shipped target size.** re-village authored
**2560×1448** and **3840×2168** `.rtex` files existing in no archive, deployed them loose with
`LooseFileLoader_Enabled`, and the engine allocated and latched the 2560 one; Tefa in the headset
called the picture clearly better `[verified-live 2026-09-06, n=1]`. Same engine, same file version,
same loose-file mechanism. That does **not** prove the *Record system* honours it — a different
consumer — so the grime row stays a real test, but "untested lever" is too strong now: the capability
is demonstrated at the account level and only this consumer is unverified.

**3. The name is not a contract.** RE8's `movie_1920_1080` is really 1920×**1088**, `movie_1280_720`
is 1280×**728**, `movie_1144_1048` is 1144×**808**, `mirror_env` a true 1024×1024. Our Record target
is square (512×512) so setting both fields equal is right — but do not assume it for a future asset.

**4. `rtex_resize.py`'s sixteen-dword dump now means something.** With the map above, its output
identifies the Record target's **format** — whether it is SRGB 8-bit like the movie targets or
something else — which bears on how grime is composited.

## Suggested next step — free, static, and it upgrades the claim

Run `rtex_resize.py` on the **shipped** `pl1000_body.rtex.5` and compare its sixteen dwords to the
table. If `+0x08` = 4, `+0x18` = 1, `+0x24` = 1 and `+0x34`/`+0x38` = 1.0, the format is confirmed
shared across RE2R and RE8, point 1 upgrades from `[inferred-static]` to `[measured]`, and the grime
row can drop "decode the format first" outright. **Any disagreement is a better finding still** — it
would mean the descriptor differs by title at the same version number.

## Credit

Sibling project `re-village-scope-vr` (`dev-archive/tools/rtex_author.py`,
`claude-memory/status/re-village-scope-vr.md` 2026-09-06). **Ekey — REE.PAK.Tool**
(<https://github.com/Ekey/REE.PAK.Tool>), whose published file lists let the sibling pull the shipped
RE8 `.rtex` set by name hash without unpacking; the decode rests on it. No public source was needed
for this finding and none was used beyond that credit.
