-- Visceral (RE2 VR) -- PROBE + LEVERS: why a two-handed shot throws the gun aside while walking on the
-- relaxed-walk splice (b012-b017, 2026-09-27). Built 2026-09-30 (/pd, static, unrun).
--
-- What the files say [measured 2026-09-30]: every stock motion in the hold bank (aim walks 0120-0136, the
-- Interpolation loops, the raise, the aim idle, reload, holster) carries a clip track
-- app.ropeway.survivor.tracks.SurvivorIkLeftArmTrack (IKBlendRatio 1.0), and NONE of the relaxed
-- OFF_GazingWalk loops (0190-0198) do -- they carry only via.motion.MotionSyncPoint. The 2026-09-27 note's
-- "the left-arm IK track is the same in vanilla and walk motions" was wrong. So while walking aimed on the
-- splice the left-hand hold is OFF (SurvivorIKLeftArmController takes 0.0 when the motion has no track,
-- dossier 8g.2), and the test copy never had visceral_lefthand_hold.lua in it. [hypothesis] the shot kick
-- with the support hand off the gun is what throws it. The arm graft (b016/b017) moved bone tracks only,
-- never the clip track, which is why it did not help.
--
-- The Village link (Tefa's idea, 2026-09-30): the same REFramework code (RE8VR.cpp update_hand_ik, used for
-- RE2 too, called from re8_vr.lua) steers a two-handed gun by the ANIMATED left-hand socket relative to the
-- right hand, read every frame; when the animation moves that socket under a held grip the gun re-aims with
-- both real hands still (village-scope dossier 9cf/9cg, worn fix 2026-09-21). With the support hand no
-- longer pinned by IK, the shot kick moves the socket, so the gun is thrown, and only while LG is held.
-- Each shot line therefore also says whether the left grip was held.
--
-- One launch, three answers (Tefa walks + shoots two-handed after each step):
--   step 1: this file + visceral_lefthand_hold.lua (ON) + the b015 walk lists      -> swing gone?  then the missing track is it
--   step 2: NUM7 (left-hand hold OFF, in visceral_lefthand_hold.lua)                -> swing back?  same answer, from the other side
--   step 3: NUM6 (ARMFIT IK OFF, held off every frame here; NUM6 again = back on)   -> if step 1 still swung: is it the wall-fit arm IK?
--
-- Measured, not judged: every shot (motion 1100-1102 seen on any layer) is followed for 120 frames; the gun
-- joint's position relative to the head is compared with the 5 frames before the shot and logged as peak /
-- at 60 / at 120 frames, in cm and in the camera's right/up/forward axes. Vanilla should settle back near 0;
-- a throw shows as a large settled value. Once a second: hold, layer-0 motion, IK bits and blend rates.
--
-- Probe: archive it once it has answered (standing rule); a data fix (graft the track into the OFF walks)
-- or the left-hand script becomes the permanent lever.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_swing]"
local NS = sdk.game_namespace
local VK_NUMPAD6 = 0x66
local SHOT_IDS = { [1100] = true, [1101] = true, [1102] = true }
local FOLLOW_FRAMES = 120
local BASELINE_FRAMES = 5
local IK_NAMES = { "LEG", "SPINE", "LOOKAT", "ARM", "ARMFIT", "HAND" }
local ARMFIT = 4

local cfg = { armfit_off = false }
local st = { key6 = false, last_log = 0, status = "idle", shots = 0, follow = nil, ring = {}, joints = {}, armfit_reads = "" }

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
local function is_aiming(player)
    local c = component(player, NS("survivor.SurvivorCondition")); if not c then return false end
    return safe(function() return c:call("get_IsHold") end) == true
end
local function joint(tf, name)
    local j = st.joints[name]
    if j == nil then j = safe(function() return tf:call("getJointByName", name) end) or false; st.joints[name] = j end
    return j or nil
end
local function pos(j) return j and safe(function() return j:call("get_Position") end) or nil end

-- gun joint relative to the head, in the camera's axes (right / up / forward), metres
local function gun_offset(player)
    local tf = safe(function() return player:call("get_Transform") end); if not tf then return nil end
    local g = pos(joint(tf, "r_weapon")) or pos(joint(tf, "r_arm_wrist"))
    local h = pos(joint(tf, "head"))
    if not g or not h then return nil end
    local d = { g.x - h.x, g.y - h.y, g.z - h.z }
    local cam = sdk.get_primary_camera()
    local ctf = cam and safe(function() return cam:call("get_GameObject") end)
    ctf = ctf and safe(function() return ctf:call("get_Transform") end)
    local ax = ctf and safe(function() return ctf:call("get_AxisX") end)
    local ay = ctf and safe(function() return ctf:call("get_AxisY") end)
    local az = ctf and safe(function() return ctf:call("get_AxisZ") end)
    if ax and ay and az then
        return { d[1] * ax.x + d[2] * ax.y + d[3] * ax.z,
                 d[1] * ay.x + d[2] * ay.y + d[3] * ay.z,
                 d[1] * az.x + d[2] * az.y + d[3] * az.z }
    end
    return d  -- world axes if the camera could not be read (the log says so once)
end

local function shot_playing(mo)
    local n = safe(function() return mo:call("get_LayerCount") end) or 0
    for i = 0, n - 1 do
        local layer = safe(function() return mo:call("getLayer", i) end)
        local id = layer and safe(function() return layer:call("get_MotionID") end)
        if id and SHOT_IDS[id] then return i, id end
    end
    return nil
end

local function fmt3(v) return string.format("(%+.1f %+.1f %+.1f)", v[1] * 100, v[2] * 100, v[3] * 100) end
local function sub(a, b) return { a[1] - b[1], a[2] - b[2], a[3] - b[3] } end
local function mag(v) return math.sqrt(v[1] * v[1] + v[2] * v[2] + v[3] * v[3]) end

-- b038 (22:45, first run): no SHOT line in 2.5 minutes although Tefa fired -- motion 1100-1102 was never seen on a
-- layer at on_frame time. Second marker: the game's own fire request (app.ropeway.survivor.Equipment.requestFire,
-- the managed method every shot goes through, dossier 2026-08-29). It also logs every layer's motion at the shot
-- and the left wrist's distance from the gun (does the support hand leave the gun through the kick?).
local fire_pending = false
do
    local eq_t = sdk.find_type_definition(NS("survivor.Equipment"))
    local m = eq_t and eq_t:get_method("requestFire")
    if m then
        sdk.hook(m, function(args) fire_pending = true end, function(rv) return rv end)
        log_line("hooked Equipment.requestFire as the shot marker")
    else
        log_line("Equipment.requestFire NOT FOUND -- shots are only caught by motion id")
    end
end
local function layers_text(mo)
    local n = safe(function() return mo:call("get_LayerCount") end) or 0
    local parts = {}
    for i = 0, n - 1 do
        local layer = safe(function() return mo:call("getLayer", i) end)
        local id = layer and safe(function() return layer:call("get_MotionID") end) or -1
        local bank = layer and safe(function() return layer:call("get_MotionBankID") end) or -1
        if id and id >= 0 then parts[#parts + 1] = string.format("L%d=%d/%d", i, bank, id) end
    end
    return table.concat(parts, " ")
end
local function wrist_to_gun(player)
    local tf = safe(function() return player:call("get_Transform") end); if not tf then return nil end
    local g = pos(joint(tf, "r_weapon")) or pos(joint(tf, "r_arm_wrist"))
    local w = pos(joint(tf, "l_arm_wrist"))
    if not g or not w then return nil end
    return mag({ g.x - w.x, g.y - w.y, g.z - w.z })
end

-- the ARMFIT lever: applied before the IK pass, read back after it (proof of effect)
re.on_pre_application_entry("LateUpdateBehavior", function()
    if not cfg.armfit_off then return end
    local p = get_player(); if not p then return end
    local ikc = component(p, NS("IkController")); if not ikc then return end
    safe(function() ikc:call("setEnable", ARMFIT, false, 0.0) end)
end)

re.on_frame(function()
    local p = get_player()
    if not p then st.follow = nil; return end

    -- NUM6: ARMFIT off / on
    local k6 = reframework:is_key_down(VK_NUMPAD6)
    if k6 and not st.key6 then
        cfg.armfit_off = not cfg.armfit_off
        if not cfg.armfit_off then
            local ikc = component(p, NS("IkController"))
            if ikc then safe(function() ikc:call("setEnable", ARMFIT, true, 0.0) end) end
        end
        log_line("NUM6: ARMFIT " .. (cfg.armfit_off and "OFF (held off every frame)" or "back ON"))
    end
    st.key6 = k6

    -- the measurement
    local off = gun_offset(p)
    if off then
        local r = st.ring
        r[#r + 1] = off
        if #r > BASELINE_FRAMES then table.remove(r, 1) end
    end
    local mo = component(p, "via.motion.Motion")
    local layer_i, shot_id = nil, nil
    if mo then layer_i, shot_id = shot_playing(mo) end
    if fire_pending and not shot_id then shot_id, layer_i = 0, -1 end   -- the fire request marks it even when no layer shows 1100
    if shot_id and not st.follow and off and #st.ring >= 2 then
        fire_pending = false
        local base = { 0, 0, 0 }
        for _, v in ipairs(st.ring) do base[1] = base[1] + v[1]; base[2] = base[2] + v[2]; base[3] = base[3] + v[3] end
        for i = 1, 3 do base[i] = base[i] / #st.ring end
        st.shots = st.shots + 1
        st.follow = { n = st.shots, id = shot_id, layer = layer_i, base = base, frame = 0, peak = 0, peak_v = { 0, 0, 0 }, peak_f = 0, at60 = nil }
        local lg = safe(function() return re8vr and re8vr.is_holding_left_grip end)
        local wg = safe(function() return re8vr and re8vr.was_gripping_weapon end)
        local wd = wrist_to_gun(p)
        st.follow.wrist0 = wd
        log_line(string.format("SHOT #%d motion %d on layer %d: gun at %s cm from head (right/up/fwd), hold=%d armfit_off=%d left_grip=%s two_handed=%s left_wrist_to_gun=%s cm layers: %s",
            st.shots, shot_id, layer_i, fmt3(base), is_aiming(p) and 1 or 0, cfg.armfit_off and 1 or 0, tostring(lg), tostring(wg),
            wd and string.format("%.1f", wd * 100) or "?", mo and layers_text(mo) or "?"))
    elseif st.follow and off then
        fire_pending = false
        local f = st.follow
        f.frame = f.frame + 1
        local d = sub(off, f.base)
        local m = mag(d)
        if m > f.peak then f.peak, f.peak_v, f.peak_f = m, d, f.frame end
        if f.frame == 60 then f.at60 = d end
        if f.frame == f.peak_f then f.wrist_peak = wrist_to_gun(p) end
        if f.frame >= FOLLOW_FRAMES then
            local wd = wrist_to_gun(p)
            log_line(string.format("SHOT #%d done: peak %.1f cm %s at frame %d; at 60 frames %s; at 120 frames %s cm; left wrist to gun %s -> %s (peak) -> %s cm  (a throw = large numbers that do not come back)",
                f.n, f.peak * 100, fmt3(f.peak_v), f.peak_f, f.at60 and fmt3(f.at60) or "?", fmt3(d),
                f.wrist0 and string.format("%.1f", f.wrist0 * 100) or "?", f.wrist_peak and string.format("%.1f", f.wrist_peak * 100) or "?", wd and string.format("%.1f", wd * 100) or "?"))
            st.follow = nil
        end
    end

    -- once a second: the state
    local now = os.clock()
    if now - st.last_log < 1.0 then return end
    st.last_log = now
    local ikc = component(p, NS("IkController"))
    local bits = ikc and safe(function() return ikc:get_field("EnableIkBits") end)
    local parts = {}
    if ikc then
        for k = 0, 5 do
            local en = safe(function() return ikc:call("isEnabled", k) end)
            local r = safe(function() return ikc:call("getBlendRate", k) end)
            parts[#parts + 1] = string.format("%s=%s/%.2f", IK_NAMES[k + 1], en and "on" or "off", type(r) == "number" and r or -1)
        end
    end
    local l0 = mo and safe(function() return mo:call("getLayer", 0) end)
    local node = l0 and safe(function() return l0:call("get_HighestWeightMotionNode") end)
    local mname = node and safe(function() return node:call("get_MotionName") end) or "-"
    local lctrl = component(p, NS("survivor.SurvivorIKLeftArmController"))
    local len = lctrl and safe(function() return lctrl:get_field("IKEnable") end)
    st.status = string.format("hold=%d layer0=%s bits=%s %s leftIK=%s shots=%d armfit_off=%d",
        is_aiming(p) and 1 or 0, tostring(mname), tostring(bits), table.concat(parts, " "), tostring(len), st.shots, cfg.armfit_off and 1 or 0)
    log_line(st.status)
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral swing probe") then return end
    local ch
    ch, cfg.armfit_off = imgui.checkbox("ARMFIT IK off (NUM6)", cfg.armfit_off)
    imgui.text(st.status)
    imgui.tree_pop()
end)

log_line("loaded: NUM6 = ARMFIT off/on; every shot is measured for 120 frames")
