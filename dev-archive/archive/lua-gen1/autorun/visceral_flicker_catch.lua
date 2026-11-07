-- visceral_flicker_catch.lua -- NUM1 = "the flicker just happened": Flickers BeG0nE saves the last 72 frames of each eye
-- (2026-10-31, Opus; Tefa: "there is the flicker in re2, that just randomly occurs with SFM"). The catcher
-- (reframework/plugins/flickers_begone.dll, settings reframework/data/flickers/flickers_begone.txt, hand catches only)
-- saves when a file named dump_now appears in its data folder; this script makes that file. NUM1 is free in RE2
-- (NUM3-NUM9 are taken). Read a catch with: python BeG0nE/flickers/tools/flicker_read.py <flickers-dump-N folder>.
if reframework:get_game_name() ~= "re2" then return end
local TAG = "[visceral_flicker_catch]"
local VK_NUMPAD1 = 0x61
local prev, count = false, 0
re.on_frame(function()
    local d = reframework:is_key_down(VK_NUMPAD1)
    if d and not prev then
        count = count + 1
        local ok, err = pcall(fs.write, "flickers/dump_now", "1")
        log.info(string.format("%s NUM1: catch %d asked for%s", TAG, count, ok and "" or (" -- could not write the file: " .. tostring(err))))
    end
    prev = d
end)
log.info(TAG .. " loaded: NUM1 saves the last second of both eyes (Flickers BeG0nE)")
