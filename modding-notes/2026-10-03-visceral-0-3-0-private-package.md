# Visceral 0.3.0 — the first private package (2026-10-03, home PC, `/pd`)

**Worn 2026-10-03 00:40, Tefa, b048:** *"it feels really good now. no weapon janking, right posture and aim-walk, and the
main menu is working also better and better."* `[verified-live 2026-10-03, n=1 session]`

**Tefa's decision, same message:** no public releases at all, now or when finished; progress goes out as YouTube videos;
builds go privately to named people. 0.3.0 is for Andyalpa. Recorded in `PREFERENCES.md` and `game-mod-rules.md`.

## What 0.3.0 is (= the test copy b048 minus the game's own files)

| part | file(s) | since |
| --- | --- | --- |
| the gun-swing fix | `dinput8.dll` = praydog pd-upscaler a24c3459 + `2026-10-02-re2-grip-socket-freeze.patch` | 2026-10-02 |
| relaxed aim-walk, Claire + Leon | `natives/.../pl00|pl10/list/hdg/base_hdg_hold.motlist.524` (b015 lists) | 2026-09-25 |
| upright aim posture | 9 `pl0000/lookat/Hold*.user.2` = copies of Default | 2026-09-25 |
| no body twist toward the aim, no camera jump at the press | `visceral_body_direct.lua`, `visceral_body_anchor.lua` | 2026-09-24 |
| left hand stays on the gun | `visceral_lefthand_hold.lua` | 2026-09-24 |
| one title scene; rain on the main menu | `visceral_title_one_scene.lua` (+ its json), `visceral_title_rain.lua` | 2026-09-27 / 10-02 |

Package: `D:/Visceral build versions/releases/Visceral RE2 VR 0.3.0/` → `Visceral-RE2-VR-0.3.0.zip` (14,177,981 bytes,
sha256 `668b479e69283f0e…`), also in `staging/visceral-re2-vr/releases/` (private). README in Tefa's layout, FILES.txt,
`for modders/` with the REFramework patch, `lookat_patch.py`, `motlist_splice.py`. Requirements as installed here:
praydog pd-upscaler a24c3459 (REFramework.zip + VR.zip, OpenXR, no openvr_api.dll), PDPerfPlugin 1.2.0, DLSS 310.9.1,
`LooseFileLoader_Enabled=true`, `FirstPerson_Enabled=true`.

## Not in it, on purpose

The flat test harness and probes (`hold_exit`, `swing`, `subcam`, `rain` probes, `hold_exit_drive.py`), `steam_appid.txt`,
`shader.cache2`, the HD hands / bracelets / zombie-face work (not in the test copy; separate lines, unverified since the
clean reinstall), the native plugin `visceral_core.dll` (not needed by anything in this package).
