# 2026-09-26: Claire's "dirty hands" are the Record system, not a second texture

Tefa asked for the dirty hands (later in the game) to get the same HD treatment the clean hands got:
HD dirt on hands and fingernails, a little worn, less shiny. Static look only; nothing was installed
(the clean-install shake bisect is running in the same game folder).

## What the game actually does `[measured 2026-09-26]`

- **There is no dirty-hands texture.** Across three access logs (09-07, 09-10, 09-25) Claire's skin only
  ever loads `pl1000_Jacket_ALBM/NRMR/ATOS/MSK1` (the clean atlas we already made HD). No `pl1000` file
  with dirt, mud or damage in its name exists in what the game requests.
- **The dirt is painted live** by the Record system into `pl1000_body.rtex` (the runtime "where is the
  dirt" canvas, sampled through UVMap1). **Ours is already 2048² instead of 512²**, so *where* the dirt
  sits is already sharp.
- **What the dirt looks like** comes from `Rec_Mud_Map` in `pl1000_Body_Mat` (and the jacket and
  trousers): the shared `VFX/RecordSystem/RecordTexture/BaseTextures/Record_Mad_Map_MSK4.tex`, **256²**,
  tiled 3× (`Rec_Mud_UVScale 3.0`). That small tiling pattern is why dirty hands look low-detail in VR.
  Its channels: R and G are a normal map (the bumps of dried mud; mean 127), B is mostly white (207–255,
  mean 247: a guess, roughness or cavity), A is the dirt pattern itself (73–214) and is visibly blocky.
  Channel meanings beyond RG = normal are `[hypothesis]`.
- `Rec_FIX` and `Rec_Protect` are `null_black` for Claire (unused slots); `Rec_Injury_*` are the shared
  wound textures.

## The plan (not started; after the bisect)

1. **A Claire-only HD grime pattern**: our own `natives/stm/visceral/visceral_grime_MSK4.tex` at
   2048², same four channels (RG from a dried-mud height field, B and A matched to the original's
   ranges), and point **only Claire's skin material** (`pl1000_Body_Mat`, in the mdf2 we already ship)
   at it. Other characters keep the shared one. Possibly lower `Rec_Mud_UVScale` so the grain reads
   larger.
2. **Less shiny when dirty**: find which channel drives roughness (B is the suspect) with one flat run:
   a copy with B forced to 0 and one with B forced to 255, on a dirty save, look at the hands' shine.
3. **Dirt in the nail beds and knuckle creases**: a tiling pattern cannot aim at nails. Candidate:
   `Rec_FIX` (unused for Claire) with a mask painted in UVMap1 space. Unknown whether FIX means
   "always on" (then clean hands would get it too). Needs a test before any painting.

Test needs a **dirty save** (Tefa knows where the hands get dirty). Game files studied are in the local
`extracted (game data - never commit)/dirty-hands-study/` folder only.
