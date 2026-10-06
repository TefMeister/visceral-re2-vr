-- visceral_bridge.lua -- the only Lua in Visceral's new native code (2026-10-05, new code).
--
-- REFramework's plugin API (1.15, the DLSS build we run) gives C++ no controller BUTTONS and no rumble;
-- those live in Lua's vrmod only. This file decides nothing. It copies the headset pose, the two controller
-- positions and the buttons into one shared System.Single[40] every frame, and fires the rumble the plugin
-- asks for. Poses stay in VR TRACKING space (the player's real room): holsters are bound to the headset,
-- never to the character's body (Tefa 2026-10-05).
-- 2026-10-06: also five VIEW readings for the ladder/cupboard hold (src/ladder.cpp) that only Lua's vrmod and
-- firstpersonmod can give: FirstPerson on/off, raw headset yaw, rotation-offset yaw, camera yaw, rendered yaw.
-- Written at LateUpdateBehavior PRE so the plugin reads them at LateUpdateBehavior POST of the same frame.
--
-- Hand-over: the array is passed once to a "mailbox" method the plugin hooks
-- (app.ropeway.RagdollControlZoneManager.set_AccessMutex, a real compiled game function); the plugin
-- recognises it by its sentinel, keeps it, skips the original, and writes 1 into the ACK slot.
-- The same idea ran live on 2026-10-03 in the old plugin; this is a fresh, smaller copy.
-- Slot map must match src/bridge.h.

local TAG = "[visceral-bridge]"
local N = 40
local S_FRAME, S_HMD = 0, 1
local S_LGRIP, S_LTRIG, S_RGRIP, S_RTRIG = 2, 3, 4, 5
local S_LA, S_LB, S_RA, S_RB = 6, 7, 8, 9
local S_RUMBLE_L_AMP, S_RUMBLE_L_SEC, S_RUMBLE_R_AMP, S_RUMBLE_R_SEC = 10, 11, 12, 13
local S_HMD_POS, S_HMD_ROT, S_LPOS, S_RPOS = 14, 17, 21, 24
local S_ACK, S_SENTINEL = 30, 31
local S_FP_USED, S_HMD_YAW, S_OFFEXT_YAW, S_CAM_YAW, S_RENDER_YAW = 32, 33, 34, 35, 36
local SENTINEL = 54321.0
local NO_VALUE = 999.0
local RUMBLE_FREQ_HZ = 160.0
local HANDOVER_EVERY_FRAMES = 180
local HANDOVER_TRIES = 5

local arr, mailbox = nil, nil
local frame, last_handover, handovers, attached, rumbles = 0, -1000, 0, false, 0

local function safe(fn) local ok, v = pcall(fn); if ok then return v end; return nil end
local function r(slot) return arr:read_float(0x20 + slot * 4) end   -- 0x20 = element 0; the plugin checks it by the sentinel
local function w(slot, v) arr:write_float(0x20 + slot * 4, v or 0.0) end

local function setup()
    arr = sdk.create_managed_array("System.Single", N)
    if not arr then log.error(TAG .. " could not create the array"); return false end
    arr:add_ref()
    for i = 0, N - 1 do w(i, 0.0) end
    for s = S_HMD_YAW, S_RENDER_YAW do w(s, NO_VALUE) end
    w(S_SENTINEL, SENTINEL)
    local t = sdk.find_type_definition("app.ropeway.RagdollControlZoneManager")
    mailbox = t and t:get_method("set_AccessMutex")
    if not mailbox then log.error(TAG .. " mailbox method not found"); return false end
    log.info(TAG .. " array ready")
    return true
end

local function handover()
    if handovers >= HANDOVER_TRIES then return end
    handovers = handovers + 1
    log.info(TAG .. " hand-over #" .. handovers .. " at frame " .. frame)
    pcall(function() mailbox:call(nil, arr) end)
    if handovers == HANDOVER_TRIES then log.warn(TAG .. " no answer from visceral_native.dll after " .. HANDOVER_TRIES .. " tries") end
end

local function w3(slot, v)
    if v and type(v.x) == "number" then w(slot, v.x); w(slot + 1, v.y); w(slot + 2, v.z) else w(slot, 0); w(slot + 1, 0); w(slot + 2, 0) end
end

local function poses(vr)
    w3(S_HMD_POS, safe(function() return vr:get_position(0) end))
    local q = safe(function() return vr:get_rotation(0) end)
    if q and type(q.w) == "number" then w(S_HMD_ROT, q.x); w(S_HMD_ROT + 1, q.y); w(S_HMD_ROT + 2, q.z); w(S_HMD_ROT + 3, q.w)
    else w(S_HMD_ROT, 0); w(S_HMD_ROT + 1, 0); w(S_HMD_ROT + 2, 0); w(S_HMD_ROT + 3, 1) end
    local c = safe(function() return vr:get_controllers() end)
    if c and c[1] then w3(S_LPOS, safe(function() return vr:get_position(c[1]) end)) end
    if c and c[2] then w3(S_RPOS, safe(function() return vr:get_position(c[2]) end)) end
end

local function buttons()
    local vr = _G.vrmod
    local live = vr and safe(function() return vr:is_hmd_active() end) == true
    w(S_HMD, live and 1 or 0)
    if not live then
        for s = S_LGRIP, S_RB do w(s, 0) end
        return
    end
    poses(vr)
    local lj = safe(function() return vr:get_left_joystick() end)
    local rj = safe(function() return vr:get_right_joystick() end)
    local grip = safe(function() return vr:get_action_grip() end)
    local trig = safe(function() return vr:get_action_trigger() end)
    local a = safe(function() return vr:get_action_a_button() end)
    local b = safe(function() return vr:get_action_b_button() end)
    local function on(action, joy)
        if not action or not joy then return 0 end
        return safe(function() return vr:is_action_active(action, joy) end) == true and 1 or 0
    end
    w(S_LGRIP, on(grip, lj)); w(S_LTRIG, on(trig, lj)); w(S_RGRIP, on(grip, rj)); w(S_RTRIG, on(trig, rj))
    w(S_LA, on(a, lj)); w(S_LB, on(b, lj)); w(S_RA, on(a, rj)); w(S_RB, on(b, rj))
end

local function rumble()
    local vr = _G.vrmod
    if not vr then return end
    local function fire(slot, getter)
        local amp, sec = r(slot), r(slot + 1)
        if amp <= 0 then return end
        w(slot, 0); w(slot + 1, 0)
        local joy = safe(getter)
        if not joy or sec <= 0 then return end
        rumbles = rumbles + 1
        pcall(function() vr:trigger_haptic_vibration(0.0, sec, RUMBLE_FREQ_HZ, amp, joy) end)
    end
    fire(S_RUMBLE_L_AMP, function() return vr:get_left_joystick() end)
    fire(S_RUMBLE_R_AMP, function() return vr:get_right_joystick() end)
end

re.on_pre_application_entry("UpdateHID", function()
    if not arr and not setup() then return end
    frame = frame + 1
    w(S_FRAME, frame)
    if not attached then
        if r(S_ACK) == 1.0 then
            attached = true
            log.info(TAG .. " plugin answered at frame " .. frame)
        elseif frame - last_handover > HANDOVER_EVERY_FRAMES then
            last_handover = frame
            handover()
        end
    end
    rumble()
    buttons()
end)

-- the view readings (2026-10-06). Same reads Arcade Controls' ladder hold made; each one NO_VALUE on failure.
local function fwd_yaw(q, z)
    local f = q * Vector3f.new(0, 0, z)
    return math.atan(f.x, f.z)
end

re.on_pre_application_entry("LateUpdateBehavior", function()
    if not arr then return end
    local vr = _G.vrmod
    local fp = _G.firstpersonmod
    w(S_FP_USED, (fp and safe(function() return fp:will_be_used() end) == true) and 1 or 0)
    local rawq = vr and safe(function() return vr:get_transform(0):to_quat() end)
    w(S_HMD_YAW, rawq and safe(function() return fwd_yaw(rawq, -1) end) or NO_VALUE)
    w(S_OFFEXT_YAW, rawq and safe(function() return fwd_yaw(vr:get_rotation_offset() * rawq, -1) end) or NO_VALUE)
    w(S_CAM_YAW, safe(function() return fwd_yaw(sdk.get_primary_camera():call("get_WorldMatrix"):to_quat(), -1) end) or NO_VALUE)
    w(S_RENDER_YAW, vr and safe(function() return fwd_yaw(vr:get_last_render_matrix():to_quat(), -1) end) or NO_VALUE)
end)

re.on_draw_ui(function()
    if imgui.tree_node("Visceral bridge (new)") then
        imgui.text(string.format("frame %d  attached=%s  rumbles fired %d", frame, tostring(attached), rumbles))
        if arr then
            imgui.text(string.format("L grip %.0f trig %.0f  |  R grip %.0f trig %.0f", r(S_LGRIP), r(S_LTRIG), r(S_RGRIP), r(S_RTRIG)))
        end
        imgui.tree_pop()
    end
end)
