# The HOLD bank is thirty named motions, the RT format is RE3's shape, and item 22 is a splice, not an animation job (2026-09-05 late evening, home PC)

Follows on from `2026-09-05c-the-motlist-dead-end-was-the-wrong-door.md`. Everything below was
done with nothing running; the specimens were unpacked to the session scratch folder and read,
nothing derived from them is in this repo.

## What was done

1. **The REFramework loose-file loader was OFF on this install, and so was its logging.**
   `re2_fw_config.txt` carried `LooseFileLoader_Enabled=false`, `LogAccessedFiles=false`,
   `LogLooseFiles=false` `[measured 2026-09-05]`. That is why `reframework_accessed_files.txt`
   held one line. **All three are now `true`** (CRLF preserved; backup of the previous file in the
   session scratch folder). Consequence: the loader is a REFramework built-in, not a Fluffy
   feature, and it was simply never switched on — the earlier note's "REFramework has its own
   loose-file loader" is confirmed as a switch, not a guess. **The next launch, any launch, will
   log every path the game requests**, which answers the RPD-lights path question and every other
   "where does X live" question without a pak extractor. Flat row added.
2. **CAF (Custom Animation Framework, godlock2000-eng / NonRTX, MIT) is cloned read-only** at
   `D:\RE2 REFramework builds\study - CAF NonRTX (MIT, read-only)\caf`. It carries what our
   2026-08-29 topic promised and more: `docs/motlist_format_guide.md`,
   `docs/mot_format_specification.md` (a 1000-line RE, with a Known / Partial / Unknown table),
   and a **working `.motlist.85` writer** (`tools/mot_writer.py`, `motbank_writer.py`,
   `validate_against_real.py`) plus a Blender exporter. It ships its own custom motlists under
   `natives/x64/CAF_custom/` — the non-RT root.
3. **A header dumper was written and run over every specimen** (scratch `motlist_peek.py`, per
   the CAF guide's layout; `u16names.py` for a name sweep). Results below.

## Finding 1 — the whole HOLD bank, by name `[measured 2026-09-05]`

`base_hdg_hold` has **30 entries, 28 unique motions** (two pointer-table entries repeat:
`Hold_Idle_Loop` and `Hold_Shoot_NoAmmo` each appear twice). Names are identical in the RT
(`.524`) and non-RT (`.85`) files, so the RT port is a **format conversion of the same motion
set**, not a different selection.

| Group | Motions | Notes |
| --- | --- | --- |
| aim-walk loops | `pl10_0110..0115_HG_Interpolation_{F,L,R,Back_L,Back_B,Back_R}_Loop` | 66–70 frames, 136 bones |
| aim strafe | `pl10_0120/0122/0124/0126_HG_StrafeL_{F,L,B,R}`, `pl10_0132/0136_HG_StrafeR_{L,R}` | 288 frames each, 172 bones |
| raise | `pl10_0140/0141/0143_HG_Hold_Start_L{0,90,180}`, `pl10_0150/0151/0153_HG_Hold_Start_R{0,90,180}` | 20–24 frames |
| hold | `pl10_0160_HG_Hold_Idle_Loop` (600 f), `pl10_0165/0167_HG_Wheel_{L,R}180` | |
| fire | `pl00_1100/1101/1102_HG_Hold_Shoot`, `pl00_1120_HG_Hold_Shoot_NoAmmo` | **`pl00` = Leon's data reused in Claire's bank** |
| reload / holster | `pl10_1200_HG_Hold_Reload`, `pl00_1301_HG_HolsterToMove`, `pl00_1311_HG_MoveToHolster` | |

All at 60 fps, 185–186 bone clips per motion. **So `pl10` is Claire and `pl00` is Leon**, read from
the names inside the file rather than the mod description — stronger than yesterday's inference,
still `[inferred-static 2026-09-05]` until seen in the game's own files.

`base_hdg_move` is **45 entries / 41 unique** and is the *weapon-holstered* locomotion (`OFF_`
prefix: Gazing idle, turns, gazing walk, jog, stairs, pivot turns); `base_stg_move` is the same
list with `TFF_`. So the three-file Jill mod replaces aim locomotion + holstered locomotion for
handgun, and holstered locomotion for shotgun.

**What item 22 actually has to change is the first two rows** — the six `Interpolation` loops
and the six `Strafe` motions. Everything else in the bank (raise, fire, reload, holster) should
stay.

## Finding 2 — the RT format is RE3's shape with one extra pointer `[measured 2026-09-05]`

| | non-RT `.motlist.85` | RT `.motlist.524` |
| --- | --- | --- |
| container header | 0x00 ver, 0x04 `mlst`, 0x10 pointer table, 0x18 collection, 0x20 name offs, 0x30 count | **identical** |
| mot entry version | 65 | **492** |
| mot entry header | 0x74 bytes; name offs at +0x50; frameCount at +0x58; bones/clips at +0x68; fps at +0x6E | **one extra 8-byte offset at +0x50**; name offs at +0x58, frameCount +0x60, bones/clips +0x70, fps +0x78 — every later field shifted by 8 |
| bone clip header | 24 bytes: idx u16, flags1, flags2=0x00, hash u32, float 1.0, pad, trackHdrOffs u64 | **12 bytes**: idx u16, flags1, **flags2=0xFF**, hash u32, trackHdrOffs u32 — the RE3 (v78/v99) shape from CAF §12 |
| bone hashes | e.g. `0xaba7de3c` for bone 0 | **same hashes** — same skeleton |
| file size (hold) | 2,157,616 | 1,351,792 — the RE3-style compression is denser |

So the RT build did not just bump a version number: **RE2 RT uses the newer RE-Engine motion
format, the one CAF's spec only partly cracked** (RE3's 2/4/5-bytes-per-key rotation packing is
in their UNKNOWN table). CAF's writer targets v85/v65 and **cannot produce our files**. Their own
Blender pipeline doc lists no public `.mot` importer/exporter either.

The empty specimens confirm the container reading: every 80-byte `.motlist.524` is a valid list
with **zero entries** (count 0 at 0x30, collection offset = 0x50 = end of file). That is how the
*Restore Mr.X dead* add-on removes cutscene motion — it blanks the list rather than editing it.

## Finding 3 — item 22 does not need a `.mot` writer at all `[hypothesis]`

The CAF guide's key property: **every offset inside a mot entry is entry-relative**, and the
container is just a pointer table over opaque entry blobs. The normal-walk motions we want under
the aim pose already exist in the game's own files, in the same v524/v492 format, on the same
`pl10` skeleton (`base_cmn_move.motlist.524` — the `CMN` bank the 2026-08-30 note tried and
failed to *redirect* to). So:

> **A new `base_hdg_hold.motlist.524` = the original's container and collection block, with the
> twelve aim-walk entry blobs replaced by byte-copied walk-cycle blobs from `base_cmn_move`,
> renamed in place, pointer table rebuilt.**

No compression knowledge, no Blender, no bone remapping. The unknowns are exactly two: whether
the collection/CLIP block at `colOffs` references entries by index or name (CAF: "not analyzed"),
and whether the game keys any per-motion behaviour (footstep events, root motion) off data inside
the entry rather than the bank slot. Both are answered by one flat run of a spliced file.

**Redistribution stays clean by construction:** the spliced file is made of the player's own game
data, so the mod ships **the splice script**, not the file. The player runs it once against their
install. That is the same shape as the `openvr_api.dll` rule — nothing original leaves the
player's disk.

### What the splice needs, in order

1. **The two original files out of the pak** — `base_hdg_hold.motlist.524` and
   `base_cmn_move.motlist.524` for `pl10` (and `pl00` for Leon). Needs a pak extractor and the RE2
   RT file list. CAF names **RETool** (FluffyQuack) as the pak tool; the list is the thing to find.
   **The accessed-files log from the next launch gives the exact internal paths for free.**
2. The splice script (Python, ~150 lines against the container layout above). Can be written
   now, blind; it is testable the moment step 1 lands.
3. One flat launch with the output under `natives/STM/…/pl10/list/hdg/` and the loader on.

## Credits

xJustAdam (2516) and Angeltitilxyz (2529) for the specimens, Shulian01 (Dragonleo) for Claire A,
godlock2000-eng for CAF and its format docs — all already in `CREDITS.md`; CAF's line should say
the format documentation and writer are what we used, if the splice ships.
