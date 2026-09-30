-- Visceral (RE2 VR) -- PROBE: is the title rain the Story menu's SUB CAMERA? (2026-09-30, /pd, static, unrun)
--
-- With visceral_title_one_scene.lua the last-save scene shows behind every menu, but the falling rain only
-- appears on the Story page (title flow state 11); the main menu shows splashes only. Six probe rounds on
-- 2026-09-27 ruled out the lamps, every effect player, the Story menu's own effect, and keeping the Story
-- screen itself alive. What was left untried: app.ropeway.gui.MenuStoryBehavior switches to a "sub camera"
-- when it opens (toSubCamera / outSubCamera, CameraStateValue TO_SUB / OUT_SUB) [inferred-static from the
-- type dump]. If the falling rain is drawn by, or attached to, that sub camera, then the main menu will never
-- rain until it uses the same camera.
--
-- What this does:
--   * logs every toSubCamera / outSubCamera / setOutSubCamera / set_CameraStateValue call (who, when)
--   * logs the primary camera's object name whenever it changes (does Story really swap cameras?)
--   * keeps the Story menu object once it has been seen (it is created at boot, before Story is pressed)
--   * NUM8 on the MAIN MENU: call toSubCamera() ourselves; NUM8 again: outSubCamera()
--     rain appears  -> the sub camera is it; the fix is to enter it for the main menu too (next round)
--     no rain       -> not the camera; next suspect is IsCheckLatestLocation / the Story menu's own timeline
--
-- Probe: archive it once it has answered (standing rule).

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_subcam]"
local VK_NUMPAD8 = 0x68
local CAM_NAMES = { [0] = "INVALID", "TO_SUB", "OUT_SUB" }

local st = { menu = nil, key8 = false, in_sub = false, cam_name = nil, last_log = 0, last_state = nil }

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end
local function remember(args)
    local obj = safe(function() return sdk.to_managed_object(args[2]) end)
    if obj and st.menu ~= obj then
        if not st.menu then log_line("Story menu object seen") end
        st.menu = obj
    end
end
local function cam_state()
    if not st.menu then return "?" end
    local v = safe(function() return st.menu:call("get_CameraStateValue") end)
    return v and (CAM_NAMES[v] or tostring(v)) or "?"
end

local t = sdk.find_type_definition("app.ropeway.gui.MenuStoryBehavior")
if not t then
    log_line("MenuStoryBehavior NOT FOUND")
    return
end
local function hook_named(name, note)
    local m = t:get_method(name)
    if not m then log_line(name .. " not found"); return end
    sdk.hook(m, function(args)
        remember(args)
        if note then log_line(name .. " called by the game (camera state was " .. cam_state() .. ")") end
    end, function(retval)
        if note then log_line(name .. " done (camera state now " .. cam_state() .. ")") end
        return retval
    end)
end
hook_named("toSubCamera", true)
hook_named("outSubCamera", true)
hook_named("setOutSubCamera", true)
hook_named("open", true)
hook_named("close", true)
hook_named("update", false)   -- only to catch the object early
local set_state = t:get_method("set_CameraStateValue")
if set_state then
    sdk.hook(set_state, function(args)
        remember(args)
        local v = safe(function() return sdk.to_int64(args[3]) & 0xFFFFFFFF end)
        log_line("camera state -> " .. (v and (CAM_NAMES[v] or tostring(v)) or "?"))
    end, function(retval) return retval end)
end

re.on_frame(function()
    -- which camera is drawing, logged on change
    local cam = sdk.get_primary_camera()
    local go = cam and safe(function() return cam:call("get_GameObject") end)
    local name = go and safe(function() return go:call("get_Name") end) or "?"
    if name ~= st.cam_name then
        log_line("primary camera is now '" .. tostring(name) .. "'")
        st.cam_name = name
    end

    local k8 = reframework:is_key_down(VK_NUMPAD8)
    if k8 and not st.key8 then
        if not st.menu then
            log_line("NUM8: Story menu object not seen yet (open Story once, back out, then try again)")
        elseif not st.in_sub then
            local ok = pcall(function() st.menu:call("toSubCamera") end)
            st.in_sub = ok
            log_line("NUM8: toSubCamera() " .. (ok and "sent" or "FAILED") .. "; camera state " .. cam_state() .. "; look for falling rain")
        else
            local ok = pcall(function() st.menu:call("outSubCamera") end)
            st.in_sub = false
            log_line("NUM8: outSubCamera() " .. (ok and "sent" or "FAILED") .. "; camera state " .. cam_state())
        end
    end
    st.key8 = k8

    local now = os.clock()
    if now - st.last_log < 2.0 then return end
    st.last_log = now
    local s = cam_state()
    if s ~= st.last_state then
        local latest = st.menu and safe(function() return st.menu:get_field("IsCheckLatestLocation") end)
        log_line("camera state " .. s .. "  IsCheckLatestLocation=" .. tostring(latest))
        st.last_state = s
    end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral title sub-camera probe") then return end
    imgui.text("Story menu object: " .. (st.menu and "seen" or "not yet") .. "  camera state: " .. cam_state() .. "  primary camera: " .. tostring(st.cam_name))
    imgui.text("NUM8 = toSubCamera / outSubCamera on the main menu")
    imgui.tree_pop()
end)

log_line("loaded: NUM8 = sub camera on/off on the main menu")
