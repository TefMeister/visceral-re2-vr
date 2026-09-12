# 2026-09-12 (late) — one bug under three symptoms: our spawned meshes never get a material

Home PC, `/lm visceral-re2-vr`, flat, two launches driven end to end.

## What this pass set out to do, and what it actually found

It set out to test the two fixes from the `/pd` pass before it (per-load state reset, bracelet retry).
The first load answered something better.

**The bracelets were created on attempt 1** — so the retry, while correct, was not what was wrong. What
was wrong is that the plug and both bracelets had **zero materials**, four seconds after creation.

So v0.17 gained a material retry: re-create the holder and re-apply it whenever a mesh reports no
materials, eight times, half a second apart. Every one of the three objects came back the same way:

```
plug:       material STILL empty after 8 attempts (set_Material ok)
bracelet l: material STILL empty after 8 attempts (set_Material ok)
bracelet r: material STILL empty after 8 attempts (set_Material ok)
```

**`set_Material` is accepted and does nothing.** Not a timing problem, and a retry does not fix it.
That withdraws the theory I was working from an hour earlier.

## Why this matters more than the three rows it replaces

Three board items turn out to be one defect: the bracelets going grey and flickering, the bracelets
being absent on a first load, and the neck plug drawing but never being visible. Grey is what a mesh
with no material draws, so all three are the same thing seen from different angles. Dossier §7k has
what is ruled out — the reading method, the file, the loose loader, both holder routes, and the mesh
itself, since `setMesh` plainly works.

## Next, and it needs no game

RE Engine binds a material file to a mesh **by material name**. Our material file declares
`visceral_bracelet_leather`, `visceral_bracelet_metal` and two colour variants. What our own `.mesh`
files call their material slots has never been read — a plain text scan finds no name table, so it
needs RE Mesh Editor. If the names differ, that is the whole bug and it is a rebuild of our meshes,
not a code change. `[hypothesis]`

## Not established

Nothing about the head shadow or the bracelets-on-first-load question was answered this pass: the
material defect masks both, because an object with no material cannot be judged by eye. Those tests
are worth re-running only once a material actually binds.
