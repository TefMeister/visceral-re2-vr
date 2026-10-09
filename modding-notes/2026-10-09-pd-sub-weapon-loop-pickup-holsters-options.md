# 2026-10-09 (home PC, /pd, Opus): sub-weapon loop, pick-up black screen, holster turn, game options

The game was NOT launched. Four builds were made, installed in the Steam game and saved (builds b109 to b112), each
compile-verified only. Nothing here has been run.

## b109 - LG held first keeps the sub weapon out (board 6d)

Tefa, 2026-10-09 00:15: holding LG at a certain controller angle and head direction made the sub weapon go in and out in
a loop (log: Flash Grenade / Matilda alternating every 1-4 s, no RG-first lines).

Read from REFramework's source (praydog/REFramework master, fetched 2026-10-09) `[inferred-static 2026-10-09]`:

- `VR.cpp openvr_input_to_re2_re3`: `SUPPORT_HOLD = is_left_grip_down && !FirstPerson::was_gripping_weapon()`.
- `FirstPerson.cpp update_player_arm_ik`: the dock switches on when `IsHold && !IsReload && (left hand within 0.1 m of
  the grip spot || (already docked && left grip held))`. The grip spot is the right hand plus the PLAYING CLIP's left
  wrist relative to its right wrist.

So with the knife/grenade itself out, `IsHold` can be true and the clip's left wrist can sit near the real left hand:
the dock switches on, the VR layer drops SUPPORT_HOLD, the sub weapon goes away, `IsHold` drops, the dock lets go,
SUPPORT_HOLD comes back `[hypothesis]` (it fits the 1-4 s period and the head-direction dependence, not measured).

Fix (`src/suppress.cpp`): while LG is held FIRST, in play (a player, no menu), the game's own
`InputSystem.setForce(SUPPORT_HOLD = 128, true)` holds the button; let go the moment LG is. setForce was proven from
native code for HOLD on 2026-09-04. RG-first (two-handed aim) is untouched, so docking on guns is unchanged.
Off switch: `cfg::KEEP_SUPPORT_HOLD_ON_LG`.

Log to read: `LG first: SUPPORT_HOLD HELD by setForce (#n, wp, IsHold)` / `... let go ...`, and the existing `weapon:`
lines. Works = holding LG with a grenade shows one `HELD` and no `weapon:` flip-flop. Still loops with `HELD` logged =
the dock is not the cause (or the game reads SUPPORT_HOLD some other way). Not yet known: whether FirstPerson's dock
still steers the left hand onto the grenade clip's spot while it is held (only cosmetic if so).

## b110 - item pick-up: probe + first try (board 6b)

A pick-up is the inventory opened in get-item mode: `GUIMaster.openInventoryGetItemMode`, called from the fsm action
`app.ropeway.fsmv2.ItemGetMenu` `[inferred-static 2026-10-09, type database 2026-09-25]`.

`src/pickup.cpp` hooks that method; while that inventory is up (headset live) every GUI element drawn is logged once by
its game object name (`pickup: drawn <name>`), through REFramework's plugin `on_pre_gui_draw_element` callback, and
`GUIBlackMask` is not drawn (first guess: the VR layer lets it draw untouched, screen-wide, for cutscene fades)
`[hypothesis]`. Outside a pick-up nothing is skipped, so fades keep their black. Off switch: `cfg::PICKUP_HIDE_ON`.

Outcomes: black gone = done. Black stays = the `drawn` list names the candidates; the next build skips the right one,
or, if the list holds nothing screen-wide, the black is the 3D scene not being drawn and the next step is the camera.

## b111 - the holster spots turn with the head (board 6c)

`visceral_bridge.lua poses()` read `vr:get_rotation(0).w`; `VR::get_rotation` returns a `Matrix4x4f`, and the Lua
`Matrix4x4f` type (ScriptRunner.cpp) has no `x/y/z/w` members, only `to_quat()` and friends `[inferred-static 2026-10-09]`.
So the slot fell back to the identity from 2026-10-05 to b110 and the holster spots stayed facing one room direction.
Now `vr:get_rotation(0):to_quat()`. Check: turn the whole body 90 degrees in the room and reach for a hip.

## b112 - game options set once (board 5)

`src/options.cpp`: the first time in play with the headset live (after 120 frames), sets through the game's own
`OptionManager` setters: `set_ControllerRunType(OFF)` (Run Type Hold), `set_ControllerAutoReloadValue(OFF)`,
`set_CameraAimAssistLevel(0)`, and `InputSystem.setOptionToggleRunType(OFF)`. Reads them back; once they stick, calls
`saveSystemSaveData_PC` and writes `reframework/data/visceral_options_set.txt`, so it never runs again and later menu
choices stand. Every launch logs `options at start: run type, auto reload, aim assist`.

Values `[inferred-static 2026-10-09]`: `OptionManager.OnOff` is ON = 0, OFF = 1; `setOptionToggleRunType(OnOff)` ON =
toggle, so Hold = OFF. Aim assist level 0 = off is a `[hypothesis]`: check the options menu after the first run.
Saves backed up first: `D:\claude video game stuff\save-backups\re2-home-2026-10-09-before-options\`.

## Also

- `re2_fw_config.txt` read CHANGED at session start: same content as b108, only re-saved with LF line endings
  (REFramework re-saves its own config). Re-recorded.
