-- Visceral (RE2 VR) -- PROBE: why does the aim (hold) state end by itself ~50 frames after a shot on the
-- relaxed-walk splice?  Built 2026-10-02 (/lm, home PC) for a FLAT run with no headset.
--
-- Measured 2026-09-30 (b039, Tefa in VR, n=4 shots): a shot fired while stopping walking on the spliced
-- base_hdg_hold ends with SurvivorCondition.IsHold 1 -> 0 at frame ~50 although the right grip stays squeezed;
-- layer 0 leaves the hold bank, the lower-the-gun pose plays, the VR hands snap back when the hold returns.
-- Dossier 8k: get_IsHold is literally StateTagHandle.hasTag(HOLD) -- a READ-OUT of the motion FSM's state, not
-- a switch anybody sets. So the question is which FSM transition leaves the HOLD-tagged state, and whether it
-- happens with the aim input provably held every frame (flat: InputSystem.setForce(HOLD) is a latch).
--
-- Run 1 (2026-10-02 16:00-16:10, flat, handgun, 10 real walking shots at the stop, ammo counted): the hold
-- NEVER dropped with the game's own HOLD input latched. So run 2 asks the other half: does a ONE-FRAME gap in
-- the hold input, at the end of the shot, give the 09-30 signature (hold 0, OFF_Gazing_Idle on layer 0, the
-- lowering pose, hold back later)? If yes, the VR cause is reduced to "the aim input is missing for a frame".
--
-- Keys (numpad, read by virtual key):
--   NUM1  forced HOLD on/off (InputSystem.setForce(64, bool))     -- the aim input, held by the game itself
--   NUM2  one shot            (InputSystem.setForce(256, true) for 8 frames, then false)
--   NUM3  dump the type surfaces this probe leans on (also done once at the first player bind)
--   NUM4  GAP mode on/off: after every NUM2 shot, at frame GAP_AT, drop HOLD for GAP_LEN frames, then re-assert
--   NUM5  block the aim turn-on-the-spot (HG_Wheel, Petient TURN) while aiming -- the throw (2026-10-02 evening); forbid-aim lever retired
--   NUM6  VR grip latch: HOLD forced on while the right grip is squeezed (vrmod), kept on 0.5 s after it reads released
--         (a shorter dip is bridged AND measured: 'grip BACK after N ms')
--   NUM8  cycle GAP_LEN (was NUM9: that key is the title script's one-scene swap, and three presses on 2026-10-02 turned it off) 1 -> 3 -> 6 -> 12 -> 1
-- What it logs: one line per CHANGE of (hold, layer-0 motion) with the frame count since the last shot, whether
-- the game sees the HOLD input on, the motion frame / end frame, the orderer's Precede/Petient, and the forbid-aim
-- controller's plain fields. SHOT #n lines mark each shot; FIRE REQUEST lines prove the game fired (hook on
-- Equipment.requestFire); FSM TRANSITION lines come from SurvivorActionOrderer.onMotionTransitionEvent.
-- Driven from outside by dev-archive/tools/hold_exit_drive.py (W/S held / released around each NUM2).
--
-- Probe: archive it once it has answered (standing rule).

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_holdexit]"
local NS = sdk.game_namespace
local VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD4, VK_NUMPAD5, VK_NUMPAD6, VK_NUMPAD8 = 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x68
local PETIENT_HOLD = 16
local PETIENT_TURN = 32         -- Petient order read as 32 whenever layer 0 played HG_Wheel_L180/R180 (2026-10-02 logs)         -- reader 2026-10-02: HOLD is a Petient order (16), decided by PlayerActionOrderer.checkOrder(Petient)
local KIND_HOLD, KIND_ATTACK = 64, 256
local ATTACK_FRAMES = 8          -- proven 2026-09-05: 8 frames of ATTACK under HOLD fires a real shot
local FOLLOW_FRAMES = 240        -- the drop came at ~50 frames; follow well past the hold coming back
local GAP_AT = 48                -- frames after the shot request; the 09-30 drops were at ~50
local GAP_LENS = { 1, 3, 6, 12 }
local GRIP_HOLDOVER = 0.5         -- s: NUM6 keeps HOLD on this long after the grip reads released (a dip shorter than this is bridged and measured)

local cfg = { hold = false, gap = false, gap_len_i = 1, noforbid = false, griplatch = false, noturn = false }
local st = { k1 = false, k2 = false, k3 = false, k4 = false, k5 = false, k6 = false, k8 = false, co_arg = nil, co_last = nil, bound = false, attack_left = 0, shots = 0,
    since_shot = -1, last = "", orderer = nil, forbid_fields = nil, drops = {}, dumped = false, follow = nil,
    gap_left = 0, fire_seen = 0, layers_debug = false, frame_n = 0, grip_latched = false, grip_lost_at = nil, turn_inhibited = false }

local tr = { left = 0, n = 0, joints = {} }   -- the after-shot trace (declared before the hooks that arm it)
local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end
local function component(player, full)
    local t = sdk.typeof(full); if not t then return nil end
    return safe(function() return player:call("getComponent(System.Type)", t) end)
end
local function input_system() return sdk.get_managed_singleton(NS("InputSystem")) end
local function input_on(kind)
    local is = input_system(); if not is then return "?" end
    local r = safe(function() return is:call("isOn", kind) end)
    if r == nil then return "?" end
    return r and 1 or 0
end
local function set_force(kind, on)
    local is = input_system(); if not is then log_line("InputSystem singleton NOT found"); return false end
    return pcall(function() is:call("setForce", kind, on) end)
end

-- ---- type surface dump (once) -------------------------------------------------------------------------------
local function dump_type(full, what)
    local t = sdk.find_type_definition(full)
    if not t then log_line("type NOT found: " .. full); return end
    if what == "methods" or what == "both" then
        local names = {}
        for _, m in ipairs(t:get_methods()) do names[#names + 1] = m:get_name() .. "/" .. m:get_num_params() end
        table.sort(names)
        log_line("methods of " .. full .. " (" .. #names .. "): " .. table.concat(names, " "))
    end
    if what == "fields" or what == "both" then
        local names = {}
        for _, f in ipairs(t:get_fields()) do
            local ft = safe(function() return f:get_type():get_full_name() end) or "?"
            names[#names + 1] = f:get_name() .. ":" .. ft
        end
        table.sort(names)
        log_line("fields of " .. full .. " (" .. #names .. "): " .. table.concat(names, " "))
    end
end
local function dump_all()
    dump_type(NS("survivor.player.PlayerForbidAimController"), "both")
    dump_type(NS("survivor.player.PlayerCondition"), "fields")
    dump_type(NS("motion.taghandle.SurvivorMotionFsmTagHandle.StateTagHandle"), "methods")
end

-- ---- hooks: the fire request (proof of a shot) and the FSM transition event ------------------------------------
do
    local eq_t = sdk.find_type_definition(NS("survivor.Equipment"))
    local m = eq_t and eq_t:get_method("requestFire")
    if m then
        sdk.hook(m, function(args)
            st.fire_seen = st.fire_seen + 1
            log_line(string.format("FIRE REQUEST #%d at f+%d", st.fire_seen, st.since_shot))
            tr.n = st.fire_seen; tr.left = 240
        end, function(rv) return rv end)
        log_line("hooked Equipment.requestFire (proof of a shot)")
    else
        log_line("Equipment.requestFire NOT FOUND")
    end
    local ao_t = sdk.find_type_definition(NS("survivor.SurvivorActionOrderer"))
    local tr = ao_t and ao_t:get_method("onMotionTransitionEvent")
    if tr then
        local params = {}
        for i, p in ipairs(tr:get_param_types()) do params[i] = p:get_full_name() end
        log_line("hooking SurvivorActionOrderer.onMotionTransitionEvent(" .. table.concat(params, ", ") .. ")")
        sdk.hook(tr, function(args)
            local a1 = sdk.to_int64(args[3]) or -1
            local a2 = sdk.to_int64(args[4]) or -1
            local o2 = safe(function() return sdk.to_managed_object(args[4]) end)
            local n2 = o2 and safe(function() return o2:get_type_definition():get_full_name() end) or "-"
            log_line(string.format("FSM TRANSITION event at f+%d: arg1=%d arg2=%d (%s)", st.since_shot, a1, a2, n2))
        end, function(rv) return rv end)
    else
        log_line("SurvivorActionOrderer.onMotionTransitionEvent NOT FOUND")
    end
    -- the HOLD decision itself (reader 2026-10-02): PlayerActionOrderer.checkOrder(Petient) with arg 16 = HOLD.
    -- Logged whenever its answer CHANGES while the forced hold is on, with the inputs it weighs.
    local pao_t = sdk.find_type_definition(NS("survivor.player.PlayerActionOrderer"))
    local co
    if pao_t then
        for _, m in ipairs(pao_t:get_methods()) do
            if m:get_name() == "checkOrder" and m:get_num_params() == 1 then
                local pt = m:get_param_types()[1]:get_full_name()
                if pt:find("Petient") then co = m end
            end
        end
    end
    if co then
        sdk.hook(co, function(args)
            st.co_arg = sdk.to_int64(args[3])
            st.co_this = sdk.to_managed_object(args[2])
        end, function(rv)
            if st.co_arg == PETIENT_HOLD then
                local r = (sdk.to_int64(rv) & 0xff) ~= 0
                -- log only a refusal while the hold is up (the moment that matters), at most once per 10 frames
                if (not r) and st.last:sub(1, 1) == "1" and (st.co_last_f == nil or st.frame_n - st.co_last_f >= 10) then
                    st.co_last_f = st.frame_n
                    local o = st.co_this
                    local cond = o and safe(function() return o:call("get_Condition") end)
                    local fa = cond and safe(function() return cond:call("get_IsForbidAim") end)
                    local timer = o and safe(function() return o:get_field("ForbidHoldDeferTimer") end)
                    local tdone = timer and safe(function() return timer:call("get_Completed") end)
                    local inc = safe(function() return o:call("get_InConstraint") end)
                    local eq = cond and safe(function() return cond:call("get_Equipment") end)
                    local ehold = eq and safe(function() return eq:call("get_EnabledHoldMainWeapon") end)
                    log_line(string.format("checkOrder(HOLD) now %s at f+%d: inputHOLD=%s IsForbidAim=%s deferTimerDone=%s InConstraint=%s EnabledHoldMainWeapon=%s",
                        tostring(r), st.since_shot, tostring(input_on(KIND_HOLD)), tostring(fa), tostring(tdone), tostring(inc), tostring(ehold)))
                end
            end
            return rv
        end)
        log_line("hooked PlayerActionOrderer.checkOrder(Petient) -- the HOLD decision")
    else
        log_line("PlayerActionOrderer.checkOrder(Petient) NOT FOUND")
    end
    -- NUM5 lever: the forbid-aim (gun lowers at a wall) answered false while on
    local fac_t = sdk.find_type_definition(NS("survivor.player.PlayerForbidAimController"))
    local gf = fac_t and fac_t:get_method("get_IsForbid")
    if gf then
        sdk.hook(gf, function(args) end, function(rv)
            if cfg.noforbid then return sdk.to_ptr(0) end
            return rv
        end)
        log_line("hooked PlayerForbidAimController.get_IsForbid (NUM5 forces it false)")
    else
        log_line("PlayerForbidAimController.get_IsForbid NOT FOUND")
    end
end

-- ---- readers ------------------------------------------------------------------------------------------------
local function motion_text(player)
    local mo = component(player, "via.motion.Motion"); if not mo then return "nomotion", "", -1, -1 end
    local l0 = safe(function() return mo:call("getLayer", 0) end); if not l0 then return "nolayer", "", -1, -1 end
    local node = safe(function() return l0:call("get_HighestWeightMotionNode") end)
    local name = node and safe(function() return node:call("get_MotionName") end) or "-"
    local frame = safe(function() return l0:call("get_Frame") end) or -1
    local endf = safe(function() return l0:call("get_EndFrame") end) or -1
    local n = safe(function() return mo:call("get_LayerCount") end) or 0
    local parts = {}
    for i = 0, n - 1 do
        local layer = safe(function() return mo:call("getLayer", i) end)
        local id = layer and safe(function() return layer:call("get_MotionID") end)
        local bank = layer and safe(function() return layer:call("get_MotionBankID") end)
        if not st.layers_debug then
            st.layers_debug = true
            log_line(string.format("layers debug: count=%s layer0=%s id=%s (%s) bank=%s", tostring(n), tostring(layer), tostring(id), type(id), tostring(bank)))
        end
        if id ~= nil then parts[#parts + 1] = string.format("L%d=%s/%s", i, tostring(bank), tostring(id)) end
    end
    return tostring(name), table.concat(parts, " "), frame, endf
end
local PLAIN = { ["System.UInt32"] = true, ["System.Int32"] = true, ["System.Boolean"] = true, ["System.Byte"] = true,
    ["System.UInt16"] = true, ["System.Int16"] = true, ["System.UInt64"] = true, ["System.Int64"] = true, ["System.Single"] = true }
local function plain_fields_of(obj)
    local names = {}
    local t = obj:get_type_definition()
    while t do
        for _, f in ipairs(t:get_fields()) do
            local ft = safe(function() return f:get_type():get_full_name() end) or ""
            if PLAIN[ft] and not f:is_static() then names[#names + 1] = f:get_name() end
        end
        t = t:get_parent_type()
    end
    return names
end
local function plain_text(obj, names, label)
    local parts = {}
    for _, name in ipairs(names) do
        local v = safe(function() return obj:get_field(name) end)
        if v ~= nil and v ~= 0 and v ~= false then
            if type(v) == "number" then parts[#parts + 1] = name .. "=" .. string.format("%g", v)
            else parts[#parts + 1] = name .. "=" .. tostring(v) end
        end
    end
    return label .. "{" .. table.concat(parts, " ") .. "}"
end
local function orderer_text(cond)
    local o = safe(function() return cond:call("get_ActionOrderer") end)
    if not o then return "orderer=?" end
    local pre = safe(function() return o:call("get_Precede") end)
    local pet = safe(function() return o:call("get_Petient") end)
    return string.format("precede=%s petient=%s", tostring(pre), tostring(pet))
end
local function forbid_text(cond)
    local fa = safe(function() return cond:call("get_IsForbidAim") end)
    local fc = safe(function() return cond:get_field("<ForbidAimController>k__BackingField") end)
    if not fc then return "IsForbidAim=" .. tostring(fa) end
    if not st.forbid_fields then
        st.forbid_fields = plain_fields_of(fc)
        log_line("forbid-aim controller plain fields: " .. table.concat(st.forbid_fields, " "))
    end
    return "IsForbidAim=" .. tostring(fa) .. " " .. plain_text(fc, st.forbid_fields, "forbid")
end

local function forbid_target(cond)
    -- reader 2026-10-02: forbid-aim is true only when an app.ropeway.AimCandidate flagged no-shoot sits inside a 0.1 m
    -- sphere cast 1 m in front of the camera (the headset in VR). Best effort: name that object at a drop.
    local fc = safe(function() return cond:get_field("<ForbidAimController>k__BackingField") end)
    if not fc then return "?" end
    local function name_of(o)
        local go = safe(function() return o:call("get_TargetGameObject") end) or safe(function() return o:call("get_GameObject") end)
        return go and safe(function() return go:call("get_Name") end)
    end
    local n = name_of(fc)
    if n then return n end
    local t = fc:get_type_definition()
    for _, f in ipairs(t:get_fields()) do
        local ft = safe(function() return f:get_type():get_full_name() end) or ""
        if ft:find("Aim") or ft:find("Forbid") or ft:find("Check") then
            local o = safe(function() return fc:get_field(f:get_name()) end)
            local m = o and type(o) == "userdata" and name_of(o)
            if m then return f:get_name() .. ":" .. m end
        end
    end
    return "unnamed"
end

-- ---- frame-by-frame trace after every real shot (2026-10-02 evening): the desktop stays black in VR even with the
-- mirror setting off, so pictures are impossible from the PC; this is the picture in numbers. For TRACE_FRAMES after
-- each fire request, one line per frame: gun joint and both wrists relative to the head in the camera's right/up/forward
-- axes (cm), the two controllers relative to the headset (cm, vrmod), hold, layer-0 motion + frame. Plotted afterwards.
local TRACE_FRAMES = 240
local function joint(tf, name)
    local j = tr.joints[name]
    if j == nil then j = safe(function() return tf:call("getJointByName", name) end) or false; tr.joints[name] = j end
    return j or nil
end
local function cam_axes()
    local cam = sdk.get_primary_camera()
    local go = cam and safe(function() return cam:call("get_GameObject") end)
    local ctf = go and safe(function() return go:call("get_Transform") end)
    if not ctf then return nil end
    local ax = safe(function() return ctf:call("get_AxisX") end)
    local ay = safe(function() return ctf:call("get_AxisY") end)
    local az = safe(function() return ctf:call("get_AxisZ") end)
    if ax and ay and az then return ax, ay, az end
    return nil
end
local function rel(p, h, ax, ay, az)
    if not p or not h then return "(? ? ?)" end
    local d = { p.x - h.x, p.y - h.y, p.z - h.z }
    if not ax then return string.format("(%+.1f %+.1f %+.1f)w", d[1] * 100, d[2] * 100, d[3] * 100) end
    return string.format("(%+.1f %+.1f %+.1f)", (d[1] * ax.x + d[2] * ax.y + d[3] * ax.z) * 100,
        (d[1] * ay.x + d[2] * ay.y + d[3] * ay.z) * 100, (d[1] * az.x + d[2] * az.y + d[3] * az.z) * 100)
end
local function trace_frame(p, hold, mname, frame)
    local tf = safe(function() return p:call("get_Transform") end); if not tf then return end
    local pos = function(j) return j and safe(function() return j:call("get_Position") end) or nil end
    local head = pos(joint(tf, "head"))
    local ax, ay, az = cam_axes()
    local gun = pos(joint(tf, "r_weapon")) or pos(joint(tf, "r_arm_wrist"))
    local rw, lw = pos(joint(tf, "r_arm_wrist")), pos(joint(tf, "l_arm_wrist"))
    -- Tefa 20:45: "all shots moved the gun equally", yet shots 1-2 showed <= 4 cm gun-vs-HEAD-JOINT. So what is seen must
    -- be the gun vs the CAMERA (the headset): log the camera position and the gun/head relative to it as well.
    local cam = sdk.get_primary_camera()
    local cgo = cam and safe(function() return cam:call("get_GameObject") end)
    local ctf = cgo and safe(function() return cgo:call("get_Transform") end)
    local cpos = ctf and safe(function() return ctf:call("get_Position") end)
    local camtxt = string.format("gun_vs_cam=%s head_vs_cam=%s", rel(gun, cpos, ax, ay, az), rel(head, cpos, ax, ay, az))
    local ctl = "ctl=?"
    if vrmod then
        local okh, hmd = pcall(function() return vrmod:get_position(0) end)
        local okr, rc = pcall(function() return vrmod:get_position(vrmod:get_right_controller_index()) end)
        local okl, lc = pcall(function() return vrmod:get_position(vrmod:get_left_controller_index()) end)
        if okh and okr and okl and hmd and rc and lc then
            ctl = string.format("rctl=(%+.1f %+.1f %+.1f) lctl=(%+.1f %+.1f %+.1f)", (rc.x - hmd.x) * 100, (rc.y - hmd.y) * 100, (rc.z - hmd.z) * 100,
                (lc.x - hmd.x) * 100, (lc.y - hmd.y) * 100, (lc.z - hmd.z) * 100)
        elseif not tr.ctl_err then
            tr.ctl_err = true
            log_line("trace: controller read failed: " .. tostring(hmd) .. " / " .. tostring(rc) .. " / " .. tostring(lc))
        end
    end
    ctl = camtxt .. " " .. ctl
    log.info(string.format("[visceral_trace] shot%d f=%d hold=%d gun=%s rwrist=%s lwrist=%s %s layer0=%s mf=%.1f",
        tr.n, TRACE_FRAMES - tr.left, hold and 1 or 0, rel(gun, head, ax, ay, az), rel(rw, head, ax, ay, az), rel(lw, head, ax, ay, az), ctl, tostring(mname), frame or -1))
end

-- ---- the per-frame trace ------------------------------------------------------------------------------------
re.on_frame(function()
    st.frame_n = st.frame_n + 1
    local p = get_player()
    if not p then
        if st.bound then log_line("player gone"); st.bound = false end
        return
    end
    if not st.bound then
        st.bound = true
        log_line("PLAYER BOUND")
        if not st.dumped then st.dumped = true; dump_all() end
    end

    -- keys
    local k1 = reframework:is_key_down(VK_NUMPAD1)
    if k1 and not st.k1 then
        cfg.hold = not cfg.hold
        local ok = set_force(KIND_HOLD, cfg.hold)
        log_line(string.format("NUM1: forced HOLD %s (setForce ok=%s)", cfg.hold and "ON" or "OFF", tostring(ok)))
    end
    st.k1 = k1
    local k4 = reframework:is_key_down(VK_NUMPAD4)
    if k4 and not st.k4 then
        cfg.gap = not cfg.gap
        log_line(string.format("NUM4: GAP mode %s (drop HOLD at f+%d for %d frame(s) after each shot)", cfg.gap and "ON" or "OFF", GAP_AT, GAP_LENS[cfg.gap_len_i]))
    end
    st.k4 = k4
    local k8 = reframework:is_key_down(VK_NUMPAD8)
    if k8 and not st.k8 then
        cfg.gap_len_i = cfg.gap_len_i % #GAP_LENS + 1
        log_line(string.format("NUM8: GAP length now %d frame(s)", GAP_LENS[cfg.gap_len_i]))
    end
    st.k8 = k9
    -- NUM5 (2026-10-02 evening): the throw is the aim-turn-on-the-spot animation (HG_Wheel_L180/R180, Petient order TURN=32)
    -- firing a moment after a stop on the relaxed walk; this blocks the TURN order while the hold is up
    local k5 = reframework:is_key_down(VK_NUMPAD5)
    if k5 and not st.k5 then
        cfg.noturn = not cfg.noturn
        log_line("NUM5: aim turn-on-the-spot (HG_Wheel) " .. (cfg.noturn and "BLOCKED (setInhibitPetient TURN while aiming)" or "back to the game"))
        if not cfg.noturn and st.turn_inhibited then
            local c0 = component(p, NS("survivor.SurvivorCondition")); local o0 = c0 and safe(function() return c0:call("get_ActionOrderer") end)
            if o0 then safe(function() o0:call("setInhibitPetient", false, PETIENT_TURN) end) end
            st.turn_inhibited = false
        end
    end
    st.k5 = k5
    if cfg.noturn then
        local c0 = component(p, NS("survivor.SurvivorCondition"))
        local o0 = c0 and safe(function() return c0:call("get_ActionOrderer") end)
        local h0 = c0 and safe(function() return c0:call("get_IsHold") end)
        if o0 and h0 and not st.turn_inhibited then
            local ok = pcall(function() o0:call("setInhibitPetient", true, PETIENT_TURN) end)
            st.turn_inhibited = true; log_line("turn block: TURN inhibited while aiming (ok=" .. tostring(ok) .. ")")
        elseif o0 and (not h0) and st.turn_inhibited then
            safe(function() o0:call("setInhibitPetient", false, PETIENT_TURN) end)
            st.turn_inhibited = false; log_line("turn block: TURN allowed again (aim down)")
        end
    end
    local k6 = reframework:is_key_down(VK_NUMPAD6)
    if k6 and not st.k6 then
        cfg.griplatch = not cfg.griplatch
        log_line("NUM6: VR grip latch " .. (cfg.griplatch and "ON (HOLD forced while the right grip is squeezed)" or "OFF"))
        if not cfg.griplatch and st.grip_latched then st.grip_latched = false; set_force(KIND_HOLD, false) end
    end
    st.k6 = k6
    -- the VR lever: while the right grip is squeezed, hold the aim input on through the latch, so neither a missing
    -- input frame nor the forbid-aim refusal can drop it (the refusal is a hard AND, so forbid still wins -- NUM5 for that)
    if cfg.griplatch and vrmod then
        local grip = safe(function() return vrmod:is_action_active(vrmod:get_action_grip(), vrmod:get_right_joystick()) end)
        local now = os.clock()
        if grip then
            if st.grip_lost_at then
                log_line(string.format("grip latch: grip BACK after %d ms away (bridged, hold never dropped)", math.floor((now - st.grip_lost_at) * 1000)))
                st.grip_lost_at = nil
            end
            if not st.grip_latched then st.grip_latched = true; set_force(KIND_HOLD, true); log_line("grip latch: HOLD forced on") end
        elseif st.grip_latched then
            if not st.grip_lost_at then st.grip_lost_at = now; log_line("grip latch: grip reads RELEASED, holding over " .. GRIP_HOLDOVER .. " s")
            elseif now - st.grip_lost_at >= GRIP_HOLDOVER then
                st.grip_latched = false; st.grip_lost_at = nil; set_force(KIND_HOLD, false); log_line("grip latch: HOLD released (grip gone for the whole hold-over)")
            end
        end
    end
    local k2 = reframework:is_key_down(VK_NUMPAD2)
    if k2 and not st.k2 and st.attack_left == 0 then
        st.attack_left = ATTACK_FRAMES
        set_force(KIND_ATTACK, true)
        st.shots = st.shots + 1
        st.since_shot = 0
        st.follow = { n = st.shots, dropped_at = nil, back_at = nil, fire0 = st.fire_seen, gap = cfg.gap and GAP_LENS[cfg.gap_len_i] or 0 }
        local cond = component(p, NS("survivor.SurvivorCondition"))
        local hold = cond and safe(function() return cond:call("get_IsHold") end)
        local mname, layers, frame, endf = motion_text(p)
        log_line(string.format("SHOT #%d requested: hold=%s inputHOLD=%s layer0=%s f=%.1f/%.1f gap=%d layers: %s %s %s",
            st.shots, tostring(hold), tostring(input_on(KIND_HOLD)), mname, frame, endf, st.follow.gap, layers,
            cond and orderer_text(cond) or "", cond and forbid_text(cond) or ""))
    end
    st.k2 = k2
    if st.attack_left > 0 then
        st.attack_left = st.attack_left - 1
        if st.attack_left == 0 then set_force(KIND_ATTACK, false) end
    end
    local k3 = reframework:is_key_down(VK_NUMPAD3)
    if k3 and not st.k3 then dump_all() end
    st.k3 = k3

    if st.since_shot >= 0 then st.since_shot = st.since_shot + 1 end

    -- the gap: drop the hold input for GAP_LEN frames at GAP_AT after the shot
    if st.follow and st.follow.gap > 0 and cfg.hold then
        if st.since_shot == GAP_AT then
            st.gap_left = st.follow.gap
            set_force(KIND_HOLD, false)
            log_line(string.format("GAP: HOLD input dropped at f+%d for %d frame(s)", st.since_shot, st.gap_left))
        elseif st.gap_left > 0 then
            st.gap_left = st.gap_left - 1
            if st.gap_left == 0 then
                set_force(KIND_HOLD, true)
                log_line(string.format("GAP: HOLD input back at f+%d", st.since_shot))
            end
        end
    end

    -- the state, logged on change
    local cond = component(p, NS("survivor.SurvivorCondition"))
    local hold = cond and safe(function() return cond:call("get_IsHold") end)
    local holdn = hold and 1 or 0
    local mname, layers, frame, endf = motion_text(p)
    local key = holdn .. "|" .. mname
    if key ~= st.last then
        local extra = ""
        if st.follow then
            if st.last:sub(1, 1) == "1" and holdn == 0 and not st.follow.dropped_at then
                st.follow.dropped_at = st.since_shot
                extra = "  <<< HOLD DROPPED (input " .. tostring(input_on(KIND_HOLD)) .. ", forbid target " .. tostring(cond and forbid_target(cond)) .. ")"
            elseif st.follow.dropped_at and holdn == 1 and not st.follow.back_at then
                st.follow.back_at = st.since_shot
                extra = "  <<< hold back"
            end
        end
        log_line(string.format("f+%d hold=%d inputHOLD=%s layer0=%s mf=%.1f/%.1f layers: %s %s %s%s",
            st.since_shot, holdn, tostring(input_on(KIND_HOLD)), mname, frame, endf, layers,
            cond and orderer_text(cond) or "", cond and forbid_text(cond) or "", extra))
        st.last = key
    end

    if tr.left > 0 then trace_frame(p, hold, mname, frame); tr.left = tr.left - 1 end

    if st.follow and st.since_shot >= FOLLOW_FRAMES then
        local f = st.follow
        local fired = st.fire_seen - f.fire0
        if f.dropped_at then
            st.drops[#st.drops + 1] = f.dropped_at
            log_line(string.format("SHOT #%d done (fire requests %d, gap %d): HOLD DROPPED at frame %d, back at %s  (drops so far: %d of %d shots)",
                f.n, fired, f.gap, f.dropped_at, tostring(f.back_at), #st.drops, st.shots))
        else
            log_line(string.format("SHOT #%d done (fire requests %d, gap %d): hold stayed up for %d frames  (drops so far: %d of %d shots)",
                f.n, fired, f.gap, FOLLOW_FRAMES, #st.drops, st.shots))
        end
        st.follow = nil
    end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral hold-exit probe") then return end
    imgui.text(string.format("forced hold (NUM1): %s   gap (NUM4): %s x%d   shots: %d   drops: %d", cfg.hold and "ON" or "off",
        cfg.gap and "ON" or "off", GAP_LENS[cfg.gap_len_i], st.shots, #st.drops))
    imgui.text("last: " .. st.last)
    imgui.tree_pop()
end)

log_line("loaded: NUM1 forced HOLD, NUM2 one shot, NUM3 dump, NUM4 gap mode, NUM8 gap length; every state change is logged")
