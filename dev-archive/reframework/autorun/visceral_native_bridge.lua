-- visceral_native_bridge.lua — the ONLY Lua in Visceral's native path.
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
--     plugin reads them. Slot map must match Plugin.cpp's `enum Slot`.
-- Nothing in here decides anything. Logic lives in the plugin.

local TAG = "[visceral-bridge]"
local N = 64
local BASE = 0x20               -- element 0 of a managed array (REArrayBase)
local S_FRAME, S_HMD, S_CTL = 0, 1, 2
local S_LPOS, S_LROT = 3, 6
local S_RPOS, S_RROT = 10, 13
local S_HPOS, S_HROT = 17, 20
local S_LSTICK = 24
local S_LGRIP, S_LTRIG, S_RGRIP, S_RTRIG = 26, 27, 28, 29
local S_ACK, S_SENTINEL = 62, 63

local arr = nil
local frame = 0
local last_handoff = -1000
local attached = false
local keepalive = nil

local function safe(fn, ...)
    local ok, r = pcall(fn, ...)
    if ok then return r end
    return nil
end

local function w(slot, v)
    arr:write_float(BASE + slot * 4, v or 0.0)
end

local function w3(slot, v)
    if v then w(slot, v.x); w(slot + 1, v.y); w(slot + 2, v.z) else w(slot, 0); w(slot + 1, 0); w(slot + 2, 0) end
end

local function w4(slot, q)
    if q then w(slot, q.x); w(slot + 1, q.y); w(slot + 2, q.z); w(slot + 3, q.w) else w(slot, 0); w(slot + 1, 0); w(slot + 2, 0); w(slot + 3, 1) end
end

local function setup()
    arr = sdk.create_managed_array("System.Single", N)
    if not arr then log.error(TAG .. " create_managed_array failed"); return false end
    for i = 0, N - 1 do w(i, 0.0) end
    w(S_SENTINEL, 12345.0)
    local t = sdk.find_type_definition("app.ropeway.RagdollControlZoneManager")
    keepalive = t and t:get_method("set_AccessMutex")
    if not keepalive then log.error(TAG .. " mailbox method RagdollControlZoneManager.set_AccessMutex not found"); return false end
    log.info(TAG .. " array ready (" .. tostring(arr:get_address()) .. ")")
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

re.on_pre_application_entry("UpdateHID", function()
    if not arr and not setup() then return end
    frame = frame + 1

    if not attached then
        if arr:read_float(BASE + S_ACK * 4) == 1.0 then
            attached = true
            log.info(TAG .. " plugin acknowledged the bridge at frame " .. frame)
        elseif frame - last_handoff > 180 then
            last_handoff = frame
            handoff()
        end
    end

    w(S_FRAME, frame)
    local vr = _G.vrmod
    if not vr then w(S_HMD, 0); w(S_CTL, 0); return end
    local hmd = safe(function() return vr:is_hmd_active() end) and 1 or 0
    local ctl = safe(function() return vr:is_using_controllers() end) and 1 or 0
    w(S_HMD, hmd); w(S_CTL, ctl)
    if hmd == 0 then return end

    local ctrls = safe(function() return vr:get_controllers() end)
    local left, right = nil, nil
    if ctrls then left, right = ctrls[1], ctrls[2] end
    if left then
        w3(S_LPOS, safe(function() return vr:get_position(left) end))
        w4(S_LROT, safe(function() return vr:get_rotation(left) end))
    end
    if right then
        w3(S_RPOS, safe(function() return vr:get_position(right) end))
        w4(S_RROT, safe(function() return vr:get_rotation(right) end))
    end
    w3(S_HPOS, safe(function() return vr:get_position(0) end))
    w4(S_HROT, safe(function() return vr:get_rotation(0) end))
    local ls = safe(function() return vr:get_left_stick_axis() end)
    if ls then w(S_LSTICK, ls.x); w(S_LSTICK + 1, ls.y) else w(S_LSTICK, 0); w(S_LSTICK + 1, 0) end
end)

re.on_draw_ui(function()
    if imgui.tree_node("Visceral native bridge") then
        imgui.text(string.format("frame %d  attached=%s  arr=%s", frame, tostring(attached), arr and tostring(arr:get_address()) or "nil"))
        if imgui.button("Re-send hand-over") then handoff() end
        imgui.tree_pop()
    end
end)
