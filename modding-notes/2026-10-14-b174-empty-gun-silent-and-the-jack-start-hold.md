# b174: no gunshot on an empty gun, and the camera held at a jack start (2026-10-14 evening, home PC, Fable, desk)

**Build:** `visceral_native.dll` sha256 `efe4787a…`, installed in the Steam game at 21:50, UNWORN. The b173 files are in
`D:\Visceral build versions\_backup-2026-10-14-before-b174\`. Game not launched; nothing here has been run.
`[compile-verified 2026-10-14]`

Tefa's evening list (21:20): *"gun still makes a gunshot sound when empty and rt is pressed"* and *"climbing ladders,
pulling switches and pushing cupboards still makes the camera jump to hmd direction and quickly back to the right
position"*. Two desk fixes, each built to prove its own effect in the log.

## 1. The empty-gun gunshot: the Fire trigger was still being raised

`src/fire.cpp` raises three of the game's own switches on RT without RG (2026-10-07 note). On an empty gun:
- b142 turned switch 1 (the ATTACK order) off at the press;
- b155/b156 skipped `Equipment.requestFire` and even muted our click -- and Tefa still heard a gunshot
  (`[verified-live 2026-10-11, n=1]`).

What every one of those builds still did at the press was **switch 2, `SurvivorUserVariablesUpdater.set_Fire(true)`**.
On 2026-10-07 the same note recorded that switch 2 without the gun's YES makes the FSM *"play the real shoot clip ...
and no round is spent"* -- and the gunshot is that clip's own sound event. On top of that, the b142 gate was undone one
frame later: the in-flight branch re-raised the order every frame "until the shot ran", which on an empty gun is never.

**b174:** an empty gun (0 rounds, magazine out, or a rack needed; `cfg::FIRE_EMPTY_SILENT`) gets **no Fire trigger**, and
the order is never re-raised for that pull. Our own guns (manual reload) get no order either, as b142 meant; the game's
own guns keep the order so the game does what it does on empty. The click still comes from `reload_block::frame()`.

Log line, once per empty pull (first 30): `fire: pull #N on an EMPTY gun (bullets 0, magazine out X, rack needed Y, our
reload Z): Fire trigger NOT raised, order off -- a gunshot now is NOT from this plugin's press`. Then the usual
`fire: pull #N bullets 0 -> 0 | requestFire 0, Gun.executeFire 0, Equipment.executeFire 0` when the window runs out.
- Gunshot gone: the Fire trigger was it `[hypothesis -> verified when worn]`.
- Gunshot still there with those lines in the log: the sound comes from somewhere this plugin never touches at the
  press (the VR layer's own input mapping of RT -> ATTACK with the gun lowered; or the aimed path, RG held) -> ask
  Tefa whether it was with RG held or not; next dig is the ATTACK input itself (`suppress.cpp` already clears bits).

## 2. The jack-start flick: the last FirstPerson view, held stripped, while FirstPerson is aside

The 2026-10-08 reading of REFramework's source (the menu note) said: a jack probably switches the camera type away from
PLAYER for a frame or two; FirstPerson steps aside; the VR layer takes the camera as it finds it (FirstPerson's last
camera, headset turn already inside) and multiplies the headset turn on again -> the head's turn relative to the body
applied twice, a flick that differs every time. `ladder.cpp` never saw those frames: its hold returns early whenever
`S_FP_USED` reads 0.

**b174 (`src/menu_body.cpp`, "jack"):** while `JackDominator.Jacked` is true and the bridge says FirstPerson is not
driving, the last FirstPerson view is pinned and written **stripped**, exactly as the menus are since b100
(`pin x inv(H0) x inv(R_off)`, position pinned). The hold ends the frame FirstPerson drives again (no settle wait: the
body is moving) or after `JACK_CAM_HOLD_FRAMES_MAX` = 6 frames, whichever is first; after that the jack is left to
ladder.cpp's own hold, as before. No second hold in the same jack. The menu hold is untouched.

Log lines (first 8 jacks): `jack: FirstPerson stepped aside at a jack (gui 0, camera type N): the last FirstPerson view
is held, stripped of the headset turn` then `jack: hold off after K frames (FirstPerson drives again; S stripped + P
plain writes; camera type 0)`.
- No `jack:` line at all on a ladder / cupboard / switch: FirstPerson never steps aside at a jack, the 10-08 reading is
  wrong for jacks, and the flick is something else (ladder.cpp's own capture frame?) -> `MODEL: FABLE`, the ladder
  probe route from the board row.
- `jack:` lines and the flick gone: cause confirmed.
- `jack:` lines and the flick stays: the frames are right but the stripped write is not what the VR layer renders at a
  jack (as b100-b108 found for menus: the bridge flag lags) -> compare `camera type` in the two lines; try the fresh
  camera-type read instead of the bridge flag for jacks.
- `cap reached` in the hold-off line: FirstPerson stays aside for the whole jack; then the ladder hold is the one to
  make stripped-aware (a bigger job).

## Not touched tonight

- **Title rain starting at a random moment**: the refusal text in every log is only `Invoke threw an exception`; the
  gate is inside `requestEffectInternal` @0x141783b40 and needs the disassembly read (the x64 tools in
  `dev-archive/tools/re-engine/` were repointed tonight: the D: test copy is gone, the EXE is the Steam game's and the
  type dump is the 2026-09-25 full backup's). See the board row.
- **Menu / inventory flicker**: needs Tefa's headset recording for `flicker_find.py` (board row 6a). Nothing to do at
  the desk.

## Housekeeping

- `builds.py` numbering: b169-b173 were installed by hand (D: backups) and never snapped, so the tool's next number
  would be b169 while the board is at b174. Left as is tonight; the session-start "matches no saved build" line is this,
  not a stray file (the installed DLL hash matched b173's `3bca152e` before tonight's install).
