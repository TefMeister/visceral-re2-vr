# 2026-09-11b — the grip slots were never written at all, and this morning's diagnosis was wrong

**`/pd`, dev PC (`DESKTOP-V8GTSIR`). The game was NOT launched. Nothing here has been run.**

REFramework being present on this machine put praydog's own VR scripts on local disk at the exact
installed revision (`2f759483`), which turned a set of web-sourced inferences into things that could
be read directly. Doing that found the real cause of the ⭐⭐ dock defect, and it is **not** what was
recorded this morning.

---

## 1. 🚨 The correction: it was never a value-type copy trap

This morning's `/gr` pass concluded — from REFramework's published docs — that "the Lua shim never
writes the grip slots" was the **documented value-type copy trap**: read a value type, mutate the
copy, lose the write silently. That was a well-sourced, plausible inference and it is **wrong**.

**The real cause, proved mechanically rather than argued:** diff the slot names *declared* in the
shim's map against the slot names *written* anywhere in the file.

```
declared but NEVER written:  S_LGRIP  S_LTRIG  S_RGRIP  S_RTRIG   (and S_ACK, correctly)
```

`S_LGRIP=26, S_LTRIG=27, S_RGRIP=28, S_RTRIG=29` were declared in the slot map on day one and
**nothing ever assigned them** `[verified-numerically 2026-09-11]`. There is no lost write, no
copy semantics, no ordering problem: **the code to populate those four slots was never implemented.**
The plugin only ever read the zeros the array was initialised with, so the dock could not fire by
controller, ever — exactly the symptom the headset run reported.

`S_ACK` is also never written here and that is correct: the plugin writes it, the shim reads it.

**⚠️ The lesson, and it is the reusable part.** The copy-trap diagnosis was reached by reading
documentation about how writes fail, and it never asked the cheaper question: *is there a write at
all?* A grep of declared-versus-written slot names would have answered it in seconds on any day since
the shim was written. **When a value never arrives, check that something sends it before investigating
how sending could fail.**

That the wrong diagnosis was *well-sourced* is what makes it worth recording: the sources were real,
the reasoning was sound, and the conclusion was still wrong — because the premise (that a write was
attempted) was never checked.

## 2. ✅ The fix, written against the shipped script rather than a summary

Four writes added, plus the two things that make them trustworthy:

- **The API shape is copied from REFramework's own `re8_vr.lua` at the installed revision**
  (`2f759483`, lines 2222–2230), read off this machine's disk — not from a web page. It fetches the
  action handles and the joystick handles, then asks
  `is_action_active(action, joystick)` per hand `[inferred-static 2026-09-11, read from the shipped
  script]`.
- **A `clear_buttons()` helper on every early-out.** Without it the last live value stays latched in
  the shared array, and the plugin would keep seeing a held grip after the headset or the controllers
  went away. Both early-outs (`no vrmod`, `hmd == 0`) now clear the four slots.
- **The four values are surfaced in the existing debug panel**, so one glance says whether the *shim*
  side is alive without needing the plugin's hand-over to be working. That matters because the two
  failures look identical from outside.

**`[compile-verified 2026-09-11]`** — the patched file compiles under a real Lua 5.4 runtime, and
**the checker was proved able to fail**: the same runtime rejects a deliberately broken snippet
(a function missing its `end`). A syntax check that cannot produce a negative is not evidence.

Deployed to this machine's install (previous copy kept as
`visceral_native_bridge.lua.bak-2026-09-11-pre-grip`, 6,209 B → 8,035 B) and re-stamped: 21 files,
`deployed.sh check` ALL MATCH.

## 3. ✅ Two research claims upgraded from "reported" to "read locally"

Both were inferred from public sources this morning and are now confirmed against the shipped code on
this disk `[inferred-static 2026-09-11, read from the installed revision]`:

- **The camera pose really is read off joint 0**, and the walk is exactly
  `sdk.get_primary_camera()` → `get_GameObject()` → `get_Transform()` → `get_Joints()[0]` →
  `joint_get_position` / `joint_get_rotation` (`re8_vr.lua:792-799`).
- **`sdk.get_primary_camera()` is called at 8+ separate sites and cached at none of them**, while the
  *method definitions* are cached once at file scope (`re8_vr.lua:61-70`). That is the distinction the
  drop drew, and it holds.
- The application-entry registrations are present as described, including **both**
  `re.on_pre_application_entry("LateUpdateBehavior")` and `re.on_application_entry("LateUpdateBehavior")`.

⚠️ **This does not fix the camera defect.** Our shim contains no camera code at all — the constant
read is in the **native plugin**, not in Lua, so the fix belongs on the C++ side and was not attempted
this pass. What has changed is that the target shape is now verified rather than inferred.

---

## What is NOT established

- **Nothing here has been run.** The grip fix is compile-verified and deployed; no frame has been
  rendered with it, on either machine.
- **That writing the slots makes the dock fire.** It removes a proven blocker — the plugin was reading
  zeros — but whether the plugin's dock logic then behaves is untested, and it has never run with
  non-zero input.
- **Whether `is_action_active` maps to the grip the way the dock expects** (a held grip vs a press).
  The shipped script uses it for exactly this purpose, which is good evidence, not proof.
- **The camera defect is untouched**, and is a native-side problem.
