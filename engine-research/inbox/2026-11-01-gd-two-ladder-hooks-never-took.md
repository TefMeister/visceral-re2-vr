# Gold Digger 2026-11-01: two of the ladder's six camera hooks fail every run, and the log counts them as in

From: `/gd` (inside `/fullscan`), five shelved REFramework logs (2026-10-27 to 10-31).

In `2026-10-31_20-29-45_PC1_re2_framework_log.txt` (lines ~5880-5900):
`Failed to hook 7ff8550b0122: unknown exception` / `Failed to hook function for 'updateCameraPosition'`
(app.ropeway.camera.PlayerCameraController), and the same for `app.ropeway.camera.TwirlerCameraControllerRoot.update`
(`7ff8550b013f`); then `[visceral-native] ladder: 6 camera-controller update hooks in`. The same 4 error lines are in
the 10-27 and 10-29 logs `[measured 2026-10-27..31, n=3 runs]`. Both targets resolve to "actual function" addresses
in a different module from the four that work (`0x7ff855...` against `0x7ff755...`).

Cause of the miscount `[inferred-static]`: `dev-archive/native/src/ladder.cpp` `install_hooks()` calls
`m->add_hook(...)` and does `++A.hook_count` without looking at the result. Effect: `apply()` never runs after
`updateCameraPosition`. The ladder hand-back is verified live, so it may not matter, but a later camera fault there
would be hunted with a wrong count. Proposed row `[PD]`: count only hooks that took (check `add_hook`'s return) and
log the failed names. Small.
