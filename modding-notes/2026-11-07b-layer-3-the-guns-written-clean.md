# Layer 3 of the clean rebuild: the guns, written clean (2026-11-07, Opus, home PC)

Tefa: *"please just do layer 3 as well, the goal of this is to write it from scratch to have a nice neat code, probes and other
things removed and so on. then i'll test things in one go at home"*. Design: `2026-11-07-clean-rebuild-design.md`; plan:
`2026-11-07-clean-rebuild-plan.md`.

## The call made, and why

The design said "Village's files through a per-game layer". Read side by side, RE2's gen-1 gun code and Village's share one
ancestor (RELOADED's state machine) and RE2's already carried Village's rules since 2026-10-31 (b182-b184), built on RE2's
own proven mechanics: its ammo calls, its magazine joints (Andyalpa's data), the patched loader's dock socket. Village's files
lean on Village-only pieces throughout (praydog's Lua VR layer, Andyalpa's holster globals, its ammo model, its joints). So
layer 3 is **RE2's gun code rewritten clean in Village's file shape**; making the files literally shared with Village is a
later step that needs Village moved onto the same `game/` layer too. `src/game/game.h` stays empty until then.

## What `src/guns/` is now

| file | one job |
| --- | --- |
| `guns.h/.cpp` | the one entry point plugin.cpp calls, and the answers to `guns_api.h` (what the body files ask) |
| `gun_frame` | the player, gun, transforms and wrists looked up once a frame (every gen-1 file did its own) |
| `ammo` | the game's rounds: loaded, capacity, carried, top up, one in, ready to fire |
| `game_reload` | the game's own reload kept off for our guns; the HUD counter |
| `magazine` + `magazine_pose` | Village's real-gun magazine rules; where it is drawn and where the hand holds it |
| `shells` | two shells per pouch grab, dock at the port, LT puts them in |
| `rack` | the slide and the pump by hand; a rack due holds the trigger; kept across a swap |
| `pump` | the game's own pump animation cut, its pump sounds refused, the spent shell on our pull |
| `left_hand` | who has the left hand (the loader's dock socket), and Hand Poser's "on the gun" flag |
| `dryfire` | the shot gate and the hammer click |
| `gun_data.h`, `settings_guns.h` | Andyalpa's per-gun data; every number, history lines gone |

Out: the tuning sliders woven through the magazine, slide and shell code; the button-rack mode (off since b165); the GM 79's
pieces inside the shotgun path (layer 4 brings the GM 79 back on its own); the debug chatter and the probes
(`watch_game_reload`, the per-press distance logs). Smaller than gen-1 (about 1,200 lines against 1,700), every file under 400.

## Tested on the OpenXR Simulator with VR Bench `[verified-live 2026-11-07, simulator, n=1 each]`

With a tools build (`-DVN_TOOLS=ON`: `src/tools/magreport`, `rackreport`, `shellreport` log the move that puts the left hand
on the magwell, slide or port; the gun code is the same) and a test-only gun-giving script, both taken out after:

- Lightning Hawk: LG on the seated magazine, right B, the magazine into the left hand; LG let go, it falls; the chambered round
  fired once with the magazine out, then the hammer click; a pouch magazine steered to the magwell went in and was topped up
  1 -> 7; a magazine change with rounds left needs no rack.
- The gun fired dry, a magazine into it: "a rack due", RT clicked, the slide racked by hand (LG on it, LT, pulled back along the
  barrel, LG let go), then it fired.
- W-870: two shells from the pouch docked at the port (0.075 m) and went in on LT; shells into an empty gun = a pump due; the
  pump by hand (LG + LT at the fore-end, pulled back, LT let go = back up by itself) let it fire; a shot with a shell behind it
  made the next pump due; pumped, fired.
- The player build deployed after is byte-for-byte the build saved as b195; the tools never ship.

Not tested on the simulator: the LE 5's pull-out by hand (LG + LT), a magazine kept out across a swap, the Spark Shot. Their
code paths are the gen-1 ones Tefa wore, unchanged in substance.

Found: one start-up hung inside REFramework's own hook install, before any plugin ran (1 in 6 launches); recorded in the
control profile's KNOWN.md.
