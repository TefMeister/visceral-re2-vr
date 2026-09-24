# Item 22 first run: the aim-walk splice works, but the left-hand IK starves while walking aimed (2026-09-24, home PC, flat)

Tefa asked to separate the aim/ready-to-fire function from the body posture it triggers, so
walking with the weapon and walking while aiming share one movement animation. That is exactly
item 22, whose splice was built on 2026-09-06 (`2026-09-06-item-22-splice-script-built-and-the-rt-container-decoded.md`)
and never run. This session ran it.

## What was deployed

`splice-out\…\pl10\list\hdg\base_hdg_hold.motlist.524` (Claire, handgun; 740,928 bytes, sha256
`b3a3a1eb4ffc3d0b…`) copied to `<RE2>\natives\STM\sectionroot\animation\player\pl10\list\hdg\`.
No file existed at that loose path, so **deleting it restores stock**. REFramework's loose-file
loader was already on and logged the file as taken (`LooseFileLoader2 … BASE_HDG_HOLD.motlist.524`,
11:51:07).

## Result

1. **The splice works** `[verified-live 2026-09-24, n=1 launch]`. Tefa: *"Claire now has the same
   aim posture"*. The log agrees: while `hold=1` and walking forward, layer 0 plays
   `pl10_0190_KFF_GazingWalk_F_Loop` from **slot id=120** (the old `StrafeL_F` slot), bank 2.
   So **the game looks motions up by slot number, not by name hash** — open question 1 of the
   06 note is answered. The file was not rejected, so the first-entry `motSize ≠ 0` did not matter
   (open question 2 answered). No crash.
2. **New defect: "when RG is held, hands twitch every second interval or so, just a quick flicker
   constantly"** (Tefa, `[reported 2026-09-24, n=1]`).

## What the log shows about the twitch `[measured 2026-09-24, n=1 launch]`

The plugin's 1 Hz summary counts how often the game calls the hooked left-hand aid-target and
left-arm IK functions (`hooks(aid=N ikL=N)`). Grouped by the layer-0 motion playing while
`hold=1`:

| layer-0 motion while aiming | seconds | avg IK calls/s | seconds under 10 calls |
| --- | --- | --- | --- |
| `pl10_0120_GG_StrafeL_F` (a vanilla aim motion) | 8 | 72.1 | 0 |
| `pl10_0160_HG_Hold_Idle_Loop` (standing aimed) | 9 | 43.9 | 1 |
| `pl10_0190_KFF_GazingWalk_F_Loop` (**our spliced walk**) | 37 | **11.5** | **26** |
| `pl10_0194_KFF_GazingWalk_R_Loop` | 2 | 42.5 | 0 |

Many spliced-walk seconds read `aid=1 ikL=1`: **the game solves the left hand onto the gun about
once a second instead of every frame.** That is the twitch: a RATE fault, which matches how
Tefa describes it (a regular quick flicker, not a wrong pose). The plugin's own `|Lw-nat|`
stat climbing to ~10 m during those stretches is a side effect: its "natural" reference is only
refreshed inside those hooks, so it goes stale. It is not a separate bug.

**Reading:** `[hypothesis]` the vanilla aim loops carry something the walk loops do not — most
likely an IK-enable curve or clip event inside the motion blob — that tells the game to run the
left-arm IK every frame. The splice copies the walk blob byte for byte, so that data is gone.

## Which step runs last

The IK request is decided while the motion is evaluated (the data inside the clip). Our hooks
run after that, as observers. So a plugin tweak to the hooked functions would be annotating, not
fixing. The two upstream levers are:

- **(a) Data:** diff the extra tracks/events of `0110–0115`/`0120–0136` against `0190–0198`, and
  graft the IK-carrying track into the spliced entries (a `motlist_splice.py` extension).
- **(b) Code:** find what the IK controller reads to decide it runs this frame (the `ARMFIT`/wrist
  path; `IkController` dump shows `UseIkWrist=1 UseIkArmFitAsWrist=1 isEnabled(ARMFIT)=1`) and
  hold it on from the plugin while `hold=1`.

(a) is static and needs no launch to prepare. Both must prove their own effect: the 1 Hz
`hooks(aid ikL)` count should read ~72/s during spliced walk seconds.
