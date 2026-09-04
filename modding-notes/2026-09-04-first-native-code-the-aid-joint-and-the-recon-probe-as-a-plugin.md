# First native code on Visceral: the game already has an off-hand joint, and the recon probe is now a C++ plugin (2026-09-04, home PC, `/lm`)

**Status at the time of writing: built, deployed, NOT RUN.** Everything below that describes the
game is `[inferred-static 2026-09-04]` from the type database dump (`il2cpp_dump.json`, taken
2026-08-29). The live results get their own entry.

## Why native, why today

Tefa set a standing rule on 2026-09-04: on every game, build features at the deepest layer that
can carry them — native code and the engine's own systems over per-frame script overrides — because
the interpreted layer only runs at the framework's callback moments and only sees what reflection
exposes, and that gap is where the jank comes from. The board's queued item was a *Lua* recon probe
written two days before the rule. So the probe was rewritten as the first piece of Visceral's
native core: a REFramework plugin, `dev-archive/plugin/` → `visceral_core.dll`.

The toolchain was already proven on the Village scope project (VS2022 Build Tools + CMake, the
`Visual Studio 17 2022` generator). The plugin API version had to be checked against **this**
game's REFramework, because RE2 runs a pinned fork build (`76298bd`, 2026-03-11, `pd-upscaler`
branch — see `TOOLCHAIN.md` and the 2026-08-24 entry). Its `include/reframework/API.h` at that
commit declares plugin API **1.15.0**, the same as the header we build against, so the loader's
version check passes. `[verified 2026-09-04, read from the fork at that commit]`

## What the type database said before any launch

Reading the dump for the classes the Lua probes had already touched turned up the shape of the
whole left-hand dock system, already in the game:

| Need (spec v2.3) | The game's own surface |
| --- | --- |
| Where the off-hand goes on a weapon | `app.ropeway.implement.Implement.get_AidJoint()` → `via.Joint`; `Equipment.getAidJointType()` → `None / ExtraNarrow / Narrow / Wide`; `get_LEFT_ARM_JOINT_NARROW()` / `_WIDE()` joint hashes |
| The game's own left-arm solution for it | `Implement.getIKLeftArmMatrix()` and `get_AidTargetWorldMatrix()`, both `Nullable<via.mat4>` |
| Driving a hand smoothly to a point | `app.ropeway.IkController.setArmTarget(int arm_index, via.vec3 pos, bool immediate)`; `setEnable(IkKind, bool, float t)`; `setArmAdjustMode(int, NONE/CANCEL/FIT)` |
| Reading what the arms are doing | `IkController.get_ArmStatusList()` → `IkArmStatus[]` (Index, ActivateTime, ResetTime, AdjustMode, AdjustedPoint) |
| Where the hands are | player skeleton joints via `via.Transform.get_Joints()` → `via.Joint[]` (`get_Name`, `get_Position`) |
| Where the muzzle and aim point are | `Gun.get_MuzzleJoint()` (`ExtraJoint`), `get_MuzzleJointWorldMatrix()`, `Implement.get_AimJoint()` (`VirtualJoint`), `get_AimJointWorldMatrix()` |
| Which layer carries locomotion (req 4) | `via.motion.Motion.getLayer(u32)` → **`via.motion.TreeLayer`** (`get_/set_Speed`, `get_HighestWeightMotionNode()` → `MotionNodeCtrl.get_MotionName`) |
| A speed lever above the layer (req 4, deeper) | `app.ropeway.survivor.SurvivorMotionSpeedController` (`TensionSpeed`, `WaterResistanceSpeed` as `RangeLerpFloat`) — unexplored |

"Aid" is Capcom's word for the support hand. This matters for the design: req 1's "dock the left
hand to the gun, smoothly" does not need our own anchor node or our own hand placement. The
anchor exists (`AidJoint`), typed per weapon, and the arm IK that already places the hand there
during the game's own two-hand aim is reachable with a blend time. The dock becomes "hand the
engine's IK a target", not "move a hand ourselves every frame" — which is the deep-end rule
applied literally.

Two type-name traps for anyone reading public RE Engine scripts against RE2: there is **no
`via.motion.MotionLayer`** here (it is `TreeLayer`), and **`via.GameObject.get_Components` is a
REFramework Lua convenience, not a type-database method** — from native code the only real
overload is `getComponent(System.Type)`; the plain name resolves to dozens of generic stubs with
no function body.

## The plugin (v0.1 recon)

`src/Plugin.cpp`, one file, ~580 lines. It reads, it does not drive.

- **Player binding** through properties, no component scans: `app.ropeway.PlayerManager` →
  `get_CurrentPlayer` / `get_CurrentPlayerCondition` → `get_Equipment`, `get_IkController`;
  `via.motion.Motion` via `getComponent(System.Type)`. Rebinds when the player object changes.
- **On player bind:** the hand/arm/weapon-named joints of the player skeleton with positions, the
  `l_hand` / `r_hand` joints cached, and the IK dump (which `IkKind`s are enabled, the arm status
  list, `get_TargetPosition` / `get_DampingPosition` on each entry with the return type named).
- **On weapon change:** the weapon dump — type, weapon type, aid joint type, the aid joint's name
  and position and owner transform, the narrow/wide hashes resolved on both the weapon and the
  player skeleton, attach joint + offset, aim joint, muzzle joint and matrix, the two nullable
  matrices, and the weapon's full joint list.
- **Every frame:** every motion layer's highest-weight motion name, logged on change (rate-limited),
  plus a 1 Hz summary line: `IsHold`, hand positions, aid joint position, `|Lhand-aid|`, the
  IK-left-arm translation, and — when the VR bridge is live — controller positions and
  `|Lctl-aid|`, `|Lctl-Lhand|`, `|Rctl-Rhand|`.
- **Hotkeys** (numpad only, game window foreground): NUM7 full dump, NUM8 toggle 10 Hz trace,
  NUM9 layer table.
- **Self-checks:** any array whose elements fail `is_managed_object`, or a float array without the
  sentinel, logs `array layout assumption wrong` and stops reading — the layout offsets
  (count @0x18, elements @0x20; string length @0x10 / data @0x14; `Nullable` value @+0x10) are
  the one place the plugin assumes rather than asks.

### The VR bridge

Plugin API 1.15 has **no VR functions** — poses and buttons are Lua-only (`vrmod`). The
plugin gets them through the smallest possible shim, `visceral_native_bridge.lua`:

1. Lua creates a `System.Single[64]`, writes the sentinel `12345` into slot 63.
2. Lua calls `System.GC.KeepAlive(arr)` once. The plugin hooks that method, recognises the
   array by type, length and sentinel, `add_ref`s it, and writes `1.0` into slot 62.
3. From then on Lua writes frame counter, HMD/controller flags, both controller poses, the HMD
   pose and the left stick into fixed slots every `UpdateHID`; the plugin reads them at
   `LockScene`.

No Lua C API is linked, so there is no dependence on the framework's Lua build; the sentinel
check verifies the only assumption (the element offset) end to end. `[compile-verified]`,
hand-over not yet seen live.

## Build and deploy

```
bash dev-archive/plugin/tools/build.sh --deploy
```

Configures once, builds Release x64, checks the two exports, copies into
`reframework/plugins/visceral_core.dll` (previous kept as `.prev`) plus the shim into
`autorun/`, and compares hashes. Zero warnings from our code; the two `size_t` narrowing warnings
in praydog's `API.hpp` are silenced in `CMakeLists.txt`.

## Live results, same evening (four launches, flat, Claire's save with the minigun `wp8700`)

Log rescued to `dev-archive/recon/2026-09-04-native-probe-first-runs/run4-latch-test.log`.
Tags below are `[verified-live 2026-09-04, n=1]` unless stated; one weapon, one save, one PC.

**Run 1 crashed in 8 s — the hand-over method, not the probe.** `System.GC.KeepAlive` is an
internal call: REFramework resolved its "actual function" to an address outside every module,
the hook failed (`Failed to hook 7ff8bcedfd20: unknown exception`) and the Lua shim's first call
to it died inside the native invoker (all crash frames in `re2.exe` past the last managed
function, one REFramework frame under them). Switched the mailbox to a compiled game method,
`app.ropeway.RagdollControlZoneManager.set_AccessMutex(System.Object)`, with the pre-hook
returning SKIP_ORIGINAL only for our sentinel array. **Lesson:** hook and call only methods
whose `function` address in the dump lies inside the executable; `System.*` internals may not.

**Run 2: the hook fired, the array read as length 1.** The count is not at `+0x18` on this
build; the sentinel probe found elements at `+0x20` and the count (64) at **`+0x1c`**
(`+0x18` holds 1 — probably the rank). `[measured 2026-09-04]` The plugin now measures the
layout from the sentinel at hand-over and uses the measured offsets for every array.

**Run 3: everything bound; floats were all zero.** `PLAYER BOUND` on the first frame after the
load (`pl1000`, `PlayerCondition`, equipment, `IkController`, transform, motion all non-null).
`get_Speed`/`get_BlendRate`/`get_Frame` returned 0 through the reflection invoke — XMM0
results are not copied into `InvokeRet` by this build. **Scalar getters now go through the
direct function-pointer route** (`Method::call<T>(vmctx, this, …)`) and read correctly
(speed 1.000, blend 1.00, sane frame counters). Pointer and value-type returns are fine through
invoke (positions, matrices, strings, arrays all consistent).

**Run 4: the findings.**

| Question | Answer |
| --- | --- |
| Hand joints | No `l_hand`. The palm points are **`l_weapon` / `r_weapon`**, wrists `l_arm_wrist` / `r_arm_wrist`, fingers `l_hand_<finger>_<n>` (190 joints on `pl1000`). |
| Aid joint | **`Implement.get_AidJoint()` = weapon joint `_101`**, and `Equipment.getAidJointType()` = 2 (Narrow). The NARROW hash resolves to `_101` and the WIDE hash to `_100` on the **weapon** skeleton; neither exists on the player. Both are 2 cm apart on the minigun's fore-end. Null while the weapon was not yet equipped at bind; present from the first idle frame on. |
| Where the left hand is | **Exactly on the aid joint**: `|l_weapon − _101| = 0.000` in idle, walking, jogging and aiming. The game's own two-hand hold pins the palm to `_101`. `AidTargetWorldMatrix` carries the same translation with a live rotation; `getIKLeftArmMatrix()` had **no value** in every state on this weapon. |
| IK state | `IkKind` LEG and **ARMFIT** enabled; ARM, HAND, SPINE, LOOKAT disabled; `UseIkArm=0`, `UseIkWrist=1`, `UseIkArmFitAsWrist=1`; `ArmStatusList` empty; `ControlStatus` has 6 entries (one per kind). Unchanged between locomotion and aim on the minigun. |
| Locomotion layer | **Layer 0.** Walk = `pl10_0190_KFF_GazingWalk_F_Loop`, back = `_0197_…Back_B_Loop`, jog = `pl10_0231_KFF_Jog_Straight_Loop` (start/end clips around it), idle = `pl10_0160_KFF_Gazing_Idle_F_Loop`. Layer 1 = arm (`pl10_2000_GFC_Arm`), layer 2 = fingers (`pl10_02_FIN_GG_LGT`), layers 3–5 empty. Speed 1.000 and blend 1.00 on all live layers. The `/gr` layer-0 guess holds for RE2. |
| Aim, natively | **`InputSystem.setForce(64, true)` from the plugin raised the aim state within a frame**: `IsHold` 0→1, layer 0 → bank 2 `pl10_0140_GG_Hold_Start_L0` → `pl10_0160_GG_Hold_Idle_Loop`, layer 2 → `pl10_50_Hold_GG_LGT`, `TargetBankType` 50 → 3145778, strafing under hold (`GG_StrafeL_F`). `setForce(64, false)` dropped it all back. The 2026-08-27 Lua latch, now native. |
| Synthetic mouse | Right-mouse via `SendInput` never raised `IsHold` (two 2 s holds). Keyboard scancodes work; numpad must be sent by virtual key. Recorded in the control profile as disproved. |
| VR bridge | Attached on frame 182 of every launch after the fix; poses are zeros because no headset was on (`hmd=0 ctl=0`). The hand-over itself is proven; the pose path is not. |

### What this means for the dock design

The engine already pins the off-hand to a named weapon joint and exposes the target matrix for
it. The dock of spec v2.3 therefore has an anchor (`_101` / `_100` by `AidJointType`) and a
proven way in and out of the aim state (`setForce(HOLD)`), both native. What it does **not** yet
have is the smooth-transition lever: on the minigun the arm IK kind (`ARM`) is off and the hand
placement comes from ARMFIT + animation, so `setArmTarget` has not been exercised. That is the
next live experiment, and it needs a **one-handed weapon** (handgun), where the aid joint may be
absent or the hand may float, to see which kind the game switches on for two-hand aim.

## Not established

- All of the above is one weapon (the minigun, always two-handed) on one save. A handgun run is
  the missing half.
- `setArmTarget` / `setEnable(ARM, …)` have not been called — read only, no writes to IK.
- The VR pose path (slots 3–24 of the bridge array) has never carried real values.
- Whether `getIKLeftArmMatrix()` carries a value on any weapon, or only in the VR/first-person
  path — every read so far said "no value".
