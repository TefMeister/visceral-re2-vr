Supersedes: `visceral-re2-vr/external-research/topics/2026-09-02-a-writable-speed-lever-exists-the-motion-layers-playback-speed.md` §Addendum, the claim "No turnkey layer-dumper tool exists publicly either"

# The public layer-dumper we said did not exist is sitting in the repo we already read — and it names a speed lever above the layers

**Status:** 🆕 new · **Priority:** high — it corrects a negative this project recorded on 2026-09-02,
and it bears directly on the native recon probe the modding lane built on 2026-09-04 to settle the
locomotion-layer index.

## The correction first

The 2026-09-02 topic's addendum says, of a tool to enumerate motion layers:

> "**No turnkey layer-dumper tool exists publicly either.** Checked alphaZomega's EMV Engine toolkit
> […] its Console/Poser/Action Monitor/Hooked Method Inspector do not include a motion-layer
> enumerator […] So the concrete next probe below is genuinely the cheapest way to get a real answer,
> not a second-best substitute for a public tool that exists somewhere."

**That is wrong, and the counter-example is in the same repository the topic was reading.**
`Junh2x/RE9-Movement-Speed-Mod` ships three files in `reframework/autorun/`, not one
`[verified-live 2026-09-05, n=1 API read of the repo tree]`:

| file | size | what it is |
| --- | --- | --- |
| `re9_movement_speed.lua` | 11,929 B | the shipping mod — the only file the 2026-09-02 topic read |
| **`re9_layer0_diag.lua`** | **9,260 B** | **a motion-layer property dumper with an in-game UI** |
| `re9_character_diag.lua` | 5,135 B | a character-identification dumper (which getter names actually resolve on the player context) |

The search that produced the negative looked in the *right ecosystem* (EMV Engine, the REFramework
book) but never listed the contents of the repo already open in front of it. Worth remembering as a
research failure mode: **the tool was not missing from the internet, it was missing from our
`ls`.** The repo has **no licence file**, so this is study-only — everything below is a description
of the technique and the interface names it exercises, with nothing copied.

## What the dumper does, and why it matters here

It resolves the player's motion component the same way the shipping mod does
(`app.CharacterManager` → player context → `get_GameObject()` →
`getComponent(via.motion.Motion)`), takes `getLayer(0)`, and then does two things the 2026-09-02
"suggested probe" did not think of:

1. **It reflects the layer type generically** — `scan_type("via.motion.Layer")`, then walks
   `td:get_methods()` and calls every zero-parameter getter, printing whatever resolves. So it does
   not need to know the property list in advance; it discovers it. It does the same for the
   highest-weight motion node's own type.
2. **It probes a fixed list of candidate getter names** and keeps the ones that exist. That list is
   itself the useful artefact, because it is one author's accumulated guess at the `via.motion`
   surface: `get_Speed`, `get_Frame`, `get_EndFrame`, **`get_Weight`**, `get_BlendRate`,
   `get_InterpolationFrame`, `get_BaseSpeed`, `get_MotionBankID`, `get_MotionID`, `get_BankID`,
   `get_CurrentBankID`, `get_CurrentMotionID`, `get_TreeCurrentNode`,
   `get_HighestWeightMotionNode`, `get_TreeObject`, `get_TreeNodeCount`, `get_CurrentNodeName`,
   `get_CurrentNodeID`, `get_Tag`, `get_MotionTag`, `get_Enabled`, `get_MaskBits`, `get_LayerType`.

### Three concrete gains for this project

**(a) `get_Weight` is the better layer-identifier, and our probe design does not use it.**
The 2026-09-02 probe and the native probe both identify the locomotion layer by *motion name*
(`get_HighestWeightMotionNode():get_MotionName()`, string-matched for `walk`/`run`). Name-matching is
what the shipping mod does, but it is a heuristic in the mod's own terms — it is how the mod decides
walk-vs-run, not how it decides *which layer*. `get_Weight` answers the layer question directly: the
layer actually driving the pose is the one carrying weight. **Logging `get_Weight` per layer
alongside the name costs one extra call and turns a string heuristic into a measurement.**
`[inferred-static 2026-09-05]` — this is a reading of what the getters mean, not a tested result.

**(b) `getLayerCount` is a real name to try, so the enumeration need not guess.** Our probe design
says to *"loop `i = 0..N` calling `getLayer(i)` until it returns `nil` (or use whatever count method
the live reflection dump exposes — … if `get_LayerCount`/similar isn't a real property)"*. The
dumper resolves the count with `getLayerCount` and falls back to `get_LayerCount`
`[reported 2026-09-05, from source]` — the same two spellings, tried in that order, by someone who
had the game in front of them. Try the no-underscore `getLayerCount` **first**; the surrounding
methods on this component (`getLayer`, `getComponent`) are all no-underscore, which is the pattern.

**(c) 🎯 `via.motion.Motion.get_PlaySpeed` — a speed lever ABOVE the layers, and it is engine-level.**
This is the find. In its "[Motion Component]" section the dumper reads **`get_PlaySpeed` on the
`via.motion.Motion` component itself**, not on a layer `[reported 2026-09-05, from source]`.

That is directly responsive to the modding lane's 2026-09-04 question. The inbox drop asked whether
there is public use of `app.ropeway.SurvivorMotionSpeedController`, reasoning that it *"sits above
the layer, which is where the 'reach for the deep end' rule says req 4 should live."* The instinct is
right and there may be a better answer than the one it names:

| candidate | scope | availability |
| --- | --- | --- |
| `via.motion.Layer.set_Speed` | one layer's playback rate | engine-level, **confirmed writable** (the shipping mod writes it) |
| **`via.motion.Motion` `PlaySpeed`** | **the whole motion component — every layer at once** | **engine-level `via.motion`, present in every RE Engine title** |
| `app.ropeway.SurvivorMotionSpeedController` | game-side player speed controller | **RE2-only**, our own static find, **no public use found** (below) |

`PlaySpeed` being on `via.motion` rather than `app.ropeway` is what makes it interesting: it is the
same class of engine-level, reflection-reachable property as `Layer.set_Speed`, so the dossier's
existing confidence about that lever transfers. **Only the getter is witnessed** — the dumper is
read-only and never writes it, so `set_PlaySpeed` is `[hypothesis]` until a reflection dump lists it.
If it is writable it is a strictly better handle than the per-layer speed for a global cap, because
it needs no layer index and no name-matching at all.

## The drop's actual question, answered

> *"If `/gr` finds any public use of `MotionSpeedController` in RE Engine titles, that is the one
> lead still worth a search."*

**No public use found** `[checked 2026-09-05]`. Two targeted searches (`MotionSpeedController` with
RE Engine/REFramework, and with il2cpp/type-dump framing) returned nothing referencing the type, and
it appears **nowhere** in the reference mod's three files. The public state of the art for RE Engine
movement speed is the pairing this project already documents — write the motion layer's speed, and
hook the game's own movement-speed getter so world travel follows the animation
(`app.MovementDriver.getMoveSpeed` in Requiem; the `app.ropeway` equivalent in RE2).

Two honest caveats on that negative: these are **search** negatives, not automated-fetch negatives,
so they carry some weight — but Nexus pages **403 automated fetch**, so a Nexus-only mod using the
type would be invisible to this check. And `app.ropeway.SurvivorMotionSpeedController` is a
**game-side, RE2-specific** type; there is no strong reason to expect Requiem modders to have
touched it. **Absence of public prior art is not evidence against the type** — it means nobody has
mapped it for us, and it stays a live in-house candidate.

## Concrete next steps

1. **Add `get_Weight` and `getLayerCount` to the native probe's per-layer log** — one extra call
   each, read-only, and (a) upgrades layer identification from a name heuristic to a measurement.
2. **Have the probe read `get_PlaySpeed` on the `via.motion.Motion` component**, and enumerate that
   component's own zero-parameter getters the way the dumper does, to see whether a
   `set_PlaySpeed` exists. If it does, it is the cleanest handle for req 4's global cap.
3. Keep `SurvivorMotionSpeedController` on the list as an in-house candidate — the negative above
   says nobody has written about it publicly, not that it does not work.

## Sources

- https://github.com/Junh2x/RE9-Movement-Speed-Mod — repo tree and the three `reframework/autorun/`
  scripts, read via the GitHub API 2026-09-05. **No licence file** — study-only, nothing copied.
  Credit: **Junh2x**, author of the mod and of both diagnostic scripts.
- https://www.nexusmods.com/residentevil22019/mods/2391 — "Better Movement Speed RE2", the same
  mod ported to RE2; page 403s automated fetch, so the RE2-specific getter name is **still unread**.
  A route to it would settle the `app.ropeway` half directly.
- This project's own `topics/2026-09-02-a-writable-speed-lever-exists-the-motion-layers-playback-speed.md`
  (the addendum this file corrects) and `topics/2026-08-29-caf-custom-animation-framework.md`
  (which already records the layer-speed + `getMoveSpeed`-hook pairing).
