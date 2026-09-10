# 2026-09-10 — first headset run: poses yes, grip no, camera dead

Home PC `RTX`, Quest 3 over Virtual Desktop, **OpenXR** (no `openvr_api.dll` — the board's
"copy it first" step was stale; `openxr_loader.dll` is what RE2 and Village both use here).
Tefa driving, one launch, two level loads (RPD arrival save, then a later level), ~7 minutes.
Log window: `re2_framework_log.txt` from line 2257, 22:06–22:14.

## What worked

- VR came up on its own: `[VR] Multipass textures are setup correctly`, OpenXR swapchains
  2688×2880 per eye `[verified-live 2026-09-10, n=1]`.
- `VR BRIDGE ATTACHED (argv[1] of 5)` and the very first summary read `hmd=1 ctl=1`.
  **The bridge has never carried a live pose before this run.** Left stick values arrived too.
- The plugin bound the player (pl1000, Claire) on both level loads, created the neck plug and
  both bracelets both times, walked the hierarchy, and ran the 1 Hz summary throughout: 414 lines.

## What did not, and why — read from the log, then confirmed in the code

### 1. Grip never arrived → the dock cannot fire by controller (code defect)

`LG=0.00 RG=0.00 RT=0.00` on all 414 summaries. `visceral_native_bridge.lua` declares the four
slots `S_LGRIP, S_LTRIG, S_RGRIP, S_RTRIG = 26, 27, 28, 29` and **never writes them** — the
per-frame block fills HMD and controller poses and `get_left_stick_axis()` and nothing else.
`Plugin.cpp:1416` gates the dock on `S_LGRIP > 0.5`. So by grip the dock was unreachable on
every build that has ever shipped; only the `NUM6` flat stand-in can engage it.
`[verified-live 2026-09-10, n=1]` for the symptom; the cause is a code fact.

Fix (Lua only, no rebuild): `vr:get_action_handle("/actions/default/in/Grip")` and `Trigger`,
then `vr:is_action_active(h, source)` per hand — REFramework serves these under OpenXR as well.

### 2. The camera the plugin reads does not move → re-basing and the reveal gate both fail

`cam=(-11.50 -3.20 4.20)` — the same triple on every summary, in both levels, while the player
sat at x≈+2.2 and then x≈−17.8. `update_camera()` reads
`via.SceneManager.get_MainView → get_PrimaryCamera → get_WorldMatrix`; under REFramework VR
that is evidently not the camera doing the rendering (REFramework itself logged
`VR: Failed to get primary camera!` three times at init). Two rows fell to this one cause:

| symptom | reading |
| --- | --- |
| mode-0 controller re-basing | `\|Lctl-Lhand\|` 2.6–5.8 m in level 1, 28–30 m in level 2 — never centimetres |
| head-hider reveal gate | `d` = 15.0, 9.98, 17.5 m → `REVEAL: camera off the head` all run; head never hidden |

This supersedes the "two positions in different spaces" guess in the head-hider row: the
position is *stale*, not mis-spaced. `[verified-live 2026-09-10, n=1]`

### 3. Observations without a diagnosis yet `[reported 2026-09-10, n=1]`

- **HD hands:** present at the RPD-arrival save ("as they did last time"), **stock in a later
  level**. `pl1000_Jacket_ALBM/NRMR/MSK1` were requested once (22:06:58); nothing was requested
  on the second `PLAYER BOUND` (22:09:13).
- **Bracelets:** "grey and flickering a little" in the later level. `BRACELET l/r CREATED` a
  second time at 22:09:13 and `visceral_bracelets.mdf2.21` re-requested, but the six bracelet
  textures were requested only once. Grey = textures not bound on re-creation; flicker = two
  sets drawing, or sleeve z-fight.
- Neck plug: created (`plug=on`) but inside a visible head — cannot be judged until item 2.
- Palms and grime: not looked at.

## Tefa, verbatim

> hands look as they did last time, when i loaded into the first save where Claire arrives in
> the RPD station. head shadow is not present and the normal neck is still there … dock did not
> work with only left grip, nothing happened … loaded into a later level and hands were back to
> stock, not with nice fingernails and skin textures, also in that later level the wristbands
> you made are showing now, grey and flickering a little

No numpad key was pressed during the run (none logged).

## Board changes

Two new ⭐⭐ `[PD]` rows (grip slots; camera read), one ⭐ `[PD]` (bracelets), the dock `[VR]`
row demoted to ⬇️ and marked blocked on them, the `[USER]` "copy `openvr_api.dll`" row closed,
the HD-hands and neck-plug rows annotated. Next headset run only after both ⭐⭐ rows are built.
