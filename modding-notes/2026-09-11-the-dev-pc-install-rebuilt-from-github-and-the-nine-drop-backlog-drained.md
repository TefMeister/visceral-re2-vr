# 2026-09-11 — the dev PC install rebuilt from GitHub alone, and the nine-drop inbox backlog drained

**`/pd`, dev PC (`DESKTOP-V8GTSIR`). The game was NOT launched. Nothing here has been run.**

Tefa reinstalled RE2 and RE Village on this machine and asked whether the home build could be
reproduced from GitHub downloads alone. It largely can, it now is, and this machine is stamped for
these two projects for the first time. Separately, the nine-drop `engine-research/inbox/` backlog —
the worst on the estate, eight of them flagged STALLED by `/gs` this morning — is empty for the first
time since 2026-09-07.

---

## 1. Can the home build be reproduced from GitHub alone? Mostly yes, and here is the exact answer

**10 of the 11 text files reproduce bit-for-bit** against the `deployed/RTX` manifest, and both
plugins rebuild to the same size. Method: for every file in the home manifest, write it, hash it, and
compare — not "copy it and assume".

| what | result |
| --- | --- |
| 6 RE2 Lua + 4 of 5 Village Lua | **identical to home, hash for hash** `[verified-numerically 2026-09-11, n=10]` |
| `visceral_core.dll` | rebuilt here, **139,264 B — home's size to the byte**, both `reframework_plugin_*` exports present |
| `re_scope_vr.dll` | rebuilt here, **165,888 B — home's size to the byte**, same exports |
| 11 committed plugin assets (bracelets, neck plug, skin-detail normal) | copied verbatim — they are our own work, so they are in git |

**⭐ The one genuinely transferable lesson: line endings have to be chosen PER FILE.** Home's deployed
copies are a **mix** of CRLF and LF — some were committed before this repo's `.gitattributes` settled
and some after — so neither "copy raw" nor "normalise to LF" reproduces the set. A first attempt that
normalised everything to LF fixed four files and **broke the two that had already been correct.** The
rule that works is *"write whichever byte-form reproduces the recorded hash, and verify"*, which is
what the deployer now does. Worth knowing for any future machine bring-up on this estate.

**The DLL hashes differ and that is expected, not a defect** — `/Brepro` is reproducible per toolset
and the two machines run different MSVC builds (this one: 14.44.35207). The estate's own standard for
a cross-machine rebuild is same source, same size, same import/export set, and all three hold.
⚠️ Still only a loader-level guarantee: a signature change behind an unchanged mangled name would be
invisible to this comparison.

### ⚠️ Three things that CANNOT come from GitHub, and why each is correct

1. **`re8_scope_vr_companion.lua` is unreproducible.** Home's copy is 6,921 B; the only committed copy
   is the 7,074 B one from the 2026-08-30 subtree import. **The home PC has an edit that was never
   pushed** `[verified-numerically 2026-09-11]`. The committed copy is deployed here, so the two
   machines are knowingly different on this one file. **This is the only real gap of its kind and it
   wants fixing at the home end, not this one.**
2. **`re_scope_vr_settings.txt` is partial** — 59 B here against home's 234. Only four of its values
   are written down anywhere (`tone_mode=1`, `exposure_gt=0.134`, `atmo_on=0`, `mir_cy=0.60`), and
   `load_settings()` overrides only keys it finds, so the rest fall back to shipped defaults. The
   missing numbers were **not invented**; a fabricated settings file that reads as "identical" would be
   worse than a short honest one.
3. **The game-derived textures are absent by design.** `pl1000_Jacket_ALBM/NRMR/MSK1`, `pl1000.mdf2`,
   `pl1000_body.rtex.5` and `visceral_bracelets.mdf2.21` are generated **from Claire's own shipped
   assets** and are correctly excluded from git (one of them by an explicit `.gitignore` line saying
   so). They must be regenerated on this machine from its own install — the tooling for that is
   committed (`hd_hands_paint.py`, `body_tex_build.py`, `skin_detail_build.py`, `rtex_resize.py`,
   `mdf_build_bracelets.py`) and Blender 5.1 is installed here.

### 🚨 And the install does not FUNCTION yet — REFramework is not here

Nothing deployed above will load: `dinput8.dll` (praydog's REFramework) is absent from both game
folders, and it is third-party, so it is not in our repos and not ours to vendor. **This also blocks
dev-PC static work that needs a type-DB read** — which is exactly what Village's ⭐⭐ "read Capcom's own
VR scope" row needs. Recorded as an open action for Tefa rather than downloaded unasked.

---

## 2. The nine-drop backlog, drained — what actually changed in the dossier

Read in full first, `grep "^Supersedes:"` run before anything was folded, and the rtex correction
applied **together with** the drop it corrects so the withdrawn field names never entered the dossier.

**New `### 7c` — ⚠️ two parameter names we were using do not exist.** The shipped material exposes
`DetailMap`, `DetailMaskMap`, `Detail_UVScale`, `Detail_Normal_Intensity`, `Detail_AO_Intensity`
`[reported 2026-09-07]`. **There is no `DetailNormalMap` and no `DetailIntensity`.**
- **⭐ And that hands us a one-number positive control that replaces the whole mask gamble:**
  `Detail_Normal_Intensity = 0` disables the detail tile for that material with **no texture edit and
  no channel gamble** `[inferred-static 2026-09-07]`. Band gone ⇒ the tile is the cause and the mask
  work is worth doing properly; band unchanged ⇒ the tile is not the cause and the mask work is not
  worth doing at all. That separates the diagnostic from the fix, which is what the board was
  hesitating over.
- **🚨 The mask already deployed at home may be inert.** It is 2048 **BC4**, and BC4_UNORM sampling
  returns `(R,0,0,1)` — so if the shader reads `.g` or `.b` it reads **0 across the whole material**.
  **That failure looks exactly like success**: palms smoother as intended, a regression everywhere
  nobody is looking `[hypothesis 2026-09-07]`.

**New `### 5b` — both ⭐⭐ headset defects are documented REFramework traps.** The grip write hit the
value-type **copy** trap (*"just a local copy … does not change anything in-game"*); the fix shape is
cached `via.Joint:set_Position`/`set_Rotation` **method** calls, written **after
`LateUpdateBehavior`** — praydog's own RE8 script writes the hand pose **three times per frame** at
progressively later entries because the engine's motion/IK pass overwrites earlier writes. The
constant camera read has a ranked cause list and **a one-line discriminator**: log the camera object's
`get_address()` beside the position — constant across a scene change ⇒ stale handle; address moving
while position does not ⇒ wrong node (the live pose is on **joint 0**).
⚠️ Three negatives recorded against claims this dossier already held: **nothing public demonstrates**
the `via.motion.*` joint-pose names, **`write_valuetype` is not in the API at all**, and **no public
mod disables IK before writing a joint**.

**`§8f` narrowed, and `### 8g` added.** Motions are addressed by `(bankID, motionID)` — **a numeric
pair, not a name hash** — so item 22's outcome (a) is now the *predicted* branch and "frozen legs" the
surprising one. `motSize` is **vestigial in v524**, so that clause is dropped from the row rather than
answered, and **our splice tool was already correct either way**. `motNumber` is a **u16 at +0x08**
with `Switch` a u16 at +0x0A — a splice changing a motion number writes a u16, not a u32. The
`.rtex.5` descriptor is decoded and a **larger authored size is proved live on the sibling project**
`[verified-live 2026-09-06, on re-village-scope-vr]` — but its `0x18`–`0x24` field names are
**a guess that fits, not a measurement** `[hypothesis 2026-09-07]`.

### Seven off-vocabulary tags fixed, each by what was actually done

`[verified …]` is not one of the eight vocabulary names: it reads as a strong claim to a human and
counts as **nothing** to every tool. `/gs` has raised it three sweeps running. All seven live uses are
now retagged individually — `[measured]` where something was read off disk or judged on renders,
`[verified-numerically]` where numbers were reproduced, `[inferred-static]` where source was read —
and any precision that is not a date or an `n=` has been moved **outside** the brackets, which is what
the rule requires. **Zero bare `[verified` remain in this project.**

### ✅ Two drops needed no action — verified, not assumed

Both were tooling/process asks rather than engine knowledge, and both had **already been fixed** by a
later session. Checked rather than taken on trust, because deleting a drop whose ask is outstanding is
the precise half-drain failure one of those two drops exists to warn about:
- `gate-scan.sh --mine` hiding all 12 non-VR rows on the home PC: `owner_of()` is now CR-tolerant
  (`prefix="${prefix%$'\r'}"`, with a comment citing 2026-09-07) **and** `*.tsv` is pinned to LF in
  `.gitattributes`. **Re-tested here: `visceral-re2-vr` and `re-village-scope-vr` both resolve to
  `HOME`, `mad-max-vr` to `DEV`** `[verified-numerically 2026-09-11]`.
- "a drop file carries one ask" is recorded in `CONVENTIONS.md`.

---

## What is NOT established

- **Nothing deployed here has run**, on either game. No frame has been rendered by either plugin on
  this machine, and it cannot be until REFramework is present.
- **The BC4-mask-reads-zero worry is a D3D inference, not an observation of RE Engine's shader.** It
  cannot be settled on this machine either way: the mask is game-derived and lives only on the home PC.
- **The `Detail_UVScale` 0.25-vs-0.5 discrepancy and whether `DetailMap` is null** need our own dumped
  `pl1000.mdf2.21` read — also home-only. A third party's preset is a strong lead and a poor authority.
- **Whether the rebuilt DLLs behave identically to home's** — same size and same exports is a loader
  guarantee, not a rendering one.
