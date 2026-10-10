-- visceral_mute_music.lua -- the game's MUSIC at zero, every launch, nothing else touched (standing rule, 2026-09-30:
-- mute the music first thing on every game; sound effects and speech stay, tests may need them).
--
-- RE2 keeps the music volume in the system save (OptionManager option 19, AudioBGMVolume, 0..10), not in a file, so
-- there is no config line to write while the game is closed. This script asks the sound engine directly instead:
-- via.wwise.WwiseManager has STATIC setters set_MuteMusic(bool) / set_VolumeMusic(float) (reader 2026-10-10,
-- type database) [inferred-static 2026-10-10]. Re-applied once a second so a game-side reset cannot win.
-- It proves its own effect: the log shows the volume read back before and after the first write.
-- Taking this file out restores the game's own setting; nothing is saved.

local WM = sdk.find_type_definition("via.wwise.WwiseManager")
if WM == nil then
    log.warn("[visceral-mute-music] via.wwise.WwiseManager not found; music not muted")
    return
end
local set_mute, set_vol = WM:get_method("set_MuteMusic"), WM:get_method("set_VolumeMusic")
local get_mute, get_vol = WM:get_method("get_MuteMusic"), WM:get_method("get_VolumeMusic")
if set_mute == nil or set_vol == nil then
    log.warn("[visceral-mute-music] set_MuteMusic / set_VolumeMusic not found; music not muted")
    return
end

local last = -10.0
local writes = 0

local function read_back()
    local m = get_mute and get_mute:call(nil) or "?"
    local v = get_vol and get_vol:call(nil) or "?"
    return tostring(m) .. " / " .. tostring(v)
end

re.on_frame(function()
    local t = os.clock()
    if t - last < 1.0 then return end
    last = t
    local before = writes == 0 and read_back() or nil
    set_mute:call(nil, true)
    set_vol:call(nil, 0.0)
    writes = writes + 1
    if before ~= nil then
        log.info("[visceral-mute-music] music muted: mute/volume before " .. before .. ", after " .. read_back())
    end
end)
