# Item 22 splice script built, verified on both characters, and the RT motlist container decoded (2026-09-06 01:50–02:05, home PC, `/pd`, static)

**The game was not launched. Nothing here has been run in the engine.** This closes the `[PD]` row
"write the splice script now, blind" from `2026-09-05d`. The script is
`dev-archive/tools/re-engine/motlist_splice.py`; its outputs for Claire and Leon are in
`D:\RE2 REFramework builds\extracted (game data - never commit)\splice-out\` and stay there.

## 1. What the container actually is `[measured 2026-09-06, n=5 files]`

Probed on the four originals (`pl10`/`pl00` × `base_hdg_hold` / `base_cmn_move`) and the modder's RT
Jill specimen:

| part | fact |
| --- | --- |
| header | `0x00` u32 524, `0x04` `mlst`, `0x10` u64 pointer table (0x50), `0x18` u64 collection offset, `0x20` u64 name offset (0x34), `0x30` u32 **slot** count, `0x34` UTF-16 name |
| pointer table | one u64 per slot; **two slots may share one entry** (Hold_Idle_Loop and Shoot_NoAmmo do in the original, in every file) |
| entries | contiguous, **16-byte aligned**, from the first pointer to the collection offset; blob = bytes to the next distinct start, padding included |
| entry header (v492) | name offset u64 at **+0x58** (entry-relative), frame count f32 at +0x60, bone/clip counts u16 at +0x70, fps u16 at +0x78, `motSize` u32 at +0x0C = blob size minus padding — and **0 on the first entry of every file**, originals and specimen alike |
| offsets | **no 8-byte header field in any entry points outside its own blob** (0 of 10 fields × 165 entries). Entry-relative, as CAF says. Byte-copying an entry between containers is sound on this axis |
| **collection block** | **72 bytes per slot, in slot order.** Seventeen of its eighteen u32 columns are constant across every record; the one that varies is u32 at **+8 = the motion NUMBER** (`0x6e` = 110 for `pl10_0110`, `0xa0` = 160 for `pl10_0160`), and u32 at +20 is 1. **Byte-identical between Claire's original and the Jill replacement**, whose entries are all different sizes — so it holds no offsets or sizes. It is the slot → motion-number table the game's motion FSM asks by. |

Consequence: a splice is header + rebuilt pointer table + re-laid entry blobs + **the original collection block
verbatim**. Names inside the blobs are left alone (the game keys by number; renaming in place would need
room the blobs do not have). `[hypothesis]` that lookup is by number and not by name hash — the collection
block is the evidence, one flat run is the proof.

## 2. The script

`motlist_splice.py --game-dir <RE2> --character pl10 --out <folder>` pulls the two originals out of the
paks itself (through `pak_pull.py`, by path hash — no file list needed), builds the spliced
`base_hdg_hold.motlist.524`, **re-parses the output independently** and checks: slot count, container name
and collection block unchanged; every slot resolves to the intended motion; every blob byte-identical to its
source; every entry 16-aligned; the original's shared slots still shared. `--hold/--move` take files instead
of a game folder; `--map AIM=WALK` overrides one mapping; `--dry-run` reports only.

Default mapping (the twelve rows from 05d, everything else kept):

| aim slot (replaced) | walk motion it now holds | frames |
| --- | --- | --- |
| `0110..0115_HG_Interpolation_{F,L,R,Back_L,Back_B,Back_R}_Loop` | `KFF_GazingWalk_{F,L,R,Back_L,Back_B,Back_R}_Loop` (0190/0191/0194/0196/0197/0198) | 344/250/243/230/391/217 |
| `0120_StrafeL_F`, `0122_StrafeL_L`, `0124_StrafeL_B`, `0126_StrafeL_R` | GazingWalk F / L / Back_B / R | |
| `0132_StrafeR_L`, `0136_StrafeR_R` | GazingWalk L / R | |

Results `[verified-numerically 2026-09-06]`: Claire 795,392 → 740,928 bytes, Leon 494,416 → 479,472 bytes,
30 slots, 12 replaced, 22 distinct entries (slots given the same walk motion share one entry, as the original
does for its own repeats). The `--game-dir` output and the `--hold/--move` output are **byte-identical**
(`cmp`), and the independent probe reads the output as a valid 524 container with the same collection head.

Redistribution: only the script ships. The output is made of the player's own game data on the player's disk.

## 3. What is NOT established — and what the one flat run decides

1. **Lookup by number.** If the game keys motions by name hash instead, the spliced slots play nothing
   (T-pose or frozen lower body while aiming). Fix would be renaming inside blobs, which needs a
   relocation of the name string — a bigger script, not a different idea.
2. **`motSize = 0` on the first entry.** The build keeps blobs byte-identical, so slot 0 now carries a real
   `motSize` where every original has 0. Every original AND the modder's file have 0 there, which reads like
   a writer quirk rather than a rule; if the file is rejected at load, this is the first thing to try
   (`--zero-first-motsize`, one line to add).
3. **Loop length.** The aim loops were 66–70 frames; the walk loops are 217–391. If the aim-walk blend
   nodes drive phase by normalised time the walk will play slow; if by frame, fine. Only the run says.
4. **Root motion / footstep events** keyed off data inside the entry — unknown, per 05d.
5. The upper body: the walk motions swing the arms. The plan is that VR IK owns the arms regardless
   (spec v2.3 dock + right-hand IK); if the arms visibly swing under the gun, that is this, not the splice.

**The flat run:** copy `splice-out\natives\stm\sectionroot\animation\player\pl10\list\hdg\base_hdg_hold.motlist.524`
to `<RE2>\natives\STM\sectionroot\animation\player\pl10\list\hdg\` (loader already on), launch as Claire,
draw the handgun, walk while aiming. Legs walk normally = item 22 done in principle. Frozen/T-posed lower
body = (1). Crash or vanilla aim-walk = the loose file was not taken or (2). **Do this on a different launch
from the head-hider test**, so a rejected motlist cannot mask that result.
