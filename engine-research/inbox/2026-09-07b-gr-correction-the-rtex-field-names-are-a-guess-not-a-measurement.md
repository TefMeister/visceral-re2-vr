# Correction: the `.rtex` field names at `0x18`–`0x24` are a guess that fits, not a measurement

**From:** `/gr` (estate sweep, 2026-09-07, second drop) · **For:** the modding lane

Supersedes: `inbox/2026-09-07-gr-the-rtex-descriptor-is-decoded-in-the-sibling-project.md` — its
field table, specifically the rows for `0x18`, `0x1C`, `0x20` and `0x24`, and the wording "mip count
is a flat `1`"

⚠️ **Read this before draining the earlier drop.** Its conclusion survives; two of its field *names*
do not.

## What changed

A later search this same session found the `.rtex` format **documented publicly** — kagenocookie's
RE-Engine-Lib, `REE-Lib/OtherFiles/RTexFile.cs` `[reported 2026-09-07]`. Comparing it with the
sibling project's decode:

| offset | our `rtex_author.py` | kagenocookie's `RTexFile.cs` | observed bytes |
| --- | --- | --- | --- |
| `0x0C` | DXGI format | **DxgiFormat enum** — agrees | 29 / 26 |
| `0x10`, `0x14` | width, height | width, height — agrees | vary |
| `0x18` | depth / array size | **depth** | `1` |
| `0x1C` | — | **mipCount** | `0` |
| `0x20` | — | **arraySize** | `0` |
| `0x24` | **mip count** | **ukn1** | `1` |
| `0x34`, `0x38` | two unnamed `f32 1.0` | ⭐ **`widthRate`, `heightRate`** | `1.0`, `1.0` |

**Both readings fit the observed bytes**, and they cannot both be right. Ours implies mip 1 / array 1;
theirs implies mip 0 / array 0. Ours is the more physically plausible, which is presumably why it was
written that way — but plausibility is not evidence, and the public library was written against a
wider corpus. **Treat `0x18`–`0x24` as unresolved**, and say so in `rtex_resize.py`'s docstring rather
than repeating a name that has not been tested.

## ⭐ What the public format adds

**`0x34` and `0x38` are `widthRate` and `heightRate`** — resolution **scale rates**, which the library
validates as greater than zero. Both projects' decodes recorded them as anonymous `1.0` floats.

**This matters for the grime row directly.** If a Record target ever turns out not to be allocated at
the literal width/height written in the file, **these two floats are where that discrepancy lives** —
and they are a second lever on target size that neither project has touched. Worth reading, and worth
trying before concluding that a size change was ignored.

Also confirmed independently: **`0x0C` is a plain `DxgiFormat` value**, not an engine-private enum, so
any format number read out of one of these files can be looked up directly.

## ⚠️ And a version mismatch worth checking

kagenocookie's `re8/file_extensions.json` records **RE2's `.rtex` version as 4**, and RE8's as 5. But
this project's Record asset is **`pl1000_body.rtex.5`**. Either **RE2RT differs from RE2** on this
extension (which would fit everything else we know — the RT patch bumped several format versions), or
the public version table needs a caveat. **Free to settle:** the version is the u32 at `0x04` of our
own shipped file, which `rtex_resize.py` already prints.

If our file really is v5 like RE8's, the sibling's field map applies with more confidence than the
earlier drop claimed — and the `widthRate`/`heightRate` pair is present in ours too, since those
fields exist only at version ≥ 5.

## What survives from the earlier drop, unchanged

**Its actual conclusion.** Every field between `0x18` and `0x30` is a small constant — `0` or `1` —
under *both* candidate layouts, and **none of them scales with width or height**. So the grime row's
*"the other fields depend on the size, decode the format first"* branch is still answered: there is no
size-dependent field to break. That argument never depended on which of the two names was right.

Also unchanged: the sibling's **byte-for-byte reproduction** of two shipped RE8 files
`[verified-numerically 2026-09-06]`, and its **live demonstration that the engine allocates and
latches an authored, non-shipped target size** `[verified-live 2026-09-06, n=1]`.

## Credit

**kagenocookie** — RE-Engine-Lib and REE-Lib-Resources (<https://github.com/kagenocookie>), the public
`.rtex` layout and the per-game file-extension version table. Added to
`external-research/CREDITS.md`. Read online only; nothing cloned or copied.
