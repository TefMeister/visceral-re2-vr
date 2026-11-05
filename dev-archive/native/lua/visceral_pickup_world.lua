-- visceral_pickup_world.lua -- the game world stays visible behind an item pick-up (2026-11-05, Fable).
--
-- Tefa: "game world can be visible on item pick up, just like a normal inventory screen would". The inventory already shows
-- the live world (menu_tint.cpp, b129-b131); the get-item screen (NewInventoryBehavior CallOpenMode 4 / 5) showed the game's
-- grey-green floor under a black sky instead. Found with lua/visceral_pickup_probe.lua `[measured 2026-11-05, flat]`: the
-- world is still there and its camera stays (b145); the game switches the scene FOLDER "Environments" (under the location's
-- folder) off with via.Folder.set_DrawSelf(false) for the pick-up, so every object in it reads draw=false while its own
-- DrawSelf stays true. set_DrawSelf(true) on that folder brings the world back at once, and the game does not fight it
-- (the next frames kept it on). Here: while a pick-up is up, every scene folder the game switched off since the last quiet
-- moment is switched on again, right before rendering; the game puts things back itself when the screen closes.
-- REFramework menu "Visceral pick-up world": a tick box, and the list of folders handled.
local TAG = "[visceral-pickup-world]"
local function L(s) log.info(TAG .. " " .. tostring(s)) end
local function safe(f, ...) local ok, v = pcall(f, ...) if ok then return v end return nil end

local on = true
local baseline, baseline_t = nil, 0     -- folder address -> {name, drawself}, taken while no menu is open
local restored = {}                     -- names switched back on in this pick-up (for the log and the menu)
local restored_logged = false
local pick_up = false

local function scene()
    local sm = sdk.get_native_singleton("via.SceneManager"); local t = sdk.find_type_definition("via.SceneManager")
    return sm and t and sdk.call_native_func(sm, t, "get_CurrentScene") or nil
end

-- every folder in the scene, by walking from the scene's first folder through children and siblings (via.Folder.get_Child /
-- get_Next), the way the engine lays them out
local function all_folders()
    local out = {}
    local sc = scene(); if not sc then return out end
    local root = safe(function() return sc:call("get_FirstFolder") end) or safe(function() return sc:call("get_Folder") end)
    local function walk(f, depth)
        local n = 0
        while f and n < 4096 do
            out[#out + 1] = f
            local child = safe(function() return f:call("get_Child") end)
            if child and depth < 32 then walk(child, depth + 1) end
            f = safe(function() return f:call("get_Next") end)
            n = n + 1
        end
    end
    if root then walk(root, 0) end
    return out
end

local function snapshot()
    local out, n = {}, 0
    for _, f in ipairs(all_folders()) do
        out[f:get_address()] = { name = safe(function() return f:call("get_Name") end) or "?", drawself = safe(function() return f:call("get_DrawSelf") end) }
        n = n + 1
    end
    return out, n
end

local function inventory_mode()
    local gm = sdk.get_managed_singleton("app.ropeway.gui.GUIMaster")
    if not gm or not safe(function() return gm:call("get_IsOpenInventory") end) then return nil end
    local sc = scene(); if not sc then return nil end
    local arr = safe(function() return sc:call("findComponents(System.Type)", sdk.typeof("app.ropeway.gui.NewInventoryBehavior")) end)
    local els = arr and safe(function() return arr:get_elements() end)
    local inv = els and els[1]
    return inv and safe(function() return inv:get_field("<OpenMode>k__BackingField") end) or -1
end

local function restore()
    if not baseline then return 0 end
    local n = 0
    for _, f in ipairs(all_folders()) do
        local b = baseline[f:get_address()]
        if b and b.drawself and safe(function() return f:call("get_DrawSelf") end) == false then
            safe(function() f:call("set_DrawSelf", true) end)
            n = n + 1
            if not restored[b.name] then restored[b.name] = true end
        end
    end
    return n
end

re.on_frame(function()
    local mode = inventory_mode()
    local t = os.clock()
    if mode == nil then
        if pick_up then L("pick-up closed; the game is drawing its own folders again"); pick_up = false end
        if t - baseline_t > 2.0 then baseline = snapshot(); baseline_t = t end
        restored, restored_logged = {}, false
    else
        pick_up = on and (mode == 4 or mode == 5)
    end
end)

re.on_pre_application_entry("PrepareRendering", function()
    if not pick_up then return end
    local n = restore()
    if n > 0 and not restored_logged then
        restored_logged = true
        local names = {}
        for k in pairs(restored) do names[#names + 1] = k end
        table.sort(names)
        L("pick-up: " .. n .. " scene folder(s) the game switched off drawn again: " .. table.concat(names, ", "))
    end
end)

re.on_draw_ui(function()
    if imgui.tree_node("Visceral pick-up world") then
        local changed, v = imgui.checkbox("The world stays visible behind an item pick-up", on)
        if changed then on = v end
        local names = {}
        for k in pairs(restored) do names[#names + 1] = k end
        imgui.text(#names > 0 and ("folders drawn again: " .. table.concat(names, ", ")) or "no pick-up up")
        imgui.tree_pop()
    end
end)
L("loaded")
