# Reload bundle 2 (slide rack, shotgun shells, pump) and the grenade on RG (2026-10-10, home PC, /ms, Fable)

## The grenade / sub weapon on the right grip -- DONE (b134 -> b139, worn)

Tefa's ask: RG never aims (the spread tiers handle accuracy now); RG at a back spot brings the knife/grenade out while
held, release puts it back, RG + RT + a throwing motion throws. What it took, each step worn by Tefa:

| build | what | result |
| --- | --- | --- |
| b134/b135 | the plugin cleared HOLD / SUPPORT_HOLD / ATTACK in the game's button record at UpdateBehavior pre and set SUPPORT_HOLD by `InputSystem.setForce` at the back spot | the clears ran (log) but the game still readied the sub weapon from LG and never from RG; `setForce` froze the picture for a second and left the menus scrolling `[verified-live 2026-10-10]` -- these three buttons are not read from that record at that point |
| b136 | REFramework patched (`VR.cpp openvr_input_to_re2_re3`): the three buttons taken from the plugin's exported `visceral_vr_buttons()`; HOLD sent with the latch | out on RG = yes, LG no longer readies it, RG never aims = yes; but letting go THREW it, RT dropped it at the feet |
| b138 | v2: HOLD only pulsed on the swing, ATTACK never with a grenade | the swing-release threw, a slow release still "let go and it exploded" |
| b139 | found it: praydog's `re2_vr_grenade.lua` throws on the RIGHT GRIP's release at the controller's speed (zero speed = at the feet). Gated: release faster than 1.5 m/s = thrown, slower = put back (log `[visceral-grenade]`). The HOLD pulse removed. | **Tefa: "it works as it should! releasing slowly puts it away, throwing and releasing in mid air lets go of it, feels superb"** `[verified-live 2026-10-10]` |

Facts to keep: with a grenade readied, the game's HOLD is the throw and ATTACK drops it at the feet; the VR mod's own
grenade script is what makes a swing throw. The knife attacks by its physical swing (praydog's `re2_vr_melee.lua`).
Spot: `cfg::BACK` = 12 cm right, 62 cm down, 30 cm behind the headset (as low as the pistol holster). REFramework
patch `2026-10-10-re2-sub-weapon-buttons-from-plugin.patch` (dinput8 77a58b0f, the b135 one kept in the game folder's
`_backup-2026-10-10-dinput8-b135`). **b139 = BASELINE 3** (`D:/Visceral build versions/GOLDEN/2026-10-10 BASELINE 3 ...`).

## Bundle 2 -- b140 INSTALLED, UNWORN `[compile-verified 2026-10-10]`

Spec: Andyalpa's ext_2 (slide) and ext_4 (pump, shells), read, rewritten; his per-weapon numbers in `reload_data.h`.

- **Slide rack / pump (`rack.cpp`)**: LG pressed with the left wrist within 20 cm of the slide (pistols: joint `_01`,
  LE 5 `_02`) or the fore-end (shotguns `_01`) puts the hand on it; LT pulls the joint back along its local Z (his
  rest/parked/back per weapon, 7 per s), letting LT go returns it (5 per s) and chambers (`Gun.executeEndReload`, plus
  `executeEndEject` on a pump). Needed, and the gun will not fire until done (reload_block, dry click): a magazine
  into an EMPTY gun; a shot on a pump gun with another shell still in it; shells into an empty gun. The last round
  fired locks a pistol's slide open (cosmetic). Written at LateUpdateBehavior post, PrepareRendering post and
  BeginRendering pre.
- **Shells (`reload.cpp`)**: W-870, Remington 870, M3, Lightning Hawk shotgun: LG at the left-hip pouch = the gun's own
  hidden shell part (31) shown on joint `_04` at the left wrist; bring it within 22 cm of the loading port (his
  `mag_exit` for the W-870) = +1 (`Inventory.reloadMainSlot(1)` inside a Commit, `executeEndReload`); LG let go = put
  back. Empty pouch or full tube = a long buzz.
- **The game's own pump (`pump_native.cpp`)**: for 2.5 s after a shot the weapon's and the player's motion layers whose
  clip name says pump/cycle/reload/eject/rack on a shotgun (his classifiers) are sent to their last frame once they
  have played 8-20 % (`TreeLayer.set_Frame(get_EndFrame)`; this build has no `set_Weight`); the spent shell's
  `ShellCartridgeController.generate` is held back and replayed (`request()`) when our pull-down reaches the back;
  Wwise trigger IDs heard in the window are LOGGED (no name lookup in this build) -- blocking the pump sound is the
  next step once the IDs are known.
- Also: `joints.h` split out of reload.cpp (shared helpers).

### What proves it (Tefa, headset)
1. Matilda emptied, magazine in: the slide sits open, the trigger clicks; LG on the slide + LT pulls it, let go = it
   closes and the gun fires. Log: `rack: ... NEEDED`, `rack: hand on the slide`, `rack: pulled back`, `cycle complete`.
2. W-870: fire with shells left: the fore-end sits back, trigger clicks, the game's own pump does not play (or is cut
   short); LG on the fore-end + LT pulls, release = pumped, shell ejected, fires. Log: `pump: ... clip ... cut`,
   `pump: shell eject held back`, `pump: spent shell ejected on the pull-down`.
3. Shells: LG at the left hip with the shotgun = a shell in hand; to the port = count +1; sounds.
4. Anything that jitters, or a hand that will not go on the slide: say which gun and where the hand was.

### Not established
- Whether the slide joint moves visibly on each pistol (his numbers; the joint's own X/Y kept).
- Whether the REFramework two-hand dock (LG within 15 cm of the aid joint) and the rack dock fight on a pistol.
- The Spark Shot (4300) is in the pump table with his slide numbers; untried.
