# The dock lands, flat: the wrist goes exactly where we say, once we say it in the game's own space (2026-09-05 morning, home PC, `/lm`)

Three launches, all driven from outside the game (`dev-archive/tools/re2drive.py`; Tefa
authorised launching for the session and was away from the PC). Each closed gracefully through
`WM_CLOSE` (now 10 times in total). Plugin versions v0.4, v0.4.1 and v0.5 of
`dev-archive/plugin/src/Plugin.cpp`, one per launch. Evidence in
`dev-archive/recon/2026-09-05-dock-v04/` (three full logs, the matrix dump, three captures).

## What the board asked for

The `[PD]` row: build the dock on the lever v0.3 found, i.e. return a **blended** matrix from the
`Implement.getIKLeftArmMatrix` post-hook instead of a shifted one, latch HOLD natively while docked,
write the controller rotation, and keep NUM6 as a flat stand-in with a synthetic target orbiting
`_101` at 10 cm. The `[FLAT]` row: run it, and see the blend over ~0.2 s, the aim kept up, the
release blending back to 0.000, and the right hand unmoved.

## v0.4 (run 10): the blend and the latch work, but the hand misses the target

Handgun, synthetic dock, 10 Hz trace `[verified-live 2026-09-05, n=1]`:

- **Blend in:** weight 0.13 → 0.83 → 1.00 across three 100 ms samples; `|l_arm_wrist − _101|`
  0.012 → 0.077 → 0.100; the wrist rotation error against the natural frame 5° → 36° → 45°. The
  solver snapped in v0.3; the hook now supplies the ramp. **Req 1 (smooth, not snap) met.**
- **HOLD rides the dock:** `setForce(64, true)` fired on the dock edge, `IsHold → 1` within 4 ms,
  `false` on release, `IsHold → 0`. **Req 2 met.**
- **Release:** weight 1.00 → 0.64 → 0.02 → 0.00, distance 0.100 → 0.068 → 0.003 → 0.000, rotation
  error 45° → 30° → 1° → 0°. **The no-muzzle-pop of req 3:** `|r_arm_wrist − muzzle|` read 0.085 on
  every sample, docked, undocked and through both edges. The right hand never moved.
- **The rotation IS consumed:** with the rotation write on, the wrist turned 45° from its natural
  frame; with it off (NUM3), 0°. The returned matrix's rotation places the wrist's orientation.
- **But** `|wrist − target|` sat at 0.27–0.39 m at full weight, while `|wrist − _101|` was exactly
  0.100. The wrist went 10 cm from the joint, in a direction that was not the target's.

## v0.4.1 (run 11): the game reads the getter in a pre-update pose

The plugin's own summary read of the getter goes through the same hook. v0.4.1 kept that read
apart from the game's (a `g_self_call` flag) and captured the inner getter too. At rest, undocked:

| read | `|value − aid joint final|` | rotation vs final |
| --- | --- | --- |
| the game's per-frame call (`natG`) | **0.18–0.20 m** | **47–49°** |
| the plugin's read at LockScene-pre (`natS`) | 0.000 | 0° |
| inner `get_AidTargetWorldMatrix` on the game's call | 0.18–0.20 m | — |

`[verified-live 2026-09-05, n=1 launch, continuous]`. So the game calls the getter at a point in
the frame where the weapon's aid joint has **not yet reached its final pose** (the value is the
joint's world matrix *at that moment*, inner and outer alike), and its solver then carries the
**offset** we add across to the final pose. That is why v0.3's +10 cm shift moved the wrist exactly
10 cm, and why v0.4's absolute target was missed by exactly the pre-update-to-final gap.

A dump of the natural rows, the target rows, the wrist's final rows and positions, once per second
over six seconds (`run11-rows-dump.txt`), fitted offline (numpy) with the row-vector convention:

```
wrist_final_rows            = returned_rows * M
wrist_final − aid_final     = (returned_t − natural_t) * M
```

with **M one constant rotation (48.2° here)**, the same on all six samples to 0.0–1.7°, and the
translation relation holding to **≤ 2 mm** on every sample (the alternative orderings failed at
2–12 cm). And M is exactly `natural_rowsᵀ · aid_final_rows` — both available to the plugin every
frame (the game's natural value from the hook, the joint's final matrix from
`via.Joint.get_WorldMatrix` at LockScene-pre). `[verified-numerically 2026-09-05, n=6 samples]`

## v0.5 (run 12): blend in final space, map back through M — the hand lands

`update_dock` now: reads the aid joint's final world matrix `A`; forms `M = Nᵀ·A` from the game's
natural `N`; builds the desired final pose (controller, or the synthetic orbit around the *final*
joint yawed 45°); blends **in final space** (`lerp` / `slerp` from `A` to the target by the eased
weight); and maps back: `returned_t = natural_t + (blended_t − A_t)·Mᵀ`,
`returned_rows = blended_rows·Mᵀ`. The hook writes exactly that. At zero offset it is the natural
value, so the blend is seamless at both ends.

Handgun, synthetic orbit, 10 Hz `[verified-live 2026-09-05, n=1 launch, 14 samples over 5 s]`:

```
w=0.17  |Lw-tgt|=0.085 rotW-T=39
w=0.87  |Lw-tgt|=0.015 rotW-T=7
w=1.00  |Lw-tgt|=0.000 rotW-T=0     ... every sample for 5 s, target moving the whole time
release: back to |Lw-aid|=0.000, rotW-N -> the rest value
```

`|r_arm_wrist − muzzle|` 0.085 throughout. In `run12-handgun-docked-v05.jpg` Claire's left hand is
up by her head, well off the pistol. **Minigun (`wp8700`, slot `2`), same test:** identical —
`|Lw-tgt|=0.000`, `rotW-T=0` at full weight, ramp over ~0.2 s, zero "no value" frames on the game's
calls (the "no value" reads of 2026-09-04/05 were the plugin's *own* dump-time reads; the game's
per-frame call always carries one). M is **18°** on the minigun against 48° on the handgun, so it is
per weapon and stance and must be computed live, as it is. Slot `1` returned to the handgun
(`|Lwrist − Rwrist|` back to 0.080). `[verified-live 2026-09-05, n=1 per weapon]`

**Spec v2.3 reqs 1–3 are met flat, on the engine's own wrist solver, with a synthetic target.**
What is left is the real target: the controller.

## Not established

- **The VR pose path has still never carried real values.** Three world-space modes are built
  (NUM1 cycles them): controller re-based HMD-relative onto the game camera's world matrix, bridge
  pose used as world directly, and the first with the camera rows transposed. The quaternion
  conventions between the bridge (glm via `vrmod`) and this plugin's rows are unverified — that is
  the whole point of the modes. **The first VR run tells which one is right, from `|Lctl − Lhand|`
  and the docked `|Lw-tgt|`.**
- The blend weight is linear-eased over 0.2 s; whether 0.2 s is right is a headset question.
- `angM` in the 1 Hz summary is stale while undocked (M is only recomputed while the dock is
  active); harmless, cosmetic.
- What an unreachable target does (arm shorter than the distance) is untested; the orbit stayed
  within reach.
- The pre-update pose is *what* exactly (last frame's? pre-attach?) — irrelevant to the dock now
  that the mapping is measured, but worth one line in the dossier as a trap.

## Automation on RE2, scored (§5a)

menu → gameplay **PROVEN** (n=6 now; capture-verified at each menu, no autosave notice today);
commands **PROVEN** (HOLD, fire, weapon slots, reload, all from outside); character movement
PROVEN (yesterday); **camera NOT proven** (mouse still dead, untouched); self-close **PROVEN**
(WM_CLOSE, 10×). Launch from a shell: `start steam://rungameid/883710`, title responsive at ~35 s.
