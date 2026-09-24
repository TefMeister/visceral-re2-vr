-- Visceral (RE2 VR) -- aim-state diff probe (read-only, 2026-09-24)
--
-- Why: with the ordinary walk spliced into the hold bank, the layer probe shows the SAME clips, weights
-- and ground speed aiming and not, yet Tefa sees straighter, careful steps while aiming. So the
-- difference is a setting the game flips in the hold state (leg IK options, motion options, condition
-- flags), not a clip. This reads every parameterless getter (get_* / is*) that returns a number or bool
-- on a list of player components, snapshots them 0.5 s after each aim change, and logs only the values
-- that differ from the previous snapshot of the other state. Writes nothing. Log tag: [visceral_diff].

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_diff]"
local NS = sdk.game_namespace
local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end

-- components to diff (full type names; missing ones are skipped)
local TYPES = {
    "via.motion.IkLeg", "via.motion.IkLeg2", "via.motion.IkLegSpine", "via.motion.Motion",
    NS("IkController"), NS("IkAttitude"), NS("survivor.SurvivorCondition"),
    NS("survivor.SurvivorIKLeftArmController"), "via.motion.IkLookAt",
    "via.physics.CharacterController", NS("survivor.SurvivorCharacterController"),
}
local PRIM = { ["System.Boolean"] = true, ["System.Single"] = true, ["System.Int32"] = true,
               ["System.UInt32"] = true, ["System.Int16"] = true, ["System.UInt16"] = true,
               ["System.Byte"] = true, ["System.Double"] = true }

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

-- getters per type, discovered once: name -> true (0 params, primitive or enum return)
local getters = {}
local function discover(full)
    if getters[full] ~= nil then return getters[full] end
    local list = {}
    local td = sdk.find_type_definition(full)
    while td do
        for _, m in ipairs(td:get_methods() or {}) do
            local n = safe(function() return m:get_name() end) or ""
            if (n:sub(1, 4) == "get_" or n:sub(1, 2) == "is") and safe(function() return m:get_num_params() end) == 0 then
                local rt = safe(function() return m:get_return_type() end)
                local rn = rt and safe(function() return rt:get_full_name() end) or ""
                local is_enum = rt and safe(function() return rt:is_a("System.Enum") end) == true
                if PRIM[rn] or is_enum then list[n] = true end
            end
        end
        td = safe(function() return td:get_parent_type() end)
    end
    getters[full] = list
    return list
end

local function snapshot(player)
    local snap = {}
    for _, full in ipairs(TYPES) do
        local c = component(player, full)
        if c then
            local vals = {}
            for n, _ in pairs(discover(full)) do
                local ok, v = pcall(function() return c:call(n) end)
                if ok and (type(v) == "number" or type(v) == "boolean") then vals[n] = v end
            end
            snap[full] = vals
        end
    end
    return snap
end

local last = { hold = nil, snap = nil, pending_t = nil, pending_hold = nil, first = true }

re.on_frame(function()
    local p = get_player(); if not p then return end
    local hold = is_aiming(p)
    if last.hold == nil then last.hold = hold end
    if hold ~= last.hold then
        last.hold = hold
        last.pending_t = os.clock() + 0.5
        last.pending_hold = hold
    end
    if last.pending_t and os.clock() >= last.pending_t then
        last.pending_t = nil
        local snap = snapshot(p)
        if last.snap then
            local lines = {}
            for full, vals in pairs(snap) do
                local prev = last.snap[full] or {}
                for n, v in pairs(vals) do
                    local pv = prev[n]
                    local differs = (pv == nil) or (type(v) == "number" and type(pv) == "number" and math.abs(v - pv) > 1e-4) or (type(v) ~= "number" and v ~= pv)
                    if differs and pv ~= nil then
                        lines[#lines + 1] = string.format("%s.%s: %s -> %s", full:gsub("^app%.ropeway%.", ""), n, tostring(pv), tostring(v))
                    end
                end
            end
            table.sort(lines)
            log.info(string.format("%s hold %s -> %s: %d value(s) changed", TAG, tostring(not last.pending_hold), tostring(last.pending_hold), #lines))
            for i = 1, math.min(#lines, 80) do log.info(TAG .. "   " .. lines[i]) end
        else
            local n = 0
            for _, vals in pairs(snap) do for _ in pairs(vals) do n = n + 1 end end
            log.info(string.format("%s first snapshot (hold=%s): %d getters across components", TAG, tostring(last.pending_hold), n))
        end
        last.snap = snap
    end
end)
