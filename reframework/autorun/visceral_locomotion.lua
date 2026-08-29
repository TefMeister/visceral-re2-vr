-- Visceral (RE2 VR) -- locomotion prototype, v5
--   (collision-safe aim speed  +  speed-driven run animation)
--
-- v4 gave collision-safe, de-jittered aim speed-up (amplify only the stick-forward
-- part of the game's own move). v5 adds the ANIMATION half:
--
-- The game already has walk/run/start/stop animations with built-in blends. The
-- `Jog` motion variable (bool) selects walk vs run and the game blends the
-- transition. Nobody drives it off our real speed, so fast movement looked like
-- skating. v5 MEASURES actual speed and flips `Jog` on at a threshold (2.1) and
-- off below it (with hysteresis so it doesn't flicker at the edge). The game
-- plays the smooth start-run / run / return-to-walk / natural stop for us.
-- Applies to BOTH aim and non-aim, so they share one system.
--
-- Two independent parts (each toggleable):
--   * aim_amplify: speed up aim-walk, collision-safe (v4).
--   * anim_by_speed: drive Jog from measured speed (the run animation).
--
-- Hotkeys: NUM1 enable-all  NUM7 mult -0.1  NUM9 mult +0.1  NUM0 panic
-- Sliders (menu, VR-pointer friendly): multiplier, smoothing, run threshold.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_loco]"
local NS = sdk.game_namespace
local VK_NUMPAD0, VK_NUMPAD1, VK_NUMPAD7, VK_NUMPAD9 = 0x60, 0x61, 0x67, 0x69

local cfg = {
    enabled = false,
    aim_amplify = true,        -- collision-safe speed-up, AIM-WALK ONLY
    all_movement = false,      -- normal walk/run stay fully VANILLA (user, 2026-08-29)
    anim_by_speed = false,     -- OFF: Jog couples speed+anim (jog-style + deadlock). Experimental.
    aim_speed_mult = 1.5,      -- tune so aim-walk ~matches normal WALK speed (~"1.3" feel)
    smoothing = 0.4,
    run_threshold = 2.1,       -- speed to switch into the run animation
    run_hysteresis = 0.3,      -- drop back to walk this much below the threshold
}

local state = {
    keys_ok = true, prev_keys = {},
    -- amplify tracking
    amp_have = false, amp_x = 0, amp_z = 0, ema_fwd = 0.0,
    -- speed measure tracking
    spd_have = false, spd_x = 0, spd_z = 0, last_t = 0.0, ema_speed = 0.0,
    -- jog
    jog_var = nil, jog_ok = false, jog_on = false,
    -- ui
    ui_player = false, ui_aiming = false, ui_input = "none",
}

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end
local gamepad_t = sdk.find_type_definition("via.hid.GamePad")

local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end
local function get_is_aiming(player)
    local t = sdk.typeof(NS("survivor.SurvivorCondition")); if not t then return false end
    local c = safe(function() return player:call("getComponent(System.Type)", t) end); if not c then return false end
    return safe(function() return c:call("get_IsHold") end) == true
end
local function get_transform(player) return safe(function() return player:call("get_Transform") end) end
local function get_pos(tr)
    local p = safe(function() return tr:call("get_Position") end); if not p then return nil end
    return safe(function() return p.x end), safe(function() return p.y end), safe(function() return p.z end)
end

local function acquire_jog(player)
    state.jog_var, state.jog_ok = nil, false
    local motion = safe(function() return player:call("getComponent(System.Type)", sdk.typeof("via.motion.Motion")) end)
    if not motion then return end
    local hub = safe(function() return motion:call("get_VariablesHub") end); if not hub then return end
    local count = tonumber(safe(function() return hub:call("get_VariableSum") end)) or 0
    for i = 0, math.min(count, 512) - 1 do
        local v = safe(function() return hub:call("getVariableFromIndex", i) end)
        if v and safe(function() return v:call("get_Name") end) == "Jog" then state.jog_var = v; state.jog_ok = true; return end
    end
end

local function set_jog(on)
    if not state.jog_ok then return end
    safe(function() state.jog_var:call("set_Bool", on) end)
    state.jog_on = on
end

local function get_left_axis()
    if _G.vrmod then
        local using = safe(function() return vrmod:is_using_controllers() end)
        if using then
            local a = safe(function() return vrmod:get_left_stick_axis() end)
            if a and safe(function() return a:length() end) and a:length() > 0.0 then state.ui_input = "vr"; return a end
        end
    end
    local gp = sdk.get_native_singleton("via.hid.GamePad")
    if not gp or not gamepad_t then return nil end
    local pad = safe(function() return sdk.call_native_func(gp, gamepad_t, "get_LastInputDevice") end)
    if not pad then return nil end
    local ax = safe(function() return pad:call("get_AxisL") end)
    if ax then state.ui_input = "pad" end
    return ax
end

local function intended_dir(sx, sy)
    local cam = sdk.get_primary_camera(); if not cam then return nil end
    local cgo = safe(function() return cam:call("get_GameObject") end); if not cgo then return nil end
    local ctr = safe(function() return cgo:call("get_Transform") end); if not ctr then return nil end
    local crot = safe(function() return ctr:call("get_Rotation") end); if not crot then return nil end
    local fwd = safe(function() return crot * Vector3f.new(0, 0, 1) end); if not fwd then return nil end
    local flat = Vector3f.new(fwd.x, 0.0, fwd.z); if flat:length() <= 0.0 then return nil end
    flat = flat:normalized()
    local move = flat:to_quat() * Vector3f.new(sx, 0.0, -sy)
    if move:length() <= 0.0 then return nil end
    move = move:normalized()
    return move.x, move.z
end

local function do_amplify(tr, x, y, z)
    if not state.amp_have then state.amp_x, state.amp_z = x, z; state.amp_have = true; return end
    local dx, dz = x - state.amp_x, z - state.amp_z
    if (dx * dx + dz * dz) > 25.0 then state.amp_x, state.amp_z = x, z; return end
    local ax = get_left_axis()
    local sx = ax and safe(function() return ax.x end) or 0.0
    local sy = ax and safe(function() return ax.y end) or 0.0
    local ix, iz = intended_dir(sx, sy)
    local extra = cfg.aim_speed_mult - 1.0
    if extra <= 0.0 or not ix then state.amp_x, state.amp_z = x, z; return end
    local forward = dx * ix + dz * iz
    if forward < 0.0 then forward = 0.0 end
    state.ema_fwd = state.ema_fwd + (forward - state.ema_fwd) * (1.0 - cfg.smoothing)
    local add = state.ema_fwd * extra
    local nx, nz = x + ix * add, z + iz * add
    local ok = safe(function() tr:call("set_Position", Vector3f.new(nx, y, nz)); return true end)
    if ok then state.amp_x, state.amp_z = nx, nz else state.amp_x, state.amp_z = x, z end
end

local function tick()
    local now = os.clock(); local dt = now - state.last_t; state.last_t = now
    local player = get_player()
    state.ui_player = player ~= nil
    if not player then state.amp_have = false; state.spd_have = false; return end
    local tr = get_transform(player); if not tr then return end
    local x, y, z = get_pos(tr); if x == nil then return end
    local aiming = get_is_aiming(player); state.ui_aiming = aiming
    if not state.jog_ok then acquire_jog(player) end

    -- measure real ground speed (includes last frame's amplification)
    if state.spd_have and dt > 0.0005 and dt < 0.5 then
        local sd = math.sqrt((x - state.spd_x) ^ 2 + (z - state.spd_z) ^ 2) / dt
        if sd < 50.0 then state.ema_speed = state.ema_speed * 0.7 + sd * 0.3 end
    end
    state.spd_x, state.spd_z = x, z; state.spd_have = true

    if not cfg.enabled then state.amp_have = false; return end

    -- run animation: flip Jog by measured speed, with hysteresis + built-in blend
    if cfg.anim_by_speed and state.jog_ok then
        local want = state.jog_on
        if state.ema_speed >= cfg.run_threshold then want = true
        elseif state.ema_speed < (cfg.run_threshold - cfg.run_hysteresis) then want = false end
        set_jog(want)
    end

    -- collision-safe speed-up: all movement (default) or aim-only
    if cfg.aim_amplify and (cfg.all_movement or aiming) then
        do_amplify(tr, x, y, z)
    else
        state.amp_have = false
    end
end

re.on_frame(function()
    tick()
    if not state.keys_ok then return end
    local function pressed(vk)
        local d = safe(function() return reframework:is_key_down(vk) end)
        if d == nil then state.keys_ok = false; return false end
        local w = state.prev_keys[vk]; state.prev_keys[vk] = d; return d and not w
    end
    if pressed(VK_NUMPAD1) then cfg.enabled = not cfg.enabled; log_line("enabled=" .. tostring(cfg.enabled)) end
    if pressed(VK_NUMPAD7) then cfg.aim_speed_mult = math.max(1.0, cfg.aim_speed_mult - 0.1) end
    if pressed(VK_NUMPAD9) then cfg.aim_speed_mult = math.min(5.0, cfg.aim_speed_mult + 0.1) end
    if pressed(VK_NUMPAD0) then cfg.enabled = false; log_line("PANIC off") end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: locomotion (v5, aim speed + run animation)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED", cfg.enabled)
    ch, cfg.aim_amplify = imgui.checkbox("speed-up (collision-safe)", cfg.aim_amplify)
    ch, cfg.all_movement = imgui.checkbox("apply to ALL movement (walk = aim-walk)", cfg.all_movement)
    ch, cfg.anim_by_speed = imgui.checkbox("run animation by speed (drives Jog)", cfg.anim_by_speed)
    ch, cfg.aim_speed_mult = imgui.slider_float("aim speed multiplier", cfg.aim_speed_mult, 1.0, 5.0, "%.2fx")
    ch, cfg.smoothing = imgui.slider_float("smoothing", cfg.smoothing, 0.0, 0.9, "%.2f")
    ch, cfg.run_threshold = imgui.slider_float("run animation threshold", cfg.run_threshold, 0.5, 4.0, "%.2f")
    imgui.separator()
    imgui.text("player: " .. (state.ui_player and "found" or "NO") .. "  aiming: " .. tostring(state.ui_aiming)
        .. "  input: " .. state.ui_input)
    imgui.text(string.format("measured speed: %.2f   Jog(run): %s   threshold %.2f (hys %.2f)",
        state.ema_speed, tostring(state.jog_on), cfg.run_threshold, cfg.run_hysteresis))
    imgui.text("Jog var: " .. (state.jog_ok and "acquired" or "NOT FOUND"))
    if imgui.button("PANIC off") then cfg.enabled = false end
    imgui.tree_pop()
end)

log_line("loaded (v5 aim speed + speed-driven run animation). NUM1 enable; tune sliders live.")
