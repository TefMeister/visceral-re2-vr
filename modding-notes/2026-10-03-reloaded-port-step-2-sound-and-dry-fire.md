# RELOADED port step 2: the sound player and the dry-fire click (2026-10-03 evening, home PC, /pd, Opus)

**The game was not launched; nothing here has been run.** Code on branch `split-plugin-2026-09-27`, commit
`5f1f82f`, on top of bridge v2 (`d1e7894`). `[compile-verified 2026-10-03]`: builds; our files add 0 warnings (the
8 MinHook warnings were there before on a clean build).

## What was built

| File | Job |
| --- | --- |
| `src/port/port_settings.h` | every number of the port, named, with the RELOADED line it came from |
| `src/port/port.h` | the port's shared declarations; `Plugin.cpp` only calls `port::dryfire_install()` and `port::dryfire_frame()` |
| `src/port/rld_data.cpp` | reads RELOADED's `re2_vr_reload.json` as data (only the `weapon_sfx` block so far), from `reframework/data/visceral/reloaded/` or, if absent, RELOADED's own `reframework/data/re2_vr/` |
| `src/port/rld_sfx.cpp` | miniaudio sound player: the ext_5 lookup rules (the weapon's file for the kind, its `sfx_folder`, the fallback weapon, master x global kind volume x weapon kind volume, 0.1 s debounce per kind), 16 voices |
| `src/port/rld_dryfire.cpp` | pre-hooks on `requestFire` of `app.ropeway.implement.Gun` and `app.ropeway.survivor.Equipment` (reload.lua 2869-2935); plays `dry_fire` when the drawn weapon has 0 rounds; never blocks the call |
| `third_party/miniaudio_impl.c` | miniaudio 0.11.21 + stb_vorbis (public domain; his sounds are OGG and WAV), compiled apart at /W0 |
| `third_party/nlohmann/json.hpp` | the JSON reader (MIT) the architecture note named |

## Self-proving lines (what the next run must print)

1. `rld data: weapon_sfx from reframework\data\visceral\reloaded\re2_vr_reload.json -- enabled=1 master=1.00, 24 weapon(s)`
2. `sfx: miniaudio 0.11.21 engine OPEN ... sounds folder ...\visceral\reloaded\custom_sfx`
3. `dryfire: 2 of 2 requestFire hooks in`
4. **The audio pipeline, without an empty gun:** the first three right-B presses play the drawn weapon's dry-fire
   click: `sfx self-test (right B) 1 of 3: played` plus `sfx: PLAYED dry_fire for wp0000 -> handgun\dry_fire.ogg at volume 2.00`.
   Someone near the headset or speakers hears it.
5. `dryfire: requestFire #N, weapon wp0000, rounds R` for the first 12 shots. Shooting the pistol empty answers the
   open question below.

## Not established

- **Whether the stock game calls `requestFire` on an empty gun at all** `[hypothesis]`. With reserve ammo it may start
  its own reload instead; RELOADED only gets the click because its blocking layer (step 3) stops that reload first.
  If no `rounds 0` line ever appears, the click waits for step 3; the sound player itself is still proved by line 4.
- The `get_GameObject().get_Name()` weapon id ("wp0000") is how RELOADED keys sounds (reload.lua 1214); not yet seen
  from C++ `[inferred-static]`.
- Not ported in this step: spatial playback at the hand (ext_5 `spatial = true`), the `common/` fallback folder.

## Installed

Test copy, build **v0.2.0-b049** ("native core, bridge v2, port step 2 sound"): `visceral_core.dll` sha256
`9c8ef02793801179`, the bridge v2 shim, the native assets, and RELOADED's JSON + `custom_sfx` (2.5 MB) under
`reframework/data/visceral/reloaded/`. ⚠️ b048 carried no native core; b049 brings back the dock, head hider,
bracelets and neck plug, so play feel may differ from b048. It is a probe build: judge it by the log lines.
To go back: `D:\Visceral build versions\v0.2.0-b048 ...`.
