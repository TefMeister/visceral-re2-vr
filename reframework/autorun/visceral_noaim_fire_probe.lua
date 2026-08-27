-- Visceral (RE2 VR) -- no-aim-fire probe, v2
--
-- v1 RESULT (2026-08-27, flat, live): CORE QUESTION ANSWERED YES.
-- setForce(HOLD=64, true) raised the aim stance (IsHold flipped true) and
-- LMB fired with RMB untouched. The force LATCHES: it survived the pulse
-- ending by 40+ seconds with no per-frame reassert. The dump shows
-- setForce(Kind, bool) is the ONLY button-force method (no clear/reset
-- sibling), so the presumed un-latch is setForce(HOLD, false) -- v2 adds
-- explicit latch/unlatch buttons to test whether false means "give the
-- button back" (clean switch) or "force it off" (would block real aim).
-- Also learned: ATTACK is 256 (256, not the old fallback 4 = WALK).
--
-- v2 RESULT (2026-08-27, flat, live): UNLATCH IS CLEAN. setForce(HOLD,
-- false) drops the stance, RMB aiming works normally right after, and the
-- fire gate returns (LMB alone refuses again). So setForce(kind, bool)
-- means "start/stop forcing this input active" -- a perfect two-call
-- on/off switch, no per-frame work, no state-machine writes. The entire
-- no-grip-to-shoot feature reduces to: latch on trigger-touch/press,
-- unlatch on release. Livability items (aim-walk speed, footstep sync,
-- item interaction while latched, LG-only grip) tracked in the Visceral
-- backlog -- most are sidestepped by only latching while the finger is
-- on the trigger.
--
-- Question under test: can the "must hold aim (RG / RMB / LT) before the
-- trigger fires" requirement be removed by forcing the game's own HOLD
-- input on, via app.ropeway.InputSystem:setForce(kind, bool)?
--
-- Known going in (from the Arcade Controls era, studied not copied):
--   * InputDefine.Kind is a bitfield enum of abstract inputs; ATTACK=4 is
--     the fire input, HOLD=64 is the aim input. Both flat and VR input
--     paths funnel into these same bits.
--   * setForce(ATTACK, false) reliably hard-blocked firing when called
--     per-frame, so setForce is a real, working lever at the input level.
--   * Writing raw FSM state directly (e.g. IsJog) has frozen the player
--     before -- input-level forcing is the safe lane; state writes are not.
--   * Raw ButtonBits get recomputed natively per frame; the proven hook
--     points for winning that ordering are post-UpdateHID and
--     pre-UpdateBehavior.
--
-- This probe deliberately does NOT ship gameplay behavior. It exposes:
--   * a live readout (player / weapon / IsHold / ButtonBits / ATTACK+HOLD
--     bit states),
--   * a one-shot dump of InputSystem's force-related methods and the
--     InputDefine.Kind enum (to learn setForce's true signature, siblings
--     like any clear/reset method, and confirm bit values),
--   * a timed PULSE of setForce(HOLD, true) (default 2s) to observe force
--     semantics: does the stance engage, does IsHold flip, does it decay
--     when the pulse ends,
--   * a continuous FORCE HOLD mode (checkbox, default off) gated on a
--     main weapon being equipped, for the actual fire-without-aim test.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_noaim]"

local NS = sdk.game_namespace

local KIND_ATTACK_FALLBACK = 4
local KIND_HOLD_FALLBACK = 64

local state = {
    enabled_continuous = false,
    pulse_until = 0,
    pulse_seconds = 2.0,
    kind_attack = nil,
    kind_hold = nil,
    enum_dumped = false,
    methods_dumped = false,
    setforce_ok_count = 0,
    setforce_fail_count = 0,
    last_error = "",
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
                if name == "ATTACK" then state.kind_attack = tonumber(value) end
                if name == "HOLD" then state.kind_hold = tonumber(value) end
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
-- The lever
-- ---------------------------------------------------------------------------

local function set_force_hold(value)
    local input_system = get_input_system()
    if not input_system then return end
    local kind = state.kind_hold or KIND_HOLD_FALLBACK
    local ok, err = pcall(function()
        input_system:call("setForce", kind, value)
    end)
    if ok then
        state.setforce_ok_count = state.setforce_ok_count + 1
    else
        state.setforce_fail_count = state.setforce_fail_count + 1
        state.last_error = tostring(err)
    end
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
    apply_force_if_wanted()
    if state.pulse_until > 0 and os.clock() >= state.pulse_until then
        state.pulse_until = 0
        log.info(TAG .. " pulse ended (stopped calling setForce; watching for decay)")
    end
end)

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
    local player = get_player()
    state.ui_player_found = player ~= nil

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
            local hold = safe(function() return cond:call("get_IsHold") end)
            state.ui_is_hold = tostring(hold)
        else
            state.ui_is_hold = "no SurvivorCondition"
        end
    else
        state.ui_is_hold = "no player"
    end

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
    if not imgui.tree_node("Visceral: no-aim fire probe") then return end

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
    if state.last_error ~= "" then
        imgui.text("last error: " .. state.last_error)
    end

    imgui.spacing()

    if imgui.button("Dump InputSystem force/hold methods to log") then
        state.methods_dumped = false
        dump_force_methods()
    end

    if imgui.button("LATCH: setForce(HOLD, true) once") then
        set_force_hold(true)
        log.info(TAG .. " latch ON (single setForce true)")
    end

    if imgui.button("UNLATCH: setForce(HOLD, false) once") then
        set_force_hold(false)
        log.info(TAG .. " latch OFF (single setForce false) -- now test if RMB aiming still works")
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
        log.info(TAG .. " continuous mode = " .. tostring(value))
    end

    if imgui.button("PANIC: stop everything + unlatch") then
        state.enabled_continuous = false
        state.pulse_until = 0
        set_force_hold(false)
        log.info(TAG .. " panic stop (unlatched)")
    end

    imgui.tree_pop()
end)

log.info(TAG .. " loaded (v1). All modes idle until used from the UI.")
