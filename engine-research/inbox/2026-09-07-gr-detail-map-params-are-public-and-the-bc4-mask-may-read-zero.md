# The detail-map parameters are published — and our deployed BC4 mask may read as zero everywhere

**From:** `/gr` (estate sweep, 2026-09-07) · **For:** the modding lane, to fold into
`ENGINE-DOSSIER.md` (materials / HD-hands section) and the board's `[PD]` texel-density row

Supersedes: the parameter names `DetailNormalMap` and `DetailIntensity` wherever this project uses
them — neither exists in RE2/RE2RT

**Full write-up:** [`external-research/topics/2026-09-07-the-shipped-detail-map-parameters-are-public-and-the-bc4-mask-is-a-channel-gamble.md`](../../external-research/topics/2026-09-07-the-shipped-detail-map-parameters-are-public-and-the-bc4-mask-is-a-channel-gamble.md)

## ⚠️ Read §1 first — it concerns a change that is already deployed

The `pl1000_Jacket_MSK1.tex.34` mask deployed 2026-09-06 15:06 is **2048 BC4**. Sampling a
**BC4_UNORM** texture in D3D11/12 returns `(R, 0, 0, 1)` — **green and blue read as 0**.

**Which channel `DetailMaskMap` is read from is not documented anywhere public** — a genuine
negative, from a corpus that *does* document channel packing for other maps (the Havens-Night wiki
gives `NRRC` and `ATOC` layouts; Riot1986's thread gives ALBM/NRMR/ATOS), so the shape of the answer
exists and the detail mask simply is not covered `[checked 2026-09-07]`.

So if the shader reads `.g` or `.b`, **our mask reads 0 across the whole material** and kills the
detail tile everywhere — and **that failure looks exactly like success**: palms smoother as intended,
regression everywhere nobody is looking. `[hypothesis 2026-09-07]` — a D3D spec inference about BC4,
not an observation of RE Engine's shader.

Three supporting points: there is **no channel-selector property** for this mask (where RE Engine
wants a runtime channel pick it ships an explicit vec4, e.g. `Rec_RTTChannelControl`), so the channel
is hard-coded; the `NullWhite` default is white in all four channels and gives nothing away; and the
engine's own masks are **four-channel `_MSK4`** (`ImperfectDetail_MSK4`, `NullGray_MSK4`) while ours
is `_MSK1`.

**Cheap disambiguation:** ship a **uniform mid-grey** mask first. Whole-material grain halves → the
shader reads a channel BC4 populates, and a black region is then a trustworthy off switch. Nothing
changes → it reads G or B and the mask needs a 4-channel format before any of this means anything.
This is the family page's own "never read back against the neutral value" rule applied to a texture.

## 2. ✏️ Two parameter names we use do not exist, and the shipped values are public

The shipped **`pl1000_Jacket_Mat`** set is published in NSACloud's RE Mesh Editor presets
`[reported 2026-09-07]`, master material `MasterMaterial/Master/Record_Player.mmtr`:

| kind | name | value |
| --- | --- | --- |
| texture | **`DetailMap`** | `MasterMaterial/Textures/NullDetail.tex` |
| texture | `DetailMaskMap` | `systems/rendering/NullWhite.tex` |
| float | `Detail_UVScale` | **`0.25`** |
| float | **`Detail_Normal_Intensity`** | `0.52` |
| float | **`Detail_AO_Intensity`** | `0.0` |

**There is no `DetailNormalMap` and no `DetailIntensity` in RE2/RE2RT.** The texture slot is
`DetailMap` (packed — hence separate normal and AO intensities).

⚠️ **Two discrepancies to check locally, not to correct from here.** The preset says
`Detail_UVScale` **0.25**; the board says 0.5. And the preset's `DetailMap` is a **null** texture. But
the preset row is `pl1000_Jacket_**Mat**` while our MDF edit targets `pl1000_Body_**Mat**` — plausibly
two different materials, given how RE2 crosses these names over. **Read our own dumped
`pl1000.mdf2.21`** and compare; it is free and it settles both. The preset is a third party's snapshot
of a shipped material, not our install: a strong lead, a poor authority.

## 3. ⭐ A cheaper positive control than the row plans — one float, no texture

`Detail_Normal_Intensity` and `Detail_AO_Intensity` are plain floats on the same material. **Setting
them to 0 disables the detail tile for that material with no texture edit and no channel gamble**
`[inferred-static 2026-09-07]`. The shipped `Detail_AO_Intensity` is already 0.0, so realistically it
is **one number**.

This answers the board's own hesitation (*"it changes skin Tefa has already judged good … not
switched on unasked"*) by separating the diagnostic from the fix: band gone with the intensity at 0 →
the tile is the cause and the mask work is worth doing properly; band unchanged → the tile is not the
cause and the mask work is not worth doing at all.

⚠️ And do not assume `Detail_UVScale` is a multiplier rather than a divisor — nothing public states
the direction, and every shipped value seen is sub-1, which fits either reading.

## 4. ✅ The row's mechanism is confirmed: the tile is UV-space

`[inferred-static 2026-09-07]`, four converging pieces, no trace of a triplanar/world-space detail
path in any RE Engine master material: the parameter is a single scalar `Detail_UVScale`; the whole
`Record_Player.mmtr` overlay family is UV-scaled and several carry `*_UseSecondaryUV` booleans (which
only mean something if the overlay samples from a UV set); RE4R exposes
`DetailMap_Tiling_Offset [33,19,0,0]` and RE9 adds `DetailMap_Rotation`/`_Scale` — UV-transform
semantics. **So a ×1.6 texel-density step across the wrist does step the tile's grain there.**

Also confirmed: a black `DetailMaskMap` region does stop the DetailMap being applied
(alphaZomega, the canonical MDF tutorial), and `NullWhite.tex` is one of an engine-internal
null-asset family under `systems/rendering/` meaning "fully on".

## 5. ✅ Format versions confirmed

`tex .34` / `mdf2.21` is exactly the **RERT** generation (plain RE2 is `.10`/`.mdf2.10`, RE3 is
`.mdf2.13`) `[reported 2026-09-07]`. `.mdf2` v21 sits in the v19+ branch (adds the GPBF buffer
name/path count and offset per material entry); the 24-byte property header **types by count, not by
a tag** (`paramCount == 1` float, `== 4` float4, 4 bytes per value); names carry **two MurmurHash3
hashes**, one over UTF-16 and one over ASCII/UTF-8. `.tex` v34 falls in the `version > 27` branch
that adds the swizzle block.

## No public report of this seam exists

No account of a detail-tile seam at a UV-density change in **any** RE Engine game, across the MDF
tutorial thread, four other forum threads, the Havens-Night wiki and ~10 targeted searches that did
surface the correct RE-Engine pages `[checked 2026-09-07]`. Read as "nobody has written it down" —
most live RE Engine material discussion is on Discord, which is not indexed. Infrastructure blocks
(grep.app 429 on four attempts, Nexus 403) mean there is **no code-search evidence either way**.

## Credit

NSACloud (RE Mesh Editor + the presets carrying the shipped parameter set); alphaZomega (alphazolam)
(the MDF tutorial, the 010 templates, the Noesis plugin format table); Che and Darkness (original MDF
structure research); Silvris / SilverEzredes (MDF-Manager); Havens-Night (channel-packing wiki);
Riot1986; terenceyao and smkquanchi. All added to `external-research/CREDITS.md`. Read online only;
nothing cloned, downloaded or copied.
