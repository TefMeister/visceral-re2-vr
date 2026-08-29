-- Visceral (RE2 VR) -- movement-speed recon, v2 (READ-ONLY, AUTO-CAPTURE)
--
-- v1 got the recon dump (component list + 59 float getters) but manual NUM2
-- samples never logged, so v2 removes the per-sample keypress: it streams while
-- you move and tracks each value's min..max, so you just walk through your
-- movement states and the log captures everything.
--
-- Goal (design-spec req 4): find the per-state speed caps (aim-walk vs walk vs
-- run) and which managed float tracks speed / feeds the locomotion anim blend
-- (via.motion.MotionFsm2 is the FSM; SurvivorMotionSpeedController holds
-- modifiers; footsteps are velocity-driven per WwiseVelocityTriggerList).
--
-- READ-ONLY: no hooks, no writes.
--
-- Hotkeys (numpad; NUM3-9 belong to the fire probe):
--   NUMPAD1 = recon + START auto-capture (dumps components, collects float
--             getters, then begins streaming to the log).
--   NUMPAD2 = STOP + print a min..max summary per getter (widest-range first
--             = most likely the speed/blend dial).
-- Protocol: NUM1, then hold each movement state a few seconds -- aim+walk,
-- walk (no aim), run -- then NUM2. IsHold is logged each line so aim-walk vs
-- walk label themselves.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_speed]"
local NS = sdk.game_namespace

local VK_NUMPAD1 = 0x61
local VK_NUMPAD2 = 0x62

local TARGET_PATTERNS = { "move", "motion", "speed", "locomotion", "walk", "run", "fsm" }
local STREAM_PERIOD = 0.30   -- seconds between stream lines
local CHANGE_EPS = 0.02      -- min change to (re)print a getter on a stream line

local state = {
    keys_ok = true,
    prev_keys = {},
    have_prev = false,
    prev_x = 0.0, prev_y = 0.0, prev_z = 0.0, prev_t = 0.0,
    speed = 0.0, raw_speed = 0.0, speed_max_seen = 0.0, vspeed = 0.0,
    getters = {},            -- { comp, label, name, last, vmin, vmax }
    recon_done = false,
    capturing = false,
    last_stream_t = 0.0,
    stream_count = 0,
    ui_player = false,
    ui_is_hold = "n/a",
    ui_default_speed = "n/a",
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

local function name_matches(lower, pats)
    for _, p in ipairs(pats) do if lower:find(p, 1, true) then return true end end
    return false
end

local function type_hierarchy(td)
    local list, d = {}, 0
    while td and d < 8 do list[#list + 1] = td; td = safe(function() return td:get_parent_type() end); d = d + 1 end
    return list
end

-- velocity every frame
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
            local dx, dy, dz = x - state.prev_x, y - state.prev_y, z - state.prev_z
            local horiz = math.sqrt(dx * dx + dz * dz) / dt
            if horiz < 50.0 then
                state.raw_speed = horiz
                state.speed = state.speed * 0.75 + horiz * 0.25
                if state.speed > state.speed_max_seen then state.speed_max_seen = state.speed end
                state.vspeed = math.abs(dy) / dt
            end
        end
    end
    state.prev_x, state.prev_y, state.prev_z = x, y, z
    state.prev_t = now; state.have_prev = true
    local ih = get_is_hold(player)
    state.ui_is_hold = tostring(ih)
end

local function collect_getters(comp, td, label)
    for _, m in ipairs(td:get_methods() or {}) do
        local name = safe(function() return m:get_name() end)
        if name then
            local np = safe(function() return m:get_num_params() end)
            local ret = safe(function() return m:get_return_type():get_full_name() end)
            if np == 0 and (ret == "System.Single" or ret == "System.Int32") then
                if name:find("^get_") or name:find("Speed") or name:find("Rate") or name:find("Blend") then
                    state.getters[#state.getters + 1] =
                        { comp = comp, label = label .. "." .. name, name = name, last = nil, vmin = nil, vmax = nil }
                end
            end
        end
    end
end

local function dump_methods(td, label)
    local tn = safe(function() return td:get_full_name() end) or "?"
    log_line("---- methods: " .. tn .. " (component " .. label .. ") ----")
    for _, m in ipairs(td:get_methods() or {}) do
        local name = safe(function() return m:get_name() end)
        if name then
            local np = safe(function() return m:get_num_params() end) or -1
            local ret = safe(function() return m:get_return_type():get_full_name() end) or "?"
            log_line(string.format("   %s %s() [%d params]", ret, name, np))
        end
    end
end

local function recon_and_start()
    local player = get_player()
    if not player then log_line("NUM1: no player"); return end
    state.getters = {}
    local comps = safe(function() return player:call("get_Components") end)
    local elems = comps and safe(function() return comps:get_elements() end) or nil
    if not elems then log_line("NUM1: could not enumerate components"); return end
    log_line("==== PLAYER component list ====")
    for _, comp in ipairs(elems) do
        local ctd = safe(function() return comp:get_type_definition() end)
        local cname = ctd and (safe(function() return ctd:get_full_name() end) or "?") or "?"
        log_line("  component: " .. cname)
        if ctd and name_matches(cname:lower(), TARGET_PATTERNS) then
            for _, td in ipairs(type_hierarchy(ctd)) do
                dump_methods(td, cname); collect_getters(comp, td, cname)
            end
        end
    end
    local msc = get_component(player, NS("survivor.SurvivorMotionSpeedController"))
    if msc then
        local ds = safe(function() return msc:call("getDefaultSpeed") end)
        state.ui_default_speed = tostring(ds); log_line("getDefaultSpeed = " .. tostring(ds))
    end
    state.recon_done = true
    state.capturing = true
    state.stream_count = 0
    log_line(string.format("NUM1 done: %d getters. CAPTURE ON -- move: aim+walk, walk, run (a few s each), then NUM2.",
        #state.getters))
end

local function stream_tick()
    if not state.capturing then return end
    local now = os.clock()
    if now - state.last_stream_t < STREAM_PERIOD then return end
    state.last_stream_t = now
    state.stream_count = state.stream_count + 1
    local changed = {}
    for _, g in ipairs(state.getters) do
        local v = safe(function() return g.comp:call(g.name) end)
        if type(v) == "number" then
            if g.vmin == nil or v < g.vmin then g.vmin = v end
            if g.vmax == nil or v > g.vmax then g.vmax = v end
            if g.last == nil or math.abs(v - g.last) > CHANGE_EPS then
                changed[#changed + 1] = string.format("%s=%.3f", g.label, v)
                g.last = v
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
    log_line("==== CAPTURE STOPPED -- getters by range (widest first = most speed-like) ====")
    local ranked = {}
    for _, g in ipairs(state.getters) do
        if g.vmin ~= nil then
            ranked[#ranked + 1] = { label = g.label, vmin = g.vmin, vmax = g.vmax, range = g.vmax - g.vmin }
        end
    end
    table.sort(ranked, function(a, b) return a.range > b.range end)
    for i = 1, math.min(#ranked, 30) do
        local r = ranked[i]
        log_line(string.format("   [range %.3f]  %.3f .. %.3f   %s", r.range, r.vmin, r.vmax, r.label))
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
    if key_pressed(VK_NUMPAD1) then recon_and_start() end
    if key_pressed(VK_NUMPAD2) then stop_and_summarize() end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: movement-speed recon v2 (read-only, auto-capture)") then return end
    imgui.text("hotkeys: NUM1=recon+start  NUM2=stop+summary" ..
        (state.keys_ok and "" or "  [KEY API UNAVAILABLE - use buttons]"))
    imgui.text("player: " .. (state.ui_player and "found" or "NOT FOUND")
        .. "   IsHold: " .. state.ui_is_hold)
    imgui.text(string.format("measured speed: %.3f (raw %.3f)  max: %.3f", state.speed, state.raw_speed, state.speed_max_seen))
    imgui.text(string.format("vertical: %.3f   default speed: %s", state.vspeed, state.ui_default_speed))
    imgui.text(string.format("capturing: %s   stream lines: %d   getters: %d",
        tostring(state.capturing), state.stream_count, #state.getters))
    if imgui.button("RECON + START capture (NUM1)") then recon_and_start() end
    if imgui.button("STOP + summary (NUM2)") then stop_and_summarize() end
    if imgui.button("reset max-seen") then state.speed_max_seen = 0.0 end
    imgui.tree_pop()
end)

log_line("loaded (v2 auto-capture, read-only). NUM1 = recon+start, NUM2 = stop+summary.")
