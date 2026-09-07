# Claire's shipped detail-map parameters are published — and our deployed BC4 mask is a channel gamble

**Status:** 🆕 new · **Priority:** high — it confirms the `[PD]` texel-density row's mechanism,
**corrects two parameter names we are using**, offers a cheaper positive control than the row plans,
and flags a real failure mode in a mask that is **already deployed and unseen**.

## Why this was looked up

The board's `[PD]` seam row rests on a stated `[hypothesis]`:

> "The shipped material still runs the engine's own `Detail_Skin` tile (128², `Detail_UVScale` 0.5)
> **in UV space**, so its grain is 1.6× coarser on the forearm than on the hand and steps exactly
> where Tefa sees a band … **We can switch it off:** the MDF already points `DetailMaskMap` at our
> own `pl1000_Jacket_MSK1`, so zeroing that mask over hands+forearms stops the tile there."

Two things in that are engine facts rather than opinions — *is the tile UV-space?* and *does a black
mask really switch it off?* — so they were asked publicly.

## ✅ 1. The tile is UV-space. The row's mechanism holds

`[inferred-static 2026-09-07]`, from four converging pieces, and no public trace of a triplanar or
world-space detail path in **any** RE Engine master material:

- The parameter is a **single float** named `Detail_UVScale`. One scalar cannot drive a world-space
  projection.
- The whole `Record_Player.mmtr` overlay family is UV-scaled (`Rec_Mud_UVScale`, `Rec_BurnUVScale`,
  `Rec_Injury_InSideUVScale`/`OutSideUVScale`) and several carry a `*_UseSecondaryUV` boolean —
  which only means anything if the overlay samples **from a UV set**.
- Later titles spell it out: RE4R's character detail material has
  `DetailMap_Tiling_Offset = [33.0, 19.0, 0.0, 0.0]` (tiling.xy / offset.zw); RE9/Requiem's adds
  `DetailMap_Rotation` and `DetailMap_Scale`. Rotation plus a 2D tiling/offset pair is UV-transform
  semantics, not triplanar.

So **a ×1.6 texel-density step across the wrist does produce a ×1.6 step in the tile's apparent
grain at exactly that seam.** The mechanism the row proposes is sound. (Nobody has written that
consequence down for RE Engine specifically — see §5 — but it is the same behaviour Unity and Unreal
document for their detail/secondary maps.)

## ✏️ 2. Two of the names we use do not exist in this game — and the shipped values are published

⭐ **The shipped material set for `pl1000_Jacket_Mat` — Claire's jacket — is public**, in NSACloud's
RE Mesh Editor material presets `[reported 2026-09-07]`:

| kind | name | shipped value (RE2RT `pl1000_Jacket_Mat`) |
| --- | --- | --- |
| texture | **`DetailMap`** | `MasterMaterial/Textures/NullDetail.tex` |
| texture | `DetailMaskMap` | `systems/rendering/NullWhite.tex` |
| float | **`Detail_UVScale`** | **`0.25`** |
| float | **`Detail_Normal_Intensity`** | `0.52` |
| float | **`Detail_AO_Intensity`** | `0.0` |

master material: `MasterMaterial/Master/Record_Player.mmtr`

**In RE2/RE2RT there is no `DetailNormalMap` and no `DetailIntensity`.** The texture slot is
`DetailMap` — a *packed* detail texture, as the presence of both `Detail_Normal_Intensity` and
`Detail_AO_Intensity` implies — and the strengths are those two floats. Neither invented name appears
in any RE Engine preset read. `[inferred-static 2026-09-07]`

⚠️ **Two discrepancies against our board, both needing a local check rather than a correction here:**

1. **`Detail_UVScale` reads 0.25 in the preset; the board says 0.5.** Do not assume either is wrong.
   The preset row is `pl1000_Jacket_**Mat**`, and the MDF edit we deployed targets
   `pl1000_Body_**Mat**` — RE2's naming genuinely crosses over (our textures are `pl1000_Jacket_*`
   while the material we patch is `pl1000_Body_Mat`). **These may simply be two different materials.**
   The check is free: our own `pl1000.mdf2.21` read already enumerates every material's properties.
2. **The preset's `DetailMap` is `NullDetail.tex` and `DetailMaskMap` is `NullWhite.tex`** — i.e. in
   NSACloud's captured state this material's detail tile is pointed at a **null** texture. If that
   holds for the material we are patching, the tile may already be off there and the wrist band has
   another cause entirely. Again: read our own dumped MDF, do not take the preset as our build.

The preset is a **snapshot of a shipped material by a third party**, not our install. It is a strong
lead and a poor authority.

## ✅ 3. A black mask does switch the tile off — and `NullWhite` means "fully on"

alphaZomega, the primary public author of RE-era MDF documentation, states it directly: where the
DetailMask map is black on a submesh, that submesh's DetailMap is not applied; and a DetailMap is
the engine's way of adding tiled fabric/skin-pore/leather normal detail that a 4K map could not carry
`[reported 2026-09-07]`.

`NullWhite.tex` is one of an engine-internal null-asset family under `systems/rendering/`
(`NullBlack`, `NullNormal`, `NullNormalRoughness`, `NullATOS`, plus master-material-local
`NullDetail`), used as the "off" value in every preset slot; alphaZomega notes these null textures
mark unused maps and can be repointed to bring a map into use. White = 1 = mask fully open, so
`1 = detail on, 0 = detail off` is the semantic. Independently visible in a public Noesis dump of
RE3R's Jill, where `DetailMaskMap → systems/rendering/NullWhite.tex` recurs across seven materials.

## ⚠️ 4. THE RISK: our deployed 2048 **BC4** mask may read as zero everywhere

This is the part that matters most, because the mask is **already deployed** (15:06 on 2026-09-06)
and has not been seen.

**Which channel of `DetailMaskMap` the shader reads is not documented anywhere public**
`[reported 2026-09-07 — a genuine negative, see below]`. And in D3D11/12, sampling a **BC4_UNORM**
texture returns `(R, 0, 0, 1)`: green and blue read as **0**, alpha as 1.

So:

- if the shader reads `.r` or `.a`, our BC4 mask behaves as intended;
- if it reads **`.g` or `.b`**, our mask reads **0 across the entire material** — killing the detail
  tile everywhere, not just on the hands.

**That failure would look like success.** The palms would go smoother exactly as the row predicts,
and the regression would be everywhere else on the material, where nobody is looking. `[hypothesis]`
— this is a D3D spec-level inference about BC4, not an observation of RE Engine's shader.

Three things point the same way and are worth weighing: there is **no channel-selector property** for
the detail mask (where RE Engine wants a runtime channel pick it ships an explicit vec4, as with
`Rec_RTTChannelControl`), so the channel is hard-coded; the default `NullWhite` is white in all four
channels and therefore gives nothing away; and **the engine's house style for masks is four-channel**
— its own are `_MSK4` (`ImperfectDetail_MSK4.tex`, `NullGray_MSK4.tex`), while ours is `_MSK1`.

### The cheap disambiguation, and it makes a negative meaningful

Ship a **uniform mid-grey** mask first, not a black-over-hands one:

- the whole material's detail grain halves → the shader reads a channel BC4 populates, and an
  all-black region is then a trustworthy off switch;
- **nothing changes at all** → it is reading G or B, and the mask needs a four-channel format before
  any of this means anything.

This is the "never read back against the neutral value" rule the RE Engine family page already
carries, applied to a texture instead of a property: a mask that is 0 because the format zeroed it is
indistinguishable from a mask that is 0 because we drew it that way.

## ⭐ 5. A cheaper positive control than the row plans — no texture, no channel gamble

`Detail_Normal_Intensity` and `Detail_AO_Intensity` are **plain floats in the same material**.
Setting both to 0 disables the detail tile for that whole material with **no texture edit at all**
`[inferred-static 2026-09-07]`.

That is a better *first* move than the mask, and it speaks directly to the board's own stated
hesitation — *"it changes skin Tefa has already judged good and `--pores` would want raising, so not
switched on unasked."* As a **diagnostic** it is one number, instantly reversible, and it answers the
only question that matters right now: **is the band the engine's detail tile at all?**

- band gone with both intensities at 0 → the tile is the cause, and the mask work is worth doing
  properly (with the channel question settled first);
- band unchanged → the tile is not the cause, and the mask work is not worth doing at all.

The shipped `Detail_AO_Intensity` is already `0.0` in the preset, so on that material only
`Detail_Normal_Intensity` (`0.52`) would actually be doing anything — one number.

⚠️ **Do not assume `Detail_UVScale` is a multiplier rather than a divisor.** Nothing public states
the direction, and every shipped value seen is sub-1 (0.25 jacket, 0.35 on RE3RT's body, our recorded
0.5), which fits either reading. One test with the value doubled settles it.

## ✅ 6. Our format versions are confirmed, and property typing is by count

`[reported 2026-09-07]` From alphaZomega's Noesis plugin format table and NSACloud's MDF module:
the **`RERT`** generation (RE2/RE3 ray-tracing builds) is exactly `tex .34` / `mdf2.21` — plain RE2
is `.10`/`.mdf2.10`, RE3 is `.mdf2.13`. Our file suffixes are the RT ones, as assumed.

Useful `.mdf2` v21 details: it falls in the v19+ branch, which adds the GPBF buffer name/path count
and offset to each material entry. The property header is 24 bytes and **types by count, not by a
type tag** — `paramCount == 1` is a float, `== 4` a float4/colour, 4 bytes per value — which matches
the presets exactly (`Detail_UVScale` count 1, `BaseColor` count 4). Property and texture names carry
**two MurmurHash3 hashes each**, one over the UTF-16 form and one over the ASCII/UTF-8 form. For
`.tex` v34 the relevant branch is `version > 27`, which adds a swizzle block
(`swizzleHeightDepth`, `swizzleWidth`, …) to the header; no public byte-offset table names v34
specifically — the templates handle it by range.

## The concrete next steps this unlocks

In order, cheapest first — the first two are static:

1. **Read our own dumped `pl1000.mdf2.21`** for the material we actually patch and compare its
   `DetailMap`, `DetailMaskMap`, `Detail_UVScale`, `Detail_Normal_Intensity` and
   `Detail_AO_Intensity` against the table in §2. This resolves the 0.25-vs-0.5 discrepancy, tells us
   whether the detail tile is even bound on that material, and costs nothing.
2. **Rename in our notes**: `DetailNormalMap` → `DetailMap`, `DetailIntensity` →
   `Detail_Normal_Intensity` / `Detail_AO_Intensity`.
3. **Before trusting the deployed mask**, run the mid-grey control in §4 — or skip the mask entirely
   for now and run the §5 intensity control, which needs no texture and cannot be defeated by a
   channel mismatch.

## Sources and credit

All read online through each project's own web viewer or public documentation; nothing cloned or
downloaded, and no code copied.

- **NSACloud** — RE Mesh Editor and its material presets, which carry the shipped `pl1000_Jacket_Mat`
  parameter set, plus `modules/mdf/file_re_mdf.py` and `modules/tex/file_re_tex.py`.
  <https://github.com/NSACloud/RE-Mesh-Editor> (archived read-only 2026-04-01)
- **alphaZomega (alphazolam)** — the canonical MDF editing tutorial (the DetailMap / DetailMask
  explanation and the null-texture note), the RE Engine 010 templates, and the Noesis plugin's
  format table. <https://residentevilmodding.boards.net/thread/12456/template-editing-materials-tutorial-updated>
  · <https://github.com/alphazolam/RE-Engine-010-Templates>
  · <https://github.com/alphazolam/fmt_RE_MESH-Noesis-Plugin>
- **Che** and **Darkness** — original MDF structure research, credited by both MDF tooling projects.
- **Silvris / SilverEzredes** — MDF-Manager, and the point that texture bindings and properties are
  fixed by the `.mmtr` shader. <https://github.com/Silvris/MDF-Manager>
- **Havens-Night** — REEngine-Modding-Documentation wiki, texture channel-packing reference.
- **Riot1986** — the ALBM/NRMR/ATOS channel-meaning thread.
- **terenceyao**, **smkquanchi** — the RE4R MDF template thread.
- Unity and Unreal detail-map documentation, cited only as the generic analogue for UV-tiled detail.

## What came back empty, and how much of it is real

- **The `DetailMaskMap` channel is a genuine negative** `[checked 2026-09-07]`. The same corpus
  documents channel packing in exactly the form sought — the Havens-Night wiki gives `NRRC` =
  Roughness(R), Normal Y(G), Cavity(B), Normal X(A) and `ATOC` = Alpha(R), Translucency(G), AO(B),
  Cavity(A); Riot1986's thread gives ALBM/NRMR/ATOS layouts — so the *shape* of the answer exists in
  the corpus and the detail mask simply is not covered.
- **No public report of a detail-tile seam at a UV-density change**, in any RE Engine game, across
  the MDF tutorial thread, four other forum threads, the Havens-Night wiki and ~10 targeted searches.
  Those searches did surface the correct RE-Engine-specific pages, so read this as **"nobody has
  written it down publicly"**, not "it does not happen" — most live RE Engine material discussion
  happens on Discord, which is not indexed.
- **Infrastructure blocks, not negatives:** grep.app returned HTTP 429 on four attempts, so there is
  **no code-search evidence either way** on these token names across GitHub; searchcode returned a
  landing page; Nexus 403s this fetcher (Cloudflare), so no mod description page was read directly.
  A `site:github.com "DetailMaskMap"` search returned unrelated results because the engine does not
  index these files' contents — a coverage gap, since the bare token *is* indexable.
- **⚠️ Method note worth keeping.** One automated fetch **fabricated a quote**, returning the five
  parameter names it had been asked to look for as though they were the page's content; a neutral
  re-fetch of the same URL showed the page contains none of them. It was discarded. Every parameter
  name reported above comes from a preset JSON or template read with a prompt that did **not** name
  the string in advance. This is the second confirmed case of a fetched page misleading an automated
  reader in this account's research (after the tcrf.net cloaked-instructions case, 2026-08-24), and
  the defence generalises: **when asking a fetcher whether a page contains a specific string, do not
  put the string in the question.**
