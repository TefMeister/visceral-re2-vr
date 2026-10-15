# 2026-10-15 — Title rain: the "refusal" is a crash inside the effect creation

`/pd`, home PC, Opus. **The game was not launched, and nothing here has been run.** Read from the log already on disk
(`re2_framework_log.txt`, the 2026-10-14 23:01 launch with b174's four readings) and from `re2.exe` with
`static-disasm.py` and the 2026-09-25 `il2cpp_dump.json`.

## What the 2026-10-14 log says

The four readings Fable added on 2026-10-14 answered on the first attempt:

```
attempt 1 at 0.05 s: prefab set, ready true, effect data BUILT | provider set, container 0 in the map true, EffectManager present
request refused (attempt 1): ok=false result=lua: error: Invoke threw an exception | Story object UpdateSelf=false
```

So by the 10-14 plan's own fork, **all four are fine while it is refused, so the fault is deeper, inside the creation**
`[verified-live 2026-10-14, n=1 boot]`.

The log holds more than the Lua line. Above every refused attempt REFramework prints the caught exception:

- `VMContext: Caught exception code c0000005` — an **access violation**, not a managed throw.
- `RIP 7ff70d010203` = `re2.exe + 0xf80203`, call stack `EffectManager.createEffectInternal` ←
  `createEffectAutoDestroy` ← `EPVStandard.requestEffect` ← `ObjectEffectManager.requestEffectInternal`.
- `RSI = 0x0063006500660066`, `RDI = 1`. RSI is UTF-16 text (`f f e c`), i.e. string bytes read as a pointer.

The same dump appears before every refused attempt (every 0.5 s) `[verified-live 2026-10-14, n=10+ attempts in one boot]`.

## What the code at that spot does `[inferred-static 2026-10-15]`

`createEffectInternal(creator, effectData, pos, rotation, followTarget, followJoint, externParams, hitObject, groupNames,
lod_info)` @0x140f7fd90 returns `via.GameObject[]` (dump). Near the end:

1. `0x140f80110`: a native call with the effect manager, `effectData` (the element, a `ProviderData`) and a name string
   fills a native list (pointer + count).
2. `0x140f8012b` allocates a managed `GameObject[]` of that count, `0x140f80176` memcpy's the native pointers into it.
3. The loop at `0x140f801e0` walks the array: `rsi = arr[i]`, then `cmp [rsi+0x18], 0` (is this GameObject alive).
   **That is the crash: entry 1 of the freshly made list is junk** (text bytes), so the list came back with a count
   but not with real objects.

So the game is asked to make the effect's objects from the element's data and gets junk back. The element itself is
there (the map lookup found it). What it points at is the effect file, through `ProviderData.getResources(i)` →
`EffectResourceHolder`. **The working guess is that this effect file is still streaming in** `[hypothesis]`: the
accept times fit loading (boot 2–3 s, after a game 8–10 s, Tefa: "launch starts straight away, coming out of the game
it takes a while"), and none of the four manager-side readings changed between refused and accepted.

## What b177 does

`visceral_title_rain.lua`, nothing else changed (board b177 = b176 + this script; builds repo
`Title rain/v0.2.0-b169`; old copy in `D:/Visceral build versions/_backup-2026-10-15-before-b177/`):

- As soon as the effect data reads BUILT, once per title visit, it takes the rain element
  (`ObjectEffectManager.getProviderData(0, element)`), reads each `getResources(i):get_ResourcePath()`, and **loads
  each file itself** with `sdk.create_resource("via.effect.EffectResource", path)` + `add_ref()` (held, never released).
- Logs: `preloaded K of N rain resources in X ms: <paths>`.
- The once-per-change reading line gains `resources isValid v/n` (`ProviderData.isValid(i)` per resource).
- The request itself, retry and hand-over are unchanged.

## The next launch reads it (any launch reaching the title, flat or VR)

| log shows | means |
| --- | --- |
| `preloaded N of N` and `rain requested ... (attempt 1 or 2, ~0.5 s)` | the effect file was the gate; rain starts at once. Check after a game too. |
| `preloaded N of N`, still refused for seconds | loading the file is not it, or `create_resource` only starts an async load. Compare `isValid` at refused vs accepted. |
| `preloaded 0 of N` / `element NOT found` / `none listed` | the paths could not be read this way; the junk list has another cause. Next is the x64dbg read of the native call at 0x140f80110 (`MODEL: FABLE`). |
| `isValid` flips at the same attempt the request is accepted | that reading is a usable gate: request only once it reads all valid (no caught crashes every 0.5 s). |

## Not established

- That the junk list comes from an unloaded file. It fits the timing; nothing has shown it directly.
- That `sdk.create_resource` loads synchronously for an `.efx` (it does for meshes in other mods `[reported]`).
- What `ProviderData.isValid(i)` means: it returns false only when the resource exists, its byte +0x39 is 0, and a
  helper (0x141f66f00) says no; the name may be the opposite of what it checks.
- Side note: REFramework has been catching one access violation every 0.5 s on the title for weeks. No harm seen, but
  it is a real fault in the game's memory being read, not a clean refusal.
