# 2026-11-23 (evening, by hand-on, PC1, Fable): the left hand's stages told to Hand Poser, and the trigger finger (RE2 and RE3)

Tefa, 2026-11-23: "no to a plan for re2/re3, just do your thing": build what Village has, so every reload and rack stage can
keep its own finger pose here too. Shared code (RE2's repo owns it, `tools/sync_shared.py` copies it to RE3). The static
groundwork was the reader's: `engine-research/inbox/2026-11-23-mod-re2-re3-left-hand-stage-plan.md` (folded in below).

## What was wrong before `[inferred-static 2026-11-23]`

- The plugin never wrote the gun to bridge slot 84 (`S_WHO`: its writer went with gen-1's `tune.cpp`), the Lua array starts at
  zero, and `hand_poser.lua` took any number >= 0: so Hand Poser would have named every gun, and no gun, `wp0000`.
- There was no number for what the left hand is doing: the fingers were the game's animation through every reload and rack.
- `hand_poser.lua` read slots 57 / 58 on RE3 (the old bridge; dead slots in the rebuilt one), and on RE2 held the left
  fingers only while slot 539 said "on the gun": a hand with a magazine or shells in it (no dock) would have been dropped.
- Hand Poser's script was not installed in either game.

## What is built

- **`src/guns/stage.cpp` / `.h`** (new): `stage::publish()` after the frame's gesture code and again after the late point
  writes: slot 84 the gun's WP number (-1 = no gun; the knife and grenades are not guns here), 85 the character, **649 the
  left hand's stage** in Village's spot numbers (3 on the slide / bolt / fore-end, 4 on the seated magazine or riding it
  in, 5 a pump's fore-end held with LG alone, 6 on the drum [RE3's MGL], 9 shells in the hand, 11 the part at the back
  [long guns and pumps; a pistol keeps 3, Village's rule], 13 a magazine in the hand or sliding out into it, **20 shells
  docked at the port** [new; Village sends 8 there, which the script names "slide from the left"]), **650 the magazine's
  way** (0 = not riding the hand, else 1 + 0..1; timed here, 0.19 s, not by distance as in Village). The log says
  `stage: WP0000 left hand 4` at every change.
- New small accessors: `rack::at_back()`, `rack::gripping()`, `magazine::left_stage()`, `cylinder::hand_on_drum()`.
- **`lua/visceral_bridge.lua`**: the array is 653 long, slot 84 starts at -1, and the table has `n`.
- **`hand_poser.lua`** (the hand-poser-vr repo): one `vbridge()` for all three games, asked in Village's slot numbers
  (RE2 / RE3: 153 -> 649, 279 -> 650, 36 -> 539, LT 3 and RT 5 the same; everything else reads 0 there); the RE3 57 / 58
  line is gone; spot 20 = "shell port"; the hand keeps its pose with a magazine or shells in it.
- **`src/hands/righthand.cpp` / `.h`** (new): the right index finger's three joints bend on RT with a gun in hand, eased
  over 0.07 s, written last in the frame (BeginRendering pre, from `guns::begin_render`), with Village's guard against the
  doubled BeginRendering of Two Frame Sequential. The loader gives pressed / not pressed only, as in Village.
- **`tools/install-list.txt`** (each game's own): layer 3 installs `hand_poser.lua` from the hand-poser-vr repo beside this one.

## Run by robot (build 218, the desk headset, Claire's all-weapons save) `[verified-live 2026-11-23, robot, n=1 unless said]`

- **The gun is reported**: slot 84 read 4300 (Spark Shot), 0 (Matilda), 1000 (W-870) as each came into the hand; the bridge has `n` = 653.
- **The stages are reported** (the Matilda, `re_pistol.sh 1`, the log's `stage:` lines): 3 with the hand on the slide (n=8), 13 with a
  spare from the pouch in the hand, 4 while it goes in, 4 with the hand on the seated magazine, 0 between. NOT run: the
  shotgun's 5 / 3 / 11 / 9 / 20 (my robot run moved the gun hand out of reach and the save carries no shells), the LE 5, RE3's drum (6).
- **The trigger finger**: all three joints found (`r_hand_index_0..2`); with RT down their own z went +4 / +20 / +10 degrees and the
  last joint came 0.6 cm nearer the wrist (`q_re_index.lua`, read at EndRendering): a bend is a POSITIVE z on the right hand `[measured]`.
  On the LEFT hand a bend is a NEGATIVE z (`l_hand_index_1` at +60 stood bent backward) `[measured]`.
- **Hand Poser works on RE2**: the window (started with `--game <RE2's folder>`: it links to the game running when it starts) took
  `pose.py l_hand_...=x,y,z` and saved `wp0000 @ slide`, `wp0000 @ magazine`, `wp0000 @ magazine in hand`; the fingers moved at once.
  The poses kept are the game's own curled hand, slightly changed: a start for Tefa, not a judged pose. Ring and little fingers
  have four joints here (`_0` is in the palm).
- **The hand on the SEATED magazine: confirmed, and why the afternoon's scan missed it**: the grab measures the left WRIST to its
  docked place, which lies about 10 cm UNDER the magazine joint (where a hand under the grip has its wrist), within 15 cm. With the
  wrist AT the magazine joint the new log line says 0.213 m (no take); 10 cm lower, 18 cm lower, and 10 lower / 8 back or right: taken
  (4 of 7 places). Right B then releases it into the hand. A place right at the limit read "0.150 m" and missed.
- **A carried magazine sits out of the fingers** `[measured, Claire, the Matilda]`: the magazine joint is 13.5 cm out of the palm
  side of the wrist while the curled fingertips reach about 6 to 8 cm; a picture shows a gap. Not changed (the hold numbers are
  Tefa's worn ones from b165-b186, and the joint may sit at the magazine's far end): for Tefa to look at in the headset.
- **My slip, said plainly**: one pose meant for "magazine" also landed in the Matilda's GENERAL pose (`wp0000`), because that LG
  press missed the magazine by a hair. The window refills it from the game on connect, so it stays: the left fingers are held in
  the game's own light curl whenever the left hand is near the Matilda with no stage. Harmless to look at; Tefa can clear the left
  hand's rows of "wp0000" in the window.
- The loader's log fills with `Failed to load file hand_poser\command.json` until the window has been opened once for the game
  (the script asks for a file the window makes).
- Only the Matilda has poses. Every other gun shows the game's own fingers until posed (a pose is per gun; "any gun @ <spot>" is
  read as a fall-back by the script, but the window has no way to save one yet: a tool job).
