# 2026-09-06 — The forearm's straight lines: my submesh diagnosis was wrong

**Supersedes:** `2026-09-06-hd-hands-nails-from-the-rig.md` § "Still open" (the paragraph beginning "The straight
lines on the forearm"), and the same claim in `claude-memory/status/visceral-re2-vr.md`.

## The claim I made, and why it was wrong

At 17:48 I wrote that most of Claire's bare forearm is drawn by the **jacket** submesh rather than the skin
submesh, so our 4K repaint stops at the submesh boundary and that boundary is the straight band across the arm.
I took it from one line of the recon dump — `pl1000_Jacket_Mat ... forearm faces=8125` against the skin
submesh's 798 — without checking what those faces actually are.

They are the sleeve and the wrist band, not bare skin. Measured properly, per centimetre along the forearm from
the wrist joint `[measured 2026-09-06, tools/blender/forearm_seam_recon.py]`:

| | skin submesh (`Body_Mat`) | jacket submesh (`Jacket_Mat`) |
| --- | --- | --- |
| left forearm | 0 → 13.7 cm, continuous | 0 → 2.2 cm (the red beaded band), **nothing 3–10 cm**, 11 cm → up (sleeve) |
| right forearm | 0 → 12.6 cm, continuous | nothing below 10 cm, 10 cm → up (sleeve) |

So the bare forearm is **entirely** on the skin submesh, which our repaint does cover, from the wrist to where the
rolled sleeve begins. There is no submesh boundary on bare skin for a seam to fall on. The claim is
`[disproved 2026-09-06]`.

The first run of that script also gave nonsense (46 cm "along the arm", 31 cm arm radius) because it selected
every `*_arm_*` bone, humerus and clavicle included. Corrected to the forearm chain only.

## What the cause actually is: still open

Two candidates, neither tested:

1. **A texel-density step between forearm UV islands.** The baked micro-relief is tiled at a fixed 10 tiles across
   the whole 4K texture, so an island that covers more surface per texel gets visibly coarser grain than its
   neighbour. That would look exactly like Tefa's screenshot — grainy one side of a line, smooth the other.
   `[hypothesis]` Testable statically: measure UV area per unit surface area for the forearm faces and look for a step.
2. **The dorsal/palm pore step.** Pore relief runs at 30 % on the palm side and 100 % on the back, feathered over
   about 4 mm. On the forearm the dorsal classification comes from a ray test, and a hard split there would draw a
   line running ALONG the arm — which matches the second line Tefa circled, but not the band across it. A note from
   14:20 already recorded "the wrist 'line' was the 30 % → 100 % pore step", so this mechanism has bitten once.
   `[hypothesis]`

A Blender render of the bare forearm at 1800 px was inconclusive: the lighting rig is tuned for a hand and the arm
came out too dark and too small to judge. A forearm-specific light and framing is the next static step.

## Why this does not block the bracelets

Tefa's idea — cover the lines with bracelets — is indifferent to the cause, which is most of its appeal. The
measurements above are what it needed anyway: **bare skin from the wrist to 12.6–13.7 cm, sleeve from 10–11 cm,
the existing left band at 0.1–2.2 cm**, so a new band belongs between roughly 3 and 10 cm from the wrist joint.
