# §8f: two of the three "open until one flat run" questions are answerable now

**From:** `/gr` (estate sweep, 2026-09-07) · **For:** the modding lane, to fold into
`ENGINE-DOSSIER.md` **§8f** (and the board's ⭐ `[FLAT]` item-22 row)

Supersedes: ENGINE-DOSSIER.md §8f — the "Open until one flat run" bullet, specifically its first two
clauses ("lookup by number vs name hash" and "whether first-entry `motSize` must be 0")

**Full write-up:** [`external-research/topics/2026-09-07-motlist-motions-are-addressed-by-number-not-by-name-hash.md`](../../external-research/topics/2026-09-07-motlist-motions-are-addressed-by-number-not-by-name-hash.md)

## The dead end this is aimed at

§8f, verbatim:

> "**Open until one flat run:** lookup by number vs name hash; whether first-entry `motSize` must be
> 0; whether the aim-walk blend drives phase by frame or normalised time (walk loops are 3–6×
> longer)."

The third stays open. The first two do not need the launch.

## 1. Lookup is by NUMBER — `(bankID, motionID)`. Not by motion-name hash

Four independent public sources agree `[reported 2026-09-07]`: animations are addressed by a numeric
`(bankID, motionID)` pair; the motion's UTF-16 name is carried in the file but is not a lookup key;
and there is **no motion-name hash field** in the documented structure. The murmur3 hashing present
in these files is for **bone** names, and the one hash-keyed lookup that exists —
`findMotionBankByNameHash` — hashes the **motlist/bank** name, i.e. is file-level.

Sources: alphaZomega's MMDK and his 010 Editor motlist template (whose only murmur3 fields are in the
bone and bone-clip headers); the RE2R Custom Animation Framework's `actor_motion_systems.md` and
`motlist_format_guide.md`. Full URLs in the topic file.

**So item 22's outcome (a) is the predicted one**, and "frozen / T-posed legs → the game keys by name
hash" becomes the *surprising* branch. ⚠️ Not a proof: no public source names every dword of the
72-byte collection entry, so a per-slot hash in an unnamed field is not formally excluded
`[hypothesis]` — though §8f's own byte-identical comparison argues hard against it.

**Bonus corroboration for §8f:** the public template independently confirms the **72-byte stride for
version ≥ 486** and one collection entry per slot, and it **names the second half of the u32 at +8**
that §8f records as varying: `motNumber` is a **u16 at +0x08** and `Switch` a **u16 at +0x0A**. Worth
naming in §8f — a future splice that changes a motion number is writing a u16, not a u32.

## 2. `motSize` — the public record and §8f disagree, and that disagreement resolves the question

§8f `[measured 2026-09-06, n=5]`: `motSize` holds a real value (blob minus padding) and is 0 on every
file's **first** entry.

Two public writers — alphaZomega's Motlist-Tool and CAF's mot writer — emit `motSize` as **0 for
every entry** in this generation (524 normalises to 486), with no first-entry special case; CAF's
spec says the field is only populated in the older RE2 **v65**.

Both are true at once, and the reading is that **the engine does not read `motSize` in v524** — it is
vestigial, tolerated at the genuine value Capcom writes and at zero `[hypothesis 2026-09-07]`.
Deliberately not stronger: "tools that zero it are in general use and their mods work" is inference
from their public use, not a controlled test.

**Either way our tool is already correct**, because it preserves each blob's own value — right if the
field is ignored, and right if it is read. **Suggested change:** drop the `motSize` clause from the
row's outcome (c) and from §8f's open list, leaving outcome (c) as "loose file not taken".

## 3. Do NOT re-raise the collection block's 15 unnamed dwords — §8f already disproved it

The public template shows 15 dwords of float/uint payload per collection entry after `motNumber`,
which invites the worry that per-slot frame or blend data is being kept verbatim while entry lengths
change 3–6×. **§8f rules this out directly**: the block is byte-identical between Claire's original
and the Jill replacement whose entries all differ in size, so it holds no per-blob metadata
`[measured 2026-09-06]`. Recorded here so the next reader of the template does not spend time on it.

## 4. One field §8f does not list, worth reading before the launch

The mot **entry header** carries a **`blending`** field the public template annotates as *"set to 0
to enable repeating"* `[inferred-static 2026-09-07]`. §8f's entry-header field list (name offset u64
+0x58, frames f32 +0x60, bones/clips u16 +0x70, fps u16 +0x78, `motSize` u32 +0x0C) does not include
it. For twelve spliced-in **loop** motions, a non-zero `blending` is a candidate cause of a walk that
plays once and holds — a static read, reachable before the third open question's blend-node theory.

## 5. A trap we are already clear of, worth recording anyway

The RE2R/RE3R ray-tracing patch moved the loose-file root from `natives/x64` to **`natives/stm`** in
**the same patch that bumped motlist to 524** `[reported 2026-09-07, two sources]`. A `.motlist.524`
under the old root is silently ignored and presents exactly as outcome (c). The row and the tool both
already use `natives/STM`, so nothing to fix — but the version bump and the root move being one event
is worth a line in §8f, so a future vanilla result is not misread.

## Also worth knowing

**No public write-up of a v524 motlist splice exists** `[checked 2026-09-07]`, from searches that
returned correct on-topic results for adjacent queries. Item 22 is novel work, not a re-tread.

## Credit

alphaZomega (alphazolam) — RE-Engine-010-Templates, Motlist-Tool, MMDK, EMV-Engine.
godlock2000-eng — RE2R Custom Animation Framework (NonRTX) documentation.
PredatorCZ (Lukas Cone) — RevilLib (version list only). praydog — REFramework / RE-BHVT-Editor.
TrikzMe / devilsnake88 — RE-Engine-Hash-tool. Havens-Night — REEngine-Modding-Documentation.
The residentevilmodding community — the ray-tracing-patch format-changes thread.
All read online through each project's own web viewer; nothing cloned, downloaded or copied.
Already added to `external-research/CREDITS.md`.
