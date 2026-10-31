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
local N = 604   -- 2026-10-31: + the eighth hold at 540 (src/bridge.h S_HT2, the hand on the magazine); b173: 84 + the hand tuning slots (src/bridge.h S_WHO .. S_HT + 7 x 64) + S_FREEZE; b188: + S_LADDER_HOLD_OFF; 2026-10-28: + S_AUTO_EXPOSURE_ON
local S_FRAME, S_HMD = 0, 1
local S_LGRIP, S_LTRIG, S_RGRIP, S_RTRIG = 2, 3, 4, 5
local S_LA, S_LB, S_RA, S_RB = 6, 7, 8, 9
local S_RUMBLE_L_AMP, S_RUMBLE_L_SEC, S_RUMBLE_R_AMP, S_RUMBLE_R_SEC = 10, 11, 12, 13
local S_HMD_POS, S_HMD_ROT, S_LPOS, S_RPOS = 14, 17, 21, 24
local S_ACK, S_SENTINEL = 30, 31
local S_FP_USED, S_HMD_YAW, S_OFFEXT_YAW, S_CAM_YAW, S_RENDER_YAW = 32, 33, 34, 35, 36
local S_LCLICK, S_LSTICK_MAG = 37, 38   -- running stop (src/run.cpp)
local S_HMD_Q, S_ROT_OFF, S_ORIGIN = 40, 44, 48   -- menu camera (src/menu_body.cpp, 2026-10-08)
local S_RROT = 52   -- the right controller's turn, room space (src/rack.cpp motion racking, b143)
-- 56..83: b156-b159 slider slots, unused since b164 (visceral_tuning.lua writes 84 onward)
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
    w(S_HMD_Q, NO_VALUE); w(S_ROT_OFF, NO_VALUE); w(S_ORIGIN, NO_VALUE)
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
    -- b111: get_rotation returns a Matrix4x4f in Lua (no .w), so this slot was the identity from 2026-10-05 to b110 and
    -- the holster spots never turned with the head; to_quat() is the path S_HMD_Q has used since b100
    local q = safe(function() return vr:get_rotation(0):to_quat() end)
    if q and type(q.w) == "number" then w(S_HMD_ROT, q.x); w(S_HMD_ROT + 1, q.y); w(S_HMD_ROT + 2, q.z); w(S_HMD_ROT + 3, q.w)
    else w(S_HMD_ROT, 0); w(S_HMD_ROT + 1, 0); w(S_HMD_ROT + 2, 0); w(S_HMD_ROT + 3, 1) end
    local c = safe(function() return vr:get_controllers() end)
    if c and c[1] then w3(S_LPOS, safe(function() return vr:get_position(c[1]) end)) end
    if c and c[2] then w3(S_RPOS, safe(function() return vr:get_position(c[2]) end)) end
    local rq = c and c[2] and safe(function() return vr:get_rotation(c[2]):to_quat() end)
    if rq and type(rq.w) == "number" then w(S_RROT, rq.x); w(S_RROT + 1, rq.y); w(S_RROT + 2, rq.z); w(S_RROT + 3, rq.w)
    else w(S_RROT, NO_VALUE); w(S_RROT + 1, 0); w(S_RROT + 2, 0); w(S_RROT + 3, 1) end
end

-- BeG0nE Camera Jitter ride-along (board row 6g, wired 2026-10-14): the headset pose as read, the camera as rendered and
-- each picture presented go to %LOCALAPPDATA%\BeG0nE\jitter\visceral-re2-vr_<time>.jl while the headset is live, so
-- `begone.py new` can say whether the stepped feel on slow turns is a rate fault (pose read less often than rendered)
-- or a pose fault. jitterlog.lua sits beside this file; without it nothing is recorded and nothing else changes.
local jl_ok, jl = pcall(require, "jitterlog")
if jl_ok and jl then jl.init("visceral-re2-vr", "openxr") else jl = nil; log.info(TAG .. " jitterlog.lua not found: no jitter recording") end
local jl_live = false
-- yaw / pitch / roll in degrees from a quaternion, the bridge's own convention: forward = q * (0,0,-1), up = q * (0,1,0)
local function ypr(q)
    local f = q * Vector3f.new(0, 0, -1)
    local u = q * Vector3f.new(0, 1, 0)
    local yaw = math.deg(math.atan(f.x, -f.z))
    local pitch = math.deg(math.asin(math.max(-1, math.min(1, f.y))))
    local r = q * Vector3f.new(1, 0, 0)
    local roll = math.deg(math.atan(-r.y, u.y))
    return yaw, pitch, roll
end
local function jl_pose(vr)
    if not jl then return end
    local q = safe(function() return vr:get_transform(0):to_quat() end)
    local p = safe(function() return vr:get_position(0) end)
    if q and type(q.w) == "number" then
        local yaw, pitch, roll = ypr(q)
        jl.pose(yaw, pitch, roll, p and p.x or 0, p and p.y or 0, p and p.z or 0)
    end
end
local function jl_camera()
    if not jl or not jl.is_on() then return end
    local m = safe(function() return sdk.get_primary_camera():call("get_WorldMatrix") end)
    local q = m and safe(function() return m:to_quat() end)
    if q and type(q.w) == "number" then
        local yaw, pitch, roll = ypr(q)
        local p = safe(function() return m[3] end)
        jl.camera(yaw, pitch, roll, p and p.x or 0, p and p.y or 0, p and p.z or 0)
    end
    jl.present()
end

local function buttons()
    local vr = _G.vrmod
    local live = vr and safe(function() return vr:is_hmd_active() end) == true
    w(S_HMD, live and 1 or 0)
    if jl and live ~= jl_live then
        jl_live = live
        if live then jl.vr_on() else jl.vr_off() end
        log.info(TAG .. " jitter recording " .. (live and "ON" or "OFF"))
    end
    if live then jl_pose(vr) end
    if not live then
        for s = S_LGRIP, S_RB do w(s, 0) end
        w(S_LCLICK, 0); w(S_LSTICK_MAG, 0)
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
    w(S_LCLICK, on(safe(function() return vr:get_action_joystick_click() end), lj))
    local axis = safe(function() return vr:get_left_stick_axis() end)
    w(S_LSTICK_MAG, axis and safe(function() return axis:length() end) or 0)
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
    -- the menu camera's three readings (2026-10-08): the raw headset turn, the VR layer's rotation offset and its
    -- standing origin. The plugin strips them off the held camera while FirstPerson is not driving, because the VR
    -- layer multiplies them back on at BeginRendering (REFramework VR.cpp update_camera_origin / apply_hmd_transform).
    local function wq(slot, q)
        if q and type(q.w) == "number" then w(slot, q.x); w(slot + 1, q.y); w(slot + 2, q.z); w(slot + 3, q.w)
        else w(slot, NO_VALUE); w(slot + 1, 0); w(slot + 2, 0); w(slot + 3, 1) end
    end
    wq(S_HMD_Q, rawq)
    wq(S_ROT_OFF, vr and safe(function() return vr:get_rotation_offset() end))
    local origin = vr and safe(function() return vr:get_standing_origin() end)
    if origin and type(origin.x) == "number" then w3(S_ORIGIN, origin) else w(S_ORIGIN, NO_VALUE); w(S_ORIGIN + 1, 0); w(S_ORIGIN + 2, 0) end
end)

-- the camera as the renderer gets it, after the VR layer's own BeginRendering pass, and one "picture presented" mark
re.on_application_entry("BeginRendering", function() jl_camera() end)   -- REFramework's "post" hook is on_application_entry (2026-10-14: on_post_... is nil and killed the whole script)

-- b156-b159's three slider menus (shell in the hand, hand on the slide, fingers on the slide) became
-- visceral_tuning.lua on 2026-10-13 (b164): per character, per gun, per hold; Tefa's numbers are built into the plugin.
_G.visceral_bridge = {
    ready = function() return arr ~= nil end,
    r = function(slot) return r(slot) end,
    w = function(slot, v) w(slot, v) end,
}

re.on_draw_ui(function()
    if imgui.tree_node("Visceral bridge (new)") then
        imgui.text(string.format("frame %d  attached=%s  rumbles fired %d", frame, tostring(attached), rumbles))
        if arr then
            imgui.text(string.format("L grip %.0f trig %.0f  |  R grip %.0f trig %.0f", r(S_LGRIP), r(S_LTRIG), r(S_RGRIP), r(S_RTRIG)))
        end
        imgui.tree_pop()
    end
end)
