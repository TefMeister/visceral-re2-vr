-- Visceral (RE2 VR) -- one title background: the last-save scene from launch (idea 5010, 2026-09-27)
--
-- Tefa: "on launch, there is only the background that shows the last saved location" -- the moving scene that
-- normally appears only after pressing Story should be the title, Options and Extras background too.
--
-- How the title screen works [verified-live 2026-09-27, rounds 1-9, see
-- modding-notes/2026-09-27-one-title-background-nine-rounds.md]:
--   launch : MainFlowManager.changeTitleCameraScene(MAIN=0, cb) + TitleBackgroundScene.start(0 open)
--   Story  : TitleBackgroundScene.start(1 decide) -> flow 11 -> changeTitleCameraScene(LATEST=11)
--   back   : changeTitleCameraScene(MAIN, cb) -> flow 10 -> TitleBackgroundScene.start(2 back)
-- The game never names the place; it resolves LATEST itself. TitleBackgroundScene is not a GUI layer: its timeline
-- drives the 3D title scene (decide fades to black, back returns the camera to the MAIN view). The flow waits on
-- changeTitleCameraScene's callback when backing out.
--
-- Round 10 (this version):
--   * launch: MAIN -> LATEST, and the open move -> the decide move (rounds 3-7: last-save scene behind the menu)
--   * Story : untouched -- the game's own LATEST request is what brings the scene back after the decide fade
--   * back  : MAIN -> LATEST while LATEST already shows would never complete, so the call is skipped and its
--             callback invoked one frame later (round 7: menus work); then, once the back move has ended, a real
--             changeTitleCameraScene(LATEST) cuts the camera back from the MAIN view the back move left it on
--
-- Round 10 (VR): everything works; the only fault left is the old main-menu picture showing where the fade to
-- black was. Round 11: the Story (decide) and back moves are skipped while the scene shows, their callbacks handed
-- over a frame later, and the game's LATEST request right after Story is skipped the same way. The 0.5 s re-cut stays
-- only as a fallback for a back move that was not skipped.
--
-- Hotkey: NUM9 = swap on/off

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_title]"
local VK_NUMPAD9 = 0x69
local SCENE_MAIN, SCENE_LATEST = 0, 11
local MOVE_OPEN, MOVE_DECIDE, MOVE_BACK = 0, 1, 2
local BACK_MOVE_WAIT = 0.5   -- seconds after the back move starts before cutting back (the move is 15 frames)
local SAVE_FILE = "visceral_title_one_scene.json"
local NAMES = { [0] = "MAIN", "GAS_STATION", "OPENING", "RPD", "RPD_UNDERGROUND", "WASTE_WATER", "WATER_PLANT",
                "ORPHAN_ASYLUM", "ORPHAN_APPROACH", "LABORATORY", "TRANSPORTATION", "LATEST" }

local cfg = { enabled = true }
local state = { current = -1, deferred_cb = nil, recut_at = nil, own_call = false, skip_story_change = false, deferred_cb2 = nil, last_flow = -1, key_prev = false }

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end
local function name(v) return (NAMES[v] or "?") .. "(" .. tostring(v) .. ")" end

local saved = safe(function() return json.load_file(SAVE_FILE) end)
if type(saved) == "table" and saved.enabled ~= nil then cfg.enabled = saved.enabled end
local function save() safe(function() json.dump_file(SAVE_FILE, { enabled = cfg.enabled }) end) end
log_line("round 10 loaded: swap " .. (cfg.enabled and "ON" or "OFF"))

local function mfm() return sdk.get_managed_singleton("app.ropeway.gamemastering.MainFlowManager") end

-- the scene switch
local mfm_t = sdk.find_type_definition("app.ropeway.gamemastering.MainFlowManager")
local change = mfm_t and mfm_t:get_method("changeTitleCameraScene")
if change then
    sdk.hook(change, function(args)
        local v = safe(function() return sdk.to_int64(args[3]) & 0xFFFFFFFF end)
        if v == nil then return end
        if state.own_call then
            log_line("changeTitleCameraScene " .. name(v) .. " (ours)")
            state.current = v
            return
        end
        if not cfg.enabled then
            log_line("changeTitleCameraScene " .. name(v) .. " (swap off)")
            state.current = v
            return
        end
        if v == SCENE_MAIN then
            if state.current == SCENE_LATEST then
                local cb = safe(function() return sdk.to_managed_object(args[4]) end)
                if cb then safe(function() cb:add_ref() end); state.deferred_cb = cb end
                log_line("changeTitleCameraScene MAIN: LATEST already set, skipped, callback " .. (cb and "next frame" or "none"))
                return sdk.PreHookResult.SKIP_ORIGINAL
            end
            args[3] = sdk.to_ptr(SCENE_LATEST)
            log_line("changeTitleCameraScene MAIN -> LATEST")
            state.current = SCENE_LATEST
            return
        end
        if v == SCENE_LATEST and state.skip_story_change then
            -- round 11: the Story move was skipped, so the scene never left; the game's re-request would fade it
            state.skip_story_change = false
            local cb = safe(function() return sdk.to_managed_object(args[4]) end)
            if cb then safe(function() cb:add_ref() end); state.deferred_cb2 = cb end
            log_line("changeTitleCameraScene LATEST after Story: already showing, skipped, callback " .. (cb and "next frame" or "none"))
            return sdk.PreHookResult.SKIP_ORIGINAL
        end
        log_line("changeTitleCameraScene " .. name(v) .. " (game's own, untouched)")
        state.current = v
    end, function(retval) return retval end)
else
    log_line("changeTitleCameraScene NOT FOUND")
end

-- the title scene's timeline moves
local bg_t = sdk.find_type_definition("app.ropeway.gui.TitleBackgroundScene")
local bg_start = bg_t and bg_t:get_method("start")
if bg_start then
    sdk.hook(bg_start, function(args)
        local v = safe(function() return sdk.to_int64(args[3]) & 0xFFFFFFFF end)
        if cfg.enabled and v == MOVE_OPEN then
            args[3] = sdk.to_ptr(MOVE_DECIDE)
            log_line("title move open -> decide")
        elseif cfg.enabled and (v == MOVE_DECIDE or v == MOVE_BACK) and state.current == SCENE_LATEST then
            -- round 10: both moves swing the camera through the old MAIN view before the scene comes back. Skip the
            -- move and hand the flow its "move finished" callback one frame later, as for the scene change.
            local cb = safe(function() return sdk.to_managed_object(args[4]) end)
            if cb then safe(function() cb:add_ref() end); state.deferred_cb = cb end
            if v == MOVE_DECIDE then state.skip_story_change = true end
            log_line("title move " .. tostring(v) .. " skipped, callback " .. (cb and "next frame" or "none"))
            return sdk.PreHookResult.SKIP_ORIGINAL
        elseif cfg.enabled and v == MOVE_BACK then
            state.recut_at = os.clock() + BACK_MOVE_WAIT
            log_line("title move back: cutting back to LATEST in " .. BACK_MOVE_WAIT .. " s")
        else
            log_line("title move " .. tostring(v))
        end
    end, function(retval) return retval end)
end

-- title flow states, for the log
local flow_t = sdk.find_type_definition("app.ropeway.gamemastering.TitleFlow")
local flow_set = flow_t and flow_t:get_method("set_StateValue")
if flow_set then
    sdk.hook(flow_set, function(args)
        local v = safe(function() return sdk.to_int64(args[3]) & 0xFFFFFFFF end)
        if v and v ~= state.last_flow and v ~= 100 then log_line("title flow state " .. tostring(v)) end
        if v then state.last_flow = v end
    end, function(retval) return retval end)
end

re.on_frame(function()
    if state.deferred_cb then
        local cb = state.deferred_cb
        state.deferred_cb = nil
        local ok = safe(function() cb:call("Invoke"); return true end)
        safe(function() cb:release() end)
        log_line("  callback called one frame later: " .. tostring(ok))
    end
    if state.deferred_cb2 then
        local cb = state.deferred_cb2
        state.deferred_cb2 = nil
        local ok = safe(function() cb:call("Invoke"); return true end)
        safe(function() cb:release() end)
        log_line("  second callback called one frame later: " .. tostring(ok))
    end
    if state.recut_at and os.clock() >= state.recut_at then
        state.recut_at = nil
        local m = mfm()
        state.own_call = true
        local ok = m and safe(function() m:call("changeTitleCameraScene", SCENE_LATEST, nil); return true end)
        state.own_call = false
        log_line("  cut back to LATEST after the back move: " .. tostring(ok))
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
        imgui.text("Scene set last: " .. name(state.current))
        imgui.tree_pop()
    end
end)
