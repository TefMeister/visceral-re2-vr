# Leon (pl00) hold-bank splice plan — the item-22 recipe carried over from Claire (reader, 2026-09-24)

Written by the read-only reader beside the live `/lm`. Nothing was installed, deployed or launched.
All eight originals were pulled with `pak_pull.py` into `%TEMP%\visceral-reader-leon\` (scratch, not the
repo). Leon's two builds were run with `--dry-run` only. Claire's two builds WERE written — into scratch
only — because that is the one way to prove the command lines below are the real recipe: both hashes
match the files installed in the game folder today (§5).

**Short version:** Leon's hold bank has the **same 30 slot numbers and the same 30 motion-name suffixes**
as Claire's, his move lists carry the same `OFF_` / `OLF_` idle and walk names, and his
`hdg_hold_stlight_01` has the same 29 empty placeholders plus the reload. So Claire's v5 and light-v1
command lines work unchanged apart from `--character pl00` and the four file paths. Both dry runs pass
verify `[measured 2026-09-24]`. What differs is only clip lengths and bone/clip counts (§7).

---

## 1. `pl00/list/hdg/base_hdg_hold.motlist.524` — the 30 slots `[measured 2026-09-24]`

494,416 bytes, 30 slots, 28 entries (two pairs share one entry, as Claire's does). Read with the splice
tool's own parser (`motlist_peek.py` prints blank names on these v492 entries; it reads v85 offsets).

| Slot | Number | Motion (pl00_…) | Frames | Bones/clips |
|---:|---|---|---:|---|
| 0 | 0x06e | 0110_HG_Interpolation_F_Loop | 70 | 136/136 |
| 1 | 0x06f | 0111_HG_Interpolation_L_Loop | 72 | 136/136 |
| 2 | 0x070 | 0112_HG_Interpolation_R_Loop | 71 | 136/136 |
| 3 | 0x071 | 0113_HG_Interpolation_Back_L_Loop | 71 | 136/136 |
| 4 | 0x072 | 0114_HG_Interpolation_Back_B_Loop | 72 | 136/136 |
| 5 | 0x073 | 0115_HG_Interpolation_Back_R_Loop | 70 | 136/136 |
| 6 | 0x078 | 0120_HG_StrafeL_F | 78 | 120/120 |
| 7 | 0x07a | 0122_HG_StrafeL_L | 77 | 120/120 |
| 8 | 0x07c | 0124_HG_StrafeL_B | 77 | 120/120 |
| 9 | 0x07e | 0126_HG_StrafeL_R | 77 | 120/120 |
| 10 | 0x084 | 0132_HG_StrafeR_L | 77 | 120/120 |
| 11 | 0x088 | 0136_HG_StrafeR_R | 77 | 120/120 |
| 12 | 0x08c | 0140_HG_Hold_Start_L0 | 20 | 120/120 |
| 13 | 0x08d | 0141_HG_Hold_Start_L90 | 22 | 120/120 |
| 14 | 0x08f | 0143_HG_Hold_Start_L180 | 24 | 120/120 |
| 15 | 0x096 | 0150_HG_Hold_Start_R0 | 20 | 120/120 |
| 16 | 0x097 | 0151_HG_Hold_Start_R90 | 22 | 120/120 |
| 17 | 0x099 | 0153_HG_Hold_Start_R180 | 24 | 120/120 |
| 18 | 0x0a0 | 0160_HG_Hold_Idle_Loop (shares entry with 20) | 432 | 120/64 |
| 19 | 0x0a5 | 0165_HG_Wheel_L180 | 77 | 136/136 |
| 20 | 0x0a6 | 0160_HG_Hold_Idle_Loop (shares entry with 18) | 432 | 120/64 |
| 21 | 0x0a7 | 0167_HG_Wheel_R180 | 77 | 136/136 |
| 22 | 0x44c | 1100_HG_Hold_Shoot | 50 | 120/64 |
| 23 | 0x44d | 1101_HG_Hold_Shoot | 50 | 120/120 |
| 24 | 0x44e | 1102_HG_Hold_Shoot | 50 | 120/120 |
| 25 | 0x460 | 1120_HG_Hold_Shoot_NoAmmo (shares with 26) | 15 | 120/64 |
| 26 | 0x46a | 1120_HG_Hold_Shoot_NoAmmo (shares with 25) | 15 | 120/64 |
| 27 | 0x4b0 | 1200_HG_Hold_Reload | 104 | 120/50 |
| 28 | 0x515 | 1301_HG_HolsterToMove | 30 | 173/47 |
| 29 | 0x51f | 1311_HG_MoveToHolster | 30 | 120/47 |

The raise slots are 20 / 22 / 24 frames on Leon exactly as on Claire, so the `@20/@22/@24` copies
translate one to one.

## 2. Ordinary gun-drawn idle/walk in `pl00/list/hdg/base_hdg_move.motlist.524` (the `OFF_` set) `[measured 2026-09-24]`

1,969,200 bytes, 46 slots, 42 entries (Claire: 45 / 41). The nine `OFF_Gazing*` names, all present:
`pl00_0160_OFF_Gazing_Idle_F_Loop` (432 frames, 120 bones/120 clips), `0190_OFF_GazingWalk_F_Loop` (374),
`0191_OFF_GazingWalk_L_Loop` (67), `0192_OFF_GazingWalk_End_LR`, `0193_OFF_GazingWalk_End_RL`,
`0194_OFF_GazingWalk_R_Loop` (64), `0196_OFF_GazingWalk_Back_L_Loop` (65), `0197_OFF_GazingWalk_Back_B_Loop`
(68), `0198_OFF_GazingWalk_Back_R_Loop` (63); the walk loops are 174 bones / 168 clips. Same names as
Claire's list (`pl10_` prefix), so the tool's built-in map needs no change.

## 3. Light-on idle/walk in `pl00/list/cmn/cmn_move_stlight.motlist.524` (the `OLF_` set) `[measured 2026-09-24]`

1,450,064 bytes, 66 slots, 44 entries (Claire: 66 / 44). The nine `OLF_Gazing*` names, all present:
`pl00_0160_OLF_Gazing_Idle_F_Loop` (2,560 frames), `0190_OLF_GazingWalk_F_Loop` (495),
`0191_OLF_GazingWalk_L_Loop` (67), `0192`/`0193` End clips, `0194_OLF_GazingWalk_R_Loop` (64),
`0196_OLF_GazingWalk_Back_L_Loop` (65), `0197_OLF_GazingWalk_Back_B_Loop` (481),
`0198_OLF_GazingWalk_Back_R_Loop` (63); walks 173 bones / 169 clips. Same names as Claire's.

## 4. `pl00/list/hdg/hdg_hold_stlight_01.motlist.524` — slot table `[measured 2026-09-24]`

41,808 bytes, 30 slots, **1 entry**: slot 27 (0x4b0) = `pl00_1200_HGL_Hold_Reload` (147 frames,
120 bones / 50 clips). The other 29 slots are empty placeholders (pointer 0) with **exactly the numbers of
§1** in the same order: 0x06e–0x073, 0x078/0x07a/0x07c/0x07e/0x084/0x088, 0x08c/0x08d/0x08f/0x096/0x097/0x099,
0x0a0/0x0a5/0x0a6/0x0a7, 0x44c/0x44d/0x44e/0x460/0x46a, 0x515/0x51f. Identical layout to Claire's
(47,600 bytes; her reload is 172 bones).

## 5. The Claire recipe, proven against the installed files `[measured 2026-09-24]`

The notes describe v5 and light-v1 in words but do not quote the command lines. Reconstructed from
`motlist_splice.py`'s `DEFAULT_MAP` (12 locomotion slots → `OFF_GazingWalk_*`, both `Hold_Idle_Loop` slots
→ `OFF_Gazing_Idle_F_Loop`) plus six `--map` overrides for the raise slots, and for the light list twenty
`--fill` entries. Built into scratch and hashed:

- Claire v5 base → `86760505090c…15b63`, **equal to the installed**
  `natives\STM\SectionRoot\Animation\player\pl10\list\hdg\base_hdg_hold.motlist.524` (1,218,544 bytes).
- Claire light v1 → `be462fc7a9fa…8e43f`, **equal to the installed**
  `…\pl10\list\hdg\hdg_hold_stlight_01.motlist.524` (674,704 bytes).

So the lines in §6 are the real recipe, not a guess.

## 6. The Leon command lines (copy-paste; run from `dev-archive/tools/re-engine/`)

`%S%` is any scratch folder (e.g. `%TEMP%\visceral-leon`). Step 1 pulls the four originals; steps 2–3
build. Drop `--dry-run` to write. `IDLE` shorthand is expanded in full below so nothing needs a shell
variable.

**1. Pull the originals (read-only on the paks):**
```
py pak_pull.py "C:\Steam\steamapps\common\RESIDENT EVIL 2  BIOHAZARD RE2" %S% natives/stm/sectionroot/animation/player/pl00/list/hdg/base_hdg_hold.motlist.524 natives/stm/sectionroot/animation/player/pl00/list/hdg/base_hdg_move.motlist.524 natives/stm/sectionroot/animation/player/pl00/list/cmn/cmn_move_stlight.motlist.524 natives/stm/sectionroot/animation/player/pl00/list/hdg/hdg_hold_stlight_01.motlist.524
```

**2. Leon v5 base list** (12 locomotion + 2 idle slots come from the built-in map; the six raise slots are the
`--map` lines):
```
py motlist_splice.py --character pl00 --hold %S%\natives\stm\sectionroot\animation\player\pl00\list\hdg\base_hdg_hold.motlist.524 --move %S%\natives\stm\sectionroot\animation\player\pl00\list\hdg\base_hdg_move.motlist.524 --out %S%\leon-v5 --dry-run --map 0140_HG_Hold_Start_L0=0160_OFF_Gazing_Idle_F_Loop@20 --map 0141_HG_Hold_Start_L90=0160_OFF_Gazing_Idle_F_Loop@22 --map 0143_HG_Hold_Start_L180=0160_OFF_Gazing_Idle_F_Loop@24 --map 0150_HG_Hold_Start_R0=0160_OFF_Gazing_Idle_F_Loop@20 --map 0151_HG_Hold_Start_R90=0160_OFF_Gazing_Idle_F_Loop@22 --map 0153_HG_Hold_Start_R180=0160_OFF_Gazing_Idle_F_Loop@24
```
Output (without `--dry-run`): `%S%\leon-v5\natives\stm\sectionroot\animation\player\pl00\list\hdg\base_hdg_hold.motlist.524`, 522,464 bytes.

**3. Leon light list v1** (`hdg_hold_stlight_01`, every empty slot filled from the `OLF_` clips, reload kept):
```
py motlist_splice.py --character pl00 --hold %S%\natives\stm\sectionroot\animation\player\pl00\list\hdg\hdg_hold_stlight_01.motlist.524 --move %S%\natives\stm\sectionroot\animation\player\pl00\list\cmn\cmn_move_stlight.motlist.524 --out %S%\leon-light-v1 --dry-run --fill 0x6e=0190_OLF_GazingWalk_F_Loop --fill 0x6f=0191_OLF_GazingWalk_L_Loop --fill 0x70=0194_OLF_GazingWalk_R_Loop --fill 0x71=0196_OLF_GazingWalk_Back_L_Loop --fill 0x72=0197_OLF_GazingWalk_Back_B_Loop --fill 0x73=0198_OLF_GazingWalk_Back_R_Loop --fill 0x78=0190_OLF_GazingWalk_F_Loop --fill 0x7a=0191_OLF_GazingWalk_L_Loop --fill 0x7c=0197_OLF_GazingWalk_Back_B_Loop --fill 0x7e=0194_OLF_GazingWalk_R_Loop --fill 0x84=0191_OLF_GazingWalk_L_Loop --fill 0x88=0194_OLF_GazingWalk_R_Loop --fill 0xa0=0160_OLF_Gazing_Idle_F_Loop --fill 0xa6=0160_OLF_Gazing_Idle_F_Loop --fill 0x8c=0160_OLF_Gazing_Idle_F_Loop@20 --fill 0x8d=0160_OLF_Gazing_Idle_F_Loop@22 --fill 0x8f=0160_OLF_Gazing_Idle_F_Loop@24 --fill 0x96=0160_OLF_Gazing_Idle_F_Loop@20 --fill 0x97=0160_OLF_Gazing_Idle_F_Loop@22 --fill 0x99=0160_OLF_Gazing_Idle_F_Loop@24
```
⚠️ **Script quirk, same as on Claire:** the tool always names its output `…\pl00\list\hdg\base_hdg_hold.motlist.524`
inside `--out` (808,688 bytes here). The light list must be **renamed** to `hdg_hold_stlight_01.motlist.524` when
it is copied into the game, exactly as Claire's was. Use separate `--out` folders for the two builds so the
second does not overwrite the first.

**Install (modding session, not the reader):** both files go to
`<RE2>\natives\STM\SectionRoot\Animation\player\pl00\list\hdg\` beside a fresh folder (today only `pl10\`
exists there `[measured 2026-09-24]`); LooseFileLoader is already on for Claire's files. Archive both with
their hashes per the keep-every-build rule.

## 7. Dry-run verify lines, pasted `[measured 2026-09-24]`

Leon v5 base:
```
hold: BASE_HDG_HOLD  30 slots, 28 entries, 494416 bytes
move: BASE_HDG_MOVE  46 slots, 42 entries, 1969200 bytes
verify: 11 slot pairs now share one entry because they received the same walk motion (the original does this for two pairs of its own)
verify: 30 slots, 20 replaced, collection verbatim, all blobs byte-identical to their sources, 16-aligned -- OK
dry run: would write 522464 bytes to ...\leon-v5\natives\stm\sectionroot\animation\player\pl00\list\hdg\base_hdg_hold.motlist.524
```
Leon light v1:
```
hold: HDG_HOLD_stLIGHT_01  30 slots, 1 entries, 41808 bytes
move: CMN_MOVE_stLIGHT  66 slots, 44 entries, 1450064 bytes
verify: 30 slots, 20 replaced, collection verbatim, all blobs byte-identical to their sources, 16-aligned -- OK
dry run: would write 808688 bytes to ...\leon-light-v1\natives\stm\sectionroot\animation\player\pl00\list\hdg\base_hdg_hold.motlist.524
```
Both slot-by-slot reports match Claire's line for line (same slot → same clip name suffix, same `@20/22/24`,
same "kept" set: Wheel ×2, Shoot ×3, NoAmmo ×2, Reload, Holster ×2); the kept-slot set in the light list is
the reload only, the other nine stay empty.

## 8. Where Leon differs from Claire (none of it changes the commands)

- **Slot numbers and names: identical**, all 30, in both the base and the light list. Nothing to remap.
- **Clip lengths differ, and Leon's walks are one cycle long.** His `OFF_GazingWalk` L/R/Back loops are 63–68
  frames (Claire's are 240; F is 374 vs 367); his `OFF_Gazing_Idle` is 432 frames (Claire 3,354); his `OLF_` idle
  is 2,560 (Claire 1,000), and his `OLF_GazingWalk_F` / `Back_B` are 495 / 481 while the other four `OLF_` walks
  are 63–67. Loops are loops, so this should not matter to the state machine `[inferred-static]`, but it is the
  first thing to look at if Leon's aim-walk looks wrong where Claire's is fine.
- **Bone/clip counts differ.** Leon's `OFF_` idle is 120 bones / 120 clips (Claire's 173 / 167); his `OFF_` walks
  174 / 168 (Claire 171 / 165); his hold-bank strafes 120 / 120 (Claire 172 / 163). The 2026-09-24 first-run note
  ties the left-hand IK loss to a track the walk clips lack; whether Leon's 120-bone idle carries or lacks
  that track is not known statically `[hypothesis]`.
- ⚠️ **Claire's own base list already contains Leon's clips**: her slots 22–26, 28–29 are named `pl00_1100/1101/
  1102_HG_Hold_Shoot`, `pl00_1120_HG_Hold_Shoot_NoAmmo`, `pl00_1301_HG_HolsterToMove`, `pl00_1311_HG_MoveToHolster`
  (120 bones) `[measured 2026-09-24]`. So shoot/holster clips are shared across characters by the game itself;
  Leon's list is all `pl00_`. No action; worth knowing when reading a layer log that shows `pl00_` on Claire.
- Leon's `base_hdg_move` has one more slot/entry than Claire's (46/42 vs 45/41); the nine names the splice needs
  are all present, so it does not matter which extra clip that is.

## 9. Gate for this item

`GATE: FLAT` for the first look (both files are read at the aim press, no headset needed to see the layer-0
clip names in the log); `VR USER` only for the feel, as with Claire. Two commands, no code. The scratch
folder `%TEMP%\visceral-reader-leon\` holds the pulled originals and the two Claire scratch builds if the
modding session wants to reuse them; nothing there is needed, it can be rebuilt from the paks in seconds.
