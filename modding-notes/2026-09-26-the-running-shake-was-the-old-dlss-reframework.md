# 2026-09-26: the running shake was the old DLSS REFramework, not Visceral

Home PC, manual session with Tefa in the headset. Plan (Tefa): clean install, then our pieces back one at a
time, a check after each. Every installed state is kept on D: (`D:\RE2 REFramework builds\bisect-2026-09-26\`,
README there is the full log) and, from the end of this session, as numbered builds (below).

## What happened, in order

1. The game folder still held the whole mod: Steam's verify does not remove loose mod files. Backed up
   (`full-backup-2026-09-26-before-bisect`, 62 files, sha256 manifest), reset to the March DLSS REFramework.
2. March DLSS REFramework + praydog scripts + our 32 natives, first person off: **shook** `[reported, Tefa]`.
3. March DLSS REFramework alone, no scripts, no DLSS files: **shook**, and the frame rate sank the longer it
   ran `[reported, Tefa]`.
4. Steam uninstall + reinstall. Steam left `re2_config.ini` and `shader.cache2` behind; both moved out first.
   Fresh game + March DLSS REFramework + DLSS files: **shook straight away** `[reported, Tefa]`.
5. The folder saved as "working-smooth-2026-09-25" turned out to be **praydog nightly 01424 (no DLSS)** with
   `VR_AlternateFrameRendering=false`, not the DLSS build `[measured, sha256]`. Installed on the fresh game:
   **shook** too, "not a performance lag, the camera visibly shakes when running" `[reported, Tefa]`. An
   ffmpeg `h264_nvenc` video job from another session was running during this one run (see below).
6. **Fresh download of praydog's own `pd-upscaler` branch, commit a24c3459** (Dev Release run 33940315972,
   2026-09-05, found through the OptiScaler wiki's RE2 page) + its VR package + PDPerfPlugin 1.1.2 + nvngx:
   **"nice and super smooth"**, log "Successfully rendered with TemporalUpscaler" `[verified-live 2026-09-26, n=1]`.
   OBS was running during it, so OBS is not the cause.
7. + our 32 natives (loose files on): **still smooth**, checked in first person `[reported, Tefa]`.

**Conclusion:** the March build (`76298bd`, dinput8 `06f626da…`) shakes on a freshly reinstalled game with
nothing of ours in it (steps 3 and 4, before the ffmpeg job started). Our natives on the new base are smooth.
Which of our later pieces (first person, plugin, Lua) is fine on the new base is still to be checked, one at
a time.

## The pistol swing is separate, and it follows the LEFT GRIP

- Frames of `D:\vid\claude to look at\Claire shot gun sideways.mp4` (2026-09-25, 20 fps): right after the
  shot the **whole right arm with the gun** leaves view to the right for ~0.45 s, then returns
  `[measured 2026-09-26]`. The 2026-09-25 notes add: the two eyes showed different moments.
- Still present with only REFramework + praydog scripts + our natives, and **only while the left grip (LG)
  is held** `[reported 2026-09-26, Tefa]`. None of our scripts or the plugin were installed, so neither our
  dock (plugin) nor `visceral_lefthand_hold.lua` (the 2026-09-24 lever that keeps the game's left-hand IK on
  while aiming) can be the cause; removing the latter on 2026-09-25 also changed nothing.
- Open checks: (a) build b003, the 6 handgun motlists out: does the LG swing remain? (b) alternate-eye
  rendering (`VR_RenderingTechnique_V2`) off, since the eyes disagreed. Tefa's rule stands: pistols aim with
  the right hand only; the left hand on a pistol is cosmetic.

## Build versions from now on

Tefa's rule: every change gets its own numbered full copy. `dev-archive/tools/snapshot_build.py` writes
`D:\Visceral build versions\v0.2.0-bNNN - <title>\` (all of ours from the game folder, MANIFEST.sha256,
CHANGES.md with the diff against the previous build and the result) and a row in `INDEX.md`.
b001 = the smooth DLSS base, b002 = + natives, b003 = natives without the handgun lists (installed now).

## Also found

- A video job with `h264_nvenc` (another session) and OBS both use the GPU encoder Virtual Desktop streams
  with; worth ruling out during any future smoothness test.
- Claire's dirty hands are the Record system: `2026-09-26-dirty-hands-are-the-record-system-not-a-texture.md`.
