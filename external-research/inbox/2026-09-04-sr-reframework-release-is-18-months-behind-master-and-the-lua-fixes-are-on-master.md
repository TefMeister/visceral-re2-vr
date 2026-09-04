# REFramework's newest release is 18 months old, and this week's Lua array / string-number fixes exist only on `master`

Filed by: `/sr`, 2026-09-04 (web half of the cross-engine sweep). Curated into the library's
[RE Engine page](https://github.com/TefMeister/flat-to-vr-cross-engine-research/blob/main/docs/engines/re-engine.md);
this drop is the project-facing half, for `/gr` to verify and fold into `external-research/`.

## The finding `[reported 2026-09-04]`

Read from the GitHub releases API and the commit list on 2026-09-04:

- Newest tagged release: **v1.5.9.1, published 2025-03-05.** Unchanged.
- `master` is committed to almost daily. The first days of September 2026 carried a run of **Lua
  data-model fixes**: `58ce1e8` *"Lua: Fix some array issues"* (2026-09-04), `57d623c` *"Lua: Fix array
  element setting"* / *"Fix array element type confusion"* (2026-09-04), `217ed82` *"Lua: Fix
  string/number ambiguity"* (2026-09-04), plus managed-array and managed-string creation fixes on
  2026-09-02–03 made while adapting to a newer RE Engine title.
- The `on_pre_gui_draw_element` `false`-return fix (PR #1809, merged 2026-08-28) is in the same
  category — fixed on `master`, in no release.

<https://github.com/praydog/REFramework> · <https://api.github.com/repos/praydog/REFramework/releases/latest>

## Why it matters to this project, in both directions

This project's whole feature set is Lua running on that framework, so the framework revision is a
variable in every result:

- **If the scripts run on a release build**, any misbehaviour around **array element access, array
  element types, or a value that could be read as either a string or a number** is a known,
  already-fixed framework bug, not a bug in our script. That is worth checking before debugging our own
  code — the crosshair and GUI-suppression work has already been bitten once by exactly this shape
  (the nine-day `on_pre_gui_draw_element` regression).
- **If they run on a `master` build**, we have those fixes and also whatever regressed this week.

## The concrete ask for `/gr`

1. Confirm which REFramework revision the shipped v0.1.0 package and the dev machines actually use.
2. If it is the release, decide whether the array/string fixes matter to any script we ship — and if
   the answer is "unknown", that itself argues for recording the revision beside every Lua result.
3. Record the standing rule in `external-research/`: **note the exact REFramework revision beside every
   Lua finding**, the same way a game patch version is recorded. On this family "it worked yesterday"
   is a statement about two programs, not one.

Applies equally to `re-village-scope-vr`, which sits on the same framework; filed once here rather than
twice, per the one-inbox-per-finding rule, and the shared form is on the library's RE Engine page where
both projects will see it.
