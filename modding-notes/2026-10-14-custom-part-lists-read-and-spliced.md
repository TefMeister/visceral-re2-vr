# The custom-part animation lists: format read, Leon's six stock-Matilda lists given the Claire treatment (2026-10-14, home PC, Fable, desk)

**Build:** b175 = b174 + six files under `natives/stm/sectionroot/animation/player/pl00/list/hdg/` (`hdg_hold_cpA_01`,
`hdg_hold_cpAC_01`, each also `stLIGHT_` and `stWATER_`), installed 23:10, UNWORN. The empty-list stop-gap copies they
replace are in `D:\Visceral build versions\_backup-2026-10-14-before-b175-matilda-lists\`. Game not launched.

Board row (2026-10-04): a handgun with a custom part aims with its OWN list, which overrides our spliced
`base_hdg_hold` slot by slot, so the stocked Matilda brought the hunched aim-walk back. Leon's stop-gap since b068
was the game's EMPTY list in those six paths (the stock lost its own shooting clips with it); Claire's and Ada's
lists got a different treatment on 2026-10-05 (the walk slots emptied by hand, shots kept) -- and Leon's Matilda
files were left "untouched" then.

## The override-list format `[measured 2026-10-14 on the pak originals: pl00 / pl10 / pl20 x cpA / cpAC x plain / stLIGHT / stWATER]`

Pulled with `pak_pull.py` into `extracted/cp-originals-2026-10-14/` (local only, never committed). Against the base list:

| | base list (`base_hdg_hold`) | override list (`hdg_hold_cpA_01` ...) |
| --- | --- | --- |
| header | u64 at 0x28 = 0 | u64 at 0x28 = 0x54: a second UTF-16 string, `SectionRoot/Animation/Player/pl00/list/HDG/BASE_HDG_HOLD.motlist` = the list it overrides; the pointer table follows both strings (0xe0; 0xf0 for the longer `stLIGHT_` / `stWATER_` names) |
| pointer table | 30 slots, none empty | 30 slots, 10-11 EMPTY (0): slots 0-5 and 25-29 (and 27 on cpA) are left to the base list |
| entries | v492 mot blobs | the same, movable whole |
| collection | 72 bytes x 30 slots | the same 72 x 30 rows, PLUS a 5624-byte tail: three "CLIP+" blocks of 1880 bytes, one per SMG1_Hold_Shoot slot; the row of such a slot carries the block's ABSOLUTE file offset at +0 and a flag at +0x17, and the blocks carry absolute offsets into the tail themselves (45 of them) |

What the stock lists hold (Leon; Claire's are the same names with longer strafes): `HG2_StrafeL_F/L/B/R`, `HG2_StrafeR_L/R`
(slots 6-11), `SMG1_Hold_Start_L0/L90/L180/R0/R90/R180` (12-17), `HG2_Hold_Idle_Loop` (18 and 20, one shared entry),
`HG2_Wheel_L180/R180` (19, 21), `SMG1_Hold_Shoot` x3 (22-24); cpAC adds `HG_Hold_Reload_quick` (27), the stLIGHT ones
`HGL_Hold_Reload`. The shooting clips of Claire's and Ada's lists are Leon's (`pl00_1100_SMG1_Hold_Shoot`).

## The tool: `dev-archive/tools/re-engine/motlist_splice_cp.py`

Two treatments, each proving its own output:

1. **`--null` (installed):** the 14 strafe / idle / raise slots are emptied in the pointer table; every other byte is
   the original. The game then takes those slots from our spliced base list, the way it already takes slots 0-5 and
   25-29. Proof that this is the established treatment and not a new idea: run on Claire's six pak originals, the tool
   reproduces her six INSTALLED 2026-10-05 files byte for byte (`cmp`, n=6) `[verified-numerically 2026-10-14]`.
2. **full splice (built, NOT installed):** the donor's clips (the character's installed `base_hdg_hold`: the KFF walks,
   the idle, the 20/22/24-frame raise) are copied into those slots, the file grows, and the collection rows + tail are
   moved with every 8-aligned u64 that pointed inside the old tail shifted by the same delta (48 offsets for Leon's
   cpA). `verify()` moves them back and demands the original bytes. Passes; whether the game accepts the relocated
   tail is `[hypothesis]` until run. Output: `extracted/cp-full-splice-2026-10-14/pl00/hdg_hold_cpA_01.motlist.524`.
   It is the fallback if the game turns out NOT to fall back per slot for these particular slots.

## What Tefa's wear tells

- Stocked Matilda, aiming and walking: body straight, no shake, same as the plain Matilda -> the per-slot fallback
  works for Leon as it did for Claire; row closed.
- Each shot with the stock: the two-handed SMG1 recoil (it was the one-handed HG recoil under the empty-list stop-gap).
- Hunched again with the stock: the fallback is not per slot for strafes -> try the full-splice cpA (one file, Matilda
  with stock, plain state) and look again.

## Housekeeping

- Leon's `stLIGHT_` / `stWATER_` lists were NOT in the game folder before tonight (only cpA_01 and cpAC_01 were, as
  the empty list); the pak originals applied in those states. Now all six are the emptied-slot versions.
- Ada (pl20) has no spliced base list, so her cp lists are left as the 2026-10-05 job set them.
