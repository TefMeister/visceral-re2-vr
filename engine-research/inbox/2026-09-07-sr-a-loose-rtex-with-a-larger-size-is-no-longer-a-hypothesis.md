# Your §7b `[hypothesis]` — "a loose rtex with a larger size is the untested lever" — was proved on the sibling project yesterday

**Filed by `/sr`, 2026-09-07.** For the modding lane. Nothing run here; this is a pointer to a
sibling project's live result on the same engine.

## The row this answers

Your dossier §7b, on the record system's dirt/blood resolution:

> `Rec_RTT` = `VFX/RecordSystem/RecordTexture/<pl>/<pl>_body.rtex.5`, a 64-byte asset with the runtime
> target's size at +0x10/+0x14 (**512×512** for the whole body, sampled through `UVMap1` where both
> hands share ~a quarter of it) … **A loose rtex with a larger size is the untested lever**
> `[hypothesis]`.

## It is no longer a hypothesis — it ran on 2026-09-06

`re-village-scope-vr`, one VR launch `[verified-live 2026-09-06, n=1]`:

- A **2560×1448** `.rtex` written from scratch and deployed as a loose file produced
  `mirror RT: using movie/rtex/movie_2560_1440.rtex (2560x1448)`, then a latch, then the pipeline's own
  raw-HDR upgrade at the new size.
- So three things hold on RE Engine, at least on that title's build: **the engine honours a width and
  height it never shipped**; **REFramework's LooseFileLoader serves a path the pak does not contain at
  all** (a loader, not merely an override); and the descriptor really is the whole of the asset.
- The observer's verdict on the picture: *"it is way better the quality"*.

**Your `[hypothesis]` can be tested the same way, and it should be cheap** — the record target is
512×512 for a whole body with both hands sharing about a quarter of it, which is exactly the kind of
budget a bigger descriptor fixes for free.

## The two things worth copying, not just the result

1. **The full descriptor layout, measured** `[measured 2026-09-06, n=6 files]`: 64 bytes —
   `RTEX`, version 5, a 4, the DXGI format, width, height, then `1, 0, 0, 1, 0, 0, 0, 1.0f, 1.0f, 0`.
   Format read from Ekey's public REE.Unpacker source; **credit Ekey**.
2. **⭐ Validate the writer by round-trip before trusting a novel size.** That project's
   `rtex_author.py` reproduces the shipped 1920 and 1280 files **byte for byte**
   `[verified-numerically 2026-09-06, n=2 files]` before any new size was attempted. A novel asset that
   loads proves the engine tolerated it; a byte-identical reproduction proves you understood the format.
   The tool lives in that project's `dev-archive/tools/`, alongside `ree_pak_extract.py` for pulling a
   shipped file out by name hash to compare against.

**⚠️ Two cautions from the same session.** Shipped heights are **name + 8** there (`movie_1920_1080` is
really 1920×1088), so do not assume the filename is the size — read the descriptor. And the loose loader
has to be enabled (`LooseFileLoader_Enabled`), which is a config change worth backing up.

**And a positive control you get for free:** a sibling project deployed a loose texture for a character
who was not on screen, and the loader **opened it while nothing changed** `[measured 2026-09-06]`. That
is the ideal first deployment on a new asset path — the open proves reach, and no visible change is not
a failure. Without it you cannot tell "the override does nothing" from "the override was never read".

## Where the curated version lives

`flat-to-vr-cross-engine-research`:
- `docs/techniques/README.md` → "When the shipped inventory has nothing big enough, the limit is on
  borrowing — not on having"
- `docs/engines/re-engine.md` → "an `.rtex` is a 64-byte descriptor — you can author render targets the
  game never shipped"

Primary evidence: `re-village-scope-vr/engine-research/ENGINE-DOSSIER.md` §9h–§9i.
Credit **Ekey** (REE.PAK.Tool / REE.Unpacker) and **praydog** (REFramework).
