# The FirstPerson settle bug, read from upstream source: the snap branch waits on a lerp that never reaches exactly zero

**Status:** 🆕 new · **Priority:** high — every Visceral release ships `FirstPerson_Enabled=true`, so
this is a defect our users see; and it turns a "file an upstream PR someday" item into a diagnosis
precise enough to write the patch from.

## The claim being checked

Recorded on the predecessor project's board (`status/arcade-controls-re2-vr.md`, 2026-08-22), about
the ~1s camera settle at jack start:

> **REFramework FirstPerson's own interp — UPSTREAM BUG FOUND** (VR snap branch requires
> `bone_scale == 0.0f` exact float equality that never becomes true → VR always takes the slow interp
> path).

Read against REFramework's published `src/mods/FirstPerson.cpp` on `master`, that is **correct in
structure and slightly wrong in mechanism** — and the real mechanism is the part that tells you how to
fix it. `[reported 2026-09-01, from a read of the published upstream source]`

## What the source actually does

Four points in the same function, in order:

```cpp
const auto wanted_camera_shake = vr->is_hmd_active() ? 0.0f : m_bone_scale->value();

m_interp_bone_scale = glm::lerp(m_interp_bone_scale, wanted_camera_shake,
                                std::clamp(delta_time * 0.05f, 0.0f, 1.0f));

auto bone_scale = (is_player_in_control || is_switching_to_player_camera)
                    ? (m_interp_bone_scale * 0.01f) : 1.0f;

// …
if (is_player_in_control && m_interp_camera_speed >= 100.0f && bone_scale == 0.0f)
```

So **VR does the right thing and then cannot get there.** In VR the *target* is set to exactly `0.0f`
— that part is deliberate and correct. But the value only **lerps toward** it, at
`clamp(delta_time * 0.05f, …)` — on the order of **0.0008 per frame at 60 fps**. An asymptotic
approach in floating point does not produce an exact `0.0f` on any timescale a play session contains.

The snap branch is therefore gated on an equality the code's own update rule will not satisfy. Not
"never becomes true" as a matter of principle — but as a matter of practice, which is the same thing
for a player.

**And the codebase already knows the answer**, a few lines earlier:

```cpp
if (vr->is_hmd_active()) { m_interpolated_bone = glm::identity<Matrix4x4f>(); }
else                     { m_interpolated_bone = glm::interpolate(…, delta_time * bone_scale * dist); }
```

That is the same problem, solved the right way: **test the VR state directly, do not infer it from a
smoothed float.** The bug is that the branch at the fourth quote did not get the same treatment.

## ⛔ The tempting workaround does not work — worth knowing before someone tries it

The obvious first thought is "set the CameraShake slider to 0". **It changes nothing.** In VR,
`wanted_camera_shake` is forced to `0.0f` regardless of the slider's value — `m_bone_scale->value()`
is only read when the HMD is *inactive*. The slider is not on the VR path at all; the lerp is.

## The shape of a fix

Two candidates, both small, and the second matches upstream's own idiom:

1. **Epsilon instead of exact equality** — compare `bone_scale` against a small tolerance rather than
   `== 0.0f`. Minimal, but leaves the settle time governed by the lerp rate.
2. **⭐ Add the VR term to the condition** — make the snap branch reachable when
   `vr->is_hmd_active()`, exactly as the `m_interpolated_bone` branch already does. This is the
   consistent fix: it makes one function treat VR the same way in both places, and it removes the
   settle rather than shortening it.

There is a third gate in that condition — `m_interp_camera_speed >= 100.0f` — which also has to be
satisfied. Any fix or test must account for it; it is not obvious from the outside whether it is
during a jack.

**One genuine unknown:** `m_interp_bone_scale`'s **initial value** is not visible in what was read. If
it initialises to `0.0f` and the HMD is active from the start, the condition would be satisfiable
after all, and the observed settle would have another cause. That single fact should be checked before
anyone writes a patch — it is the difference between a confirmed diagnosis and a very good one.

## ⚠️ Read caveat, stated because it bounds everything above

The file was read through an automated fetcher. The **GitHub blob view truncated at line 1000** and
returned nothing about `bone_scale` — a clean example of an automated negative that is not a negative.
The raw view did return the occurrences quoted above, but with **approximate line numbers**, and it
did not confirm the file's total length when asked.

So: the expressions above are quoted, and the logic follows from them. **Confirm the exact lines in a
browser before writing a patch or opening a PR** — and note that upstream `master` moves, so these
line positions are a snapshot, not an address.

## Concrete next steps

1. Confirm in a browser: the four expressions, their exact lines, and **`m_interp_bone_scale`'s
   initial value**.
2. If it holds, this is a clean, tiny, well-motivated **upstream PR to `praydog/REFramework`** —
   consistent with an existing VR special-case a few lines away, affecting every VR user of
   FirstPerson, not just us. That is a better outcome than a local patch and was already the preferred
   route on the predecessor's board.
3. Nothing here should be copied into our scripts. This is a diagnosis of someone else's defect,
   reported back to them.

## Sources

- https://github.com/praydog/REFramework/blob/master/src/mods/FirstPerson.cpp
- https://github.com/praydog/REFramework/blob/master/src/mods/FirstPerson.hpp
- https://github.com/praydog/REFramework/blob/master/src/mods/VR.cpp
- https://cursey.github.io/reframework-book/troubleshooting/VR-Troubleshooting.html
- https://reframework.dev/
