-- Visceral (RE2 VR) -- motion-variable recon, v1 (READ-ONLY, AUTO-CAPTURE)
--
-- The speed recon found that the locomotion move-speed value is NOT a plain
-- property getter -- it lives as a motion USER VARIABLE. Path (confirmed from
-- the method dumps + SDK dump):
--   player -> via.motion.Motion -> get_VariablesHub() : via.userdata.UserVariablesHub
--   hub:get_VariableSum() = count; hub:getVariableFromIndex(i) : via.userdata.Variable
--   Variable: get_Name(), get_F() float, get_S()/get_U()/get_Bool(),
--             get_TypeKind(), get_ReadOnly(), get_WriteProtect(), get_Min/MaxF()
--
-- Goal: enumerate every motion variable, watch which float swings in lockstep
-- with MEASURED ground speed (aim-walk vs walk vs run), and note whether it is
-- writable -> that's the walk<->run blend / move-speed dial for req 4.
--
-- READ-ONLY: reads variable values only; never sets. Writability is only
-- REPORTED (get_ReadOnly / get_WriteProtect) for the future build.
--
-- Hotkeys (numpad; NUM3-9 = fire probe):
--   NUMPAD1 = enumerate variables (logs the full inventory) + START capture.
--   NUMPAD2 = STOP + min..max summary per variable (widest range first).
-- Protocol: NUM1, then move -- aim+walk, walk, run (a few s each) -- then NUM2.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_motvar]"
local NS = sdk.game_namespace

local VK_NUMPAD1 = 0x61
local VK_NUMPAD2 = 0x62
local STREAM_PERIOD = 0.30
local CHANGE_EPS = 0.02
local MAX_VARS = 512

local state = {
    keys_ok = true, prev_keys = {},
    have_prev = false, prev_x = 0, prev_y = 0, prev_z = 0, prev_t = 0,
    speed = 0, raw_speed = 0, speed_max_seen = 0,
    ui_player = false, ui_is_hold = "n/a",
    hub = nil,
    vars = {},          -- { v(obj), name, idx, vmin, vmax, last }
    recon_done = false, capturing = false,
    last_stream_t = 0, stream_count = 0,
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

local function vec_xyz(v)
    if v == nil then return nil end
    local x = safe(function() return v.x end); if x == nil then x = safe(function() return v:get_field("x") end) end
    local y = safe(function() return v.y end); if y == nil then y = safe(function() return v:get_field("y") end) end
    local z = safe(function() return v.z end); if z == nil then z = safe(function() return v:get_field("z") end) end
    return x, y, z
end

local function get_player_pos(player)
    local tr = safe(function() return player:call("get_Transform") end); if not tr then return nil end
    local p = safe(function() return tr:call("get_Position") end); if not p then return nil end
    return vec_xyz(p)
end

local function get_is_hold(player)
    local c = get_component(player, NS("survivor.SurvivorCondition")); if not c then return nil end
    return safe(function() return c:call("get_IsHold") end)
end

local function update_velocity()
    local player = get_player()
    state.ui_player = player ~= nil
    if not player then state.have_prev = false; return end
    local x, y, z = get_player_pos(player)
    if x == nil then return end
    local now = os.clock()
    if state.have_prev then
        local dt = now - state.prev_t
        if dt > 0.0005 and dt < 0.5 then
            local dx, dz = x - state.prev_x, z - state.prev_z
            local horiz = math.sqrt(dx * dx + dz * dz) / dt
            if horiz < 50.0 then
                state.raw_speed = horiz
                state.speed = state.speed * 0.75 + horiz * 0.25
                if state.speed > state.speed_max_seen then state.speed_max_seen = state.speed end
            end
        end
    end
    state.prev_x, state.prev_y, state.prev_z = x, y, z
    state.prev_t = now; state.have_prev = true
    state.ui_is_hold = tostring(get_is_hold(player))
end

-- read a Variable's best numeric value (prefer float)
local function var_value(v)
    local f = safe(function() return v:call("get_F") end)
    if type(f) == "number" then return f end
    local s = safe(function() return v:call("get_S") end)
    if type(s) == "number" then return s end
    local u = safe(function() return v:call("get_U") end)
    if type(u) == "number" then return u end
    local b = safe(function() return v:call("get_Bool") end)
    if type(b) == "boolean" then return b and 1 or 0 end
    return nil
end

local function enumerate_and_start()
    local player = get_player()
    if not player then log_line("NUM1: no player"); return end
    local motion = get_component(player, "via.motion.Motion")
    if not motion then log_line("NUM1: no via.motion.Motion component"); return end
    local hub = safe(function() return motion:call("get_VariablesHub") end)
    if not hub then log_line("NUM1: get_VariablesHub returned nil (variables may be on another Motion/ActorMotion)"); return end
    state.hub = hub
    local count = safe(function() return hub:call("get_VariableSum") end)
    count = tonumber(count) or 0
    log_line(string.format("==== MOTION VARIABLES (VariableSum=%d) ====", count))
    if count <= 0 or count > MAX_VARS then
        log_line("NUM1: variable count 0 or implausible (" .. tostring(count) .. ") -- hub may be the wrong one")
    end
    state.vars = {}
    local n = math.min(count, MAX_VARS)
    for i = 0, n - 1 do
        local v = safe(function() return hub:call("getVariableFromIndex", i) end)
        if v then
            local name = safe(function() return v:call("get_Name") end)
            local kind = safe(function() return v:call("get_TypeKind") end)
            local ro = safe(function() return v:call("get_ReadOnly") end)
            local wp = safe(function() return v:call("get_WriteProtect") end)
            local val = var_value(v)
            log_line(string.format("  [%3d] %-40s kind=%s val=%s RO=%s WP=%s",
                i, tostring(name), tostring(kind), tostring(val), tostring(ro), tostring(wp)))
            state.vars[#state.vars + 1] =
                { v = v, name = tostring(name), idx = i, vmin = nil, vmax = nil, last = nil }
        end
    end
    state.recon_done = true
    state.capturing = true
    state.stream_count = 0
    log_line(string.format("NUM1 done: %d variables. CAPTURE ON -- move aim+walk / walk / run, then NUM2.",
        #state.vars))
end

local function stream_tick()
    if not state.capturing then return end
    local now = os.clock()
    if now - state.last_stream_t < STREAM_PERIOD then return end
    state.last_stream_t = now
    state.stream_count = state.stream_count + 1
    local changed = {}
    for _, e in ipairs(state.vars) do
        local val = var_value(e.v)
        if type(val) == "number" then
            if e.vmin == nil or val < e.vmin then e.vmin = val end
            if e.vmax == nil or val > e.vmax then e.vmax = val end
            if e.last == nil or math.abs(val - e.last) > CHANGE_EPS then
                changed[#changed + 1] = string.format("%s=%.3f", e.name, val)
                e.last = val
            end
        end
    end
    local head = string.format("STREAM #%d speed=%.3f raw=%.3f IsHold=%s",
        state.stream_count, state.speed, state.raw_speed, state.ui_is_hold)
    if #changed > 0 then
        log_line(head .. " | " .. table.concat(changed, "  "))
    else
        log_line(head)
    end
end

local function stop_and_summarize()
    state.capturing = false
    log_line("==== CAPTURE STOPPED -- variables by range (widest first = speed-like) ====")
    local ranked = {}
    for _, e in ipairs(state.vars) do
        if e.vmin ~= nil and e.vmax ~= nil then
            ranked[#ranked + 1] = { name = e.name, vmin = e.vmin, vmax = e.vmax, range = e.vmax - e.vmin }
        end
    end
    table.sort(ranked, function(a, b) return a.range > b.range end)
    for i = 1, math.min(#ranked, 30) do
        local r = ranked[i]
        log_line(string.format("   [range %.3f]  %.3f .. %.3f   %s", r.range, r.vmin, r.vmax, r.name))
    end
    log_line(string.format("measured ground speed max seen = %.3f", state.speed_max_seen))
end

local function key_pressed(vk)
    local down = safe(function() return reframework:is_key_down(vk) end)
    if down == nil then state.keys_ok = false; return false end
    local was = state.prev_keys[vk]; state.prev_keys[vk] = down
    return down and not was
end

re.on_frame(function()
    update_velocity()
    stream_tick()
    if not state.keys_ok then return end
    if key_pressed(VK_NUMPAD1) then enumerate_and_start() end
    if key_pressed(VK_NUMPAD2) then stop_and_summarize() end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: motion-variable recon (read-only)") then return end
    imgui.text("hotkeys: NUM1=enumerate+start  NUM2=stop+summary" ..
        (state.keys_ok and "" or "  [KEY API UNAVAILABLE]"))
    imgui.text("player: " .. (state.ui_player and "found" or "NOT FOUND") .. "   IsHold: " .. state.ui_is_hold)
    imgui.text(string.format("measured speed: %.3f (max %.3f)", state.speed, state.speed_max_seen))
    imgui.text(string.format("capturing: %s  vars: %d  stream: %d",
        tostring(state.capturing), #state.vars, state.stream_count))
    if imgui.button("ENUMERATE + START (NUM1)") then enumerate_and_start() end
    if imgui.button("STOP + summary (NUM2)") then stop_and_summarize() end
    imgui.tree_pop()
end)

log_line("loaded (v1 motion-variable recon, read-only). NUM1 = enumerate+start, NUM2 = stop+summary.")
