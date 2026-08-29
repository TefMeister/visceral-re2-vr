# Research index

Every research topic gathered for this project, newest first. Each row links to a self-contained
write-up in `topics/`. Status tags:

- 🆕 **new** — found, not yet acted on by the modding side.
- 👀 **reviewed** — a modding session has read it and factored it into a decision, but nothing shipped from it yet.
- ✅ **incorporated** — directly led to a real change (code, a test, a note) in one of the other five repos; linked below.
- ❌ **dead end** — checked out, didn't pan out; kept for the record so it isn't re-investigated from scratch.

| Date | Topic | Status | Summary |
| --- | --- | --- | --- |
| 2026-08-29 | [Weapon-equipped state surface](topics/2026-08-29-weapon-equipped-state-surface.md) | 🆕 new | The public RE2 class surface for "a weapon is out": `Equipment.<EquipWeapon>k__BackingField`, `implement.Gun`/`Melee`, `IsHold`/`IsReload`, and REFramework's `FireBulletType Camera→AlongMuzzle` flip as the model behavior-change-while-armed. |
| 2026-08-29 | [CAF custom animation framework](topics/2026-08-29-caf-custom-animation-framework.md) | 🆕 new | Working public custom-animation system for RE2 with an RE3-style dodge (= roadmap 15 prior art): DynamicMotionBank loading, MotionFsm2 pause, set_Position+warp root motion — and the armed-state gating it lacks. Plus the layer-speed + getMoveSpeed-hook technique for aim-walk speed. |
| 2026-08-29 | [EMV Engine toolkit map](topics/2026-08-29-emv-engine-toolkit-map.md) | 🆕 new | EMV has zero weapon code — its value is instrumentation: Console, Action Monitor, Hooked Method Inspector, Poser, and the RE2 motion-bank path/ID reference, mapped to the armed-state study each roadmap front needs. |

## How to add a topic

1. New file in `topics/`, named `YYYY-MM-DD-short-slug.md`.
2. One row added to the table above, newest at the top.
3. Update the status tag here as it moves through review → incorporated/dead-end (the modding side should update this when it acts on a lead, so the index reflects reality without the research side needing to poll).
