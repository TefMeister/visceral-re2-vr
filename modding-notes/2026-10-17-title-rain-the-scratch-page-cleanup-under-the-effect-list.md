# 2026-10-17 — Title rain: the game's scratch-page cleanup ran under its own effect list, and a counter hold stops it

Home PC, Fable, 21:50–22:40. Read from `re2.exe` with `static-disasm.py`, then **seen live** with x64dbg attached to the
running game (flat, Claude-driven, no headset): three launches, one Continue-into-the-save-and-Quit-to-title round.
Builds: board **b178** = builds repo `Title rain/v0.2.0-b170`. Everything below the "What was seen" line is
`[verified-live 2026-10-17]` unless tagged otherwise.

## The short version

- Every refused main-menu rain request since 2026-10-04 was the same thing: inside the game's own
  `EffectManager.createEffectInternal`, the one-entry list of effect objects lives in the calling thread's **scratch
  page** (temporary objects), and the first ordinary object allocation inside that same call ran the thread context's
  **scratch cleanup**, which rewinds the page to its base and zeroes it. The next object landed on top of the list, so
  the loop read a junk length (`0xNN0000`) and a junk entry (UTF-16 `"ffec"`), and crashed.
- The cleanup runs when the context's **allocation-nesting counter** (`[ctx+0x78]`) drops back to 0 at the end of an
  allocation. The game's own rain request (Story open) arrives with that counter already at 1 (seen: `counter 1 -> 2`),
  ours arrived at 0 (seen: `counter 0 -> 1`). That is the whole difference between "never crashes" and "crashes at
  random until the title has settled".
- **Cure (b178):** the native plugin (`dev-archive/native/src/rainhold.cpp`) hooks
  `ObjectEffectManager.requestEffectInternal` and holds the counter one higher for the length of every call, restoring
  it in the post hook. Result: rain requested at **attempt 1, 0.00 s**, at boot and after quitting a game to the title
  (n=2, no access violation). Unworn by Tefa.

## What was tried first, and what it showed

1. **Thread theory (wrong).** The 10-16 log's 22 dumps all had `RSI = 0x0063006500660066`, `RAX = 0xNN0000` (the
   length read), and `R15 = R14 + 0x60` (two objects made back to back from the same page). The array allocator
   (`0x141f79930`, `r9b == 0` path) takes the array from `[ctx+0x60]`, a 4000-byte page on the thread context,
   stamping the object `0x80000000` (temporary). Guess: `re.on_frame` runs on the render thread, whose page is never
   reset. Moved the whole tick to `re.on_pre_application_entry("UpdateBehavior")`: **still refused 8 times** on the
   main thread (21:52). `[disproved 2026-10-17]`. The move stays (the request now runs inside the game's own update;
   UpdateBehavior does not fire while the title is loading, which is fine).
2. **x64dbg, launched under the debugger:** REFramework threw an unhandled C++ exception and the game died; and the
   default "break on TLS callbacks" paused on every DLL. Switched off in `x64dbg.ini` (`TlsCallbacks=0`,
   `TlsCallbacksSystem=0`, `EntryBreakpoint=0`; backup `x64dbg.ini.bak-2026-10-17`) and **attached** to the running game
   instead (`attach <hex pid>`; the Steam re-spawn changes the pid once, ~6 s after launch). Note: this x64dbg has
   `C0000005` on its second-chance-only ignore list, so the access violation itself never pauses it; the REFramework
   log still shows it.
3. **Logging breakpoints** (never pause) at the list build (`re2+0xf80115`), the array allocation (`+0xf80130`) and the
   loop top (`+0xf801e0`): every call of the game's own made a list of 1, an array of length 1, and left the loop at
   index 1 — dozens during the save load, all clean. Our call: length 1 at index 0, then at index 1 **length
   `0x960000`, entry 0 junk, entry 1 `"ffec"`**, the page offset moved from `0x940` to `0x950`.
4. **Hardware write-watch on the array's length field**, armed by the array breakpoint (`bphws rax+1C, w, 4`):
   it fired *inside our own call's thread*, in `vcruntime140!memset` called from `re2+0x1f79196`, zeroing `0xB0` bytes
   from `page+0x900` — below the array at `page+0x950`. So the page had been **rewound** before that allocation.
5. **Conditional hardware watch on the page offset word** (`[page+0x12]`, pause only when written smaller than the
   array's offset) caught the rewind itself at `re2+0x1f7919a` = `0x141F7919A`, inside the cleanup routine
   `0x141f790b0`: `offset = base ([page+0x10]); memset(page+0x40+base, 0, old-base)`. Caller chain from the stack:
   `createEffectInternal` body (`0x140f808a1`) → `0x141f8b8c0` (make a component) → `0x141f5d750` (allocate a managed
   object) → the cleanup. In `0x141f5d750`: `inc [ctx+0x78]` at entry, `sub [ctx+0x78], 1` at exit, and **when it
   reaches 0** it runs `0x141f7a210` (exception check), `0x141f790b0` (the cleanup) and `0x141f781f0` (leave frame).

## The cure, and why it is safe enough

- `rainhold.cpp`: pre hook on `ObjectEffectManager.requestEffectInternal` reads `argv[0]` (the thread context; checked
  equal to `API::get_vm_context()` of the calling thread, else no hold and a log line), does `[ctx+0x78] += 1`, and the
  post hook does `-= 1`. Per-thread nesting is tracked (8 deep). First six calls are logged.
- With the counter ≥ 1 the allocator skips its enter/leave frame pair symmetrically and the cleanup waits for the next
  outermost allocation on that context; the scratch objects simply live a little longer. The game's own calls already
  run at 1 (`1 -> 2` seen), so for them nothing changes.
- The hold is on every `requestEffectInternal` call, not only ours: it is the game's entry point for this family of
  effects, cheap, and the plugin cannot tell our call from the game's without a bridge flag. If anything odd ever
  shows up around effects, this is the first thing to switch off (`rainhold::install()` in `plugin.cpp`).

## Seen in the log (b178)

```
rainhold: hook in: ObjectEffectManager.requestEffectInternal (allocation counter at ctx+0x78 held for the call)
title found; rain on the main menu armed
rainhold: requestEffectInternal: counter 0 -> 1 (held)
rain requested for the main menu (attempt 1, 0.00 s after the title was found, from UpdateBehavior)      -- boot
rainhold: requestEffectInternal: counter 1 -> 2 (held)   x3                                              -- the game's own, loading the save
rainhold: requestEffectInternal: counter 0 -> 1 (held)
rain requested for the main menu (attempt 1, 0.00 s after the title was found, from UpdateBehavior)      -- after Quit to title
```

## Not established

- Whether the rain is *visible* at 0.00 s in the headset; Claude saw drops on the main menu flat at 22:08 (before the
  cure, after a late acceptance). Tefa's eyes decide.
- What `[[ctx+0x50]+8]` is (the cleanup returns early when it is non-zero); not needed for the cure.
- The exact rule that gives the game's own calls a counter of 1 (an outer allocation still open, most likely the
  menu's own object creation). Not needed either.

## Driving notes, for the next session that needs them

- Quit to title from gameplay: `ESC`, `DOWN` x4 (Quit), `SPACE`, `UP` (Yes), `SPACE`. Recorded in this session; worth
  adding to the control profile's routes.
- With x64dbg open, SendInput keys land in the debugger (Space opens its assemble box). Minimise it first
  (scratch `winfix.py` did: WM_CLOSE to any "Assemble at" box, SW_MINIMIZE, then AttachThreadInput + SetForegroundWindow).
