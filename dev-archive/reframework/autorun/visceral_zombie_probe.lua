-- visceral_zombie_probe.lua -- ZOMBIE VARIETY step-1 probe (2026-09-12).
-- Proves, from inside the game and without walking to a zombie, that the loose montage tables were
-- taken: reads the montage manager's outfit list, the face key each re-dealt outfit resolved to, and
-- asks the game to load the NEW face's prefab (FACE08) so the loose-file log shows its files.
-- One shot, ~12 s after the player binds; NUM9 re-runs it. Logs [zprobe] lines to the REFramework log.
-- Read-only on game state except for the prefab standby requests (which only load resources).

local NS = "app.ropeway.enemy.em0000."
local fired, t0 = false, nil
local WANT_IDS = { "ID000", "ID004", "ID009", "ID016", "ID021", "ID201", "ID206", "ID305" }
-- FACE20+ are BEYOND the EM0000_MONTAGE_PARTS_FACE enum: the runtime looks keys up as strings,
-- so these resolving is the proof that the face pool is not capped at the enum (2026-09-12).
local WANT_FACES = { "FACE00", "FACE08", "FACE20", "FACE29", "FACE37", "FACE76" }
local keep = {}   -- keep prefab refs alive

local function L(fmt, ...) log.info("[zprobe] " .. string.format(fmt, ...)) end

local function manager()
    local td = sdk.find_type_definition("app.ropeway.RopewaySingletonBehaviorRoot`1<" .. NS .. "Em0000MontageManager>")
    if not td then return nil, "no singleton type" end
    local m = td:get_method("get_Instance")
    if not m then return nil, "no get_Instance" end
    local ok, inst = pcall(function() return m:call(nil) end)
    if not ok then return nil, "get_Instance threw: " .. tostring(inst) end
    return inst
end

local function str_array(arr)
    local out = {}
    if arr == nil then return out end
    local n = arr:call("get_Length") or 0
    for i = 0, n - 1 do out[#out + 1] = tostring(arr:call("get_Item", i)) end
    return out
end

local function run()
    local mgr, why = manager()
    if not mgr then L("manager: %s", why) return end
    L("manager ok %s", tostring(mgr))
    local ok, names = pcall(function() return mgr:call("getMontageIDNames", false, false) end)
    if ok and names then
        local t = str_array(names)
        L("outfits known to the manager: %d (first: %s ... last: %s)", #t, t[1] or "?", t[#t] or "?")
    else
        L("getMontageIDNames failed: %s", tostring(names))
    end
    for _, id in ipairs(WANT_IDS) do
        local ok2, md = pcall(function() return mgr:call("getMontageData", id) end)
        if ok2 and md then
            L("outfit %s -> face=%s body=%s shirt=%s pants=%s", id,
                tostring(md:get_field("FaceKeyName")), tostring(md:get_field("BodyKeyName")),
                tostring(md:get_field("ShirtKeyName")), tostring(md:get_field("PantsKeyName")))
        else
            L("outfit %s -> getMontageData failed/nil: %s", id, tostring(md))
        end
    end
    for _, key in ipairs(WANT_FACES) do
        local ok3, pf = pcall(function() return mgr:call("getFacePrefab", key) end)
        if ok3 and pf then
            local path = "?"
            pcall(function() path = tostring(pf:call("get_Path")) end)
            local sb = "?"
            pcall(function() sb = tostring(pf:call("get_Standby")) end)
            L("face %s -> prefab %s path=%s standby(before)=%s", key, tostring(pf), path, sb)
            pcall(function() pf:call("set_Standby", true) end)   -- ask the engine to load it
            keep[#keep + 1] = pf
        else
            L("face %s -> getFacePrefab nil/failed: %s", key, tostring(pf))
        end
    end
    pcall(function() mgr:call("updatePrefabStandby", true) end)
    L("standby requested for %d face prefabs; watch reframework_accessed_files.txt for Face08/", #keep)
end

local function player_bound()
    local pm = sdk.get_managed_singleton("app.ropeway.PlayerManager")
    if not pm then return false end
    local ok, p = pcall(function() return pm:call("get_CurrentPlayer") end)
    return ok and p ~= nil
end

re.on_frame(function()
    if fired then
        if reframework:is_key_down(0x69) then fired = false; t0 = nil end   -- VK_NUMPAD9 re-arms
        return
    end
    if not player_bound() then return end
    if not t0 then t0 = os.clock() return end
    if os.clock() - t0 < 12 then return end
    fired = true
    local ok, err = pcall(run)
    if not ok then L("run threw: %s", tostring(err)) end
    -- report standby state a little later, on the next frames
    local t1 = os.clock()
    local reported = false
    re.on_frame(function()
        if reported or os.clock() - t1 < 4 then return end
        reported = true
        for i, pf in ipairs(keep) do
            local sb, rd = "?", "?"
            pcall(function() sb = tostring(pf:call("get_Standby")) end)
            pcall(function() rd = tostring(pf:call("get_Ready")) end)
            L("prefab %d standby=%s ready=%s", i, sb, rd)
        end
    end)
end)

L("loaded")
