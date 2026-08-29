-- Visceral (RE2 VR) -- locomotion prototype, v2 (collision-safe "Jog style")
--
-- v1 tried driving player position directly -> clipped through doors (dealbreaker
-- for all-movement). v2 switches to the collision-safe approach: use the game's
-- OWN movement and just flip its run switch on.
--
-- "Jog style" (default): force the motion variable `Jog` = true (set_Bool) so the
-- game's native, COLLISION-AWARE locomotion runs at run speed. No position writes,
-- so walls/doors stop you normally. Proven in the write-test (speed 1.9->3.1).
-- KNOWN LIMIT (proven): `Jog` does NOT affect the aim state -- aiming stays slow.
-- That's resolved later by the dock design (don't hold the game aim state while
-- moving in VR), not here.
--
-- "aim override" (optional, AC-style): position override ONLY while aiming, at a
-- slow tunable speed. Kept as a fallback for the aim case; low speed so clipping
-- is minimal. Off by default.
--
-- Hotkeys (numpad): NUM1 = enable/disable   NUM2 = cycle mode (jog / aim-override)
--   NUM7 = aim-override speed -0.1   NUM9 = +0.1   NUM0 = panic off
-- Or use the REFramework menu (works via the VR controller pointer).

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_loco]"
local NS = sdk.game_namespace

local VK_NUMPAD0, VK_NUMPAD1, VK_NUMPAD2 = 0x60, 0x61, 0x62
local VK_NUMPAD7, VK_NUMPAD9 = 0x67, 0x69

local cfg = {
    enabled = false,
    mode = "jog",              -- "jog" (collision-safe) or "aim_override"
    aim_speed_cap = 2.0,       -- only used in aim_override mode; tune live
}

local state = {
    keys_ok = true, prev_keys = {},
    jog_var = nil, jog_ok = false,
    last_t = 0.0, have_pos = false, our_x = 0, our_y = 0, our_z = 0,
    ui_player = false, ui_aiming = false, ui_jog = "n/a",
    ui_stick = 0.0, ui_speed = 0.0, ui_input = "none",
    write_ok = 0, write_fail = 0,
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

-- find the writable `Jog` motion variable (bool)
local function acquire_jog(player)
    state.jog_var, state.jog_ok = nil, false
    local motion = safe(function()
        local t = sdk.typeof("via.motion.Motion")
        return player:call("getComponent(System.Type)", t)
    end)
    if not motion then return false end
    local hub = safe(function() return motion:call("get_VariablesHub") end); if not hub then return false end
    local count = tonumber(safe(function() return hub:call("get_VariableSum") end)) or 0
    for i = 0, math.min(count, 512) - 1 do
        local v = safe(function() return hub:call("getVariableFromIndex", i) end)
        if v and safe(function() return v:call("get_Name") end) == "Jog" then
            state.jog_var = v; state.jog_ok = true; break
        end
    end
    log_line("acquire_jog: " .. tostring(state.jog_ok))
    return state.jog_ok
end

local function get_left_axis()
    if _G.vrmod then
        local using = safe(function() return vrmod:is_using_controllers() end)
        if using then
            local a = safe(function() return vrmod:get_left_stick_axis() end)
            if a and safe(function() return a:length() end) and a:length() > 0.0 then
                state.ui_input = "vr"; return a
            end
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

-- JOG STYLE: flip the game's run switch; native collision-aware movement
local function apply_jog()
    if not state.jog_ok then return end
    local ok = safe(function() state.jog_var:call("set_Bool", true); return true end)
    if ok then state.write_ok = state.write_ok + 1 else state.write_fail = state.write_fail + 1 end
end

-- AIM OVERRIDE: position integration while aiming only (slow, tunable)
local function apply_aim_override(player, dt)
    local tr = safe(function() return player:call("get_Transform") end); if not tr then return end
    local p = safe(function() return tr:call("get_Position") end); if not p then return end
    local px, py, pz = safe(function() return p.x end), safe(function() return p.y end), safe(function() return p.z end)
    if px == nil then return end
    if not state.have_pos then state.our_x, state.our_y, state.our_z = px, py, pz; state.have_pos = true end

    local ax = get_left_axis()
    local sx = ax and safe(function() return ax.x end) or 0.0
    local sy = ax and safe(function() return ax.y end) or 0.0
    local mag = math.min(math.sqrt(sx * sx + sy * sy), 1.0)
    state.ui_stick = mag
    if mag <= 0.05 then state.our_x, state.our_y, state.our_z = px, py, pz; state.ui_speed = 0; return end

    local cam = sdk.get_primary_camera(); if not cam then return end
    local cgo = safe(function() return cam:call("get_GameObject") end); if not cgo then return end
    local ctr = safe(function() return cgo:call("get_Transform") end); if not ctr then return end
    local crot = safe(function() return ctr:call("get_Rotation") end); if not crot then return end
    local fwd = safe(function() return crot * Vector3f.new(0, 0, 1) end); if not fwd then return end
    local flat = Vector3f.new(fwd.x, 0.0, fwd.z); if flat:length() <= 0.0 then return end
    flat = flat:normalized()
    local move = flat:to_quat() * Vector3f.new(sx, 0.0, -sy); if move:length() <= 0.0 then return end
    move = move:normalized()
    local step = mag * cfg.aim_speed_cap * dt
    state.ui_speed = mag * cfg.aim_speed_cap
    local nx, nz = state.our_x + move.x * step, state.our_z + move.z * step
    local ok = safe(function() tr:call("set_Position", Vector3f.new(nx, py, nz)); return true end)
    if ok then state.our_x, state.our_y, state.our_z = nx, py, nz end
end

local function tick()
    local now = os.clock(); local dt = now - state.last_t; state.last_t = now
    local player = get_player()
    state.ui_player = player ~= nil
    if not player then state.have_pos = false; return end
    if not state.jog_ok then acquire_jog(player) end
    local aiming = get_is_aiming(player); state.ui_aiming = aiming
    if state.jog_var then state.ui_jog = tostring(safe(function() return state.jog_var:call("get_Bool") end)) end

    if not cfg.enabled then state.have_pos = false; return end
    if dt <= 0 or dt > 0.5 then state.have_pos = false; return end

    if cfg.mode == "jog" then
        apply_jog()
        state.have_pos = false
    elseif cfg.mode == "aim_override" then
        if aiming then apply_aim_override(player, dt) else state.have_pos = false; state.ui_stick = 0 end
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
    if pressed(VK_NUMPAD2) then cfg.mode = (cfg.mode == "jog") and "aim_override" or "jog"; log_line("mode=" .. cfg.mode) end
    if pressed(VK_NUMPAD7) then cfg.aim_speed_cap = math.max(0.2, cfg.aim_speed_cap - 0.1) end
    if pressed(VK_NUMPAD9) then cfg.aim_speed_cap = math.min(6.0, cfg.aim_speed_cap + 0.1) end
    if pressed(VK_NUMPAD0) then cfg.enabled = false; log_line("PANIC off") end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: locomotion (v2, collision-safe jog)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED", cfg.enabled)
    imgui.text("mode: " .. cfg.mode .. "  (NUM2 to switch)")
    if imgui.button("mode: JOG style (collision-safe, run speed)") then cfg.mode = "jog" end
    if imgui.button("mode: aim-override (position, slow, tunable)") then cfg.mode = "aim_override" end
    ch, cfg.aim_speed_cap = imgui.slider_float("aim-override speed", cfg.aim_speed_cap, 0.2, 6.0, "%.2f")
    imgui.separator()
    imgui.text("player: " .. (state.ui_player and "found" or "NO") .. "  aiming: " .. tostring(state.ui_aiming)
        .. "  Jog(live): " .. state.ui_jog)
    imgui.text("Jog var: " .. (state.jog_ok and "acquired" or "NOT FOUND")
        .. string.format("  writes ok=%d fail=%d", state.write_ok, state.write_fail))
    imgui.text("input: " .. state.ui_input .. string.format("  stick: %.2f  aim-target: %.2f/s", state.ui_stick, state.ui_speed))
    imgui.text("NOTE: JOG does not speed up aiming (proven) -- aim stays slow until the dock design.")
    if imgui.button("PANIC off") then cfg.enabled = false end
    imgui.tree_pop()
end)

log_line("loaded (v2 collision-safe jog locomotion). NUM1 enable, NUM2 mode, NUM0 panic.")
