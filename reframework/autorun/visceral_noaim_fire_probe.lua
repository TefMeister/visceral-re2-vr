-- Visceral (RE2 VR) -- no-aim-fire probe, v3 (livability sweep recorder)
--
-- v1 RESULT (2026-08-27, flat, live): CORE QUESTION ANSWERED YES.
-- setForce(HOLD=64, true) raised the aim stance (IsHold flipped true) and
-- LMB fired with RMB untouched. The force LATCHES: it survived the pulse
-- ending by 40+ seconds with no per-frame reassert. The dump shows
-- setForce(Kind, bool) is the ONLY button-force method (no clear/reset
-- sibling). Also learned: ATTACK is 256 (not the old fallback 4 = WALK).
--
-- v2 RESULT (2026-08-27, flat, live): UNLATCH IS CLEAN. setForce(HOLD,
-- false) drops the stance, RMB aiming works normally right after, and the
-- fire gate returns (LMB alone refuses again). So setForce(kind, bool)
-- means "start/stop forcing this input active" -- a perfect two-call
-- on/off switch, no per-frame work, no state-machine writes.
--
-- v3 (2026-08-29): the feature is proven; the open questions are about
-- BREAKAGE. This build turns the probe into a sweep recorder so one flat
-- playthrough answers them from the log alone:
--   Q3 (livability): does anything misbehave while latched -- doors,
--      pickups, inventory, ladders, saves, cutscenes, grabs, loads?
--      -> user plays the checklist with the latch on; the probe timestamps
--         every stance drop and context change around whatever they see.
--   Q4 (does the game fight the latch): a spy hook on InputSystem.setForce
--      logs every call the GAME makes (ours are flagged and skipped), with
--      kind name, value, and whether our latch was on at the time.
--   Q2 (latch->stance latency): stopwatch from the latch call to IsHold
--      flipping true, logged in milliseconds (decides trigger-touch vs
--      trigger-press latching later).
-- New numpad hotkeys (project rule: numpad only, never F-keys):
--   NUMPAD7 = latch ON (single setForce true)
--   NUMPAD8 = unlatch  (single setForce false)
--   NUMPAD9 = PANIC: stop continuous mode + unlatch
-- Everything is also still available from the REFramework UI tree.
-- All modes idle until used; the spy hook only ever observes.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_noaim]"

local NS = sdk.game_namespace

local KIND_ATTACK_FALLBACK = 4
local KIND_HOLD_FALLBACK = 64

local VK_NUMPAD7 = 0x67
local VK_NUMPAD8 = 0x68
local VK_NUMPAD9 = 0x69

local state = {
    enabled_continuous = false,
    pulse_until = 0,
    pulse_seconds = 2.0,
    kind_attack = nil,
    kind_hold = nil,
    kind_names = {},          -- value -> enum name, filled by the enum dump
    enum_dumped = false,
    methods_dumped = false,
    setforce_ok_count = 0,
    setforce_fail_count = 0,
    last_error = "",
    -- sweep recorder
    latched = false,          -- our belief: we latched HOLD on and never released it
    latch_time = nil,         -- os.clock() at latch, cleared once latency is logged
    in_our_call = false,      -- re-entrancy flag so the spy skips our own setForce
    spy_hooked = false,
    foreign_call_count = 0,
    foreign_seen = {},        -- "kind:value" -> {count=n, last_log=t}
    events = {},              -- ring buffer of timestamped event strings for the UI
    prev_is_hold = nil,
    prev_player_found = nil,
    prev_weapon_name = nil,
    keys_ok = true,
    prev_keys = {},
    -- live readouts, refreshed each frame for the UI
    ui_player_found = false,
    ui_weapon_name = "none",
    ui_is_hold = "n/a",
    ui_bits_on = "n/a",
    ui_bits_down = "n/a",
    ui_attack_on = false,
    ui_hold_on = false,
}

local function safe(fn)
    local ok, r = pcall(fn)
    if ok then return r end
    return nil
end

-- every sweep observation goes to the log file AND a small on-screen list
local function record_event(msg)
    local line = string.format("[t=%8.2fs] %s", os.clock(), msg)
    log.info(TAG .. " EVENT " .. line)
    local ev = state.events
    ev[#ev + 1] = line
    if #ev > 40 then table.remove(ev, 1) end
end

local function get_input_system()
    return sdk.get_managed_singleton(NS("InputSystem"))
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

local function get_survivor_condition(player)
    if not player then return nil end
    return get_component(player, NS("survivor.SurvivorCondition"))
end

local function get_equipped_weapon(player)
    if not player then return nil end
    local equipment = get_component(player, NS("survivor.Equipment"))
    if not equipment then return nil end
    return safe(function() return equipment:get_field("<EquipWeapon>k__BackingField") end)
end

-- ---------------------------------------------------------------------------
-- Recon dumps (run once each, on demand or on first player sighting)
-- ---------------------------------------------------------------------------

local function dump_kind_enum()
    if state.enum_dumped then return end
    state.enum_dumped = true

    local td = sdk.find_type_definition(NS("InputDefine.Kind"))
    if not td then
        log.warn(TAG .. " InputDefine.Kind type not found")
        return
    end
    for _, f in ipairs(td:get_fields()) do
        if safe(function() return f:is_static() end) then
            local name = safe(function() return f:get_name() end)
            local value = safe(function() return f:get_data(nil) end)
            if name and value ~= nil then
                log.info(string.format("%s Kind.%s = %s", TAG, name, tostring(value)))
                local num = tonumber(value)
                if num then state.kind_names[num] = name end
                if name == "ATTACK" then state.kind_attack = num end
                if name == "HOLD" then state.kind_hold = num end
            end
        end
    end
    log.info(string.format("%s enum resolved: ATTACK=%s HOLD=%s",
        TAG, tostring(state.kind_attack), tostring(state.kind_hold)))
end

local function dump_force_methods()
    if state.methods_dumped then return end
    state.methods_dumped = true

    local input_system = get_input_system()
    if not input_system then
        state.methods_dumped = false
        return
    end
    local td = safe(function() return input_system:get_type_definition() end)
    local depth = 0
    while td and depth < 8 do
        local tname = safe(function() return td:get_full_name() end) or "?"
        for _, m in ipairs(td:get_methods() or {}) do
            local name = safe(function() return m:get_name() end)
            if name then
                local lower = name:lower()
                if lower:find("force") or lower:find("hold") then
                    local nparams = safe(function() return m:get_num_params() end) or -1
                    local ret = safe(function() return m:get_return_type():get_full_name() end) or "?"
                    local ptypes = {}
                    local pt = safe(function() return m:get_param_types() end)
                    if pt then
                        for _, p in ipairs(pt) do
                            ptypes[#ptypes + 1] = safe(function() return p:get_full_name() end) or "?"
                        end
                    end
                    log.info(string.format("%s [%s] %s %s(%s)  [%d params]",
                        TAG, tname, ret, name, table.concat(ptypes, ", "), nparams))
                end
            end
        end
        td = safe(function() return td:get_parent_type() end)
        depth = depth + 1
    end
    log.info(TAG .. " force/hold method dump complete")
end

-- ---------------------------------------------------------------------------
-- The spy: log every setForce call the GAME makes (Q4). Observation only --
-- the hook never blocks or alters the call.
-- ---------------------------------------------------------------------------

local function kind_label(kind)
    local name = state.kind_names[kind]
    if name then return string.format("%s(%d)", name, kind) end
    return tostring(kind)
end

local function on_foreign_setforce(kind, value)
    state.foreign_call_count = state.foreign_call_count + 1
    local key = tostring(kind) .. ":" .. tostring(value)
    local seen = state.foreign_seen[key]
    local now = os.clock()
    if not seen then
        state.foreign_seen[key] = { count = 1, last_log = now }
        record_event(string.format("GAME called setForce(%s, %s) [latched=%s] -- first sighting",
            kind_label(kind), tostring(value), tostring(state.latched)))
    else
        seen.count = seen.count + 1
        -- identical calls can come per-frame; summarize instead of flooding
        if now - seen.last_log > 5.0 then
            seen.last_log = now
            record_event(string.format("GAME setForce(%s, %s) again [x%d total, latched=%s]",
                kind_label(kind), tostring(value), seen.count, tostring(state.latched)))
        end
    end
end

local function install_setforce_spy()
    if state.spy_hooked then return end
    local td = sdk.find_type_definition(NS("InputSystem"))
    if not td then return end
    local method = td:get_method("setForce")
    if not method then
        log.warn(TAG .. " setForce method not found for spy hook")
        return
    end
    local ok, err = pcall(function()
        sdk.hook(method,
            function(args)
                if state.in_our_call then return end
                local kind = safe(function() return sdk.to_int64(args[3]) & 0xFFFFFFFF end)
                local raw = safe(function() return sdk.to_int64(args[4]) end)
                local value
                if raw ~= nil then value = (raw & 1) ~= 0 else value = "?" end
                safe(function() on_foreign_setforce(kind or -1, value) end)
            end,
            function(retval) return retval end)
    end)
    if ok then
        state.spy_hooked = true
        log.info(TAG .. " setForce spy hook installed (observe-only)")
    else
        log.warn(TAG .. " setForce spy hook FAILED: " .. tostring(err))
    end
end

-- ---------------------------------------------------------------------------
-- The lever
-- ---------------------------------------------------------------------------

local function set_force_hold(value)
    local input_system = get_input_system()
    if not input_system then return end
    local kind = state.kind_hold or KIND_HOLD_FALLBACK
    state.in_our_call = true
    local ok, err = pcall(function()
        input_system:call("setForce", kind, value)
    end)
    state.in_our_call = false
    if ok then
        state.setforce_ok_count = state.setforce_ok_count + 1
    else
        state.setforce_fail_count = state.setforce_fail_count + 1
        state.last_error = tostring(err)
    end
end

local function do_latch()
    set_force_hold(true)
    state.latched = true
    state.latch_time = os.clock()
    record_event("LATCH ON (single setForce true)")
end

local function do_unlatch()
    set_force_hold(false)
    state.latched = false
    state.latch_time = nil
    record_event("LATCH OFF (single setForce false)")
end

local function do_panic()
    state.enabled_continuous = false
    state.pulse_until = 0
    set_force_hold(false)
    state.latched = false
    state.latch_time = nil
    record_event("PANIC stop (continuous off + unlatched)")
end

local function force_wanted()
    if state.pulse_until > 0 and os.clock() < state.pulse_until then return true end
    if not state.enabled_continuous then return false end
    -- continuous mode only while an actual weapon is in hand
    local player = get_player()
    if not player then return false end
    return get_equipped_weapon(player) ~= nil
end

local function apply_force_if_wanted()
    if force_wanted() then
        set_force_hold(true)
    end
end

-- proven ordering points: after the HID recompute, before behavior consumes
re.on_application_entry("UpdateHID", apply_force_if_wanted)

re.on_pre_application_entry("UpdateBehavior", function()
    dump_kind_enum()
    install_setforce_spy()
    apply_force_if_wanted()
    if state.pulse_until > 0 and os.clock() >= state.pulse_until then
        state.pulse_until = 0
        log.info(TAG .. " pulse ended (stopped calling setForce; watching for decay)")
    end
end)

-- ---------------------------------------------------------------------------
-- Hotkeys (numpad only, per project rule)
-- ---------------------------------------------------------------------------

local function key_pressed(vk)
    local down = safe(function() return reframework:is_key_down(vk) end)
    if down == nil then
        state.keys_ok = false
        return false
    end
    local was = state.prev_keys[vk]
    state.prev_keys[vk] = down
    return down and not was
end

local function poll_hotkeys()
    if not state.keys_ok then return end
    if key_pressed(VK_NUMPAD7) then do_latch() end
    if key_pressed(VK_NUMPAD8) then do_unlatch() end
    if key_pressed(VK_NUMPAD9) then do_panic() end
end

-- ---------------------------------------------------------------------------
-- Sweep watchdog: timestamp stance drops while latched, context changes,
-- and the latch->stance latency (Q2/Q3)
-- ---------------------------------------------------------------------------

local function watch_transitions(player_found, weapon_name, is_hold)
    -- latch->stance stopwatch
    if state.latch_time and is_hold == true then
        record_event(string.format("stance up %.0f ms after latch",
            (os.clock() - state.latch_time) * 1000.0))
        state.latch_time = nil
    end

    -- stance dropped while we believe the latch is on = the game overrode us
    if state.prev_is_hold == true and is_hold == false
        and (state.latched or state.enabled_continuous) then
        record_event(string.format(
            "STANCE DROPPED while latched [player=%s weapon=%s]",
            tostring(player_found), weapon_name))
    end
    -- stance came back on its own while latched (after a drop)
    if state.prev_is_hold == false and is_hold == true
        and (state.latched or state.enabled_continuous) and not state.latch_time then
        record_event("stance back up while latched")
    end
    state.prev_is_hold = is_hold

    -- context markers: player object swaps (cutscene/load) and weapon changes
    if state.prev_player_found ~= nil and player_found ~= state.prev_player_found then
        record_event(player_found and "player object appeared" or
            "player object GONE (cutscene/load/menu?)")
    end
    state.prev_player_found = player_found

    if state.prev_weapon_name ~= nil and weapon_name ~= state.prev_weapon_name then
        record_event(string.format("weapon changed: %s -> %s",
            state.prev_weapon_name, weapon_name))
    end
    state.prev_weapon_name = weapon_name
end

-- ---------------------------------------------------------------------------
-- Live readouts + UI
-- ---------------------------------------------------------------------------

local function read_button_bits(input_system)
    local bb = safe(function() return input_system:call("get_ButtonBits") end)
    if not bb then return nil, nil end
    local on = safe(function() return bb:get_field("On") end)
    local down = safe(function() return bb:get_field("Down") end)
    if on == nil then on = safe(function() return bb:read_qword(0x18) end) end
    if down == nil then down = safe(function() return bb:read_qword(0x10) end) end
    return on, down
end

local function refresh_readouts()
    poll_hotkeys()

    local player = get_player()
    state.ui_player_found = player ~= nil

    local is_hold = nil
    state.ui_weapon_name = "none"
    if player then
        local weapon = get_equipped_weapon(player)
        if weapon then
            local wt = safe(function() return weapon:get_field("_WeaponType") end)
            local go = safe(function() return weapon:call("get_GameObject") end)
            local goname = go and safe(function() return go:call("get_Name") end) or nil
            state.ui_weapon_name = string.format("%s (type %s)",
                tostring(goname or "?"), tostring(wt or "?"))
        end
        local cond = get_survivor_condition(player)
        if cond then
            is_hold = safe(function() return cond:call("get_IsHold") end)
            state.ui_is_hold = tostring(is_hold)
        else
            state.ui_is_hold = "no SurvivorCondition"
        end
    else
        state.ui_is_hold = "no player"
    end

    watch_transitions(state.ui_player_found, state.ui_weapon_name, is_hold)

    local input_system = get_input_system()
    if input_system then
        local on, down = read_button_bits(input_system)
        local ka = state.kind_attack or KIND_ATTACK_FALLBACK
        local kh = state.kind_hold or KIND_HOLD_FALLBACK
        if type(on) == "number" then
            state.ui_bits_on = string.format("0x%X", on)
            state.ui_attack_on = (on & ka) ~= 0
            state.ui_hold_on = (on & kh) ~= 0
        else
            state.ui_bits_on = "unreadable"
        end
        if type(down) == "number" then
            state.ui_bits_down = string.format("0x%X", down)
        else
            state.ui_bits_down = "unreadable"
        end
    else
        state.ui_bits_on = "no InputSystem"
        state.ui_bits_down = "no InputSystem"
    end
end

re.on_frame(refresh_readouts)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: no-aim fire probe (v3 sweep)") then return end

    imgui.text("hotkeys: NUM7=latch  NUM8=unlatch  NUM9=panic"
        .. (state.keys_ok and "" or "  [KEY API UNAVAILABLE - use buttons]"))
    imgui.text("latched (our belief): " .. tostring(state.latched))
    imgui.text("player: " .. (state.ui_player_found and "found" or "NOT FOUND"))
    imgui.text("weapon: " .. state.ui_weapon_name)
    imgui.text("SurvivorCondition.IsHold: " .. state.ui_is_hold)
    imgui.text("ButtonBits On:   " .. state.ui_bits_on)
    imgui.text("ButtonBits Down: " .. state.ui_bits_down)
    imgui.text(string.format("ATTACK bit: %s   HOLD bit: %s",
        state.ui_attack_on and "ON" or "off",
        state.ui_hold_on and "ON" or "off"))
    imgui.text(string.format("setForce calls ok=%d fail=%d",
        state.setforce_ok_count, state.setforce_fail_count))
    imgui.text(string.format("spy: %s, game-made setForce calls seen: %d",
        state.spy_hooked and "hooked" or "NOT hooked", state.foreign_call_count))
    if state.last_error ~= "" then
        imgui.text("last error: " .. state.last_error)
    end

    imgui.spacing()

    if imgui.button("Dump InputSystem force/hold methods to log") then
        state.methods_dumped = false
        dump_force_methods()
    end

    if imgui.button("LATCH: setForce(HOLD, true) once") then
        do_latch()
    end

    if imgui.button("UNLATCH: setForce(HOLD, false) once") then
        do_unlatch()
    end

    if imgui.button(string.format("PULSE: force HOLD for %.1fs", state.pulse_seconds)) then
        state.pulse_until = os.clock() + state.pulse_seconds
        log.info(TAG .. " pulse started")
    end

    if state.pulse_until > 0 then
        imgui.text(string.format("pulse active: %.2fs left",
            math.max(0, state.pulse_until - os.clock())))
    end

    local changed, value = imgui.checkbox(
        "FORCE HOLD continuously (while weapon equipped)", state.enabled_continuous)
    if changed then
        state.enabled_continuous = value
        record_event("continuous mode = " .. tostring(value))
    end

    if imgui.button("PANIC: stop everything + unlatch") then
        do_panic()
    end

    imgui.spacing()

    if imgui.tree_node(string.format("sweep event log (%d)", #state.events)) then
        for i = #state.events, 1, -1 do
            imgui.text(state.events[i])
        end
        imgui.tree_pop()
    end

    imgui.tree_pop()
end)

log.info(TAG .. " loaded (v3 sweep recorder). All modes idle until used.")
