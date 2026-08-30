# 2026-08-24 — A vanilla baseline, and the REFramework version trap

Two things happened this morning, both of them groundwork rather than mod work:
the game was reduced to a genuinely clean install, and we found out the hard way
which REFramework build you actually want.

## The vanilla baseline

Before Visceral's first line gets written, the install it will be developed
against should be stock. It now is.

The route-fix work that had been living in this install (a merged build of
someone else's file-replacement mods, plus our own switcher and text-override
scripts) has been handed to that mod's author and removed from the machine
entirely. What came off:

- `natives\` — deleted. The mod manager's own uninstall had emptied it, but the
  next launch put 97 files straight back: our switcher's "startup heal" saw a
  recorded route, decided the file set was inconsistent, and helpfully restored
  it. A self-healing mod is a nice feature right up until you are trying to
  uninstall it.
- `reframework\` — deleted whole, including the two autorun scripts and their
  data payload that the uninstall had left behind.
- REFramework's generated state (`re2_fw_config.txt`, `ref_ui.ini`, the log,
  the accessed/loose-file lists) — deleted, so nothing carries a previous
  project's tuning into this one.

**Lesson worth keeping:** a mod manager uninstalls the files it deployed, not the
files a *script* created afterwards. Anything a mod writes at runtime — settings,
caches, restored payloads — is invisible to it and survives the uninstall. If you
want a clean baseline, verify it yourself rather than trusting the uninstall
button, and check whether anything you wrote will resurrect itself on next
launch.

The reassuring half of that: RE Engine modding is completely reversible, because
everything lives in three places — `dinput8.dll`, `natives\`, `reframework\`.
Remove those and the game is stock. The install is now stock plus `dinput8.dll`.

## The REFramework version trap

The question was simply "install the latest stable REFramework that has DLSS."
The answer turned out to be "the one already installed, and do not upgrade."

REFramework's mainline has **never** contained the temporal upscaler that
provides DLSS. We downloaded seventeen nightly builds spanning March to August
2026 and counted the upscaler symbols in each `dinput8.dll`. Every one: zero.
The latest nightly at the time of writing (01397, 2026-08-20) is no exception.

The upscaler exists only on a `pd-upscaler` branch in a fork. The build we have
is from that branch, dated 2026-03-11, and it announces itself in the log as
`Branch: pd-upscaler`. That fork publishes no releases, its newest source commit
is from 2026-03-27, and its only CI runs both failed — so no newer binary exists
at all. Installing "the latest REFramework" would have silently removed the
feature we were trying to guarantee.

**Lesson worth keeping:** when a feature lives on a fork, "latest" and "has the
feature" can point in opposite directions, and a version number tells you
neither. Check the binary, not the release date. Symbol-counting a DLL is a crude
tool and it answered the question in minutes.

The second half of the confusion is that even the right build does not upscale
anything on its own: REFramework's upscaler UI is a front end, and the actual
implementation is a separate plugin by a different author. That distinction is
why so many reports read "the option is missing" or "the option does nothing" —
those are two different missing files.

All of it — which versions, where to download them, how to install by hand, and
which log line means which missing piece — is now written up in
[`TOOLCHAIN.md`](TOOLCHAIN.md) so nobody else has to spend a morning on it.
