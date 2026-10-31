# 2026-10-31 — Village's reload rules brought back into RE2 (and RE3)

Tefa, 2026-10-31: *"could you look at re8 right now and how the reloads work and implement this system into re2 and 3 as
best as possible?"*

Village's reload is itself RE2's (RE2 → RE7 RELOADED → RE8), so this is the newer Village rules coming home, built on
RE2's own magazine joints, hand holds and racking. Same code in `visceral-re3-vr` (CQBR as the long gun there).

## What came over `[compile-verified 2026-10-31]`, unworn in RE2 / RE3

| Village rule (worn there) | Here |
| --- | --- |
| A loaded slide gun keeps one round in the chamber when the magazine comes out; it fires once with the magazine out, no slide lock-back (b012/b014, "wow this is so cool") | `CHAMBER_ROUND`. The game's own count is never written (RE2's ammo writes are fragile, dossier B.4): the fire gate `reload::fire_blocked()` lets one shot through, `watch_chamber()` sees the count drop, the HUD read shows 1 then 0 |
| The same magazine back in holds only its own rounds; a pouch magazine is topped up | Falls out of the above: the rounds never left the count, so the same magazine back = no top-up (`seat_done`); a pouch one = `top_up()` as before (the dropped one's rounds come back with it) |
| Chamber fired empty with the magazine out = a rack once a magazine is back | `g_dry` → `rack::need` in both seat paths |
| Handguns: LG holding the magazine + right B = it comes out INTO the left hand (10-30) | `seated_controls()`: B with LG held and the drawn left wrist within `MAG_GRAB_M` (0.15, first guess) of the magazine joint; otherwise B drops it as before. Every B press logs the distance |
| Long guns: LG pressed at the magazine, then LT, pulls it out; B does nothing (10-25, "unbelievable") | `MAG_LONG_WP` = LE 5 (RE2) / CQBR (RE3) |
| The same magazine goes back only with LT; a pouch one seats by itself (10-31) | `g_lt_to_seat`: LT pressed or held at the magwell, never the pull's own LT, not within 0.25 s |
| Ammo counter shown the moment a magazine comes out (b016) | `show_ammo_counter` on every take-out |
| Slide racked any time, a rack with none due is cosmetic (b012) | `RACK_SLIDE_ANY_TIME`: only on a fresh LG press within `RACK_ANYTIME_HAND_M` (0.08) of the slide joint, so a two-handed pistol grip (LG held, hand under the gun, inside the old 0.20 m) is never taken; `complete()` skips the game's reload-end calls when none was due |

Ghost Hand: the magazine now logs `mag` start (pouch grab or pull-out), done (insert) and fail (let go) with
`mag_closest_m`.

## Not brought over

- The hand drawn ON the magazine through the insert until LG is let go (Village's IK target writes): RE2 draws the hand
  through REFramework's own two-hand grip and our hand poses; a separate job if wanted.
- Village's grip-zone spheres, revolver drum and GM 79 by hand, the game's own empty-click sound (unworn in Village).

## Installed

Home PC 2026-10-31 17:1x. RE2 `visceral_native.dll` db1d6f6a (saved build `manual reloads/v0.2.0-b182`), old file in
`D:/Visceral build versions/_backup-2026-10-31-*-before-re8-reload-rules/`. RE3 3b5c1a14, old file (a9fae9a8) in
`D:/RE3 REFramework builds/_backup-2026-10-31-*-before-re8-reload-rules/`.

## What to look for in the headset (one gun is enough to start)

1. Matilda, loaded: B alone = magazine falls; the counter shows 1; RT fires once, then clicks.
2. Two-handed grip (LG) + B = the magazine lands in the left hand; let go = it drops; bring it back + LT = in, same rounds.
3. LE 5 (RE2) / CQBR (RE3): LG on the magazine, LT = it comes out into the hand.
4. LG pressed right on the slide of a loaded pistol: the hand takes the slide; a two-handed grip must NOT jump to it.

Log lines: `reload: right B ... the left hand X m from the magazine`, `reload: into the left hand ...`, `the chambered
round was fired`, `rack: LG, left hand X m from the slide (need 0.08): HAND ON (no rack due: cosmetic)`.


## First wear, and the hand on the magazine (2026-10-31 evening, builds.py b183)

Tefa, Lightning Hawk: *"mag does land in my hand but because the LG also grips the grip handle, mag just gets stuck through
the right hand. is is possible to in this game also use all the windiw view tools and hold on to the mag that is in the gun
and not the grip itself? so when inserted or ejected, hand travels with the mag."*

The log agreed `[measured 2026-10-31, n=3]`: after B with LG held, the left hand stayed docked on the grip (the VR layer's
aid-joint dock, sticky while LG is held), so the magazine in it never came closer than 0.111 m to the magwell (needs 0.06).
Pouch magazines went in fine (pressed away from the gun, so never docked). The B-into-the-hand rule itself worked.

b183 (`[compile-verified 2026-10-31]`, installed, unworn) gives the reload the last word on the left hand, through the
same dock socket the slide rack uses (`rack.cpp reload_socket`, after the rack's own socket each frame):

- **LG pressed at the seated magazine** (within 0.15 m of the spot the hand takes on it) = the hand docks ON the magazine
  instead of the grip. Holding it like that still aims two-handed (steer on).
- **B (handguns) / LT (LE 5)** = the magazine slides out with the hand riding on it (right hand aims alone while it moves).
- **Out:** the hand is free (socket owned, inactive: no grip dock while a magazine is in it) and carries it.
- **Going in:** the hand rides up with it and stays on it until LG is let go (Village's 2026-10-24 rule).
- Where the hand sits: the magazine-in-the-hand hold undone (so the "Magazine in the hand" sliders still count), moved
  `MAG_DOCK_DOWN_M` 0.05 m toward where the magazine comes out, plus a new slider set in the hand-tuning menu:
  **"LEFT hand on the magazine in the gun"** (hand move/turn; bridge array 539 -> 604, slots at 540).
- The any-time slide grab is not taken while the hand is on the magazine.

Log lines: `LG pressed with the magazine seated: the hand X m from its spot on the magazine`, `rack: the reload has the
left hand (docked on the magazine | free, a magazine in it)`, `the left hand docked on the magazine (state): x y z`.

**RE3 is not done:** its VR layer is the older socket (version 1, no "owned"), which cannot keep the hand off the grip
while a magazine is in it. It needs RE2's VR-layer socket changes rebuilt for RE3 first.

Old RE2 files: `D:/Visceral build versions/_backup-2026-10-31-*-before-hand-on-magazine/`.


## b183 worn, b184 (2026-10-31 evening)

Tefa: *"the mag works, it now comes out with LG and B and stays in my hand. problem is that grip handle is still a docking
point as well, and docking hand on the mag turns the weapon in right hand and left hand motion controller can move the gun
around"* `[verified-live 2026-10-31, headset, n=1]` for the magazine into the hand.

b184 (`[compile-verified 2026-10-31]`, installed, unworn): on handgun-type magazine guns the magazine is the ONLY left-hand
dock spot (`MAG_ONLY_DOCK`: the socket says "owned" whenever the reload has nothing else on, so the VR layer never docks on
the grip handle; the slide's own socket still works), and the hand on the magazine never turns the gun (steer off: the
right hand alone aims). The LE 5 keeps its grip dock.

Same build, the lighting: Tefa decided RE2 is developed on Single Frame Multipass without DLSS (config already so:
`VR_RenderingTechnique_V2=2`, `TemporalUpscaler_Enabled=false`) and the eyes differ. `exposure.cpp` now also writes the
Disable and the pinned limits in a post-hook on `app.ropeway.posteffect.ToneMapController.lateUpdate`, Village's cure.
