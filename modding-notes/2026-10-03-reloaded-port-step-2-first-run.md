# RELOADED port step 2: first run (2026-10-03 18:23-18:27, home PC, /lm, flat)

Build v0.2.0-b049 in the test copy. Evidence: `dev-archive/recon/2026-10-03-port-step2-first-run/`.

## What the run showed

| Check | Result |
| --- | --- |
| RELOADED's JSON read as data | `weapon_sfx ... enabled=1 master=1.00, 24 weapon(s)` `[verified-live 2026-10-03, n=1]` |
| Sound engine | `miniaudio 0.11.21 engine OPEN`, sounds folder found `[verified-live 2026-10-03, n=1]` |
| requestFire hooks | **1 of 2**: `survivor.Equipment.requestFire` hooked; `implement.Gun.requestFire` is **not found** on this build (the drawn weapon IS an `implement.Gun`). The Equipment hook catches every shot, so nothing is missing `[verified-live 2026-10-03, n=29 shots]` |
| Round count in the hook | read BEFORE the shot: 23, 22, ... 13 for shots 2-12 `[verified-live 2026-10-03]` |
| **Empty gun** | **the stock game keeps calling requestFire at 0 rounds** (it did not start a reload by itself here), and each call played `handgun\dry_fire.ogg` (5 clicks for 5 empty pulls) `[verified-live 2026-10-03, n=5]`. Open question from step 2: answered |
| Bridge v2 | `handshake: shim 2, late tick live` `[verified-live 2026-10-03, n=1]`; the `differ=`, button and rumble checks need the headset (not connected) |
| Frame order | **measured** (frame 299): LockScene, WaitRendering, BeginRendering, EndRendering, UpdateScene, UpdateHID, UpdateBehavior, UpdateMotion, LateUpdateBehavior, PrepareRendering, UpdateJointExpression (pre and post each) `[verified-live 2026-10-03, n=1]`. Matches the order the architecture note assumed; note UpdateJointExpression runs AFTER PrepareRendering |

## Not established

- **Audible?** The sound was played to the default output by miniaudio; nobody was there to hear it, and the play
  call reports success, not loudness. One listen settles it.
- **Volume 4.00:** master 1 x global dry_fire 2.0 x the pistol's own dry_fire 2.0. RELOADED multiplies the same way
  (ext_5 `resolve_kind_volume_multiplier`), so it is faithful; whether REAudio capped it is unknown.
- Whether the empty gun with RESERVE ammo reloads instead of clicking (this save may have had none) `[hypothesis]`.
- The headset half of the bridge (differ count, buttons, rumble proof, the right-B sound self-test).
