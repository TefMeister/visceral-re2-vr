Description


Both eyes get the same lighting in VR when REFramework draws with Single Frame Multipass.

Without it, one eye is drawn with the game's automatic brightness and the other without it, so one eye looks brighter than the other.

What it does, every frame:
- holds the game's automatic brightness at each area's own level, so both eyes get the same picture
- switches off the game's per-area colour tint (the blue in dark places)
- sets contrast to 1.2 (the game has 1.0)

Each of these can be changed in the REFramework menu, under "Visceral: steady light".

Caution: Resident Evil 2 in VR is still being worked on. VR can cause motion sickness and discomfort; stop and rest if you feel unwell.


Installation instructions


STEP 1 - The game and REFramework VR

1.1  Resident Evil 2 (Steam) with praydog's REFramework and its VR files already installed and working in the headset.

1.2  In the REFramework menu > VR, set the rendering technique to Single Frame Multipass. (This mod is the fix for that mode; with Two Frame Sequential it is not needed.)


STEP 2 - This mod

2.1  With Fluffy Mod Manager: put this zip in Fluffy's RE2 mods folder, open Fluffy, choose Resident Evil 2 and tick "Visceral Steady Light - RE2 VR".

2.2  By hand instead: copy the "reframework" folder from this zip into the game folder (the folder that contains re2.exe), and say yes to merging folders.

2.3  Start the game. In the REFramework menu, "Visceral: steady light" shows the settings.


Settings


Light: "area" (default) holds each area's own brightness. "hold" uses one brightness everywhere (the "Held EV" slider). "game" switches the mod's lighting off, to compare.

Per-area colour grading: "off" (default) or "game".

Contrast: 1.2 by default; 0 leaves the game's own.

The settings are saved in reframework/data/visceral_steady_light.json.


Requirements


Resident Evil 2 (Steam)

praydog's REFramework with VR

Not needed with Visceral RE2 VR, which already has this fix built in.


UNINSTALL


Untick it in Fluffy Mod Manager, or delete these two files from the game folder:

reframework/autorun/visceral_steady_light.lua

reframework/data/visceral_steady_light.json


Shout outs


praydog - REFramework and its VR mode, which this runs on

FluffyQuack - Fluffy Mod Manager
