# HD hands: every file since last night went to Sherry — Claire is `pl1000` (2026-09-06 12:07–13:40 local, home PC, `/pd` + Tefa in VR)

**Supersedes:** the character identification in `2026-09-06-hd-hands-tier-1a-pores-through-the-detail-slot.md`,
`2026-09-06-hd-hands-tier-1b-first-4k-repaint-procedural.md` and `2026-09-06-hd-hands-veins-and-tendons-from-the-rig.md`
(everything those notes say about *what was seen in game* is withdrawn; what they say about the scripts and the
texture pipeline stands and was carried over to Claire), and `ENGINE-DOSSIER.md` §7's "Claire's skin ships with the
detail slot NULL" line.

## What happened `[verified-live 2026-09-06, n=4 restarts, Tefa]`

Tefa restarted after the veins deploy and reported **no veins and rough palms**; after the palm/vein fix, the same.
Three diagnostics in a row, each one restart:

| deployed as `pl3000_Body_ALBM` | in the headset |
| --- | --- |
| 4K, mips 0–1 green-banded over the hands, lower mips plain | plain skin |
| 4K, green bands at **every** mip | plain skin |
| 1K, green bands, the **shipped file's header byte for byte** | plain skin |

The REFramework log showed each file opened by the loose loader at launch, so the bytes were served and then had no
effect. The access log then answered it: the running game loaded `Character/Player/pl1000/` 71 times and
`pl3000/` 16 times. **`pl3000` is Sherry** (her mesh carries `Chain_Mat` + `Pendant_Mat`; she is preloaded for
cutscenes). **Claire is `pl1000`**, with the face under `pl1050` and hair under `pl1070`.

The fourth diagnostic — the same green bands on `pl1000_Jacket_ALBM` at 4K — showed **green over both hands and
forearms, crisp at 9 mm band spacing** `[verified-live 2026-09-06, n=1]`. The loose-file path works, and the game
samples the 4K mips of a loose texture.

**Consequences, stated plainly:** the pores Tefa judged at 02:33 ("better for sure, but grainy"), the pass-2
"much better" and the 03:00 "really good, unbelievable" were **vanilla Claire**. Her skin material already runs
`SectionRoot/Character/Textures/Detail_Skin.tex` — a **128×128** tile — at `Detail_UVScale` 0.5,
`Detail_Normal_Intensity` 0.5. Nothing of ours was on screen until 13:18. Those three verdicts are
`[disproved 2026-09-06]`; the loose-loader, MDF-patch and detail-slot claims from tier 1a are `[hypothesis]` again
until re-run on Claire. Sherry's loose files (`pl3000.mdf2.21`, `_Body_ALBM/_NRMR/_MSK1`) were moved out of the game
folder to `extracted…\sherry-loose-removed-2026-09-06\`; the `visceral_skin_detail_NRM.tex.34` tile stays (unreferenced now).

## Claire's skin, from her own files `[measured 2026-09-06]`

| | |
| --- | --- |
| mesh | `pl1000.mesh.2109108288`; skin submesh `Group_0_Sub_0__pl1000_Body_Mat`, 5 531 verts, hands 7 948 faces |
| skin material | **`pl1000_Body_Mat`** on `Record_Player.mmtr`, samples the **`pl1000_Jacket_*`** atlas (ALBM BC7 sRGB, NRMR BC7, ATOS), one 1024² set shared with `Jacket_Mat`, `Tanktop_Mat`, `Tanktop_Shirt_Mat` |
| hands in the atlas | the same bottom strip: v 0.005–0.144, **left hand u 0.52–0.99, right hand u 0.005–0.49** (Sherry's is mirrored: left on the left); hand+forearm mask 11.1 % of the texture |
| detail slot | `Detail_Skin.tex` 128², UV scale 0.5, intensity 0.5; `DetailMaskMap` = `NullWhite` (no per-texel mask — a palm mask needs a new MSK1 **and** an MDF edit pointing `DetailMaskMap` at it) |
| record system | `pl1000_body.rtex.5`, 64 bytes, same layout as Sherry's |
| bones | identical names and layout to Sherry's (`index_0`/`middle_0` at the knuckle, `ring_0`/`little_0` metacarpals) |
| pose | **flatter, straighter**: finger curl +0.48 (Sherry +1.69), thumb abducted forward out of the palm plane |

## Four fixes the pose change forced in `hd_hands_paint.py` (all also correct on Sherry)

1. **Character parameters**: `--character pl1000 --skin-mat Body_Mat --tex-base pl1000_Jacket` on the paint, recon, render
   and debug-render scripts and `body_tex_build.py --tex-base`. Defaults are Claire now.
2. **Palm plane from the knuckle row, not the thumb.** The thumb-derived plane normal was tilted ~45° sideways on
   Claire; rays from the bones exited the back displaced, and the tendon fan came out 3.6 cm wide for 5.4 cm of
   knuckles (the endpoint probe: knuckles x 0.339→0.393, hits x 0.333→0.369). `lat = axis × (little − index)`.
3. **Projection by ray, from the bone outward.** Lift-then-nearest drifted with any tilt; a ray from inside the hand
   along the back direction lands exactly above the bone. (A ray started 3 cm *below* the bone enters through the palm
   first — tendons went to 0 % until the origin moved to the bone itself.)
4. **Dorsal weight by occlusion, not by normal.** A vertex is on the back if a 4 cm ray from it toward the back meets
   no more hand. The normal test dropped the domed index side; a per-bone mid-plane test misclassified the back-of-hand
   verts driven by the wrist bone, which sits 5 cm up the forearm. Dorsal coverage 44 % → 60 % of the hand mask.

Plus: the arch at half strength and vein ends pulled back, so the wrist convergence no longer saturates into a patch.

## Deployed for Claire at 13:39 `[compile-verified 2026-09-06]`, unseen in game

`<RE2>\natives\STM\sectionroot\character\player\pl1000\pl1000\pl1000_Jacket_ALBM.tex.34` + `_NRMR.tex.34`, 4K:
pass-2 grain, joint creases, wrinkles, veins + tendons (`--anatomy 1.0`), palm pores 30 % (baked relief only — Claire
has no detail mask texture yet, so the material's own 128² tile still runs at full strength on the palms). **The MDF is
untouched**: this pass tests the verified path only. Delete the two files to restore stock.

Blender renders (`claire-hands-work/renders/`): back of the left hand shows tendons to each knuckle and veins between
them under a raking light; the palm shows creases with the grain reduced.

## What the next look decides (FLAT, any launch as Claire, restart required)

| back of the hands | means |
| --- | --- |
| pores + creases + tendons and veins present | the whole pipeline is on Claire; tune by number |
| present but too strong / too weak | `--anatomy 0.6` / `1.5`, `--pores`, restart |
| palms still rougher than the backs | the vanilla 128² tile: next pass adds a `pl1000_Jacket_MSK1` + an MDF edit pointing `DetailMaskMap` at it (tier 1a's MDF patcher, retargeted), or replaces `Detail_Skin` with our 1024 tile |
| hands unchanged | check `reframework_loose_files.txt` for `pl1000_Jacket_ALBM` |

## Rule, written down so it is not learned twice

**A new asset path gets a loud diagnostic before a single pixel of real work.** Three passes were built and judged
on a character name taken from a note. The green-band texture (`mip_diag_build.py` builds the partial-mip version;
`body_tex_build.py` on a striped PNG builds the full one) is a one-restart proof that a loose texture is on screen,
and it is also what proved the 4K mips are sampled. Also noted by Tefa on the same run: **the legs no longer rotate to
match the body** — not from textures; nothing on the body scripts or the plugin changed today, so it is v0.8's head
hider or the spot in the game. Logged for the next `/lm`.
