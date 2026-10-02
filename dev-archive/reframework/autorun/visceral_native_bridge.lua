-- visceral_native_bridge.lua — the ONLY Lua in Visceral's native path.  BRIDGE v2 (2026-10-03).
--
-- REFramework's plugin API (1.15) has no VR functions: controller and HMD poses
-- are reachable from Lua's `vrmod` only. This shim ferries them into the native
-- core (visceral_core.dll) through one shared System.Single[64]:
--   * created here, sentinel 12345 written to slot 63;
--   * handed over ONCE by calling a mailbox method the plugin hooks —
--     app.ropeway.RagdollControlZoneManager.set_AccessMutex(System.Object), a
--     real compiled game function; the plugin's pre-hook recognises the array
--     by type + length + sentinel, add_refs it, and SKIPS the original so the
--     game's setter never runs with our array (System.GC.KeepAlive was the
--     first choice on 2026-09-04 and crashed the game: it is an internal call
--     with no resolvable body in this build);
--   * the plugin acknowledges by writing 1.0 into slot 62; until it does, the
--     hand-over is repeated every ~2 s;
--   * every frame from then on this script writes poses into the array and the
--     plugin reads them. Slot map must match visceral.h's `enum Slot`.
-- Nothing in here decides anything. Logic lives in the plugin.
--
-- v2 (the RELOADED port, modding-notes/2026-10-03-reloaded-native-architecture.md):
--   * poses are written TWICE a frame: UpdateHID pre (buttons for input gating) and
--     LateUpdateBehavior post (where RELOADED read the hand). The UpdateHID copy of the
--     left position is kept in slots 53-55 so the plugin can measure whether the two
--     ever differ inside one frame — that one number settles the "stepped hands" risk;
--   * standing origin, rotation offset, right stick, A/B buttons, stick clicks added;
--   * rumble goes the OTHER way: the plugin writes amp+seconds per hand into 49-52,
--     this script fires vrmod:trigger_haptic_vibration at the next UpdateHID and zeroes them.

local TAG = "[visceral-bridge]"
local BRIDGE_VERSION = 2.0
local N = 64
local BASE = 0x20               -- element 0 of a managed array (REArrayBase)
local S_FRAME, S_HMD, S_CTL = 0, 1, 2
local S_LPOS, S_LROT = 3, 6
local S_RPOS, S_RROT = 10, 13
local S_HPOS, S_HROT = 17, 20
local S_LSTICK = 24
local S_LGRIP, S_LTRIG, S_RGRIP, S_RTRIG = 26, 27, 28, 29
local S_CINE, S_FP = 30, 31        -- v0.8: cinematic gate verdict, FirstPerson-mod-active (both Lua-only facts)
-- v2
local S_WRITE_PT, S_WRITE_SEQ = 32, 33
local S_STAND, S_ROTOFF = 34, 37
local S_RSTICK = 41
local S_LA, S_LB, S_RA, S_RB = 43, 44, 45, 46
local S_LCLICK, S_RCLICK = 47, 48
local S_RUMBLE_L_AMP, S_RUMBLE_L_SEC, S_RUMBLE_R_AMP, S_RUMBLE_R_SEC = 49, 50, 51, 52
local S_LPOS_HID = 53
local S_LATE_SEEN = 56
local S_STAND_OK = 57
local S_BRIDGE_VER = 61
local S_ACK, S_SENTINEL = 62, 63
local RUMBLE_FREQ_HZ = 160.0        -- one named number; amplitude and length come from the plugin per request
local WRITE_HID, WRITE_LATE = 1, 2

local arr = nil
local frame = 0
local seq = 0
local last_handoff = -1000
local attached = false
local keepalive = nil
local rumbles_fired = 0

local function safe(fn, ...)
    local ok, r = pcall(fn, ...)
    if ok then return r end
    return nil
end

local function r(slot) return arr:read_float(BASE + slot * 4) end

local function w(slot, v)
    arr:write_float(BASE + slot * 4, v or 0.0)
end

local function w3(slot, v)
    if v then w(slot, v.x); w(slot + 1, v.y); w(slot + 2, v.z) else w(slot, 0); w(slot + 1, 0); w(slot + 2, 0) end
end

-- 2026-09-11: the button slots must be cleared on every early-out, or the last
-- live value stays latched in the array and the plugin keeps seeing a held grip after
-- the headset or the controllers go away.
local function clear_buttons()
    w(S_LGRIP, 0); w(S_LTRIG, 0); w(S_RGRIP, 0); w(S_RTRIG, 0)
    w(S_LA, 0); w(S_LB, 0); w(S_RA, 0); w(S_RB, 0); w(S_LCLICK, 0); w(S_RCLICK, 0)
    w(S_RSTICK, 0); w(S_RSTICK + 1, 0)
end

local function w4(slot, q)
    if q then w(slot, q.x); w(slot + 1, q.y); w(slot + 2, q.z); w(slot + 3, q.w) else w(slot, 0); w(slot + 1, 0); w(slot + 2, 0); w(slot + 3, 1) end
end

local function setup()
    arr = sdk.create_managed_array("System.Single", N)
    if not arr then log.error(TAG .. " create_managed_array failed"); return false end
    for i = 0, N - 1 do w(i, 0.0) end
    w(S_SENTINEL, 12345.0)
    w(S_BRIDGE_VER, BRIDGE_VERSION)
    w4(S_ROTOFF, nil)
    local t = sdk.find_type_definition("app.ropeway.RagdollControlZoneManager")
    keepalive = t and t:get_method("set_AccessMutex")
    if not keepalive then log.error(TAG .. " mailbox method RagdollControlZoneManager.set_AccessMutex not found"); return false end
    log.info(TAG .. " array ready (" .. tostring(arr:get_address()) .. "), bridge v" .. tostring(BRIDGE_VERSION))
    -- v0.8: the head hider's reveal logic reads slots 30/31; say once whether their Lua sources exist at all
    log.info(TAG .. " slot sources: firstpersonmod=" .. tostring(_G.firstpersonmod ~= nil)
        .. " cinematic_gate=" .. tostring(type(_G.__visceral_cinematic_blocking) == "function"))
    return true
end

local handoffs = 0
local function handoff()
    if not keepalive then return end
    if handoffs >= 5 then return end        -- five tries, then give up loudly rather than forever
    handoffs = handoffs + 1
    log.info(TAG .. " hand-over #" .. handoffs .. " at frame " .. frame .. " (mailbox call)")
    local ok, err = pcall(function() keepalive:call(nil, arr) end)
    if not ok then log.warn(TAG .. " hand-over threw: " .. tostring(err)) end
    if handoffs == 5 then log.warn(TAG .. " five hand-overs without acknowledgement — plugin hook missing? check re2_framework_log for 'Failed to hook'") end
end

-- The pose write, shared by both frame points. Returns false when there is nothing to write (no VR).
local function write_poses(point)
    seq = seq + 1
    w(S_WRITE_SEQ, seq)
    w(S_WRITE_PT, point)
    local vr = _G.vrmod
    if not vr then w(S_HMD, 0); w(S_CTL, 0); clear_buttons(); return false end
    local hmd = safe(function() return vr:is_hmd_active() end) and 1 or 0
    local ctl = safe(function() return vr:is_using_controllers() end) and 1 or 0
    w(S_HMD, hmd); w(S_CTL, ctl)
    if hmd == 0 then clear_buttons(); return false end

    local ctrls = safe(function() return vr:get_controllers() end)
    local left, right = nil, nil
    if ctrls then left, right = ctrls[1], ctrls[2] end
    if left then
        local lp = safe(function() return vr:get_position(left) end)
        w3(S_LPOS, lp)
        if point == WRITE_HID then w3(S_LPOS_HID, lp) end
        w4(S_LROT, safe(function() return vr:get_rotation(left) end))
    end
    if right then
        w3(S_RPOS, safe(function() return vr:get_position(right) end))
        w4(S_RROT, safe(function() return vr:get_rotation(right) end))
    end
    w3(S_HPOS, safe(function() return vr:get_position(0) end))
    w4(S_HROT, safe(function() return vr:get_rotation(0) end))

    -- v2: the two numbers every hand-in-world formula needs (ext_2's get_controller_game_world_pos)
    local so = safe(function() return vr:get_standing_origin() end)
    if so and type(so.x) == "number" then w3(S_STAND, so); w(S_STAND_OK, 1) else w(S_STAND_OK, 0) end
    local ro = safe(function() return vr:get_rotation_offset() end)
    w4(S_ROTOFF, ro)
    return true
end

local function write_buttons()
    local vr = _G.vrmod
    if not vr then return end
    local ls = safe(function() return vr:get_left_stick_axis() end)
    if ls then w(S_LSTICK, ls.x); w(S_LSTICK + 1, ls.y) else w(S_LSTICK, 0); w(S_LSTICK + 1, 0) end
    local rs = safe(function() return vr:get_right_stick_axis() end)
    if rs then w(S_RSTICK, rs.x); w(S_RSTICK + 1, rs.y) else w(S_RSTICK, 0); w(S_RSTICK + 1, 0) end

    -- 2026-09-11 (`/pd`, dev PC, NOT RUN): slots 26-29 were declared in the map above on
    -- day one and never written by anything -- proved by diffing declared slot names
    -- against written ones. That is the whole of the ⭐⭐ "the shim never sends the grip"
    -- defect: the dock could not fire by controller because the plugin only ever read
    -- zeros. Not the value-type copy trap that was inferred from public docs this morning.
    --
    -- Shape taken verbatim from REFramework's own re8_vr.lua at the revision installed
    -- here (2f759483), lines 2222-2230: the action handles and the joystick handles are
    -- fetched, then `is_action_active(action, joystick)` is asked per hand.
    local lj = safe(function() return vr:get_left_joystick() end)
    local rj = safe(function() return vr:get_right_joystick() end)
    local a_grip = safe(function() return vr:get_action_grip() end)
    local a_trig = safe(function() return vr:get_action_trigger() end)
    local a_a = safe(function() return vr:get_action_a_button() end)
    local a_b = safe(function() return vr:get_action_b_button() end)
    local a_click = safe(function() return vr:get_action_joystick_click() end)
    local function active(action, joy)
        if not action or not joy then return 0 end
        return safe(function() return vr:is_action_active(action, joy) end) == true and 1 or 0
    end
    w(S_LGRIP, active(a_grip, lj)); w(S_LTRIG, active(a_trig, lj))
    w(S_RGRIP, active(a_grip, rj)); w(S_RTRIG, active(a_trig, rj))
    w(S_LA, active(a_a, lj)); w(S_LB, active(a_b, lj))
    w(S_RA, active(a_a, rj)); w(S_RB, active(a_b, rj))
    w(S_LCLICK, active(a_click, lj)); w(S_RCLICK, active(a_click, rj))
end

-- v2: rumble requests written by the plugin (amp 0..1, seconds); fire and clear.
local function pump_rumble()
    local vr = _G.vrmod
    if not vr then return end
    local function fire(amp_slot, joy_getter)
        local amp = r(amp_slot)
        if amp <= 0 then return end
        local sec = r(amp_slot + 1)
        w(amp_slot, 0); w(amp_slot + 1, 0)
        local joy = safe(joy_getter)
        if not joy or sec <= 0 then return end
        rumbles_fired = rumbles_fired + 1
        pcall(function() vr:trigger_haptic_vibration(0.0, sec, RUMBLE_FREQ_HZ, amp, joy) end)
    end
    fire(S_RUMBLE_L_AMP, function() return vr:get_left_joystick() end)
    fire(S_RUMBLE_R_AMP, function() return vr:get_right_joystick() end)
end

re.on_pre_application_entry("UpdateHID", function()
    if not arr and not setup() then return end
    frame = frame + 1

    if not attached then
        if r(S_ACK) == 1.0 then
            attached = true
            log.info(TAG .. " plugin acknowledged the bridge at frame " .. frame)
        elseif frame - last_handoff > 180 then
            last_handoff = frame
            handoff()
        end
    end

    w(S_FRAME, frame)
    -- v0.8: two facts only Lua can see, for the head hider's reveal logic. Written before the VR early-out
    -- so they are live on a flat launch too.
    local cine = _G.__visceral_cinematic_blocking
    w(S_CINE, (type(cine) == "function" and safe(cine) == true) and 1 or 0)
    local fp = _G.firstpersonmod
    w(S_FP, (fp and safe(function() return fp:will_be_used() end) == true) and 1 or 0)

    pump_rumble()
    if write_poses(WRITE_HID) then write_buttons() end
end)

-- v2: the second write, at the point RELOADED read the hand. Poses only; buttons stay as read at UpdateHID.
re.on_application_entry("LateUpdateBehavior", function()
    if not arr then return end
    write_poses(WRITE_LATE)
end)

re.on_draw_ui(function()
    if imgui.tree_node("Visceral native bridge") then
        imgui.text(string.format("v%.0f  frame %d  attached=%s  late_seen=%s  arr=%s", BRIDGE_VERSION, frame, tostring(attached),
            arr and tostring(r(S_LATE_SEEN) == 1.0) or "nil", arr and tostring(arr:get_address()) or "nil"))
        if arr then
            imgui.text(string.format("Lgrip %.0f  Ltrig %.0f  LA %.0f  LB %.0f  |  Rgrip %.0f  Rtrig %.0f  RA %.0f  RB %.0f  |  rstick %.2f %.2f",
                r(S_LGRIP), r(S_LTRIG), r(S_LA), r(S_LB), r(S_RGRIP), r(S_RTRIG), r(S_RA), r(S_RB), r(S_RSTICK), r(S_RSTICK + 1)))
            imgui.text(string.format("standing origin ok=%.0f  rumbles fired %d  seq %d", r(S_STAND_OK), rumbles_fired, seq))
        end
        if imgui.button("Re-send hand-over") then handoff() end
        imgui.tree_pop()
    end
end)
