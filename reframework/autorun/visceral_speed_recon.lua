-- Visceral (RE2 VR) -- movement-speed recon, v1 (READ-ONLY)
--
-- Goal: de-risk design-spec req 4 (unify walk/aim/run to one speed cap +
-- drive body sway/pose/leg-anim/footsteps from MEASURED player speed).
-- We need to know:
--   * the player's actual ground speed (units/sec) at different LS deflections
--     -> this IS the measurement the speed-driven presentation will use, so
--        computing it here is both recon and a prototype of that measurement;
--   * which managed float on the player tracks that speed / feeds the
--     locomotion animation blend (SurvivorMotionSpeedController + any
--     move/motion/speed/locomotion component) -> the "safe" dial to drive
--     instead of hand-authoring leg animation.
--
-- Everything here only READS. No hooks, no setForce, nothing written.
--
-- Hotkeys (numpad only, per project rule; NUM3-9 belong to the fire probe):
--   NUMPAD1 = recon: dump the player's move/motion/speed components (methods
--             to the log) and snapshot every 0-param float getter's value now.
--   NUMPAD2 = sample: log a numbered snapshot = current measured speed + every
--             discovered float getter's value. Push the LS to a chosen
--             deflection, hold it, tap NUM2. Do several (crawl / half / full)
--             so we can see which float tracks speed and what the caps are.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_speed]"
local NS = sdk.game_namespace

local VK_NUMPAD1 = 0x61
local VK_NUMPAD2 = 0x62

-- component type-name fragments worth dumping + sweeping for float getters
local TARGET_PATTERNS = { "move", "motion", "speed", "locomotion", "walk", "run" }

local state = {
    keys_ok = true,
    prev_keys = {},
    -- velocity measurement
    have_prev = false,
    prev_x = 0.0, prev_z = 0.0, prev_y = 0.0,
    prev_t = 0.0,
    speed = 0.0,          -- smoothed horizontal speed (units/sec)
    raw_speed = 0.0,
    speed_max_seen = 0.0,
    vspeed = 0.0,         -- vertical (to spot jumps/falls/teleports)
    -- discovered float getters: array of { comp, label, name }
    getters = {},
    recon_done = false,
    sample_count = 0,
    -- ui
    ui_player = false,
    ui_default_speed = "n/a",
}

local function safe(fn)
    local ok, r = pcall(fn)
    if ok then return r end
    return nil
end

local function log_line(msg)
    log.info(TAG .. " " .. msg)
end

local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager"))
    if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end

local function get_component(go, type_name)
    local t = sdk.typeof(type_name)
    if not t then return nil end
    return safe(function() return go:call("getComponent(System.Type)", t) end)
end

-- read a via.vec3/vec4 return as x,y,z regardless of accessor style
local function vec_xyz(v)
    if v == nil then return nil end
    local x = safe(function() return v.x end)
    if x == nil then x = safe(function() return v:get_field("x") end) end
    local y = safe(function() return v.y end)
    if y == nil then y = safe(function() return v:get_field("y") end) end
    local z = safe(function() return v.z end)
    if z == nil then z = safe(function() return v:get_field("z") end) end
    return x, y, z
end

local function get_player_pos(player)
    local tr = safe(function() return player:call("get_Transform") end)
    if not tr then return nil end
    local p = safe(function() return tr:call("get_Position") end)
    if not p then return nil end
    return vec_xyz(p)
end

local function name_matches(lower, patterns)
    for _, p in ipairs(patterns) do
        if lower:find(p, 1, true) then return true end
    end
    return false
end

local function type_hierarchy(td)
    local list, depth = {}, 0
    while td and depth < 8 do
        list[#list + 1] = td
        td = safe(function() return td:get_parent_type() end)
        depth = depth + 1
    end
    return list
end

-- ---------------------------------------------------------------------------
-- Velocity measurement (every frame) -- horizontal ground speed
-- ---------------------------------------------------------------------------

local function update_velocity()
    local player = get_player()
    state.ui_player = player ~= nil
    if not player then
        state.have_prev = false
        return
    end
    local x, y, z = get_player_pos(player)
    if x == nil then return end

    local now = os.clock()
    if state.have_prev then
        local dt = now - state.prev_t
        if dt > 0.0005 and dt < 0.5 then   -- ignore first frame / hitches / loads
            local dx = x - state.prev_x
            local dz = z - state.prev_z
            local dy = y - state.prev_y
            local horiz = math.sqrt(dx * dx + dz * dz) / dt
            local vert = math.abs(dy) / dt
            -- reject teleports/loads: implausible jump in one step
            if horiz < 50.0 then
                state.raw_speed = horiz
                -- EMA smoothing so the readout is legible
                state.speed = state.speed * 0.8 + horiz * 0.2
                if state.speed > state.speed_max_seen then
                    state.speed_max_seen = state.speed
                end
                state.vspeed = vert
            end
        end
    end
    state.prev_x, state.prev_y, state.prev_z = x, y, z
    state.prev_t = now
    state.have_prev = true
end

-- ---------------------------------------------------------------------------
-- Recon (NUM1): dump move/motion/speed components + collect float getters
-- ---------------------------------------------------------------------------

local function collect_float_getters(comp, td, label)
    for _, m in ipairs(td:get_methods() or {}) do
        local name = safe(function() return m:get_name() end)
        if name then
            local np = safe(function() return m:get_num_params() end)
            local ret = safe(function() return m:get_return_type():get_full_name() end)
            if np == 0 and (ret == "System.Single" or ret == "System.Int32") then
                if name:find("^get_") or name:find("Speed") or name:find("Rate") then
                    state.getters[#state.getters + 1] =
                        { comp = comp, label = label .. "." .. name, name = name }
                end
            end
        end
    end
end

local function dump_type_methods(td, label)
    local tname = safe(function() return td:get_full_name() end) or "?"
    log_line("---- methods: " .. tname .. " (component " .. label .. ") ----")
    for _, m in ipairs(td:get_methods() or {}) do
        local name = safe(function() return m:get_name() end)
        if name then
            local np = safe(function() return m:get_num_params() end) or -1
            local ret = safe(function() return m:get_return_type():get_full_name() end) or "?"
            log_line(string.format("   %s %s() [%d params]", ret, name, np))
        end
    end
end

local function recon()
    local player = get_player()
    if not player then
        log_line("NUM1: no player")
        return
    end
    state.getters = {}

    local comps = safe(function() return player:call("get_Components") end)
    local elems = comps and safe(function() return comps:get_elements() end) or nil
    if not elems then
        log_line("NUM1: could not enumerate player components")
        return
    end

    log_line("==== PLAYER component list ====")
    for _, comp in ipairs(elems) do
        local ctd = safe(function() return comp:get_type_definition() end)
        local cname = ctd and (safe(function() return ctd:get_full_name() end) or "?") or "?"
        log_line("  component: " .. cname)
        if ctd and name_matches(cname:lower(), TARGET_PATTERNS) then
            for _, td in ipairs(type_hierarchy(ctd)) do
                dump_type_methods(td, cname)
                collect_float_getters(comp, td, cname)
            end
        end
    end

    -- explicit: SurvivorMotionSpeedController.getDefaultSpeed if present
    local msc = get_component(player, NS("survivor.SurvivorMotionSpeedController"))
    if msc then
        local ds = safe(function() return msc:call("getDefaultSpeed") end)
        state.ui_default_speed = tostring(ds)
        log_line("SurvivorMotionSpeedController.getDefaultSpeed = " .. tostring(ds))
    end

    state.recon_done = true
    log_line(string.format("NUM1 done: %d float getters collected. Now move at various LS deflections and tap NUM2 to sample.",
        #state.getters))
end

-- ---------------------------------------------------------------------------
-- Sample (NUM2): log measured speed + every float getter, numbered
-- ---------------------------------------------------------------------------

local function sample()
    if not state.recon_done then
        log_line("NUM2: run NUM1 first")
        return
    end
    state.sample_count = state.sample_count + 1
    log_line(string.format("==== SAMPLE #%d  measuredSpeed=%.3f (raw=%.3f, maxSeen=%.3f, vert=%.3f) ====",
        state.sample_count, state.speed, state.raw_speed, state.speed_max_seen, state.vspeed))
    for _, g in ipairs(state.getters) do
        local v = safe(function() return g.comp:call(g.name) end)
        if v ~= nil then
            log_line(string.format("   %s = %s", g.label, tostring(v)))
        end
    end
end

-- ---------------------------------------------------------------------------
-- Hotkeys + frame + UI
-- ---------------------------------------------------------------------------

local function key_pressed(vk)
    local down = safe(function() return reframework:is_key_down(vk) end)
    if down == nil then state.keys_ok = false; return false end
    local was = state.prev_keys[vk]
    state.prev_keys[vk] = down
    return down and not was
end

re.on_frame(function()
    update_velocity()
    if not state.keys_ok then return end
    if key_pressed(VK_NUMPAD1) then recon() end
    if key_pressed(VK_NUMPAD2) then sample() end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: movement-speed recon (read-only)") then return end
    imgui.text("hotkeys: NUM1=recon(dump+collect)  NUM2=sample" ..
        (state.keys_ok and "" or "  [KEY API UNAVAILABLE - use buttons]"))
    imgui.text("player: " .. (state.ui_player and "found" or "NOT FOUND"))
    imgui.text(string.format("measured ground speed: %.3f  (raw %.3f)", state.speed, state.raw_speed))
    imgui.text(string.format("max speed seen: %.3f", state.speed_max_seen))
    imgui.text(string.format("vertical speed: %.3f (spikes = jump/fall/load)", state.vspeed))
    imgui.text("default speed (MotionSpeedController): " .. state.ui_default_speed)
    imgui.text(string.format("float getters collected: %d   samples logged: %d",
        #state.getters, state.sample_count))
    if imgui.button("RECON: dump + collect (NUM1)") then recon() end
    if imgui.button("SAMPLE now (NUM2)") then sample() end
    if imgui.button("reset max-seen") then state.speed_max_seen = 0.0 end
    imgui.tree_pop()
end)

log_line("loaded (v1 movement-speed recon, read-only). NUM1 to recon, NUM2 to sample.")
