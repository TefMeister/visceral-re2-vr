-- visceral_zombie_census.lua -- ZOMBIE VARIETY: what face did each LIVE zombie actually get?
-- (2026-09-12, /lm lane.)
--
-- Step 1 proved the loose montage tables are accepted and that a new face in an empty slot loads.
-- What that did NOT show is the thing the whole feature is about: that REAL spawned zombies now
-- wear the re-dealt faces, and that a FACE08 zombie exists at all. Walking a first-person camera
-- around looking for one is slow and blind; the scene graph already knows.
--
-- Every 3 s (and on NUM7) this lists every live Em0000SimpleMontageBase in the scene with the
-- montage ID and the part key names its MontageGroupID resolved to, plus the material name on its
-- face mesh -- so a grey face (material present, texture unbound) can be told from a missing one.
-- Read-only. Logs "[zcensus]" lines.

local function L(fmt, ...) log.info("[zcensus] " .. string.format(fmt, ...)) end

local last = 0
local seen_face = {}

local function scene()
    local sm = sdk.get_native_singleton("via.SceneManager")
    if not sm then return nil end
    return sdk.call_native_func(sm, sdk.find_type_definition("via.SceneManager"), "get_CurrentScene")
end

local function components(type_name)
    local td = sdk.find_type_definition(type_name)
    local sc = scene()
    if not td or not sc then return {} end
    local ok, arr = pcall(function() return sc:call("findComponents(System.Type)", td:get_runtime_type()) end)
    if not ok or not arr then return {} end
    local out, n = {}, 0
    pcall(function() n = arr:get_size() end)
    for i = 0, n - 1 do
        local ok2, c = pcall(function() return arr:get_element(i) end)
        if ok2 and c then out[#out + 1] = c end
    end
    return out
end

local function go_name(comp)
    local ok, go = pcall(function() return comp:call("get_GameObject") end)
    if not ok or not go then return "?" end
    local ok2, nm = pcall(function() return go:call("get_Name") end)
    return ok2 and tostring(nm) or "?"
end

local function str_array(arr)
    local out = {}
    if arr == nil then return out end
    local ok, n = pcall(function() return arr:call("get_Length") end)
    if not ok or not n then return out end
    for i = 0, n - 1 do
        local ok2, v = pcall(function() return arr:call("get_Item", i) end)
        out[#out + 1] = ok2 and tostring(v) or "?"
    end
    return out
end

local function mesh_material(mesh)
    if mesh == nil then return "no mesh" end
    local num = 0
    pcall(function() num = mesh:call("get_MaterialNum") or 0 end)
    local names = {}
    for i = 0, math.min(num, 4) - 1 do
        local ok, nm = pcall(function() return mesh:call("getMaterialName", i) end)
        names[#names + 1] = ok and tostring(nm) or "?"
        -- first texture of the first material tells whether our loose file is the bound one
        if i == 0 then
            local ok2, tn = pcall(function() return mesh:call("getMaterialTextureName", 0, 0) end)
            if ok2 and tn then names[#names] = names[#names] .. " tex0=" .. tostring(tn) end
        end
    end
    return table.concat(names, ", ")
end

-- findComponents(System.Type) matches the EXACT type, so asking for the base class finds nothing:
-- RE2's zombies carry the derived Em0000/0100/0200SimpleMontage. Learned the hard way 2026-09-12,
-- when a save room, a pump room and a corridor all reported "no zombies" while the loose-file log
-- showed face prefabs being served. Ask for every concrete type.
local ZOMBIE_TYPES = {
    "app.ropeway.enemy.em0000.Em0000SimpleMontage",
    "app.ropeway.enemy.em0000.Em0100SimpleMontage",
    "app.ropeway.enemy.em0000.Em0200SimpleMontage",
    "app.ropeway.enemy.em0000.Em0000SimpleMontageBase",
}

local function census()
    local list = {}
    for _, t in ipairs(ZOMBIE_TYPES) do
        for _, c in ipairs(components(t)) do list[#list + 1] = c end
    end
    if #list == 0 then L("no zombies in the scene right now") return end
    L("---- %d zombie(s) ----", #list)
    for i, z in ipairs(list) do
        local done = "?"
        pcall(function() done = tostring(z:call("get_CompletedMontage")) end)
        local gid, mid, keys = nil, "?", {}
        pcall(function() gid = z:call("get_MontageGroupID") end)
        if gid then
            pcall(function() mid = tostring(gid:call("get_MontageID")) end)
            pcall(function() keys = str_array(gid:get_field("BodyPartsKeyNames")) end)
        end
        local face = keys[1] or "?"
        seen_face[face] = (seen_face[face] or 0) + 1
        local fm = nil
        pcall(function() fm = z:call("get_FaceMesh") end)
        L("  %d '%s' montage=%s complete=%s parts=[%s] | faceMesh: %s",
            i, go_name(z), mid, done, table.concat(keys, " "), mesh_material(fm))
    end
    local tally = {}
    for k, v in pairs(seen_face) do tally[#tally + 1] = string.format("%s x%d", k, v) end
    table.sort(tally)
    L("faces seen so far this session: %s", table.concat(tally, ", "))
end

re.on_frame(function()
    local now = os.clock()
    if reframework:is_key_down(0x67) then last = 0 end   -- VK_NUMPAD7 forces one now
    if now - last < 3.0 then return end
    last = now
    local ok, err = pcall(census)
    if not ok then L("census threw: %s", tostring(err)) end
end)

L("loaded")
