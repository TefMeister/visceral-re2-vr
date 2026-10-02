# The swing is the support hand leaving the gun through the kick, and the VR mod aiming after it (2026-10-02 evening, home PC, Tefa in the headset)

**Measured, every shot, 9 of 9 two-handed shots in three rounds (b043 + per-frame trace), plus Tefa's headset video
(`D:/CCCC/VirtualDesktop.Android-20261002-205101-0.mp4`, 5–11 s; contact sheets in
`dev-archive/recon/2026-10-02-throw-trace/`)** `[verified-live 2026-10-02, n=9]`:

- 6–7 frames after the fire request the LEFT wrist leaves the gun: 7 cm to the left, 2 cm down, 2–3 cm forward (7.5 → 14 cm
  from the gun joint), stays off for 31–33 frames (~0.35 s at 90 Hz), then returns. Identical in all nine shots, walking or
  not, NUM5 (turn block) on or off.
- The RIGHT wrist stays 8.4–8.6 cm from the gun joint throughout. The gun joint itself moves ≤ 3.6 cm (the kick), relative to
  the head joint AND relative to the camera. The aim state stays up. The head does not move relative to the camera.
- In the video the gun visibly points left/up for a moment after each flash and comes back — a ROTATION about the right
  hand, not a displacement, which is why the position trace of the gun joint shows nothing.
- **One-handed (left grip not held): no swing** (Tefa, 3 shots) `[verified-live 2026-10-02, n=3]`.

**Reading** `[verified-live for the mechanism; the "why the hand leaves" part is inferred]`: praydog's `RE8VR::update_hand_ik`
aims a two-handed gun along the line right hand → the animated left-hand socket, read every frame (village-scope §9cf/§9cg,
the 2026-09-30 note's Village link). The kick (shot motion 1100, additive on layer 4) moves the left arm off the gun on our
relaxed-walk base pose, so for ~0.35 s the socket is 7 cm left and the mod swings the gun after it; when the hand returns the
gun snaps back. This is the Village rifle jerk, same code path, confirmed on RE2 by the one-handed test.

**Why only on the splice** `[hypothesis]`: an additive kick is authored over the stock aim pose, where its joint-angle deltas
keep the support hand on the gun. On the relaxed OFF_GazingWalk base the left arm is in a different configuration (IK brings
the hand to the gun from elsewhere), so the same deltas displace the hand. b016 (stock aim-walk arms grafted in) changed the
direction of the throw without removing it, which fits a different base configuration giving a different displacement.

**Withdrawn tonight:** the 09-30 "the throw is the aim state dropping out" reading (the hold stayed 1 through every shot; the
09-30 drops were the grip relaxing) `[disproved 2026-10-02]`; the forbid-aim (near-object) rule and the aim-turn-on-the-spot
(`HG_Wheel`) as causes `[disproved 2026-10-02]`; NUM5/NUM6 levers retired.

## Fix candidates, cheapest first

1. **Data: strip the left-arm bone tracks from the kick clip (pl10/pl00 `1100–1102 HG_Hold_Shoot`) in our loose motlist**, so
   the kick moves the right arm and the gun only; the left hand stays where IK put it. Our own tooling (`motlist_graft_arms.py`
   family) already moves bone tracks. Needs one headset check. ⚠️ Flat players see a one-armed kick — ship it in the VR list
   only.
2. **Freeze the socket during the kick** — the Village fix, in REFramework's `RE8VR.cpp` (patched build, 2026-09-21), ported to
   the pd-upscaler branch; or upstream to praydog. Bigger: a REFramework rebuild.
3. **Make the mod treat the gun as one-handed for ~40 frames after each fire request** (`re8vr.is_holding_left_grip=false`) —
   `re8vr` is not reachable from our scripts (nil, 2026-09-30), so this needs the native side too.
4. Re-order IK after the additive, or re-pin the wrist joint after animation from Lua — unexplored.

## Tooling that made it

`visceral_hold_exit_probe.lua` (b043): `[visceral_trace]` one line per frame for 240 frames after every `Equipment.requestFire`
(gun, both wrists, head vs camera; hold; layer-0 motion); `dev-archive/tools/plot_trace.py` summarises (matplotlib not
installed here, numbers only). Desktop capture is impossible in VR (black window even with the mirror setting off), but the
Virtual Desktop headset recording in `D:/CCCC` is frame-exact and ffmpeg contact sheets (`fps=15, tile=5x3`) make it readable.


## Later the same evening: two fixes tried, neither is it — and the steering is NOT praydog's Lua

1. **Kick without its left-arm tracks (b045, `motlist_kick_strip.py`)** — 21 left arm/hand tracks switched off in 1100–1102 and
   1120, both lists, 84 bytes. Tefa: *"it swings a little at a different angle than before, but still a big swing … left
   hand also behaved differently"*. Trace: the left wrist still leaves the gun by ~7 cm for ~30 frames, now down/back while
   the gun kicks up 5 cm `[verified-live 2026-10-02, n=3]`. So the left hand is not dragged off by its own kick tracks: the
   gun kicks and the left hand does not follow it, although `visceral_lefthand_hold.lua` held the left-hand IK at 1.00 the
   whole time (`IKEnable=true cur=1.00`, forced 72/s while aiming). Reverted; tool and both versions kept in
   `D:/RE2 REFramework builds/kick-strip-2026-10-02/` `[disproved 2026-10-02]` as the fix.
2. **One-handed for 0.5 s after each shot, patched into praydog's `re8_vr.lua`** — never ran: that script returns at line 6
   on anything but RE7/RE8 (`if not is_re7 and not is_re8 then return end`). **So on RE2 the two-handed aim is NOT the
   Lua/`RE8VR.cpp` path the 09-30 note assumed** `[verified-live 2026-10-02, the patch's load line never printed]`; it is
   REFramework's native RE2 VR/FirstPerson code (no `plugins/` folder in the test copy, no other script reads the grip).
   Patch reverted (original file hash 8a9076514f0e back in place).

**Where it stands** `[hypothesis]`: during the kick the left-hand IK target (the gun's support point) and the gun the VR
code pins to the controller disagree for ~30 frames — the IK solves to the animated, kicked gun before the native VR code
moves the gun to the controller, so the hand lands off the real gun and the native two-hand aim follows the hand. Next
needs the native side: read praydog's RE2 two-handed aim in the REFramework source (pd-upscaler a24c3459) — which
socket it steers by and when it runs relative to the game's IK — then either re-solve the left hand after it, or freeze
its socket for the kick as Village did. That is FABLE-level reading.
