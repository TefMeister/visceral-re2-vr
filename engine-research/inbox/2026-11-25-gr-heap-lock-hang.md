# /gr 2026-11-25: REFramework's 11-17 startup deadlock fix is not in our build

Cross-project from Village (its b164 freeze). kananlib, REFramework's helper library, fixed a deadlock on 2026-11-17:
a game thread frozen while holding the process heap lock hangs the next allocation, "an intermittent REFramework
startup hang in a game that was loading". RE2 runs the same `pd-upscaler` build as Village (commit 76298bd,
2026-03-11, from the RE2 log of 2026-11-23), older than the fix. Nothing has frozen in RE2 for this reason as far as
the notes say; a line for the dossier: "a hang with the game idle may be REFramework's own thread freezing; fixed
upstream in nightly 01441". [reported]

Topic: visceral-re8-vr `external-research/topics/2026-11-25-kananlib-heap-lock-fix-startup-hang.md`.
