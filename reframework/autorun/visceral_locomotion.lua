-- Visceral (RE2 VR) -- smooth locomotion prototype, v1 (WRITTEN FRESH)
--
-- Purpose: tune the movement feel LIVE in the headset. The flat-measured speed
-- numbers do not translate to VR, so speed is a live slider here.
--
-- Approach (studied from AC, written from scratch per the no-copy rule):
-- own the player's world position and integrate it from stick input, camera-
-- relative. `speed = stick_mag * SPEED_CAP * dt`. ONE cap for everything, so
-- aim and non-aim feel identical -- nothing to "match".
--
-- Two modes so you can feel the tradeoff:
--   * "aim only" (default, SAFE): we only drive position while AIMING (bypasses
--     the aim-walk throttle); normal movement stays the game's own, so wall
--     COLLISION is preserved during normal play. This is AC's proven shape.
--   * "all movement": we drive position for everything -> perfectly consistent
--     speed, BUT direct position writes bypass collision (can clip walls). Try
--     it, feel it, decide.
--
-- Live tuning: REFramework menu slider (usable via the VR controller pointer)
-- + nudge buttons. Hotkeys if you have a keyboard:
--   NUMPAD1 = enable/disable    NUMPAD2 = toggle aim-only / all-movement
--   NUMPAD7 = speed -0.1        NUMPAD9 = speed +0.1        NUMPAD0 = panic off

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_loco]"
local NS = sdk.game_namespace

local VK_NUMPAD0 = 0x60
local VK_NUMPAD1 = 0x61
local VK_NUMPAD2 = 0x62
local VK_NUMPAD7 = 0x67
local VK_NUMPAD9 = 0x69

local cfg = {
    enabled = false,
    all_movement = false,     -- false = aim-only (safe), true = all movement
    speed_cap = 2.0,          -- START point per user; tune live
}

local state = {
    keys_ok = true, prev_keys = {},
    last_t = 0.0,
    have_pos = false,
    our_x = 0, our_y = 0, our_z = 0,   -- our authoritative integrated position
    -- readouts
    ui_player = false, ui_aiming = false,
    ui_stick = 0.0, ui_speed = 0.0,
    ui_input_src = "none",
}

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local gamepad_t = sdk.find_type_definition("via.hid.GamePad")

local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager"))
    if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end

local function get_is_aiming(player)
    local t = sdk.typeof(NS("survivor.SurvivorCondition"))
    if not t then return false end
    local c = safe(function() return player:call("getComponent(System.Type)", t) end)
    if not c then return false end
    return safe(function() return c:call("get_IsHold") end) == true
end

-- left stick as Vector2f (x,y); tries VR controllers, then the game pad
local function get_left_axis()
    if _G.vrmod then
        local using = safe(function() return vrmod:is_using_controllers() end)
        if using then
            local a = safe(function() return vrmod:get_left_stick_axis() end)
            if a and safe(function() return a:length() end) and a:length() > 0.0 then
                state.ui_input_src = "vr"
                return a
            end
        end
    end
    local gp = sdk.get_native_singleton("via.hid.GamePad")
    if not gp or not gamepad_t then return nil end
    local pad = safe(function() return sdk.call_native_func(gp, gamepad_t, "get_LastInputDevice") end)
    if not pad then return nil end
    local ax = safe(function() return pad:call("get_AxisL") end)
    if ax then state.ui_input_src = "pad" end
    return ax
end

local function get_pos(player)
    local tr = safe(function() return player:call("get_Transform") end); if not tr then return nil end
    local p = safe(function() return tr:call("get_Position") end); if not p then return nil, tr end
    local x = safe(function() return p.x end); local y = safe(function() return p.y end); local z = safe(function() return p.z end)
    return { x = x, y = y, z = z }, tr
end

local function drive_locomotion()
    local now = os.clock()
    local dt = now - state.last_t
    state.last_t = now
    if dt <= 0.0 or dt > 0.5 then state.have_pos = false; return end

    local player = get_player()
    state.ui_player = player ~= nil
    if not player then state.have_pos = false; return end

    local pos, tr = get_pos(player)
    if not pos or pos.x == nil or not tr then state.have_pos = false; return end

    local aiming = get_is_aiming(player)
    state.ui_aiming = aiming

    -- decide whether WE drive position this frame
    local we_drive = cfg.enabled and (cfg.all_movement or aiming)

    if not we_drive then
        -- keep our authoritative position synced so there's no jump when we take over
        state.our_x, state.our_y, state.our_z = pos.x, pos.y, pos.z
        state.have_pos = true
        state.ui_stick = 0.0
        return
    end

    local ax = get_left_axis()
    local sx = ax and safe(function() return ax.x end) or 0.0
    local sy = ax and safe(function() return ax.y end) or 0.0
    local stick_mag = math.min(math.sqrt(sx * sx + sy * sy), 1.0)
    state.ui_stick = stick_mag

    if not state.have_pos then
        state.our_x, state.our_y, state.our_z = pos.x, pos.y, pos.z
        state.have_pos = true
    end

    if stick_mag <= 0.05 then
        -- standing still: hold our position in sync with the game
        state.our_x, state.our_y, state.our_z = pos.x, pos.y, pos.z
        state.ui_speed = 0.0
        return
    end

    -- camera-relative movement direction
    local cam = sdk.get_primary_camera()
    if not cam then return end
    local cam_go = safe(function() return cam:call("get_GameObject") end); if not cam_go then return end
    local cam_tr = safe(function() return cam_go:call("get_Transform") end); if not cam_tr then return end
    local cam_rot = safe(function() return cam_tr:call("get_Rotation") end); if not cam_rot then return end

    local fwd = safe(function() return cam_rot * Vector3f.new(0, 0, 1) end)
    if not fwd then return end
    local flat = Vector3f.new(fwd.x, 0.0, fwd.z)
    if flat:length() <= 0.0 then return end
    flat = flat:normalized()
    local flat_rot = flat:to_quat()
    local move = flat_rot * Vector3f.new(sx, 0.0, -sy)
    if move:length() <= 0.0 then return end
    move = move:normalized()

    local speed = stick_mag * cfg.speed_cap * dt
    state.ui_speed = stick_mag * cfg.speed_cap   -- per-second, for the readout

    local nx = state.our_x + move.x * speed
    local nz = state.our_z + move.z * speed
    local ny = pos.y   -- keep the game's vertical (gravity/stairs)

    local ok = safe(function() tr:call("set_Position", Vector3f.new(nx, ny, nz)); return true end)
    if ok then
        state.our_x, state.our_y, state.our_z = nx, ny, nz
    end
end

re.on_frame(function()
    drive_locomotion()
    if not state.keys_ok then return end
    local function pressed(vk)
        local d = safe(function() return reframework:is_key_down(vk) end)
        if d == nil then state.keys_ok = false; return false end
        local w = state.prev_keys[vk]; state.prev_keys[vk] = d; return d and not w
    end
    if pressed(VK_NUMPAD1) then cfg.enabled = not cfg.enabled; log_line("enabled=" .. tostring(cfg.enabled)) end
    if pressed(VK_NUMPAD2) then cfg.all_movement = not cfg.all_movement; log_line("all_movement=" .. tostring(cfg.all_movement)) end
    if pressed(VK_NUMPAD7) then cfg.speed_cap = math.max(0.2, cfg.speed_cap - 0.1); log_line("speed_cap=" .. string.format("%.2f", cfg.speed_cap)) end
    if pressed(VK_NUMPAD9) then cfg.speed_cap = math.min(6.0, cfg.speed_cap + 0.1); log_line("speed_cap=" .. string.format("%.2f", cfg.speed_cap)) end
    if pressed(VK_NUMPAD0) then cfg.enabled = false; log_line("PANIC: disabled") end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: smooth locomotion (prototype)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED", cfg.enabled)
    ch, cfg.all_movement = imgui.checkbox("apply to ALL movement (off = aim-only, keeps collision)", cfg.all_movement)
    ch, cfg.speed_cap = imgui.slider_float("speed cap", cfg.speed_cap, 0.2, 6.0, "%.2f")
    imgui.text("hotkeys: NUM1=enable NUM2=aim/all NUM7=-0.1 NUM9=+0.1 NUM0=panic"
        .. (state.keys_ok and "" or "  [KEYS UNAVAILABLE - use controls above]"))
    imgui.separator()
    imgui.text("player: " .. (state.ui_player and "found" or "NOT FOUND")
        .. "   aiming: " .. tostring(state.ui_aiming))
    imgui.text("mode: " .. (cfg.all_movement and "ALL movement (no collision!)" or "aim-only (safe)")
        .. "   input: " .. state.ui_input_src)
    imgui.text(string.format("stick: %.2f   speed cap: %.2f   target speed: %.2f/s",
        state.ui_stick, cfg.speed_cap, state.ui_speed))
    if imgui.button("speed -0.1") then cfg.speed_cap = math.max(0.2, cfg.speed_cap - 0.1) end
    imgui.same_line(); if imgui.button("speed +0.1") then cfg.speed_cap = math.min(6.0, cfg.speed_cap + 0.1) end
    if imgui.button("PANIC off") then cfg.enabled = false end
    imgui.tree_pop()
end)

log_line("loaded (v1 smooth locomotion prototype). Enable in the menu or NUM1; tune the slider live.")
