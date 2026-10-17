# 2026-10-17 evening — Menu flicker: the open frame (b179, worn: gone) and the inventory's close frame (b180, unworn)

Home PC, Fable, desk; the game was Tefa's to run. Read against REFramework's source at the installed build
(`D:\ref-76b\src\mods\VR.cpp`, `FirstPerson.cpp`) and Tefa's own 23:07–23:10 log with `menu_probe` on.

## Going in (b179) — gone, Tefa 23:15

The bridge's "FirstPerson used" flag is read at UpdateHID, before the menu opened, so in the open frame it still said
"driving" and the hold wrote the FULL pin, while the VR layer was already in its menu regime and added the headset turn
on top: one frame with the head's turn doubled. b179: while a menu is open and the fresh read (GUI state + camera type)
says the menus own the camera, the fresh read wins. The close keeps the bridge (b108). Worn: *"main menu flicker gone"*
`[verified-live 2026-10-17, Tefa]`.

## Going out of the inventory (b180) — the frame itself, from the probe `[verified-live 2026-10-17, n=2 closes + 1 pause close]`

```
f7046 LockScene.post       open=1 gui=1 fp=0  rot= 0.000 0.740  0.000 -0.673   (held, stripped)
f7047 LateUpdateBehavior   open=0 gui=0 fp=0  rot= 0.000 0.740  0.000 -0.673   (closed this frame)
f7047 LockScene.post       open=0 gui=0 fp=0  rot= 0.000 0.740  0.000 -0.673   fov 51.2 -> 85.3
f7047 BeginRendering.pre   open=0 gui=0 fp=0  rot=-0.064 0.861 -0.045 -0.503   <- yaw ~24 deg off, for this frame only
f7048 LateUpdateBehavior   open=0 gui=0 fp=1  rot=-0.072 0.736 -0.030 -0.672   right
```

The pause close (f5077) goes straight to the right view at BeginRendering. The difference, read in `FirstPerson.cpp`:
the inventory switches to its own camera controller (`InventoryCameraController`); while it is open FirstPerson's
`on_update_camera_controller2` records the ACTIVE controller's turn as `m_last_controller_rotation`, and in the first
allowed frame `update_camera_transform` interpolates `m_rotation_offset` from where it was (the inventory camera's
direction) toward the player's view: the first step is ~24 deg short. The pause menu keeps the player's controller, so
nothing to interpolate from. That write lands between LockScene POST and BeginRendering PRE, after every game-thread
write of ours; the hold's own measure ("swung up to 1.4 deg") never saw it because it measures at LockScene PRE.

b180: for the first `MENU_CAM_CLOSE_RENDER_FRAMES` (3) frames after a menu reads closed, `render_point()` writes the
FULL held view (`pin x inv(H0) x H`, H from the bridge) into the camera transform and the root joint at BeginRendering
PRE — after FirstPerson's write and after the VR layer's camera pass, so it is the view that renders whatever regime the
VR layer picked. Cost: H is one pose old for three frames (b103's "shaky" was this for the whole hold; here it is three
frames against a 24 deg jump). Not during the open hold, not for jack holds.

## If b180 does not do it

The renderer may use something else than the transform/joint at this point (FirstPerson also re-runs its camera write
from the render thread after submit, VR.cpp ~1684). Next: log at BeginRendering PRE what the root joint reads before and
after our write, and compare with the eye.

## Also recorded

- `menu_probe` stops after 30 dumps (`g_dumps >= 30`): with 18 pause opens first, only two inventory closes were dumped.
  Raise it or make it per-menu-type before the next probe run.
- Tefa 23:15: *"rain is now fixed, main menu flicker gone, and only the inventory exit flicker is present"*.
