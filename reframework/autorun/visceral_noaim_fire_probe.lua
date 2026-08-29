-- Visceral (RE2 VR) -- no-aim-fire probe, v4 (fire-path gate hunt)
--
-- v1 RESULT (2026-08-27, flat, live): CORE QUESTION ANSWERED YES.
-- setForce(HOLD=64, true) raised the aim stance (IsHold flipped true) and
-- LMB fired with RMB untouched. The force LATCHES (no per-frame reassert).
-- ATTACK is 256.
--
-- v2 RESULT (2026-08-27): UNLATCH IS CLEAN. setForce(kind, bool) is a
-- perfect two-call on/off switch.
--
-- v3 RESULT (2026-08-29, flat, live): latch is flawless (stance up 0-4 ms,
-- zero game-made setForce calls, zero IsHold drops) BUT doors and item
-- pickups are blocked while latched -- and equally blocked while vanilla-RMB
-- aiming. The HOLD state itself bundles the interaction lock + speed cap +
-- banned aim animation. Verdict + reframe:
-- modding-notes/2026-08-29-doors-items-test-result.md.
--
-- v4 ROUND-1 RESULT (2026-08-29, flat, minigun): JACKPOT in the dump.
-- app.ropeway.implement.Gun carries the whole fire surface:
-- executeFire(System.Int32) (first-sighted only around a real aimed shot;
-- the minigun's spin-up put it ~0.5s after the press), plus gate-shaped
-- booleans enableFire(), enableAttack(), get_/set_EnableExecuteFire(bool),
-- isPossibleFireFromMuzzle(), checkEnableRapidFire(uint). NUM5 was a dud
-- by bad auto-selection: getFireMatrix() is a per-frame muzzle-transform
-- getter (400+ calls per press window) that reacted first, so the manual
-- call returned a mat4 and correctly fired nothing. Per-press totals were
-- flooded by it. The decisive experiment has NOT actually run yet.
--
-- v4.1 (same day): three fixes aimed at the kill:
--   * per-press summaries list DISTINCT methods with counts (the real
--     aimed-vs-unaimed diff), not a flooded total;
--   * every 0-param System.Boolean candidate logs its return value near a
--     press (enableFire/enableAttack/isPossibleFireFromMuzzle now report
--     as gate reads, not just call counts);
--   * executeFire(int) is the star: its argument is captured on every real
--     shot and always logged; NUM5 now replays executeFire(captured arg)
--     on the live gun while NOT aiming. Muzzle flash = removable check.
-- Protocol note: use the HANDGUN this round -- the minigun's loop-fire
-- spin-up muddies press timing.
--
-- v4 (2026-08-29): THE HUNT. Leave HOLD alone; find where the game ignores
-- ATTACK when the character is not in the aim stance. If that is a
-- removable check, the gun can fire from normal locomotion and all four
-- design-spec requirements collapse into one fix.
--   Step 1 (NUM4): recon + arm -- dump the equipped weapon GameObject's
--     component list and the full method tables of every weapon/gun/
--     implement component to the log, then install OBSERVE-ONLY hooks on
--     every method whose name smells like firing (fire/shoot/trigger/
--     shell/launch/attack).
--   Step 2 (play): pull LMB unaimed, then aim (RMB) and fire real shots,
--     then latch (NUM7) and fire. Every ATTACK press gets a per-press
--     summary line: how many candidate methods reacted within 0.35s, with
--     IsHold/latched context. Bool-getter candidates also log their return
--     value near a press (a gate check read directly).
--   Step 3 (NUM5): the decisive experiment -- manually call the selected
--     0-parameter fire candidate on the live weapon while NOT aiming.
--     A muzzle flash from normal locomotion = the gate is a check, not
--     structure.
-- Hotkeys (numpad only, per project rule):
--   NUMPAD4 = recon + arm hooks     NUMPAD5 = manual-call selected candidate
--   NUMPAD7 = latch HOLD on         NUMPAD8 = unlatch
--   NUMPAD9 = PANIC (unlatch, nothing else runs continuously)
-- Hooks install only on demand (NUM4), never at boot. Everything is
-- observe-only except the deliberate NUM5 call.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_noaim]"

local NS = sdk.game_namespace

local KIND_ATTACK_FALLBACK = 256  -- learned in v1 (old guess 4 was wrong)
local KIND_HOLD_FALLBACK = 64

local VK_NUMPAD4 = 0x64
local VK_NUMPAD5 = 0x65
local VK_NUMPAD7 = 0x67
local VK_NUMPAD8 = 0x68
local VK_NUMPAD9 = 0x69

-- method names that smell like the fire path
local CANDIDATE_PATTERNS = { "fire", "shoot", "trigger", "shell", "launch", "attack" }
-- component type names worth a full method dump + candidate scan
local COMPONENT_PATTERNS = { "weapon", "gun", "implement" }

local REACT_WINDOW = 0.35   -- seconds after an ATTACK press that counts as "reacted"

local state = {
    kind_attack = nil,
    kind_hold = nil,
    kind_names = {},
    enum_dumped = false,
    setforce_ok_count = 0,
    setforce_fail_count = 0,
    last_error = "",
    latched = false,
    latch_time = nil,
    in_our_call = false,
    events = {},
    prev_is_hold = nil,
    prev_player_found = nil,
    prev_weapon_name = nil,
    keys_ok = true,
    prev_keys = {},
    -- fire-path hunt
    armed = false,
    dumped_types = {},        -- type full name -> true (recon done)
    hooked_sigs = {},         -- "type.method(n)" -> true (hook installed)
    candidates = {},          -- array of candidate records (see arm_hooks)
    hook_count = 0,
    selected_idx = nil,       -- hand-picked candidate for NUM5 (fallback)
    exec_fire_idx = nil,      -- the executeFire candidate, preferred NUM5 target
    last_fire_arg = 0,        -- executeFire's argument captured from a real shot
    press_count = 0,
    press_time = nil,         -- pending press awaiting its summary
    press_calls = 0,          -- candidate calls seen since press_time
    press_methods = {},       -- sig -> count within the current press window
    press_hold = "n/a",       -- IsHold at the moment of the press
    prev_attack_on = false,
    -- live readouts
    ui_player_found = false,
    ui_weapon_name = "none",
    ui_is_hold = "n/a",
    ui_bits_on = "n/a",
    ui_attack_on = false,
    ui_hold_on = false,
}

local function safe(fn)
    local ok, r = pcall(fn)
    if ok then return r end
    return nil
end

local function record_event(msg)
    local line = string.format("[t=%8.2fs] %s", os.clock(), msg)
    log.info(TAG .. " EVENT " .. line)
    local ev = state.events
    ev[#ev + 1] = line
    if #ev > 60 then table.remove(ev, 1) end
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

local function name_matches(lower_name, patterns)
    for _, p in ipairs(patterns) do
        if lower_name:find(p, 1, true) then return true end
    end
    return false
end

-- ---------------------------------------------------------------------------
-- Kind enum (for ATTACK/HOLD bit values and labels)
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

-- ---------------------------------------------------------------------------
-- The latch (proven lever from v1-v3, kept for A/B comparison)
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
    record_event("LATCH ON")
end

local function do_unlatch()
    set_force_hold(false)
    state.latched = false
    state.latch_time = nil
    record_event("LATCH OFF")
end

local function do_panic()
    set_force_hold(false)
    state.latched = false
    state.latch_time = nil
    record_event("PANIC (unlatched; hooks stay but only observe)")
end

-- ---------------------------------------------------------------------------
-- Recon: component list + method dumps + candidate discovery (NUM4)
-- ---------------------------------------------------------------------------

local function type_hierarchy(td)
    local list = {}
    local depth = 0
    while td and depth < 8 do
        list[#list + 1] = td
        td = safe(function() return td:get_parent_type() end)
        depth = depth + 1
    end
    return list
end

local function dump_and_scan_type(td, comp_label)
    local tname = safe(function() return td:get_full_name() end) or "?"
    if state.dumped_types[tname] then return end
    state.dumped_types[tname] = true

    log.info(string.format("%s ---- method dump: %s (component %s) ----", TAG, tname, comp_label))
    for _, m in ipairs(td:get_methods() or {}) do
        local name = safe(function() return m:get_name() end)
        if name then
            local nparams = safe(function() return m:get_num_params() end) or -1
            local ret = safe(function() return m:get_return_type():get_full_name() end) or "?"
            local ptypes = {}
            local pt = safe(function() return m:get_param_types() end)
            if pt then
                for _, p in ipairs(pt) do
                    ptypes[#ptypes + 1] = safe(function() return p:get_full_name() end) or "?"
                end
            end
            log.info(string.format("%s   %s %s(%s)", TAG, ret, name, table.concat(ptypes, ", ")))

            local lower = name:lower()
            if name_matches(lower, CANDIDATE_PATTERNS) and not lower:find("^set_") then
                local sig = tname .. "." .. name .. "(" .. tostring(nparams) .. ")"
                if not state.hooked_sigs[sig] then
                    local c = {
                        sig = sig,
                        type_name = tname,
                        name = name,
                        nparams = nparams,
                        is_bool_probe = (ret == "System.Boolean") and (nparams == 0),
                        is_execute_fire = (name == "executeFire") and (nparams == 1),
                        method = m,
                        hooked = false,
                        calls = 0,
                        reacted = 0,
                        last_log = 0,
                    }
                    state.candidates[#state.candidates + 1] = c
                    if c.is_execute_fire then
                        state.exec_fire_idx = #state.candidates
                        record_event("star candidate found: " .. sig .. " (NUM5 will replay it)")
                    end
                end
            end
        end
    end
end

local function hook_candidate(c)
    if c.hooked or state.hooked_sigs[c.sig] then return end
    local ok, err = pcall(function()
        sdk.hook(c.method,
            function(args)
                if not state.armed then return end
                c.calls = c.calls + 1
                local now = os.clock()
                -- the star: always log executeFire and capture its argument
                if c.is_execute_fire then
                    local arg = safe(function() return sdk.to_int64(args[3]) & 0xFFFFFFFF end)
                    if arg ~= nil then state.last_fire_arg = arg end
                    if now - c.last_log > 1.0 then
                        c.last_log = now
                        record_event(string.format("FIRE! executeFire(arg=%s) [IsHold=%s latched=%s]",
                            tostring(arg), state.ui_is_hold, tostring(state.latched)))
                    end
                end
                local near_press = state.press_time and (now - state.press_time) <= REACT_WINDOW
                if near_press then
                    state.press_calls = state.press_calls + 1
                    c.reacted = c.reacted + 1
                    state.press_methods[c.sig] = (state.press_methods[c.sig] or 0) + 1
                elseif c.calls == 1 then
                    record_event("first sighting (no press nearby): " .. c.sig)
                end
            end,
            function(retval)
                if state.armed and c.is_bool_probe and state.press_time
                    and (os.clock() - state.press_time) <= REACT_WINDOW then
                    local v = safe(function() return (sdk.to_int64(retval) & 1) ~= 0 end)
                    local now = os.clock()
                    if now - c.last_log > 1.0 then
                        c.last_log = now
                        record_event(string.format("gate read near press #%d: %s -> %s",
                            state.press_count, c.sig, tostring(v)))
                    end
                end
                return retval
            end)
    end)
    if ok then
        c.hooked = true
        state.hooked_sigs[c.sig] = true
        state.hook_count = state.hook_count + 1
    else
        record_event("hook FAILED: " .. c.sig .. " -- " .. tostring(err))
    end
end

local function recon_and_arm()
    local player = get_player()
    local weapon = get_equipped_weapon(player)
    if not weapon then
        record_event("NUM4: no equipped weapon -- equip a gun first")
        return
    end

    local wtd = safe(function() return weapon:get_type_definition() end)
    local wtype = wtd and (safe(function() return wtd:get_full_name() end) or "?") or "?"
    record_event("recon: EquipWeapon component type = " .. wtype)

    -- the weapon component's own hierarchy, always
    if wtd then
        for _, td in ipairs(type_hierarchy(wtd)) do
            dump_and_scan_type(td, "EquipWeapon")
        end
    end

    -- sibling components on the same GameObject
    local go = safe(function() return weapon:call("get_GameObject") end)
    if go then
        local comps = safe(function() return go:call("get_Components") end)
        local elems = comps and safe(function() return comps:get_elements() end) or nil
        if elems then
            log.info(TAG .. " ---- component list on weapon GameObject ----")
            for _, comp in ipairs(elems) do
                local ctd = safe(function() return comp:get_type_definition() end)
                local cname = ctd and (safe(function() return ctd:get_full_name() end) or "?") or "?"
                log.info(TAG .. "   component: " .. cname)
                if ctd and name_matches(cname:lower(), COMPONENT_PATTERNS) then
                    for _, td in ipairs(type_hierarchy(ctd)) do
                        dump_and_scan_type(td, cname)
                    end
                end
            end
        else
            record_event("component enumeration unavailable (get_Components failed)")
        end
    end

    for _, c in ipairs(state.candidates) do
        hook_candidate(c)
    end
    state.armed = true
    record_event(string.format("ARMED: %d candidates, %d hooked. Now: LMB unaimed x3, then RMB-aim + fire x3, then NUM7 latch + fire x3.",
        #state.candidates, state.hook_count))
end

-- ---------------------------------------------------------------------------
-- The decisive experiment: manual-call a candidate while NOT aiming (NUM5)
-- ---------------------------------------------------------------------------

local function manual_call(idx)
    local c = state.candidates[idx]
    if not c then
        record_event("NUM5: no candidate selected yet (arm with NUM4, fire once aimed, or pick in the UI)")
        return
    end
    if c.nparams ~= 0 and not c.is_execute_fire then
        record_event("NUM5: selected candidate takes parameters, cannot call blind: " .. c.sig)
        return
    end
    local player = get_player()
    local weapon = get_equipped_weapon(player)
    if not weapon then
        record_event("NUM5: no equipped weapon")
        return
    end
    -- call on the component whose type matches the candidate's type if possible
    local target = weapon
    local go = safe(function() return weapon:call("get_GameObject") end)
    if go then
        local comp = get_component(go, c.type_name)
        if comp then target = comp end
    end
    local ok, r
    if c.is_execute_fire then
        local arg = state.last_fire_arg or 0
        record_event(string.format("MANUAL CALL %s with arg=%d [IsHold=%s latched=%s]",
            c.sig, arg, state.ui_is_hold, tostring(state.latched)))
        ok, r = pcall(function() return target:call(c.name, arg) end)
    else
        record_event(string.format("MANUAL CALL %s [IsHold=%s latched=%s]",
            c.sig, state.ui_is_hold, tostring(state.latched)))
        ok, r = pcall(function() return target:call(c.name) end)
    end
    record_event(string.format("MANUAL CALL result: ok=%s ret=%s -- did a shot come out?",
        tostring(ok), tostring(r)))
end

-- ---------------------------------------------------------------------------
-- Hotkeys
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
    if key_pressed(VK_NUMPAD4) then recon_and_arm() end
    if key_pressed(VK_NUMPAD5) then manual_call(state.exec_fire_idx or state.selected_idx or 1) end
    if key_pressed(VK_NUMPAD7) then do_latch() end
    if key_pressed(VK_NUMPAD8) then do_unlatch() end
    if key_pressed(VK_NUMPAD9) then do_panic() end
end

-- ---------------------------------------------------------------------------
-- Watchdog + press-edge detection + readouts
-- ---------------------------------------------------------------------------

local function watch_transitions(player_found, weapon_name, is_hold)
    if state.latch_time and is_hold == true then
        record_event(string.format("stance up %.0f ms after latch",
            (os.clock() - state.latch_time) * 1000.0))
        state.latch_time = nil
    end
    if state.prev_is_hold == true and is_hold == false and state.latched then
        record_event(string.format("STANCE DROPPED while latched [player=%s weapon=%s]",
            tostring(player_found), weapon_name))
    end
    state.prev_is_hold = is_hold

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

local function read_button_bits(input_system)
    local bb = safe(function() return input_system:call("get_ButtonBits") end)
    if not bb then return nil end
    local on = safe(function() return bb:get_field("On") end)
    if on == nil then on = safe(function() return bb:read_qword(0x18) end) end
    return on
end

local function finalize_pending_press()
    if state.press_time and os.clock() - state.press_time > REACT_WINDOW then
        local parts = {}
        for sig, n in pairs(state.press_methods) do
            parts[#parts + 1] = { sig = sig, n = n }
        end
        table.sort(parts, function(a, b) return a.n > b.n end)
        local buf = {}
        for i, p in ipairs(parts) do
            if i > 12 then buf[#buf + 1] = "+more" break end
            buf[#buf + 1] = string.format("%s x%d",
                p.sig:gsub("app%.ropeway%.implement%.", ""):gsub("app%.ropeway%.", ""), p.n)
        end
        record_event(string.format("press #%d summary [IsHold=%s latched=%s]: %s",
            state.press_count, state.press_hold, tostring(state.latched),
            #parts == 0 and "0 calls -- THE GATE ATE IT" or table.concat(buf, ", ")))
        state.press_time = nil
        state.press_calls = 0
        state.press_methods = {}
    end
end

local function refresh_readouts()
    dump_kind_enum()
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
    finalize_pending_press()

    local input_system = get_input_system()
    if input_system then
        local on = read_button_bits(input_system)
        local ka = state.kind_attack or KIND_ATTACK_FALLBACK
        local kh = state.kind_hold or KIND_HOLD_FALLBACK
        if type(on) == "number" then
            state.ui_bits_on = string.format("0x%X", on)
            local attack_now = (on & ka) ~= 0
            state.ui_hold_on = (on & kh) ~= 0
            -- ATTACK press edge -> start a per-press observation window
            if attack_now and not state.prev_attack_on and state.armed then
                finalize_pending_press()
                state.press_count = state.press_count + 1
                state.press_time = os.clock()
                state.press_calls = 0
                state.press_methods = {}
                state.press_hold = state.ui_is_hold
                record_event(string.format("ATTACK pressed (#%d) [IsHold=%s latched=%s]",
                    state.press_count, state.press_hold, tostring(state.latched)))
            end
            state.prev_attack_on = attack_now
            state.ui_attack_on = attack_now
        else
            state.ui_bits_on = "unreadable"
        end
    else
        state.ui_bits_on = "no InputSystem"
    end
end

re.on_frame(refresh_readouts)

-- ---------------------------------------------------------------------------
-- UI
-- ---------------------------------------------------------------------------

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: fire-gate probe (v4.1)") then return end

    imgui.text("hotkeys: NUM4=arm  NUM5=manual-call  NUM7=latch  NUM8=unlatch  NUM9=panic"
        .. (state.keys_ok and "" or "  [KEY API UNAVAILABLE - use buttons]"))
    imgui.text("armed: " .. tostring(state.armed)
        .. string.format("   candidates: %d   hooked: %d", #state.candidates, state.hook_count))
    imgui.text("latched: " .. tostring(state.latched))
    imgui.text("player: " .. (state.ui_player_found and "found" or "NOT FOUND"))
    imgui.text("weapon: " .. state.ui_weapon_name)
    imgui.text("SurvivorCondition.IsHold: " .. state.ui_is_hold)
    imgui.text("ButtonBits On: " .. state.ui_bits_on
        .. string.format("   ATTACK: %s   HOLD: %s",
            state.ui_attack_on and "ON" or "off",
            state.ui_hold_on and "ON" or "off"))
    imgui.text(string.format("presses observed: %d   setForce ok=%d fail=%d",
        state.press_count, state.setforce_ok_count, state.setforce_fail_count))
    local num5_idx = state.exec_fire_idx or state.selected_idx
    if num5_idx and state.candidates[num5_idx] then
        local c5 = state.candidates[num5_idx]
        imgui.text("NUM5 target: " .. c5.sig
            .. (c5.is_execute_fire and string.format("  [replay arg=%d]", state.last_fire_arg or 0) or ""))
    else
        imgui.text("NUM5 target: none selected yet")
    end
    if state.last_error ~= "" then
        imgui.text("last error: " .. state.last_error)
    end

    imgui.spacing()

    if imgui.button("ARM: recon weapon + hook fire candidates (NUM4)") then
        recon_and_arm()
    end
    if imgui.button("LATCH (NUM7)") then do_latch() end
    if imgui.button("UNLATCH (NUM8)") then do_unlatch() end
    if imgui.button("PANIC (NUM9)") then do_panic() end

    imgui.spacing()

    if imgui.tree_node(string.format("candidates (%d)", #state.candidates)) then
        for i, c in ipairs(state.candidates) do
            imgui.text(string.format("[%s] x%d reacted:%d %s",
                c.hooked and "hooked" or "  --  ", c.calls, c.reacted, c.sig))
            if c.nparams == 0 or c.is_execute_fire then
                imgui.same_line()
                if imgui.button("select##" .. i) then
                    state.selected_idx = i
                    record_event("NUM5 target set by hand: " .. c.sig)
                end
                imgui.same_line()
                if imgui.button("call##" .. i) then
                    manual_call(i)
                end
            end
        end
        imgui.tree_pop()
    end

    if imgui.tree_node(string.format("event log (%d)", #state.events)) then
        for i = #state.events, 1, -1 do
            imgui.text(state.events[i])
        end
        imgui.tree_pop()
    end

    imgui.tree_pop()
end)

log.info(TAG .. " loaded (v4.1 fire-gate hunt, executeFire replay). Hooks install only on NUM4; all idle until used.")
