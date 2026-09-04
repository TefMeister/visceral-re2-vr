# The aid joint is an anchor, the ARM kind is dead, and the native trigger fires (2026-09-05, home PC, `/lm`)

One flat launch, driven end to end from outside the game (Tefa authorised launching for this
session; the game was closed gracefully at 00:25 through `WM_CLOSE`). Static half first, because it
decided what the launch was for.

## Static: three decompiles settle the direction of the constraint

Read with ghidrust from `re2.exe` (the addresses come from `il2cpp_dump.json`). **Reading rule that
fell out, reusable for every RE2 decompile:** the first register is the **VM context**, `this` is
the **second**; the `*(rcx+0x50)->+0x18 != 0 → return` prologue is the VM's interrupt check, not a
field of the object. `[inferred-static 2026-09-05, three functions]`

| function | address | what it does |
| --- | --- | --- |
| `Implement.setupAidJoint` | `0x140ef7a10` | reads an object at `this+0xe8`, virtual call, **switches on 0/1/2/3** (= `getAidJointType()`: None / ExtraNarrow / Narrow / Wide), per case resolves a joint through one lookup helper (`FUN_141f7a0d0`, eight call sites, third arg always 0) and stores it. Setup only — no per-frame write. |
| `Implement.updateJointConstraint` | `0x140f11ad0` | reads **`this+0x78`** and nothing else on `this` (besides `+0x10`); copies a vec3 from `(+0x78)->+0x20..0x28` into a target slot and sets a has-value byte. |
| `Implement.get_AidTargetWorldMatrix` | `0x140ebf3e0` | reads **`this+0x80`**; if null, returns the static null `Nullable<mat4>` at `0x1491842c0`; else fetches a **joint world matrix** into the return buffer. |

`Implement`'s fields, from the dump (`offset_from_base`): `+0x70 JointConstraintExpressionID`,
**`+0x78 AttachJoint` (`JointConstraintInfo`)**, **`+0x80 AidJoint` (`via.Joint`)**, `+0x88 AimJoint`,
`+0x98 Motion`, `+0xd8 MotionFsm`.

So: **`updateJointConstraint` is the weapon→right-hand ATTACH constraint** (the `setProp_A_00`
joint with the (−0.005, 0.010, −0.060) offset the probe reads at `JointConstraintInfo+0x10/+0x20`),
and it never touches the aid joint. The "`_101` is a follower written by Implement's constraint"
hypothesis of 2026-09-04 is **`[disproved 2026-09-05]`**. And the aid *target* is served **from the
weapon joint's world matrix** — the source is weapon-side. That is anchor semantics.

## Live: the reload test says the same thing in numbers

Handgun `wp0200` (the Continue save now lands with it equipped — Tefa left it that way). 10 Hz
trace, NUM4 HOLD on → NUM5 fire (ammo 13 → 12, `pl00_1100_HG_Hold_Shoot` on layer 4) → HOLD off →
`R`. `pl10_1200_HG_Hold_Reload` played on **layer 3** (104 frames) and `|l_arm_wrist − _101|` went:

```
0.000 → 0.017 → 0.038 → 0.130 → 0.256 → 0.349 → 0.326 → 0.262 → 0.227 → 0.147 → 0.119
      → 0.118 → 0.084 → 0.052 → 0.052 → 0.052 → 0.085 → 0.014 → 0.000   (clip ends)
```

while `|_101 − _100|` stayed 8 mm the whole time and `|Lwrist − Rwrist|` opened to 0.419. **The
weapon joint stayed on the weapon and the hand left it, then a solver put the wrist back to exactly
0.000 in one 100 ms step.** `[verified-live 2026-09-05, n=1 reload, handgun]` If `_101` followed the
hand the distance would never have opened. Static and live agree: **`_101` is an anchor.**

## Live: the ARM kind is not the lever — it cannot even be switched on

NUM6 v0.2, handgun, unaimed and under HOLD, arm index 0 and 1, ~1,200 frames each
`[verified-live 2026-09-05, n=1 per case]`:

- `IkController.setEnable(ARM, true, 0.2f)` through the **direct** route (real ABI, float in place)
  and, when that did not stick, through the **invoke** route (float bits in the 8-byte slot): both
  executed without exception, and **`isEnabled(ARM)` read 0 before, 0 after, and 0 on every frame
  since.** The enable does not take, by either route.
- `setArmTarget(idx, aid + 10 cm, false)` **throws an internal game exception on every call, for
  index 0 and index 1** (`setArmFitTarget` had accepted index 0 yesterday). `ArmStatusList` stays at
  0 entries. `|Lwrist − target|` stays 0.100; the before/after screenshots are identical.
- `getIkTwoArm()` and **`getIkHand()` are null** on the handgun and the minigun, at bind and with the
  weapon held (n=2 weapons, 3 reads).

So the `IkController` ARM path, the two-arm solver and the hand solver are all out. What remains
enabled is **ARMFIT with `UseIkArmFitAsWrist=1` and `UseIkWrist=1`** — "use the arm-fit solver as
the wrist" — on both weapons, unchanged between locomotion and aim. The reading now is that the
wrist is fitted to the aid target through ARMFIT-as-wrist, and that the game rewrites that target
every frame from `AidTargetWorldMatrix`, which is why yesterday's `setArmFitTarget` write was
overwritten before the solver ran. `[hypothesis]` — v0.3 tests it (below).

## Two corrections to the 2026-09-04 entry

1. **The joint at 0.000 on `_101` is `l_arm_wrist`, not the palm `l_weapon`.** The plugin fills
   `l_hand` with the first joint whose name matches, and joint[19] `l_arm_wrist` precedes joint[20]
   `l_weapon`; the log says so on every bind (`l_hand=l_arm_wrist r_hand=r_arm_wrist`). Every
   `|Lhand-…|` figure yesterday and today is wrist-to-joint. It is the better fact: an arm IK's end
   effector is the wrist. **Supersedes** the "palm `l_weapon` sits on it" wording in
   `2026-09-04-first-native-code-…` and dossier §8c.
2. **`getIKLeftArmMatrix()` DOES carry a value** on the handgun in the ready stance — it appeared in
   the same second as `AidJoint` did, equal to the aid position, and stayed through walking and
   HOLD. It was "no value" at bind (weapon not yet equipped) and "no value" on the minigun at the
   NUM7 read. Yesterday's "never has a value" was read from dump-time only. `n=1` each way.

## Also learned, for free

- **The native trigger works:** `InputSystem.setForce(ATTACK=256, true)` held 8 frames under forced
  HOLD fires a real shot (ammo counter 13 → 12, muzzle flash in the capture, `HG_Hold_Shoot` clip).
  Combined with the HOLD latch this is "aim and fire" with no synthetic mouse. `[verified-live
  2026-09-05, n=1]`
- **`R` reloads** (with a partly empty magazine). **Weapon slots: `2` = minigun `wp8700`, `4` =
  `wp2200` (WeaponType 23), `1` no change with the handgun already up, `3` empty on this save.**
  `[verified-live 2026-09-05, n=1 each]`
- **Layer map grows:** 0 locomotion / hold body, 1 arm (`dummy` on the handgun), 2 fingers, **3 =
  upper-body action** (`HG_Hold_Start`, `HG_Hold_Reload`), **4 = shoot overlay**
  (`HG_Hold_Shoot_NoAmmo` flickers first, then `HG_Hold_Shoot`), 5 empty.
- The title → Continue → gameplay route ran clean twice now (n=2) with a verify at each menu;
  `PLAYER BOUND` at +108 s from launch, plugin up at +6 s.

## Not established

- **What consumes the aid target** — ARMFIT-as-wrist is the surviving candidate, not a proven one.
- Whether `getIKLeftArmMatrix`'s value is the solver's input or a readout of its output.
- Why `setArmTarget` throws (no ARM entries to index is the guess; the exception text is not
  surfaced by the invoke path).
- The minigun's `getIKLeftArmMatrix` (no value at the one read).

## v0.3, same night: THE DOCK LEVER IS FOUND — the aid target is a hookable managed getter

Second launch (game closed and relaunched for the plugin swap). v0.3 post-hooks
`Implement.get_AidTargetWorldMatrix` and `Implement.getIKLeftArmMatrix` (both return
`Nullable<via.mat4>` through a hidden return-buffer pointer — at return, `*ret_val` is that buffer:
`u8 HasValue @0`, the matrix at `+0x10`), counts the calls, and on NUM6 adds 10 cm to the returned
translation. Handgun, unaimed and under forced HOLD. `[verified-live 2026-09-05, n=1 weapon,
2 stances, 3 modes each]`:

| NUM6 mode | what is shifted | `\|l_arm_wrist − _101\|` (joint-to-joint, no hook in the read) | `\|Lwrist − Rwrist\|` |
| --- | --- | --- | --- |
| 0 | nothing | 0.000 | 0.080 |
| 1 | `get_AidTargetWorldMatrix` +10 cm | **0.100** | 0.10 |
| 2 | `getIKLeftArmMatrix` +10 cm | **0.100** | 0.10 |
| 3 | both | **0.200** | 0.19 |
| 0 again | nothing | 0.000 | 0.080 |

Identical under HOLD (aiming continued, laser dot up, left hand visibly floating above the pistol
in the capture). **The wrist goes where the returned matrix says.** Three more facts fell out:

- **The game calls each getter once per frame** (~345/s at this room's ~350 fps; my own summary
  adds 1/s). The managed getters *are* the per-frame read path — no native bypass to chase.
- **Mode 3 is additive, and the two call counts are always equal** → `getIKLeftArmMatrix()` calls
  `get_AidTargetWorldMatrix()` inside itself. The chain is: player-side wrist solver →
  `getIKLeftArmMatrix()` → `get_AidTargetWorldMatrix()` → `AidJoint` world matrix. Hook the outer
  one to own the target; hook the inner one to own it for anything else that reads it too.
- **The solver SNAPS, it does not blend**: the 10 Hz trace across the NUM6 edge goes 0.000 → 0.100
  in one sample (≤ 100 ms), and the reload's return was one step too. So "smooth, not snap" (spec
  v2.3 req 1) is ours to provide — trivial, since the hook returns a fresh matrix every frame: blend
  the returned translation from `_101` toward the target over ~0.2 s on dock, and back on release.

**What the dock now is, concretely:** while LG is held, the post-hook returns the *left controller*
pose (from the bridge slots) instead of the joint's, blended in over a few frames; the game's own
solver places the wrist there with its own IK, and HOLD is latched natively (`setForce(64)`) for
the two-hand aim of req 2. On release, blend back to `_101` and drop HOLD. Nothing per-frame of
ours touches the skeleton. That is spec v2.3 reqs 1–2 on the engine's own machinery, and req 3's
"no muzzle pop" is the same blend run backwards.

## Not established, updated

- Whether the *rotation* part of the returned matrix is consumed (only the translation was
  shifted). Matters for a hand that cups the gun at an angle.
- Whether the solver honours a target the arm cannot reach (clamping vs. stretching).
- The VR pose path (bridge slots 3–24) has still never carried real values — headset off.
- The minigun's `getIKLeftArmMatrix` (no value at the one read; the handgun's has one).

## Automation on RE2, scored (§5a)

menu→gameplay **PROVEN** (n=3, verify between steps; one new hazard — an "autosave feature" notice
can precede the title on a boot, and eats the first ENTER) · commands **PROVEN**, now including fire
· character movement PROVEN (yesterday) · **camera NOT proven** (mouse dead, untouched today) ·
self-close **PROVEN** (WM_CLOSE, 7 times). New this session: weapon slots 2/4 and reload from
outside. Driver: `dev-archive/tools/re2drive.py`.

## Evidence

`dev-archive/recon/2026-09-05-arm-kind-and-reload/`: `run8-arm-kind-reload-slots.txt` (launch 1:
bind, dumps, ARM frames, the reload trace, the slot test) and `run9-aid-target-override.txt`
(launch 2: hook installs, call rates, the shift table, the 10 Hz edge trace). Screenshots were
taken but not committed (size); the logs carry every number above.
