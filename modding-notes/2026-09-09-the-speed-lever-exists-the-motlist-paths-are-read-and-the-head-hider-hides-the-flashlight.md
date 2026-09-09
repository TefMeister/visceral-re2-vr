# The speed lever exists, the motlist paths are read — and the head hider is hiding the flashlight

**2026-09-09, home PC (`RTX`), `/lm`, ONE FLAT LAUNCH.** Continue → last save → three board rows
answered on the same launch. Game closed with `WM_CLOSE`, exited cleanly, nothing saved.

Evidence: `dev-archive/recon/2026-09-09-speed-lever-and-the-motlist-paths/`.

## 1. ✅ req 4 HAS a component-wide speed lever — `set_PlaySpeed` is there

The board's row: *"`set_PlaySpeed` appearing in the `via.motion.Motion` surface list ⇒ spec req 4 has
a component-wide speed lever above every layer, needing no layer index and no motion-name gating."*

It appears. From the NUM7 dump `[verified-live 2026-09-09, n=1 launch]`:

```
via.motion.Animation :: System.Single get_PlaySpeed()
via.motion.Animation :: System.Void   set_PlaySpeed(System.Single value)
via.motion.Animation :: System.Single get_SecondaryPlaySpeed()
via.motion.Animation :: System.Void   set_SecondaryPlaySpeed(System.Single value)
via.motion.Animation :: System.Single get_CurrentPlaySpeed()
```

and the live read `get_PlaySpeed = 1.0000` — a real float, not the `NaN` the probe prints when the
method is absent.

**Note where it lives: `via.motion.Animation`, not `via.motion.Motion`.** It is on the base class
that `via.motion.Motion` inherits, which is exactly what "above every layer" means — one setter for
the whole component, no layer index, no motion-name gating. `getLayerCount = 6`, and all six layers
independently read `get_Speed = 1.0000`, so the per-layer levers are there too and currently neutral.

⚠️ **`get_Weight` returned `nan` on every one of the six layers.** Either the accessor is absent
under that spelling or it is not a float. Not chased; recorded because any future blend-weight work
will hit it immediately.

## 2. ⚠️ But the speed lever is HALF the mechanism — the `/gr` drop of today is right and unanswered

`inbox/2026-09-09-gr-the-speed-lever-is-a-pair-not-one-clamp.md` corrects §8d's *"holds by
construction"*: both public implementations (Junh2x's shipping mod, and `Namsku/re-engine-trainer`
independently) pair the layer/component rate with a **`app.MovementDriver:getMoveSpeed` return
scale at the same factor** `[reported 2026-09-09, from source]`. Two authors would not both scale
the driver's returned speed if clamping the rate already moved the character.

**This launch cannot speak to that**, because the NUM7 probe dumps the motion component only and
never touches `app.MovementDriver`. So req 4 now has a confirmed first half and an unconfirmed
second half. **Extending the probe to dump `app.MovementDriver` is one `[PD]` edit** and it should
happen before req 4 is designed.

## 3. ✅ The motlist paths are read — item 22 no longer needs to guess

Loose-file logging was armed on 2026-09-05 and had never been read back. This launch produced
**10,901** accessed-file lines and **26** loose-file lines. Extracted and saved:

- **64 distinct `pl10` motlists**, in six weapon/state folders: `CMN`, `ETC`, `FCE`, `HDG`, `SMG`, `STG`.
- The **HDG** set the board asked for by name, all at extension **`.524`**:
  `BASE_HDG_FINGER`, `BASE_HDG_HOLD`, `BASE_HDG_MOVE`, `HDG_FINGER_01`, `HDG_HOLD_01`,
  `HDG_HOLD_cpB_01`, `HDG_MOVE_cnFINE_stCOMBAT_01`, `HDG_MOVE_cnFINE_stNORMAL_01`, plus the
  `stLIGHT` / `stWATER` variants of each.
- 166 distinct animation paths overall, enemies included.

Full internal path shape, which is what the splice tool needs:
`natives/STM/SectionRoot/Animation/Player/pl10/list/HDG/<NAME>.motlist.524`
`[verified-live 2026-09-09, n=1 launch]`

## 4. ⛔️ The head hider RUNS, the mechanism WORKS — and it is hiding the flashlight

This is the interesting one, and it is a clean negative with a diagnosis rather than a puzzle.

**What went right.** `slot sources: firstpersonmod=true cinematic_gate=true` — both true on the
first read, so the mode-1 "reveals forever" trap the row warned about did not apply. And forcing
mode 2 with `NUM.` proved the hiding mechanism itself is sound:

```
head: HIDE again (d=19.66 m)
head: 1 mesh(es) hidden, shadow kept
head: restored 1 mesh(es) — NUM. off
```

Per-pass draw flags work, the shadow is kept, and the restore is clean. **That part of v0.8 is
proven** `[verified-live 2026-09-09, n=1 launch]`.

**What went wrong — the walk never reaches the player's head.** The full mesh table:

| mesh found | materials | verdict the hider reached |
| --- | --- | --- |
| `visceral_bracelet_r` | n=0 | keep |
| `visceral_bracelet_l` | n=0 | keep |
| `visceral_neckplug` | n=0 | keep |
| `Transceiver` | n=2, `sm69_001_Transceiver_*` | keep |
| `FlashLight` | n=3, `wp4530_FlashLight_Mat`, `…Mat2`, `…Lens_Mat` | **HIDE** |

`head: 25 transform(s) walked, 5 mesh(es) found, 1 to hide`.

**Three of the five meshes are our own injected objects**, and the other two are accessories. **The
player's head and body mesh is not in the walked hierarchy at all.** So the row's diagnostic
`M ≤ 1 ⇒ the walk did not descend` does not fit — the walk descends fine, it just descends into the
wrong subtree, and 25 transforms is far too few for a character rig.

And the one thing it did pick is a **false positive**: `FlashLight` matched the head pattern. So the
forced mode above hid the player's flashlight and kept the flashlight's shadow.

⚠️ **The technique is NOT disproved.** The row's kill condition was *"head gone but no shadow ⇒
RE2's shadow pass ignores the flag"*, and that never got a chance to be tested, because no head was
ever hidden. This is a **mesh-discovery** bug, not a rendering one.

## 5. ⛔️ And the reveal-distance gate is broken independently

```
head: REVEAL — not first person (d=19.68 m)
head: REVEAL — camera off the head (d=19.68 m)
```

The row expects `d ≈ 0.1 m` standing and treats `> 0.35` as "camera not at the head joint". **19.68 m
is not a near miss**, and it is suspiciously close to the magnitude of the world-space hand positions
the dock trace prints in the same log (`Lhand=(-20.27, -10.51, 20.66)`). That is the signature of
**comparing two positions that are in different spaces** — one world, one local — so `d` is really
the length of a world position rather than a separation.

Both automatic branches therefore reveal forever, and only the forced mode can ever hide anything.
`[verified-live 2026-09-09, n=1 launch]` — two independent defects, and either alone would have been
enough to make the row look like "the technique does not work".

## What was NOT done

No movement, no gestures, no dock, nothing saved. The `app.MovementDriver` half of req 4 is
untouched. `HEAD_REVEAL_DIST_M` was not re-tuned — tuning a threshold whose input is in the wrong
space would only hide the bug.

## Housekeeping

The install was **stamped for the first time on this machine** — `deployed.sh check` reported
`NO-RECORD`. The deployed `visceral_core.dll` was confirmed **same source as `origin/main`** by
rebuilding and comparing with `same-build.py` (6 differing bytes, all linker timestamps), so it is
current; seven files are now recorded in `deployed/RTX/visceral-re2-vr.tsv`.

## Automation scorecard

self-launch ✅ (Steam appid 883710) · menu → gameplay ✅ (title → Story → **Continue**, highlight
capture-verified before every commit, with `New Game` two rows below the one taken) · commands ✅
(numpad probes, NumLock checked first) · character + camera ⚠️ not exercised, not needed ·
self-close ✅ `WM_CLOSE`, clean exit, no `taskkill`.
