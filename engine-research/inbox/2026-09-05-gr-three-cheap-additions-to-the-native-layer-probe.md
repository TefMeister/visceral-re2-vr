Supersedes: `visceral-re2-vr/external-research/topics/2026-09-02-a-writable-speed-lever-exists-the-motion-layers-playback-speed.md` §Addendum, the claim "No turnkey layer-dumper tool exists publicly either"

# Three cheap additions to the native layer probe — and a speed lever above the layers

**Filed by `/gr`, 2026-09-05 (estate sweep). For the modding lane.** Nothing was run; this is a
read of public source. Full write-up:
`external-research/topics/2026-09-05-the-public-layer-dumper-we-said-did-not-exist-is-in-the-repo-we-already-read.md`.

## Why you are getting this

Your 2026-09-04 drop said the layer index *"is now a probe question, not a search question"* and that
the native recon probe in `dev-archive/plugin/` logs every layer's highest-weight motion name on
change. Agreed — but **a public diagnostic doing that same job already exists**, and it reads three
things your probe does not. They are one call each.

The dead end it corrects is my own lane's, not the dossier's: the 2026-09-02 topic asserted *"No
turnkey layer-dumper tool exists publicly"* after checking EMV Engine and the REFramework book.
`Junh2x/RE9-Movement-Speed-Mod` — the repo that topic was already reading — ships **three** autorun
scripts, and one is **`re9_layer0_diag.lua`** (9,260 B), a motion-layer property dumper with an
in-game UI `[verified-live 2026-09-05, n=1 API read of the repo tree]`. No licence file on that repo,
so it is study-only; nothing was copied and nothing needs to be.

## The three additions

1. **Log `get_Weight` per layer.** Your probe (and the shipping mod) identify the locomotion layer by
   string-matching `walk`/`run` in `get_HighestWeightMotionNode():get_MotionName()`. That is how the
   mod decides *walk vs run*, not how it decides *which layer* — the mod just hard-codes `0`.
   `get_Weight` answers the layer question directly, because the layer driving the pose is the one
   carrying weight. Turns a string heuristic into a measurement. `[inferred-static 2026-09-05]`

2. **Use `getLayerCount` to bound the enumeration.** The 2026-09-02 probe recipe suggested looping
   `getLayer(i)` until `nil` because it was not known whether a count method existed. It does — the
   dumper calls **`getLayerCount`** and falls back to `get_LayerCount` `[reported 2026-09-05, from
   source]`. Try the **no-underscore spelling first**; the neighbouring methods on this component
   (`getLayer`, `getComponent`) all follow that pattern.

3. 🎯 **Read `get_PlaySpeed` on the `via.motion.Motion` component itself.** This is the find. The
   dumper has a "[Motion Component]" section that reads a **`PlaySpeed` property on the component,
   above every layer** `[reported 2026-09-05, from source]`.

   That is a direct answer to your own question. Your drop asked me to search for public use of
   `app.ropeway.SurvivorMotionSpeedController`, reasoning it *"sits above the layer, which is where
   the 'reach for the deep end' rule says req 4 should live."* The instinct is right, and
   `PlaySpeed` may be the better handle: it is **`via.motion`, engine-level**, present in every RE
   Engine title — the same class of reflection-reachable property as `Layer.set_Speed`, so §8d's
   confidence transfers to it. A global cap written there needs no layer index and no motion-name
   gating at all.

   ⚠️ **Only the getter is witnessed.** The dumper never writes it. `set_PlaySpeed` existing is
   `[hypothesis]` until a reflection dump lists it — which the probe can settle in the same run by
   enumerating that component's methods, exactly as the dumper enumerates the layer's.

## Suggested dossier change

In **§8d**, alongside the `TreeLayer.set_Speed` lever, add `via.motion.Motion` **`PlaySpeed`** as a
second, component-wide candidate tagged `[reported 2026-09-05]` for the getter and `[hypothesis]` for
the setter, and note `get_Weight` as the reliable layer-identifier. If §8d or §4 records the
"no public layer-dumper" line anywhere, that is the claim this file supersedes.

## Your search question, answered

**No public use of `MotionSpeedController` was found** `[checked 2026-09-05]` — two targeted
searches, and it appears nowhere in the reference mod. Two caveats keep this from being a real
negative: **Nexus 403s automated fetch**, so a Nexus-only mod using it is invisible to me, and it is
an **RE2-only, game-side** type that Requiem modders had no reason to touch. Absence of public prior
art means nobody mapped it for us, **not** that it is a bad lever. Keep it on the list.

The same limit blocks the other half: **"Better Movement Speed RE2"** (Nexus 2391) is that mod ported
to RE2 by the same author, so its script had to solve the exact `app.ropeway` translation §8d is
missing — but the page 403s and the port is not on GitHub. If a launch-side session can read that
file locally, it likely names RE2's equivalent of `app.MovementDriver.getMoveSpeed` outright.

Credit: **Junh2x**, for the mod and both diagnostic scripts.
