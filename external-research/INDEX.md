# Research index

**Last `/gr` pass: 2026-09-02 (estate sweep) — CHECK-IN** (board OPEN block + INDEX + dossier §8/§11; topics not re-read)**.** Inbox empty. One new topic answering the board's main technical risk: **a writable speed lever exists** — public REFramework scripts ship `via.motion.MotionLayer.set_Speed` gated by motion name, and with root motion that moves body, legs and footsteps together. Pointer in `engine-research/inbox/`. Layer index and the awareness half stay unverified.
_Previous: Last `/gr` pass: 2026-09-01 (second pass, estate sweep) — CHECK-IN (one targeted upstream-source question; no dossier or topic re-read). Inbox was empty. One new topic: the FirstPerson ~1s settle carried over from the pr…_

_Earlier the same day — CHECK-IN:_ inbox drained (three files): two status flips applied, and the
REFramework GUI-callback regression closed out — the sweep filed it, the modding side date-checked our
pinned revision, and we are **not** affected. No new web research that pass.

Every research topic gathered for this project, newest first. Each row links to a self-contained
write-up in `topics/`. Status tags:

- 🆕 **new** — found, not yet acted on by the modding side.
- 👀 **reviewed** — a modding session has read it and factored it into a decision, but nothing shipped from it yet.
- ✅ **incorporated** — directly led to a real change (code, a test, a note) in one of the other five repos; linked below.
- ❌ **dead end** — checked out, didn't pan out; kept for the record so it isn't re-investigated from scratch.

> **Inbox drained, 2026-09-01 — and one closed loop worth recording.**
>
> - **The two 2026-08-29 rows above move to 👀 reviewed.** Both were drained into
>   `ENGINE-DOSSIER.md` as a new **§8b**, deliberately kept apart from the verified §8 material and
>   headed *"from public sources, NOT yet verified live"* — everything in it stays `[reported]`.
>   Reviewed rather than incorporated, because nothing has been tested in VR yet.
> - **The REFramework `on_pre_gui_draw_element` regression is closed, and we were never exposed.**
>   The `/sr` sweep filed it after spotting that returning `false` from that callback silently
>   stopped hiding GUI elements between **2026-08-19 (PR #1503)** and **2026-08-28 (PR #1809)** — the
>   exact mechanism `visceral_crosshair.lua` ships on. The modding side date-checked our pinned
>   revision the same day: **`76298bd` is dated 2026-03-11, five months before the broken window**,
>   so v0.1.0's crosshair hiding is unaffected and no re-test was forced.
>   **The standing consequence:** any future REFramework upgrade must land on a build from
>   **2026-08-28 or later**. A dossier HABIT bullet now records the general form — *when a
>   HUD-suppression script "does nothing", date-check the framework revision before doubting the
>   Lua*, because the framework under a script can break the script's documented contract without a
>   single log line.

| Date | Topic | Status | Summary |
| --- | --- | --- | --- |
| 2026-09-02 | [A writable speed lever exists: the motion layer's playback speed](topics/2026-09-02-a-writable-speed-lever-exists-the-motion-layers-playback-speed.md) | 🆕 new | ⭐ Junh2x's Requiem "Better Movement Speed" (ported to RE2 on Nexus) writes `set_Speed(k)` on the player's `via.motion.Motion` layer each `LateUpdateBehavior`, gated on `"walk"`/`"run"` in `get_HighestWeightMotionNode():get_MotionName()`; praydog's `re2_smooth_movement.lua` instead writes the body transform per `UpdateMotion`. Root motion means a playback-rate clamp scales travel, leg cycle and footsteps as one — req 4 by construction, not a blend. Unverified: which layer carries RE2 locomotion, whether awareness reads a separate speed. |
| 2026-09-01 | [The FirstPerson settle bug is a lerp that never reaches zero](topics/2026-09-01-the-firstperson-settle-bug-is-a-lerp-that-never-reaches-zero.md) | 🆕 new | Every Visceral release ships `FirstPerson_Enabled=true`, so this is our users' bug. Upstream sets `wanted_camera_shake = 0.0f` in VR, then only **lerps** `m_interp_bone_scale` toward it at `clamp(delta_time*0.05f, …)`; the snap branch tests `bone_scale == 0.0f` exactly, so it never fires. **The CameraShake slider is not a workaround** — VR never reads it. The consistent fix is to add `vr->is_hmd_active()` to that condition, exactly as the `m_interpolated_bone` branch a few lines up already does. Clean candidate for an upstream PR. One unknown left: `m_interp_bone_scale`'s initial value. Line numbers are approximate — the blob view truncated at 1000 and returned a false negative first. |
| 2026-08-29 | [Weapon-equipped state surface](topics/2026-08-29-weapon-equipped-state-surface.md) | 👀 reviewed | The public RE2 class surface for "a weapon is out": `Equipment.<EquipWeapon>k__BackingField`, `implement.Gun`/`Melee`, `IsHold`/`IsReload`, and REFramework's `FireBulletType Camera→AlongMuzzle` flip as the model behavior-change-while-armed. |
| 2026-08-29 | [CAF custom animation framework](topics/2026-08-29-caf-custom-animation-framework.md) | 👀 reviewed | Working public custom-animation system for RE2 with an RE3-style dodge (= roadmap 15 prior art): DynamicMotionBank loading, MotionFsm2 pause, set_Position+warp root motion — and the armed-state gating it lacks. Plus the layer-speed + getMoveSpeed-hook technique for aim-walk speed. |
| 2026-08-29 | [EMV Engine toolkit map](topics/2026-08-29-emv-engine-toolkit-map.md) | 🆕 new | EMV has zero weapon code — its value is instrumentation: Console, Action Monitor, Hooked Method Inspector, Poser, and the RE2 motion-bank path/ID reference, mapped to the armed-state study each roadmap front needs. |

## How to add a topic

1. New file in `topics/`, named `YYYY-MM-DD-short-slug.md`.
2. One row added to the table above, newest at the top.
3. Update the status tag here as it moves through review → incorporated/dead-end (the modding side should update this when it acts on a lead, so the index reflects reality without the research side needing to poll).
