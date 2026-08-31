# `/sr` drop: REFramework's `on_pre_gui_draw_element` ignored `false` returns for nine days — check which revision Visceral is pinned to

**From:** `/sr` cross-project research sweep, 2026-08-31
**Why this is for you:** `visceral_crosshair.lua` (shipped in v0.1.0 on 2026-08-30) hides
`GUI_Reticle` by returning `false` from `on_pre_gui_draw_element`. That is exactly the mechanism
that was broken.

## The finding

`[reported]` — read from the public pull requests, not tested by us.

Returning `false` from `re.on_pre_gui_draw_element` is the documented way to stop REFramework
drawing a GUI element. That stopped working, silently, as a side effect of a callback refactor:

- **Broken by** [praydog/REFramework PR #1503](https://github.com/praydog/REFramework/pull/1503),
  merged **2026-08-19** ("Callback fixes and allow re.on_* callbacks to remove itself").
- **Fixed by** [praydog/REFramework PR #1809](https://github.com/praydog/REFramework/pull/1809)
  (author **ErwinGunsmith**), merged **2026-08-28**.

**Cause**, per the PR description: the Lua result type (`sol::protected_function_result`) has no
explicit boolean conversion of its own, so an inherited templated conversion operator matched the
`if`. The check that was meant to ask *"did the call succeed?"* instead evaluated *"what did the
script return?"* — so a script returning `false` was read as a failed call and the element was drawn
anyway. No error, no log line, nothing wrong-looking in the script.

## Why it matters here specifically

`STATUS.md` records the project's REFramework as **rev `76298bd`**. If that revision sits between
2026-08-19 and 2026-08-28, **the crosshair-hiding feature shipped in v0.1.0 does not work**, and it
will not look broken — it will look like the script is fine and the reticle just is not hiding.
Worth confirming before anyone debugs the Lua.

## Suggested next step

1. Date-check rev `76298bd` against that window (its commit date settles it outright).
2. If it falls inside, update REFramework to a build at or after 2026-08-28 and re-test the crosshair
   toggle in VR before the next release.
3. Either way, this is worth a line in the project's dossier: **a HUD-suppression script that
   "does nothing" should have the framework revision date checked before the script is doubted.**

## Also filed upstream

The general lesson (a truthiness test on a wrapper type reads the wrapper, not the value) is now in
the cross-engine library alongside a near-identical UEVR bug found the same week, and the family-wide
warning is on the RE Engine page:

- [`docs/techniques/README.md` → silent no-ops](https://github.com/TefMeister/flat-to-vr-cross-engine-research/blob/main/docs/techniques/README.md#silent-no-ops-verification-that-cannot-see-the-failure)
- [`docs/engines/re-engine.md`](https://github.com/TefMeister/flat-to-vr-cross-engine-research/blob/main/docs/engines/re-engine.md)
