# Ladder + cupboard view hold: ported to C++ (b086, 2026-10-06)

**Build:** `D:\Visceral build versions\Ladder climb\v0.2.0-b086 - ladder and cupboard view hold, first C++ port\`
(the first build stored in a FEATURE folder; Tefa 2026-10-06: every feature gets its own folder; `snapshot_build.py --feature`).
Installed in the Steam game; unworn. Everything from b085 (holsters, aim suppressor) is still in it.

## What it is

A line-by-line C++ re-write of Arcade Controls' `re2_vr_ladder_body_yaw_fix.lua` v12.2 (staging `028a678`), in
`dev-archive/native/src/ladder.cpp`. Same steps, same order, same numbers (`settings.h`, ladder block):

1. **View hold during every jack** (`JackDominator.get_Jacked`: ladders, cupboard pushes, switches). The player camera
   controller's `<Yaw>`, `PrevYaw`, `<CameraRotation>` and `SyncCameraRotation`'s quaternions are pinned every frame
   and right after each controller update (post-hooks), aimed at the body's facing, turned with the body (the 180 at
   the top), nudged until the RENDERED view faces the body, and handed back at the end as `rendered view - headset`.
2. **Climbing body guard:** while a motion named LADDER/CLIMB/HASHIGO plays, the body rotation the game set is put back
   after FirstPerson turns it (LockScene pre, PrepareRendering post).

Left out on purpose: the experiments Arcade Controls had already switched off (comfort cone, camera clamp, head-yaw lock).

The view readings only Lua can give (FirstPerson on, raw headset yaw, rotation-offset yaw, camera yaw, rendered yaw)
come through the bridge: `visceral_bridge.lua` now has 40 slots and writes them at LateUpdateBehavior PRE.

## Known before testing

- **Tefa (2026-10-06): the Arcade Controls ladder fix "definitely had problems and was not fixed properly."** This port
  reproduces it as it was, so the same problems are expected; it is the starting point, not the answer. The one written
  down (2026-08-23 board): **a snap, then a turn, as a jack starts.** Arcade Controls traced it to REFramework
  FirstPerson's own smoothing (its snap check `bone_scale == 0.0f` never fires), which this plugin cannot reach.
  `[inferred-static]` Ask Tefa what else was wrong.
- Untested in C++ `[compile-verified 2026-10-06]`: the motion-name read (direct calls on `via.motion` objects), the
  camera-controller search, the field writes. Each one logs on first use.

## What to look for in the log (`re2_framework_log.txt`, `[visceral-native] ladder:`)

- `motion reads count=1 layer=1 node=1 name=1` once; then `layer0 motion <name>` lines while walking about.
- At a ladder: `CLIMB START (<name>)` / `CLIMB END ... put back N frames`. If climbing never logs, read the
  `layer0 motion` names during the climb and add the real one to `CLIMB_NAME_PARTS`.
- Every jack: `jacked -> 1`, `camera controller FOUND ... yaw=1 prevyaw=1 rot=1`, `N camera-controller update hooks in`,
  `HOLD captured ... branch=formula` (first) or `branch=K` (later), `held ...` twice a second (`verr` should fall near 0),
  then `jacked -> 0` and `RELEASE`.

## How to take it out

Put back b085 (`D:\Visceral build versions\v0.2.0-b085 ...`): copy its `reframework\plugins\visceral_native.dll` and
`reframework\autorun\visceral_bridge.lua` over the game's.
