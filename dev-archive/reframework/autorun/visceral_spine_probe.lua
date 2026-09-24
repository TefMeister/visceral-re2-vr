-- Visceral (RE2 VR) -- spine probe (read-only, 2026-09-24)
--
-- Measures how much the game's post-animation passes (LookAt / IK) bend the spine, so the
-- LookAt-profile change (lookat_patch.py) can prove its own effect: read spine_0/1/2 local
-- rotation BEFORE the IK phase (LateUpdateBehavior) and again at the last point before render
-- (PrepareRendering), and log the difference once a second, with the aim state.
--   stock profiles : while aiming, the AFTER pose should differ from BEFORE by tens of degrees on spine_2
--   patched profiles: the difference should stay within the Default profile's ~10 degrees
-- Writes nothing to the game. Log tag: [visceral_spineprobe].

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_spineprobe]"
local NS = sdk.game_namespace
local JOINTS = { "spine_0", "spine_1", "spine_2", "neck_0", "head" }
local PERIOD_S = 1.0

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end

local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end
local function get_transform(player) return safe(function() return player:call("get_Transform") end) end
local function get_joint(tf, name) return safe(function() return tf:call("getJointByName", name) end) end
local function is_aiming(player)
    local t = sdk.typeof(NS("survivor.SurvivorCondition")); if not t then return false end
    local c = safe(function() return player:call("getComponent(System.Type)", t) end); if not c then return false end
    return safe(function() return c:call("get_IsHold") end) == true
end

-- quaternion (x,y,z,w) -> pitch/yaw/roll in degrees (X, Y, Z axes; enough to compare before/after)
local function euler_deg(q)
    local x, y, z, w = q.x, q.y, q.z, q.w
    local sinp = 2 * (w * x - y * z)
    if sinp > 1 then sinp = 1 elseif sinp < -1 then sinp = -1 end
    local pitch = math.deg(math.asin(sinp))
    local yaw = math.deg(math.atan(2 * (w * y + x * z), 1 - 2 * (x * x + y * y)))
    local roll = math.deg(math.atan(2 * (w * z + x * y), 1 - 2 * (x * x + z * z)))
    return pitch, yaw, roll
end
-- angle between two rotations, degrees
local function qangle(a, b)
    local d = math.abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w)
    if d > 1 then d = 1 end
    return math.deg(2 * math.acos(d))
end

local pre, post = {}, {}
local last_log = 0

local function snapshot(into)
    local p = get_player(); if not p then return end
    local tf = get_transform(p); if not tf then return end
    for _, n in ipairs(JOINTS) do
        local j = get_joint(tf, n)
        if j then
            local r = safe(function() return j:call("get_LocalRotation") end)
            if r and type(r.x) == "number" then into[n] = { x = r.x, y = r.y, z = r.z, w = r.w } end
        end
    end
end

re.on_pre_application_entry("LateUpdateBehavior", function() snapshot(pre) end)

re.on_pre_application_entry("PrepareRendering", function()
    snapshot(post)
    local now = os.clock()
    if now - last_log < PERIOD_S then return end
    last_log = now
    local p = get_player(); if not p then return end
    local hold = is_aiming(p) and 1 or 0
    local parts = {}
    for _, n in ipairs(JOINTS) do
        local a, b = pre[n], post[n]
        if a and b then
            local pp, py, pr = euler_deg(a)
            local qp, qy, qr = euler_deg(b)
            parts[#parts + 1] = string.format("%s pre(p%.0f y%.0f r%.0f) post(p%.0f y%.0f r%.0f) d=%.1f",
                n, pp, py, pr, qp, qy, qr, qangle(a, b))
        end
    end
    log.info(string.format("%s hold=%d | %s", TAG, hold, table.concat(parts, " | ")))
end)
