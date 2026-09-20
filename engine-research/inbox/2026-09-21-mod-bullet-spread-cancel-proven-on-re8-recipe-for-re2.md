# Bullet spread can be cancelled — proven on RE8, and the recipe is ready for RE2 (2026-09-21)

From: modding lane, `re-village-scope-vr` (dossier §9bk / §9bl). Pointer, not a duplicate — the full
finding is in `flat-to-vr-cross-engine-research/inbox/2026-09-21-mod-re-engine-bullet-spread-is-a-rotation-swapped-in-before-the-bullet-is-built.md`.

**Why Visceral wants this:** Tefa, 2026-09-21: *"please save this as something we use in RE2 and other
games, the idea is that putting two hands on the weapon makes the spread go away … any weapon, even
pistols, when two handed the weapon bullet spread is 0, when having one hand on the gun, the bullet
spread is like vanilla game."* Filed on `mod-ideas` → `games/all-games.md`. **Floating, not settled.**

**What is proven, on RE8 only** `[verified-live 2026-09-21, n=5 + the wearer's eyes]`: the game builds
each bullet with a deliberately scattered rotation (`createBulletImple`'s quaternion argument), and a
native pre-hook there can overwrite it with the clean one computed from the ray `createBullet` was
handed a call earlier. Hip shots carrying up to 11.7° of scatter flew straight.

**What is NOT known for RE2 — do not assume any of it transfers:**
- whether RE2's gun core has the same `createBullet` → `createBulletImple` → `setupDiffusion` order, or
  those names at all (RE2's dump shows `set_Diffusion` / `get_Diffusion`; its firing path is untraced)
- whether RE2's ray and rotation use the same layout and the same `+Z` shortest-arc convention

**How to find out, cheaply, in this order:** (1) grep `il2cpp_dump.json` for `createBullet`,
`createBulletImple`, `setupDiffusion` and their parameter lists — static, no game; (2) one live trace
of the firing order; (3) log the scatter per shot, aimed vs hip, **several** hip shots; (4) only then
build a lever, at the step that BUILDS the bullet, and make it re-measure after it writes.

**Two traps already paid for:** a value-type argument cannot be written from a REFramework **Lua** hook
(both routes return copies) — this needs `visceral_core.dll`; and in the native pre-hook `arg_tys[i]`
are handles, not pointers — casting one crashed RE8 on the first shot.

The two-hand half (knowing both hands are on the gun) is not built anywhere yet.
