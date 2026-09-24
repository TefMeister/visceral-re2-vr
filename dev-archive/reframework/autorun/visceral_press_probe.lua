-- Visceral (RE2 VR) -- aim-press burst probe (read-only, 2026-09-24)
--
-- Why: after the anchor, direct-body, LookAt and animation fixes, Tefa still sees the body nudge forward
-- and the camera step a little left at the RG press. Instead of guessing again, this records, frame by
-- frame for 45 frames around every aim change (15 before, 30 after): the player root position, the world
-- position of the pelvis, spine_0 and head joints, and the game camera position. Whatever moves at the
-- press is the thing to fix. Log tag: [visceral_press]. Writes nothing.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_press]"
local NS = sdk.game_namespace
local JOINTS = { "cog", "hips", "pelvis", "spine_0", "spine_2", "head" }   -- missing names are skipped
local BEFORE, AFTER = 15, 30

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
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
local function camera_pos()
    local sm = sdk.get_native_singleton("via.SceneManager"); if not sm then return nil end
    local td = sdk.find_type_definition("via.SceneManager"); if not td then return nil end
    local view = safe(function() return sdk.call_native_func(sm, td, "get_MainView") end); if not view then return nil end
    local cam = safe(function() return view:call("get_PrimaryCamera") end); if not cam then return nil end
    local go = safe(function() return cam:call("get_GameObject") end); if not go then return nil end
    local tf = safe(function() return go:call("get_Transform") end); if not tf then return nil end
    return safe(function() return tf:call("get_Position") end)
end

local ring, ring_n, RING = {}, 0, BEFORE + 1
local after_left, prev_hold, frame_no = 0, nil, 0
local joint_cache = {}

local function sample(player)
    local tf = safe(function() return player:call("get_Transform") end); if not tf then return nil end
    local s = { f = frame_no }
    local rp = safe(function() return tf:call("get_Position") end)
    if rp then s.root = { rp.x, rp.y, rp.z } end
    for _, n in ipairs(JOINTS) do
        local j = joint_cache[n]
        if j == nil then j = safe(function() return tf:call("getJointByName", n) end) or false; joint_cache[n] = j end
        if j then
            local p = safe(function() return j:call("get_Position") end)
            if p then s[n] = { p.x, p.y, p.z } end
        end
    end
    local cp = camera_pos()
    if cp then s.cam = { cp.x, cp.y, cp.z } end
    return s
end

local function fmt(s, key)
    local v = s[key]
    if not v then return key .. "=?" end
    return string.format("%s=(%.3f %.3f %.3f)", key, v[1], v[2], v[3])
end

local function emit(s, tag)
    local parts = { string.format("%s f=%d hold=%d", tag, s.f, s.hold and 1 or 0) }
    parts[#parts + 1] = fmt(s, "root")
    for _, n in ipairs(JOINTS) do if s[n] then parts[#parts + 1] = fmt(s, n) end end
    parts[#parts + 1] = fmt(s, "cam")
    log.info(TAG .. " " .. table.concat(parts, " "))
end

re.on_pre_application_entry("PrepareRendering", function()
    frame_no = frame_no + 1
    local p = get_player(); if not p then return end
    local hold = is_aiming(p)
    local s = sample(p); if not s then return end
    s.hold = hold
    if prev_hold ~= nil and hold ~= prev_hold then
        log.info(string.format("%s ==== aim %s at f=%d: the %d frames before ====", TAG, hold and "ON" or "OFF", frame_no, ring_n))
        for i = 1, ring_n do emit(ring[i], "pre ") end
        after_left = AFTER
    end
    prev_hold = hold
    if after_left > 0 then
        emit(s, "post")
        after_left = after_left - 1
        if after_left == 0 then log.info(TAG .. " ==== end of window ====") end
    end
    -- keep the last BEFORE samples
    table.insert(ring, s)
    if #ring > BEFORE then table.remove(ring, 1) end
    ring_n = #ring
end)
