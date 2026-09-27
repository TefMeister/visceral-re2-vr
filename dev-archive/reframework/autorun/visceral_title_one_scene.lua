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
-- Run 1 (2026-09-27, VR): the camera's own setter/getter were NEVER called, so nothing changed. The scene is
-- switched through MainFlowManager.changeTitleCameraScene(TitleScene, Action), and the menus (main, Options,
-- Extras) cover the view with GUIMaster.openSelectBackground (the dark layer). The police-station scene
-- appears at title flow state 11 (TITLE_MENU_STORY). Run 2 hooks those instead:
--   * changeTitleCameraScene(MAIN) -> the last-save scene; any other value is learned and remembered
--   * entering main menu / Extras / Bonuses / Options (flow 10, 12, 13, 14) re-issues the last-save scene
--   * openSelectBackground is skipped while the swap is on
--
-- Run 2 (2026-09-27, VR): still the dark picture. Log: Story sends changeTitleCameraScene(LATEST) -- the game
-- never names the place, it resolves LATEST itself -- and backing out sends MAIN plus TitleBackgroundScene.start(2
-- back); start-up sends MAIN plus start(0 open). Run 3: MAIN always becomes LATEST (or a learned place), and the
-- open/back timeline moves are replaced by the decide (Story) move.
-- Run 3 (VR): start-up shows the last-save scene. Story still fades dark and back; backing out left an EMPTY main
-- menu (had to close the game): MAIN->LATEST while LATEST already showed, so the camera never changed and the
-- flow waited on changeTitleCameraScene's callback forever. Run 4: if the scene already shows, skip the call and
-- invoke the callback ourselves; the back move plays as the game wants; a real Story move jumps to its end frame.
-- Run 4 (VR): main menu fine; Story -> dark, no text, no way back: the jump skipped the move's end event, flow
-- never reached state 11. Run 5: the Story move is played at x20 speed instead of jumped.
-- Run 5 (VR): Story fades out and in to the same view; back out = menu text gone again although the callback was
-- called. Run 6: back lets MAIN through (menu re-issue restores LATEST); the same-scene request after Story is dropped.
--
-- Hotkey: NUM9 = swap on/off

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_title]"
local VK_NUMPAD9 = 0x69
local SCENE_MAIN, SCENE_LATEST = 0, 11
local STORY_MOVE_SPEED = 20.0   -- the Story camera move (normally ~10 frames of fade) played this much faster
local SAVE_FILE = "visceral_title_one_scene.json"
local NAMES = { [0] = "MAIN", "GAS_STATION", "OPENING", "RPD", "RPD_UNDERGROUND", "WASTE_WATER", "WATER_PLANT",
                "ORPHAN_ASYLUM", "ORPHAN_APPROACH", "LABORATORY", "TRANSPORTATION", "LATEST" }

local cfg = { enabled = true, learned = nil }
local state = { current = -1, skip_decide = false, bg_this = nil, sped_tl = nil, deferred_cb = nil, last_real = -1, last_given = -1, last_flow = -1, swaps = 0, key_prev = false }

local pending = false   -- set when a menu state is entered; the next frame re-issues the scene
local MENU_STATES = { [10] = true, [12] = true, [13] = true, [14] = true }   -- main menu, Extras, Bonuses, Options

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
            if MENU_STATES[v] then pending = true end
        end
    end, function(retval) return retval end)
end

-- run 2: the real switch
local mfm_t = sdk.find_type_definition("app.ropeway.gamemastering.MainFlowManager")
local change = mfm_t and mfm_t:get_method("changeTitleCameraScene")
if change then
    sdk.hook(change, function(args)
        local v = safe(function() return sdk.to_int64(args[3]) & 0xFFFFFFFF end)
        if v == nil then return end
        local give = v
        if cfg.enabled and v == SCENE_MAIN then give = target() end
        log_line("changeTitleCameraScene " .. name(v) .. (give ~= v and (" -> " .. name(give)) or ""))
        if v ~= SCENE_MAIN and v ~= SCENE_LATEST and v ~= cfg.learned then
            cfg.learned = v; save(); log_line("remembered last-save scene " .. name(v))
        end
        if cfg.enabled and give == state.current then
            -- already showing it: the camera would never change and the flow would wait forever (run 3). Skip the
            -- call and say "done" ONE FRAME LATER: in run 5 the callback was invoked inside the call, before the
            -- flow had started waiting, so the flow missed it. Run 6 passed MAIN through instead, and the back
            -- timeline then pulled the camera to the dark view.
            local cb = safe(function() return sdk.to_managed_object(args[4]) end)
            if cb then safe(function() cb:add_ref() end); state.deferred_cb = cb end
            log_line("  already on " .. name(give) .. ": skipped, callback " .. (cb and "deferred one frame" or "none"))
            return sdk.PreHookResult.SKIP_ORIGINAL
        end
        if give ~= v then args[3] = sdk.to_ptr(give); state.swaps = state.swaps + 1 end
        state.current = give
    end, function(retval) return retval end)
else
    log_line("changeTitleCameraScene NOT FOUND")
end

local gm_t = sdk.find_type_definition("app.ropeway.gui.GUIMaster")
for _, mn in ipairs({ "openSelectBackground", "closeSelectBackground" }) do
    local m = gm_t and gm_t:get_method(mn)
    if m then
        sdk.hook(m, function(args)
            local skip = cfg.enabled and mn == "openSelectBackground"
            log_line(mn .. (skip and " (skipped)" or ""))
            if skip then return sdk.PreHookResult.SKIP_ORIGINAL end
        end, function(retval) return retval end)
    end
end

local bg_t = sdk.find_type_definition("app.ropeway.gui.TitleBackgroundScene")
local bg_start = bg_t and bg_t:get_method("start")
if bg_start then
    sdk.hook(bg_start, function(args)
        local v = safe(function() return sdk.to_int64(args[3]) & 0xFFFFFFFF end)
        if state.sped_tl then   -- any new move plays at normal speed again
            safe(function() state.sped_tl:call("set_PlaySpeed", 1.0) end)
            safe(function() state.sped_tl:release() end)
            state.sped_tl = nil
        end
        local give = v
        if cfg.enabled and v == 0 then give = 1 end   -- start-up open -> the Story (decide) move (run 3: works)
        -- a real Story press (1) while the scene already shows: jump the move to its end, no fade (run 4)
        state.skip_decide = cfg.enabled and v == 1 and state.current == target()
        state.bg_this = state.skip_decide and args[2] or nil
        log_line("TitleBackgroundScene.start " .. tostring(v) .. (give ~= v and (" -> " .. give) or "") .. " (0 open, 1 decide, 2 back)")
        if give ~= v then args[3] = sdk.to_ptr(give) end
    end, function(retval)
        if state.skip_decide and state.bg_this then
            state.skip_decide = false
            local bg = safe(function() return sdk.to_managed_object(state.bg_this) end)
            local tl = bg and safe(function() return bg:call("get_Timeline") end)
            -- run 4: set_Frame(209) skipped the move's end event and the Story menu never came (stuck, dark).
            -- Run 5: play it fast instead, so every frame and event still happens.
            local ok = tl and safe(function() tl:call("set_PlaySpeed", STORY_MOVE_SPEED); return true end)
            state.sped_tl = ok and tl or nil
            if state.sped_tl then safe(function() state.sped_tl:add_ref() end) end
            log_line("  Story move sped up x" .. STORY_MOVE_SPEED .. ": " .. tostring(ok))
        end
        return retval
    end)
end

re.on_frame(function()
    if state.deferred_cb then
        local cb = state.deferred_cb
        state.deferred_cb = nil
        local ok = safe(function() cb:call("Invoke"); return true end)
        safe(function() cb:release() end)
        log_line("  deferred callback called: " .. tostring(ok))
    end
    if pending and cfg.enabled then
        pending = false
        local mfm = sdk.get_managed_singleton("app.ropeway.gamemastering.MainFlowManager")
        local loaded = mfm and safe(function() return mfm:call("get_IsTitleSceneEnvironmentLoaded") end)
        log_line("menu entered: re-issuing " .. name(target()) .. ", environment loaded=" .. tostring(loaded))
        if mfm then safe(function() mfm:call("changeTitleCameraScene", target(), nil) end) end
    end
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
