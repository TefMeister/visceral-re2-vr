-- Visceral (RE2 VR) -- one title background: the last-save scene from launch (idea 5010, 2026-09-27)
--
-- Tefa: "on launch, there is only the background that shows the last saved location" -- the moving scene
-- that normally appears only after pressing Story should be the title, Options and Extras background too.
--
-- How the game picks it [inferred-static 2026-09-27, from the type dump]: the title camera
-- (app.ropeway.camera.TitleCameraController) carries one enum, TitleSceneValue:
--   0 MAIN (the usual start-up picture), 1 GAS_STATION, 2 OPENING, 3 RPD, 4 RPD_UNDERGROUND, 5 WASTE_WATER,
--   6 WATER_PLANT, 7 ORPHAN_ASYLUM, 8 ORPHAN_APPROACH, 9 LABORATORY, 10 TRANSPORTATION, 11 LATEST
-- This script answers MAIN with the last-save scene instead. That scene is learned the first time the game
-- itself picks a non-MAIN value (press Story once), and remembered in a file so the next launch has it from
-- the start. Before anything is learned it tries LATEST (11).
--
-- FIRST RUN IS ALSO A PROBE: every value the game asks for and every title-flow state is logged
-- ([visceral_title] lines in re2_framework_log.txt), because the dump cannot say whether the last-save area
-- is already loaded at start-up. If it is not, forcing it may show an empty or black background -- NUM9
-- turns the swap off on the spot.
--
-- Hotkey: NUM9 = swap on/off

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_title]"
local VK_NUMPAD9 = 0x69
local SCENE_MAIN, SCENE_LATEST = 0, 11
local SAVE_FILE = "visceral_title_one_scene.json"
local NAMES = { [0] = "MAIN", "GAS_STATION", "OPENING", "RPD", "RPD_UNDERGROUND", "WASTE_WATER", "WATER_PLANT",
                "ORPHAN_ASYLUM", "ORPHAN_APPROACH", "LABORATORY", "TRANSPORTATION", "LATEST" }

local cfg = { enabled = true, learned = nil }
local state = { last_real = -1, last_given = -1, last_flow = -1, swaps = 0, key_prev = false }

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end
local function name(v) return (NAMES[v] or "?") .. "(" .. tostring(v) .. ")" end

local saved = safe(function() return json.load_file(SAVE_FILE) end)
if type(saved) == "table" then
    if saved.learned ~= nil then cfg.learned = saved.learned end
    if saved.enabled ~= nil then cfg.enabled = saved.enabled end
end
local function save() safe(function() json.dump_file(SAVE_FILE, { learned = cfg.learned, enabled = cfg.enabled }) end) end
log_line("loaded: swap " .. (cfg.enabled and "ON" or "OFF") .. ", remembered scene " .. (cfg.learned and name(cfg.learned) or "none yet"))

local function target()
    return cfg.learned or SCENE_LATEST
end

-- the value the game ASKS FOR (setter): learn real last-save picks, log everything
local cam_t = sdk.find_type_definition("app.ropeway.camera.TitleCameraController")
local setter = cam_t and cam_t:get_method("set_TitleSceneValue")
local getter = cam_t and cam_t:get_method("get_TitleSceneValue")

if setter then
    sdk.hook(setter, function(args)
        local v = safe(function() return sdk.to_int64(args[3]) & 0xFFFFFFFF end)
        if v == nil then return end
        if v ~= state.last_real then
            log_line("game sets " .. name(v))
            state.last_real = v
        end
        if v ~= SCENE_MAIN and v ~= SCENE_LATEST and v ~= cfg.learned then
            cfg.learned = v
            save()
            log_line("remembered last-save scene " .. name(v))
        end
    end, function(retval) return retval end)
else
    log_line("set_TitleSceneValue NOT FOUND")
end

-- the value the camera READS (getter): MAIN becomes the last-save scene
if getter then
    sdk.hook(getter, function(args) end, function(retval)
        local v = safe(function() return sdk.to_int64(retval) & 0xFFFFFFFF end)
        if v == nil then return retval end
        local give = v
        if cfg.enabled and v == SCENE_MAIN then give = target() end
        if give ~= state.last_given then
            log_line("camera reads " .. name(v) .. (give ~= v and (" -> given " .. name(give)) or ""))
            state.last_given = give
        end
        if give ~= v then
            state.swaps = state.swaps + 1
            return sdk.to_ptr(give)
        end
        return retval
    end)
else
    log_line("get_TitleSceneValue NOT FOUND")
end

-- title flow states, for the probe
local flow_t = sdk.find_type_definition("app.ropeway.gamemastering.TitleFlow")
local flow_set = flow_t and flow_t:get_method("set_StateValue")
if flow_set then
    sdk.hook(flow_set, function(args)
        local v = safe(function() return sdk.to_int64(args[3]) & 0xFFFFFFFF end)
        if v and v ~= state.last_flow then
            log_line("title flow state " .. tostring(v))
            state.last_flow = v
        end
    end, function(retval) return retval end)
end

re.on_frame(function()
    local d = safe(function() return reframework:is_key_down(VK_NUMPAD9) end)
    if d and not state.key_prev then
        cfg.enabled = not cfg.enabled
        save()
        log_line("NUM9: swap " .. (cfg.enabled and "ON" or "OFF"))
    end
    state.key_prev = d or false
end)

re.on_draw_ui(function()
    if imgui.tree_node("Visceral: one title background") then
        local changed, v = imgui.checkbox("Use the last-save scene everywhere (NUM9)", cfg.enabled)
        if changed then cfg.enabled = v; save() end
        imgui.text("Remembered scene: " .. (cfg.learned and name(cfg.learned) or "none yet (press Story once)"))
        imgui.text("Game last set: " .. name(state.last_real) .. "   camera given: " .. name(state.last_given))
        imgui.text("Swaps so far: " .. tostring(state.swaps))
        imgui.tree_pop()
    end
end)
