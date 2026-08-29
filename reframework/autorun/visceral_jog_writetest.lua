-- Visceral (RE2 VR) -- Jog write-test, v1 (FIRST WRITE -- reversible)
--
-- Motion-var recon proved locomotion is driven by ~47 writable named variables
-- on player -> via.motion.Motion -> get_VariablesHub(). `Jog` is the walk<->run
-- selector (0=walk ~1.9 u/s, 1=jog/run ~3.0-3.8). This probe tests the req-4
-- lever: force `Jog`=1 (per-frame) and watch whether it raises movement speed --
-- ESPECIALLY while aiming (docked state), and whether the animation looks right.
--
-- via.userdata.Variable setters: set_F(float), set_Bool(bool).
-- WRITE is limited to Jog and MoveStickPower, both documented writable
-- (RO=false, WP=false). Fully reversible: toggles off + NUM0 panic; the game
-- also recomputes these every frame, so stopping the write restores vanilla.
--
-- Hotkeys (numpad; NUM3-9 = fire probe; recon probes retired):
--   NUMPAD1 = toggle FORCE Jog=1 (per-frame). Move + aim to see the effect.
--   NUMPAD2 = toggle FORCE MoveStickPower=1 (analog input; 0 on KB/M vanilla).
--   NUMPAD0 = PANIC: stop all forcing.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_jog]"
local NS = sdk.game_namespace

local VK_NUMPAD0 = 0x60
local VK_NUMPAD1 = 0x61
local VK_NUMPAD2 = 0x62

local state = {
    keys_ok = true, prev_keys = {},
    force_jog = false, force_msp = false,
    jog_var = nil, msp_var = nil,
    vars_ok = false,
    write_ok = 0, write_fail = 0, last_err = "",
    -- velocity readout
    have_prev = false, prev_x = 0, prev_z = 0, prev_t = 0,
    speed = 0, speed_max = 0,
    ui_player = false, ui_is_hold = "n/a",
    ui_jog = "n/a", ui_msp = "n/a",
}

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager"))
    if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end

local function get_component(go, tn)
    local t = sdk.typeof(tn); if not t then return nil end
    return safe(function() return go:call("getComponent(System.Type)", t) end)
end

local function get_is_hold(player)
    local c = get_component(player, NS("survivor.SurvivorCondition")); if not c then return nil end
    return safe(function() return c:call("get_IsHold") end)
end

local function vec_xz(player)
    local tr = safe(function() return player:call("get_Transform") end); if not tr then return nil end
    local p = safe(function() return tr:call("get_Position") end); if not p then return nil end
    local x = safe(function() return p.x end); if x == nil then x = safe(function() return p:get_field("x") end) end
    local z = safe(function() return p.z end); if z == nil then z = safe(function() return p:get_field("z") end) end
    return x, z
end

-- locate the Jog and MoveStickPower Variable objects by name
local function acquire_vars()
    state.jog_var, state.msp_var, state.vars_ok = nil, nil, false
    local player = get_player(); if not player then return false end
    local motion = get_component(player, "via.motion.Motion"); if not motion then return false end
    local hub = safe(function() return motion:call("get_VariablesHub") end); if not hub then return false end
    local count = tonumber(safe(function() return hub:call("get_VariableSum") end)) or 0
    for i = 0, math.min(count, 512) - 1 do
        local v = safe(function() return hub:call("getVariableFromIndex", i) end)
        if v then
            local name = safe(function() return v:call("get_Name") end)
            if name == "Jog" then state.jog_var = v end
            if name == "MoveStickPower" then state.msp_var = v end
        end
    end
    state.vars_ok = (state.jog_var ~= nil)
    log_line(string.format("acquire_vars: Jog=%s MoveStickPower=%s",
        tostring(state.jog_var ~= nil), tostring(state.msp_var ~= nil)))
    return state.vars_ok
end

local function set_var(v, value)
    if not v then return end
    local ok = safe(function() v:call("set_F", value); return true end)
    if not ok then ok = safe(function() v:call("set_Bool", value ~= 0); return true end) end
    if ok then state.write_ok = state.write_ok + 1
    else state.write_fail = state.write_fail + 1; state.last_err = "set failed" end
end

-- write the forces; called at the proven ordering point + on_frame as backup
local function apply_forces()
    if not (state.force_jog or state.force_msp) then return end
    if not state.vars_ok then acquire_vars() end
    if state.force_jog then set_var(state.jog_var, 1.0) end
    if state.force_msp then set_var(state.msp_var, 1.0) end
end

-- after the game recomputes behavior/input, before motion consumes it
re.on_application_entry("UpdateBehavior", apply_forces)

local function update_readouts()
    local player = get_player()
    state.ui_player = player ~= nil
    if not player then state.have_prev = false; return end
    local x, z = vec_xz(player)
    if x ~= nil then
        local now = os.clock()
        if state.have_prev then
            local dt = now - state.prev_t
            if dt > 0.0005 and dt < 0.5 then
                local dx, dz = x - state.prev_x, z - state.prev_z
                local sp = math.sqrt(dx * dx + dz * dz) / dt
                if sp < 50.0 then
                    state.speed = state.speed * 0.75 + sp * 0.25
                    if state.speed > state.speed_max then state.speed_max = state.speed end
                end
            end
        end
        state.prev_x, state.prev_z, state.prev_t = x, z, now; state.have_prev = true
    end
    state.ui_is_hold = tostring(get_is_hold(player))
    if state.jog_var then state.ui_jog = tostring(safe(function() return state.jog_var:call("get_F") end)) end
    if state.msp_var then state.ui_msp = tostring(safe(function() return state.msp_var:call("get_F") end)) end
end

local function key_pressed(vk)
    local down = safe(function() return reframework:is_key_down(vk) end)
    if down == nil then state.keys_ok = false; return false end
    local was = state.prev_keys[vk]; state.prev_keys[vk] = down
    return down and not was
end

local function panic()
    state.force_jog, state.force_msp = false, false
    if state.jog_var then set_var(state.jog_var, 0.0) end
    log_line("PANIC: all forcing OFF")
end

re.on_frame(function()
    update_readouts()
    apply_forces()   -- backup write path
    if not state.keys_ok then return end
    if key_pressed(VK_NUMPAD1) then
        if not state.vars_ok then acquire_vars() end
        state.force_jog = not state.force_jog
        log_line("FORCE Jog=1 " .. (state.force_jog and "ON" or "OFF"))
    end
    if key_pressed(VK_NUMPAD2) then
        if not state.vars_ok then acquire_vars() end
        state.force_msp = not state.force_msp
        log_line("FORCE MoveStickPower=1 " .. (state.force_msp and "ON" or "OFF"))
    end
    if key_pressed(VK_NUMPAD0) then panic() end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: Jog write-test (WRITE, reversible)") then return end
    imgui.text("hotkeys: NUM1=force Jog=1  NUM2=force MoveStickPower=1  NUM0=panic" ..
        (state.keys_ok and "" or "  [KEY API UNAVAILABLE]"))
    imgui.text("player: " .. (state.ui_player and "found" or "NOT FOUND") .. "   IsHold: " .. state.ui_is_hold)
    imgui.text(string.format("FORCE Jog: %s    FORCE MoveStickPower: %s",
        tostring(state.force_jog), tostring(state.force_msp)))
    imgui.text(string.format("live Jog=%s  MoveStickPower=%s", state.ui_jog, state.ui_msp))
    imgui.text(string.format("measured speed: %.3f  (max %.3f)", state.speed, state.speed_max))
    imgui.text(string.format("writes ok=%d fail=%d  %s", state.write_ok, state.write_fail, state.last_err))
    if imgui.button("toggle FORCE Jog=1 (NUM1)") then
        if not state.vars_ok then acquire_vars() end
        state.force_jog = not state.force_jog
    end
    if imgui.button("toggle FORCE MoveStickPower=1 (NUM2)") then
        if not state.vars_ok then acquire_vars() end
        state.force_msp = not state.force_msp
    end
    if imgui.button("PANIC (NUM0)") then panic() end
    if imgui.button("reset max") then state.speed_max = 0 end
    imgui.tree_pop()
end)

log_line("loaded (v1 Jog write-test). NUM1=force Jog, NUM2=force MoveStickPower, NUM0=panic. All off until used.")
