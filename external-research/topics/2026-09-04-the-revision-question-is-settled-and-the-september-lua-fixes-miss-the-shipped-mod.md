# The framework-revision question is already settled — and September's Lua fixes land on the recon probes, not on the shipped mod

**Status:** 🆕 new · **Priority:** medium — it closes a question `/sr` has now asked twice, and it
separates the part that was answered on 2026-08-31 from the genuinely new part that was not.

## What was asked, and what was already known

`/sr`'s 2026-09-04 drop reports that REFramework's newest tagged release is **v1.5.9.1
(2025-03-05)** while `master` is committed to daily, and that the first days of September 2026
carried a run of **Lua data-model fixes** — array element setting, array element type confusion,
string/number ambiguity — none of them in any release `[reported 2026-09-04]`. It asked this project
to confirm which revision we actually run.

**That half is already answered, and has been since 2026-08-31.** `ENGINE-DOSSIER.md` records it
under the "date-check the revision before doubting the Lua" habit: our pinned revision **`76298bd`
is dated 2026-03-11**, checked on the home PC, **months before** the 2026-08-19 → 2026-08-28 window
in which `on_pre_gui_draw_element` silently ignored `false` returns (PR #1503 → PR #1809). Since
`visceral_crosshair.lua` hides `GUI_Reticle` by returning `false` and has no other mechanism, that
check was the one that mattered, and it came back clean. The same conclusion is on `STATUS.md`.

An independent reconstruction from our own `TOOLCHAIN.md` agrees and adds the reason the number is
what it is: **the build in use is neither a release nor `master`.** It is a fork build from the
`pd-upscaler` branch, dated 2026-03-11, which announces itself in the log as `Branch: pd-upscaler`.
Mainline REFramework has never contained the temporal upscaler that provides DLSS — seventeen
nightlies from March to August 2026 were symbol-counted and every one had zero upscaler symbols.

**Re-checked online 2026-09-04:** that fork still publishes **no releases at all**, and its
repository was last pushed 2026-08-06 `[verified-live 2026-09-04, n=1 API read]`. So no newer
DLSS-capable binary exists, and the standing "do not upgrade REFramework for RE2" rule stands
unchanged.

### A small tension worth naming, because it resolves cleanly

The one concrete reason anyone would upgrade this project's framework is the `false`-return fix.
**We do not need it** — our build predates the regression that PR #1809 repaired. So the argument
that would have competed with the DLSS rule does not arise, and the rule wins unopposed. If a future
upgrade ever does happen, it must land on a build dated **2026-08-28 or later**, or the crosshair
script silently stops working.

## The genuinely new part: the September fixes are a *different* question

The 2026-08-31 date-check cleared **one callback**. It says nothing about the array and
string/number fixes, which are about the Lua data model rather than the GUI path. Our build is
2026-03-11, so the exposure depends on something the commit messages alone cannot settle: whether
those fixes repair **long-standing** bugs (in which case a March build has them) or **recent
regressions** met while adapting to a newer RE Engine title (in which case a March build never had
them). We do not read or copy the source to find out, so this stays open rather than being guessed.

**What can be settled is the blast radius, and it is small.** Of the five scripts shipped in v0.1.0
— `cinematic_gate`, `crosshair`, `foot_ground`, `locomotion`, `spine_straighten` — **none reads a
managed array.** The only `ipairs` in the shipped set walks a plain Lua table of candidate bone
names. Managed-array access (`get_elements`, iterating returned collections, indexing into engine
containers) appears in the **recon probes** in `dev-archive/`, chiefly
`visceral_noaim_fire_probe.lua` `[inferred-static 2026-09-04]`.

So the practical reading is:

- **The shipped mod is not exposed** to the September fixes on any surface we can see.
- **Recon work is.** A probe that walks `get_elements` or reads a value that could be a string or a
  number is the place where a March-2026 data-model bug would show up — and it would show up as a
  wrong answer, not an error, which is the worst shape for reconnaissance whose whole product is an
  answer.

## The standing rule this argues for

`/sr` asked for it and it is worth adopting exactly as put: **record the exact REFramework revision
beside every Lua finding**, the way a game patch version is recorded. On this family, "it worked
yesterday" is a statement about two programs, not one. The estate already has the machinery for
this — a confidence tag with a date — so the cost is one clause per finding.

Concretely, for this project: any recon result that comes from array iteration should carry the
revision alongside its tag, and any surprising result from a probe should trigger the date-check
habit before the Lua is doubted.

## Applies to `re-village-scope-vr` too — and there it is not recorded at all

That project sits on the same framework and **has no revision recorded anywhere** in its dossier or
its board `[verified-live 2026-09-04, n=1 grep]`. That is a gap of exactly the kind this topic is
about: its Lua results are dated, but the program underneath them is not. Recording the revision it
actually runs is a cheap, no-launch action for whichever lane next touches it.

## Sources

- `/sr`'s inbox drop of 2026-09-04, and the cross-engine library's RE Engine page, which carries the
  shared form of the release-versus-master finding.
- Our own `modding-notes/TOOLCHAIN.md` and `modding-notes/2026-08-24-vanilla-baseline-and-the-reframework-version-trap.md`
  — the seventeen-nightly symbol count and the `pd-upscaler` provenance.
- `engine-research/ENGINE-DOSSIER.md`, the revision date-check habit (2026-08-31).
- https://github.com/praydog/REFramework — PR #1503 (2026-08-19) and PR #1809 (2026-08-28, by
  ErwinGunsmith), read online.
- https://github.com/gmankab/reframework-pd-upscaler-build — release list and repository metadata,
  read via the GitHub API on 2026-09-04.
