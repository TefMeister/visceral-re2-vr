# 2026-09-12 (evening) — "first load versus every load after" is our own state

Home PC, `/pd visceral-re2-vr`. **The game was not launched by this session and nothing here has been
run.** Tefa had the game up from their own session and reported the observation this pass is built on.

## The observation that made three symptoms into one bug

*"first load after launching the game the head shadow is there, when i load a game then it's gone, even
if i load the same save i loaded when the game first launched"* — and the same shape for the bracelets,
absent on the first load and present on every load after `[verified-live 2026-09-12, n=1 wearer]`.

**So the save is not the variable. Whether a level has already been loaded in this process is.** That
single sentence turned "the bracelets are flaky", "the shadow is lost" and "it behaves differently on
later levels" into one class of fault: state of ours that survives a load and should not.

⚠️ **It also withdraws a claim I made an hour earlier.** I had written that the head's shadow is not
kept and that the 2026-08-26 kill condition was met. It is not: the shadow survives a clean first load.
Dossier §7h carries the correction beside the original wording.

## Two fixes, both static, neither run

- **The caught-mesh list is cleared on every player re-bind**, and the draw flags are now **restored
  before** the head state is wiped rather than after — previously the only record of what we had
  altered was thrown away at the exact moment it was needed. A generation counter keys the per-frame
  bookkeeping so a new life cannot inherit the old one's indices.
- **The bracelets retry.** They had one creation attempt per bind, fired the instant the player binds,
  which lands before the arm's joints are ready; there was no second chance until the next level load.
  That is exactly why they showed up on the second load and not the first. Now six attempts half a
  second apart, then a log line saying it gave up. ⚠️ The neck plug has the identical one-shot shape
  and is the obvious next candidate, but it is left alone this pass rather than changed on a guess.

Both `[compile-verified 2026-09-12]`, 195,072 bytes. **Not deployed** — Tefa's game was running, and
copying files into a running game is what froze a load earlier today.

## Not established

Nothing here has been tested. The next run should read, in order: does the head keep its shadow on a
SECOND load; do the bracelets appear on the FIRST load; and does `bracelet ... created on attempt N`
say N > 1, which would confirm the timing reading rather than just the fix.

Also still open and untouched: the bracelets do not turn with the wrist, which is a knob (`k` ships at
0, `NUM-` cycles it) rather than a defect, and the neck plug draws but is not visible.
