-- Visceral (RE2 VR) -- motion-layer probe (read-only, 2026-09-24)
--
-- Why: with the ordinary walk spliced into the hold bank, Tefa still sees "stiff leg animation" while
-- aiming, although the layer-0 clip is the ordinary walk and its loop length (~62 frames) is the same
-- aimed and unaimed. Suspect: the hold tree blends idle<->walk by MOVEMENT SPEED, and aiming caps the
-- speed, so the walk only gets part of the weight. This logs, once a second: aim state, the player's
-- real ground speed (m/s), and for every layer each node carrying weight (name, weight, frame/end).
-- Writes nothing. Log tag: [visceral_layers].

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_layers]"
local NS = sdk.game_namespace
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

local last = { t = nil, pos = nil }

re.on_frame(function()
    local now = os.clock()
    if last.t and now - last.t < 1.0 then return end
    local p = get_player(); if not p then return end
    local tf = safe(function() return p:call("get_Transform") end)
    local pos = tf and safe(function() return tf:call("get_Position") end) or nil
    local speed = "?"
    if pos and last.pos and last.t then
        local dx, dz = pos.x - last.pos.x, pos.z - last.pos.z
        speed = string.format("%.2f", math.sqrt(dx * dx + dz * dz) / (now - last.t))
    end
    last.t = now
    if pos then last.pos = { x = pos.x, y = pos.y, z = pos.z } end

    local mo = component(p, "via.motion.Motion"); if not mo then return end
    local n = safe(function() return mo:call("getLayerCount") end) or 0
    local parts = {}
    for i = 0, n - 1 do
        local layer = safe(function() return mo:call("getLayer", i) end)
        if layer then
            local cnt = safe(function() return layer:call("getMotionNodeCount") end) or 0
            local nodes = {}
            for k = 0, cnt - 1 do
                local node = safe(function() return layer:call("getMotionNode", k) end)
                if node then
                    local w = safe(function() return node:call("get_Weight") end) or 0
                    if w > 0.01 then
                        local nm = safe(function() return node:call("get_MotionName") end) or "?"
                        local f = safe(function() return node:call("get_Frame") end) or -1
                        local e = safe(function() return node:call("get_EndFrame") end) or -1
                        nodes[#nodes + 1] = string.format("%s w=%.2f %.0f/%.0f", nm, w, f, e)
                    end
                end
            end
            if #nodes > 0 then
                local sp = safe(function() return layer:call("get_Speed") end) or -1
                local nt = safe(function() return layer:call("get_NormalizeTime") end) or -1
                parts[#parts + 1] = string.format("L%d(spd %.2f nt %.2f): %s", i, sp, nt, table.concat(nodes, ", "))
            end
        end
    end
    log.info(string.format("%s hold=%d speed=%s m/s | %s", TAG, is_aiming(p) and 1 or 0, speed, table.concat(parts, " | ")))
end)
