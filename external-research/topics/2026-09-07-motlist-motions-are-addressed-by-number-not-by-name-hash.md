# `.motlist` motions are addressed by NUMBER, not by name hash — item 22's outcome (a) is the predicted one

**Status:** 🆕 new · **Priority:** high — it answers the ⭐ `[FLAT]` item-22 splice row's central
either/or from public sources before the launch that would decide it, **closes a second of §8f's
three open questions**, names one unrecorded field worth reading first, and confirms we are already
on the right side of a silent-failure trap. It also retires a worry the public template would
otherwise raise, because our own §8f measurement already disproved it.

## Why this was looked up

The board's ⭐ `[FLAT]` **item 22** row ships a built and numerically-verified splice tool
(`dev-archive/tools/re-engine/motlist_splice.py`, `[verified-numerically 2026-09-06]`) and hands the
launch a three-way reading table:

> "Legs walk normally = item 22 works in principle; **frozen / T-posed legs = the game keys motions
> by name hash, not by the collection block's number** (rename-in-blob is the next script); vanilla
> aim-walk or a load crash = loose file not taken, **or the first entry's `motSize` must be 0**
> (one-line flag to add); walk visibly slow = the blend node drives phase by normalised time."

The index-versus-hash question is the one the whole row turns on, and it is a *format* question —
exactly the kind the public RE Engine modding community answers. So it was asked.

## ✅ The answer: numeric addressing. `(bankID, motionID)`

Multi-source, and consistent across four independent tools `[reported 2026-09-07]`:

- **alphaZomega's MMDK** (RE Engine moveset kit) documents a `MotionKey` as activating an animation
  (**MotionID**) from a motlist file (**MotionType**), and registers a dynamic motion bank by
  binding a motlist *path* to a **numeric bank id** — its own example notes that reusing a
  character's original numeric bankID makes their original MotionKeys work without edit.
  <https://github.com/alphazolam/MMDK>
- **The RE2R Custom Animation Framework's** research documentation states the engine API as
  `changeMotion(bankID, motionID, …)` and, in its own words, that every animation is addressed by a
  `(bankID, motionID)` pair — the bank identifying the `.motlist`, the motion identifying which
  animation within it. The only hash-keyed lookup it documents is
  `findMotionBankByNameHash(motlistNameHash)`, which hashes the **motlist/bank name** — a *file*-level
  hash, never a per-motion one.
  <https://github.com/godlock2000-eng/ResidentEvil2_CustomAnimationFramework_NonRTX/blob/main/docs/actor_motion_systems.md>
- **The same project's `motlist` format guide** says motions are resolved by **index position in the
  entry pointer table**, that each entry carries its name as a plain UTF-16 string at `namesOffs`,
  and that there is **no motion-name hash field**; the MurmurHash3-32 (seed `0xFFFFFFFF`, UTF-16LE)
  hashing in these files applies to **bone** names.
  <https://github.com/godlock2000-eng/ResidentEvil2_CustomAnimationFramework_NonRTX/blob/main/docs/motlist_format_guide.md>
- **alphaZomega's 010 Editor template** corroborates this independently and structurally: the only
  murmur3 field in the entire motlist template is the **bone** hash in the bone header and bone-clip
  header. No hash field exists in the motion header or in the collection entry.
  `[inferred-static 2026-09-07]` <https://github.com/alphazolam/RE-Engine-010-Templates>

**So outcome (a) is what the public record predicts**, and the "frozen / T-posed legs → rename-in-blob"
branch is the *unlikely* one rather than a coin flip. That does not make the launch unnecessary — it
makes a frozen-legs result *surprising*, and therefore more informative if it happens.

⚠️ **One real gap, stated plainly.** No public source names every dword of the 72-byte collection
entry. A per-slot hash hiding in an unnamed field **cannot be ruled out from the public record
alone** `[hypothesis]` — though our own §8f measurement argues strongly against it (below). The
multi-source conclusion above is about the *documented* structure.

There is also a harmless ambiguity that does not affect us: sources do not cleanly separate whether
the runtime `motionID` is the entry's **array position** or its **`motNumber` value**. Because the
splice keeps the collection block verbatim and reorders nothing, **both models resolve to the same
pointer slot**, so the distinction cannot change this test's outcome.

### ✏️ One field-name refinement to §8f, and a confirmation

Dossier §8f records the collection block as *"72 bytes per slot; only **u32** at +8 varies and it is
the motion NUMBER"* `[measured 2026-09-06, n=5 files]`. The public 010 template splits that same
dword into **`motNumber` (u16 at +0x08)** and **`Switch` (u16 at +0x0A)** `[inferred-static 2026-09-07]`.

The two agree if `Switch` is zero in every file we measured — which is what "only the u32 at +8
varies, and its values are 0x6e and 0xa0" implies. **So §8f can name the second half of that dword
rather than leaving it inside an anonymous u32**, and a future splice that ever needs to *change* a
motion number now knows it is writing a u16, not a u32.

The template also **independently corroborates §8f's strongest structural claim**: it iterates the
collection block exactly `numOffs` times — one entry per slot — and confirms the 72-byte stride for
version ≥ 486 (24 bytes below that, 12 for v60). Our n=5 measurement and the public template were
arrived at separately and match.

## ⭐ `motSize`: the public record and our own measurement DISAGREE — and the disagreement is the answer

This is the most useful thing the search returned, and it needs stating carefully because **our own
measurement is the stronger evidence and it is not what the public tools do.**

**What §8f measured** `[measured 2026-09-06, n=5 files]`: `motSize` at `+0x0C` holds a **real value**
— the blob minus padding — and is **0 on every file's first entry**, the modder's RT specimen
included. Hence the row's open question: *"whether first-entry `motSize` must be 0"*.

**What the public writers do** `[reported 2026-09-07, two independent tools]`: they write `motSize`
as **0 unconditionally, for every entry**, in this generation.

- alphaZomega's **Motlist-Tool** MaxScript writes literal `0` at `+0x0C` whenever the version is 486
  or 99 — and **524 normalises to 486** — inside the per-motion export loop, with no first-entry
  special case. <https://github.com/alphazolam/Motlist-Tool>
- The CAF **mot writer** packs `0` at `+0x0C` for every entry with an inline note that it must be 0
  in RE2 motlists; its spec adds that `motSize` is only populated in the older RE2 **v65** (where it
  holds the total entry size) and is 0 from RE3 v78 onward.

**Both can be true, and together they answer the open question.** Shipped Capcom files carry a real
size (what we measured); public tools emit zero everywhere and **their outputs are used in shipped
mods that work**. The straightforward reading is that **the engine does not read `motSize` in v524**
— it is vestigial, tolerated at any value including the genuine one Capcom writes.

`[hypothesis 2026-09-07]`, and deliberately not stronger: "tools that zero it produce working mods"
is inferred from those tools being in general public use, not from a controlled test, and this
lane has not verified any specific mod built with them.

**What that means for item 22, either way:** our splice preserves each blob's own `motSize` verbatim.
If the engine ignores the field, that is safe. If it does read it, preserving the source value is
*also* the correct thing to do — a blob moved whole keeps the size that describes it. **There is no
version of this where the current tool is wrong**, so the row's "one-line flag to add" is not a
pending fix, and outcome (c) can drop its `motSize` clause and keep only "loose file not taken".

The cheap static check that would settle it: assert `motSize` on **all 30** output entries equals the
value in the source blob each came from, and separately record whether the *first* output entry's is
0 — since the output's first slot may now be a different blob than the input's was.

## The 15-dword collection payload is NOT a risk here — §8f already disproved it

The public template shows 15 further dwords of mixed float/uint payload in each 72-byte collection
entry, which raises an obvious worry: if any of it encodes frame count or blend data per slot, then
keeping the block verbatim while swapping 66–70-frame entries for **217–391-frame** loops could clip
or mistime playback.

**Our own record already rules that out**, and more directly than any public source could. §8f:
the collection block is *"byte-identical between Claire's original and the Jill replacement whose
entries all differ in size → it holds no offsets or sizes"* `[measured 2026-09-06]`. Two banks whose
every entry differs in length share the same block byte for byte, so the payload is not per-blob
metadata. **No change to the row's expectations.** Noted here only so the next reader who finds the
template's 15 unnamed dwords does not re-raise it.

## One genuinely new field to read before the launch

The mot **entry header** — not the collection block — carries a `blending` field that the public
template annotates as *"set to 0 to enable repeating"* `[inferred-static 2026-09-07]`. §8f's field
list for the v492/524 entry header (name offset u64 `+0x58`, frames f32 `+0x60`, bones/clips u16
`+0x70`, fps u16 `+0x78`, `motSize` u32 `+0x0C`) **does not include it**.

For twelve spliced-in **loop** motions that is worth a static read: if `blending` is non-zero on the
`KFF_GazingWalk_*` blobs, it is a candidate cause of a walk that plays once and holds rather than
repeating — reachable before the blend-node theory the row currently reaches for, and testable
without a launch.

## ✅ And a trap we are already on the right side of

The ray-tracing patch for RE2R/RE3R moved the loose-file root from **`natives/x64`** to
**`natives/stm`** (the Steam build), in the same patch that bumped motlist to **524**
`[reported 2026-09-07, two independent sources]`. A `.motlist.524` placed under the old root is
**silently ignored** — which presents exactly as the row's outcome (c) "vanilla aim-walk", for a
reason having nothing to do with the splice.

**We are fine:** the row already specifies `<RE2>\natives\STM\sectionroot\animation\player\pl10\list\hdg\`
and the tool's output tree is `splice-out\natives\stm\…`. Recorded so that a future session reading
a vanilla result does not spend a launch rediscovering the trap — and so the version bump and the
root move are known to be *the same event*.

## No one has published a v524 motlist splice

Searches for version-524-specific animation-replacement write-ups returned only the community
format-changes thread `[checked 2026-09-07]`. Adjacent queries in the same searches returned correct
on-topic results, so the searches were capable of a positive — this reads as **no public write-up of
a v524 motlist splice exists**, which makes item 22 genuinely novel rather than a re-tread. Per
research rule 7 that is "none found", not proof none exists.

## The concrete next steps this unlocks

All three are static, and all are cheaper than the launch they precede:

1. **Drop the `motSize` clause from outcome (c).** Our splice preserves each blob's own value, which
   is correct whether or not the engine reads the field. Optionally assert that all 30 output entries
   match their source values.
2. **Read `blending` off each spliced-in `KFF_GazingWalk_*` blob** and record it beside the
   217–391-frame figures already on the board. It is not in §8f's field list, and the public
   annotation ties it to whether a motion repeats.
3. **Name `Switch` (u16 at +0x0A)** in §8f's collection-entry description, splitting the anonymous
   u32 at +8 into `motNumber` + `Switch`.

Then the launch runs with a sharper table: legs walk = confirmed, as the public record predicts;
**frozen legs = surprising**, and the first suspect is an unnamed collection-entry field rather than
a motion-name hash, which the documented structure does not have; a non-repeating walk = read
`blending` before reaching for the blend node.

## Sources and credit

All read online through each project's own web viewer; nothing was cloned or downloaded, and no
code was copied.

- **alphaZomega (alphazolam)** — RE Engine 010 Editor templates
  (<https://github.com/alphazolam/RE-Engine-010-Templates>), Motlist-Tool
  (<https://github.com/alphazolam/Motlist-Tool>), MMDK (<https://github.com/alphazolam/MMDK>),
  EMV-Engine (<https://github.com/alphazolam/EMV-Engine>).
- **godlock2000-eng** — RE2R Custom Animation Framework (NonRTX), `docs/actor_motion_systems.md` and
  `docs/motlist_format_guide.md`
  (<https://github.com/godlock2000-eng/ResidentEvil2_CustomAnimationFramework_NonRTX>).
- **PredatorCZ (Lukas Cone)** — RevilLib (<https://github.com/PredatorCZ/RevilLib>), cited for its
  supported-version list only.
- **praydog** — REFramework and RE-BHVT-Editor.
- **The residentevilmodding community** — the ray-tracing-patch format-changes thread.
- **TrikzMe / devilsnake88** — RE-Engine-Hash-tool. **Havens-Night** — REEngine-Modding-Documentation.

## What came back empty, and why the emptiness is believable

- **Havens-Night/REEngine-Modding-Documentation** has no animation section. The fetch returned the
  repo's real structure and README contents, so it was capable of a positive; animation simply is
  not covered *in the repo*. Its wiki pages were not enumerable, so this is "not in the repo", not
  "absent from the wiki".
- **praydog's BHVT-editor docs** returned real content with no motion-bank material — a genuine
  negative for that page.
- **EMV-Engine's Lua source** 404'd on a guessed path and the follow-up listing also 404'd. That is a
  **failed lookup, not a negative**: the `changeMotion` signature therefore rests on the CAF
  document alone (single source), while the numeric-addressing conclusion itself is multi-source and
  does not depend on it.
