-- visceral_pickup_probe.lua -- TEST ONLY (2026-11-05, Fable): what hides the world behind a pick-up (the grey-green stage).
--
-- Tefa: "game world can be visible on item pick up, just like a normal inventory screen would". The inventory shows the live
-- world since b129-b131; the get-item screen (CallOpenMode 4) still shows the game's grey-green floor under a black sky, even
-- with the item camera skipped (b145). So something else is drawn around the camera, or the world is switched off. This
-- probe takes a snapshot of every mesh in the scene (game object name, enabled, drawn) outside the menus and again a few
-- frames into a pick-up, and logs the difference: meshes that appeared (a stage?) and meshes that stopped drawing (the world?).
-- Commands through reframework/data/visceral/pickup_probe.txt (one word, cleared after reading):
--   snap            take the baseline now (also taken by itself every 2 s while no menu is open)
--   diff            log the difference between now and the baseline
--   hide <name>     set draw off on every game object with that exact name (and its children); show <name> puts it back
--   cam             log the primary camera's object, position, fov and every zero-argument getter with Mask / Layer / Draw in its name
--   open            try GUIMaster.openInventoryGetItemMode on the first item in the inventory (logs the method's parameter list first)
-- Everything is logged as [visceral-pickup-probe]. Not installed with the mod; copy into autorun for a run, take it out after.
local TAG = "[visceral-pickup-probe]"
local CMD = "visceral/pickup_probe.txt"
local function L(s) log.info(TAG .. " " .. tostring(s)) end
local function safe(f, ...) local ok, v = pcall(f, ...) if ok then return v end return nil end

local baseline, baseline_t, in_pickup_frames, diffed = nil, 0, 0, false
local restore_on, restore_logged = true, 0   -- "restore off|on": put the world's draw flags back while a pick-up is up
local hidden = {}   -- name -> list of game objects we switched off

local function scene()
    local sm = sdk.get_native_singleton("via.SceneManager"); local t = sdk.find_type_definition("via.SceneManager")
    return sm and t and sdk.call_native_func(sm, t, "get_CurrentScene") or nil
end

local function components(tname)
    local sc = scene(); if not sc then return {} end
    local arr = safe(function() return sc:call("findComponents(System.Type)", sdk.typeof(tname)) end)
    local els = arr and safe(function() return arr:get_elements() end)
    return els or {}
end

local function go_name(go) return safe(function() return go:call("get_Name") end) or "?" end

-- every mesh: key = component address; value = {name, enabled, draw, drawself, mesh path}
local function snapshot()
    local out, n = {}, 0
    for _, mc in ipairs(components("via.render.Mesh")) do
        local go = safe(function() return mc:call("get_GameObject") end)
        if go then
            local res = safe(function() return mc:call("getMesh") end)
            local path = res and (safe(function() return res:call("get_Path") end) or safe(function() return res:call("get_ResourcePath") end)) or "?"
            out[mc:get_address()] = {
                name = go_name(go),
                enabled = safe(function() return mc:call("get_Enabled") end),
                draw = safe(function() return go:call("get_Draw") end),
                drawself = safe(function() return go:call("get_DrawSelf") end),
                path = tostring(path),
            }
            n = n + 1
        end
    end
    return out, n
end

local function restore_world()
    if not baseline then return 0 end
    local n = 0
    for _, mc in ipairs(components("via.render.Mesh")) do
        local b = baseline[mc:get_address()]
        if b and b.draw then
            local go = safe(function() return mc:call("get_GameObject") end)
            if go and safe(function() return go:call("get_Draw") end) == false then
                safe(function() go:call("set_Draw", true) end); n = n + 1
            end
        end
    end
    return n
end

local function fmt(e) return string.format("%s (enabled %s, draw %s, drawself %s) %s", e.name, tostring(e.enabled), tostring(e.draw), tostring(e.drawself), e.path) end

local function diff(why)
    if not baseline then L("diff: no baseline yet"); return end
    local now, n = snapshot()
    local appeared, gone, off, on = {}, {}, {}, {}
    for k, e in pairs(now) do
        local b = baseline[k]
        if not b then appeared[#appeared + 1] = fmt(e)
        elseif (b.draw and not e.draw) or (b.enabled and not e.enabled) or (b.drawself and not e.drawself) then off[#off + 1] = fmt(e)
        elseif (not b.draw and e.draw) or (not b.enabled and e.enabled) then on[#on + 1] = fmt(e) end
    end
    for k, b in pairs(baseline) do if not now[k] then gone[#gone + 1] = fmt(b) end end
    L(string.format("diff (%s): %d meshes now; appeared %d, gone %d, switched off %d, switched on %d", why, n, #appeared, #gone, #off, #on))
    local function dump(label, list)
        table.sort(list)
        for i, s in ipairs(list) do if i <= 40 then L("  " .. label .. ": " .. s) end end
        if #list > 40 then L("  " .. label .. ": ... and " .. (#list - 40) .. " more") end
    end
    dump("APPEARED", appeared); dump("GONE", gone); dump("OFF", off); dump("ON", on)
end

local function cam_report()
    local cam = sdk.get_primary_camera(); if not cam then L("cam: no primary camera"); return end
    local go = safe(function() return cam:call("get_GameObject") end)
    local xf = go and safe(function() return go:call("get_Transform") end)
    local p = xf and safe(function() return xf:call("get_Position") end)
    L(string.format("cam: %s at %s fov %s near %s far %s", go and go_name(go) or "?", p and string.format("%.2f %.2f %.2f", p.x, p.y, p.z) or "?",
        tostring(safe(function() return cam:call("get_FOV") end)), tostring(safe(function() return cam:call("get_NearClipPlane") end)), tostring(safe(function() return cam:call("get_FarClipPlane") end))))
    local td = cam:get_type_definition()
    while td do
        for _, m in ipairs(td:get_methods()) do
            local nm = m:get_name()
            if m:get_num_params() == 0 and (nm:find("Mask") or nm:find("Layer") or nm:find("Draw") or nm:find("Target") or nm:find("Scene")) and nm:sub(1, 4) == "get_" then
                L("  cam " .. nm .. " = " .. tostring(safe(function() return cam:call(nm) end)))
            end
        end
        td = td:get_parent_type()
    end
end

local function set_draw(name, on)
    local n = 0
    for _, xf in ipairs(components("via.Transform")) do
        local go = safe(function() return xf:call("get_GameObject") end)
        if go and go_name(go) == name then
            safe(function() go:call("set_Draw", on) end); safe(function() go:call("set_DrawSelf", on) end)
            n = n + 1
        end
    end
    L(string.format("%s %s: %d object(s)", on and "show" or "hide", name, n))
end

local function inventory_mode()
    local gm = sdk.get_managed_singleton("app.ropeway.gui.GUIMaster")
    local open = gm and safe(function() return gm:call("get_IsOpenInventory") end)
    if not open then return nil end
    local inv = components("app.ropeway.gui.NewInventoryBehavior")[1]
    local mode = inv and safe(function() return inv:get_field("<OpenMode>k__BackingField") end)
    return mode or -1
end

-- open <item id>: a StockItem made here (herbs are ids 4-7 in this save's game, KNOWN.md), handed to the game's own
-- get-item screen. The second argument (the gimmick's save data) is left nil: a script-made pick-up has none.
local function try_open(arg)
    local gm = sdk.get_managed_singleton("app.ropeway.gui.GUIMaster"); if not gm then L("open: no GUIMaster"); return end
    local id = tonumber(arg) or 4
    local stock = safe(function() return sdk.create_instance("app.ropeway.gamemastering.InventoryManager.StockItem") end)
    if not stock then L("open: could not make a StockItem"); return end
    pcall(function() stock = stock:add_ref() end)
    local ok1, e1 = pcall(function() stock:call("setStock(app.ropeway.gamemastering.Item.ID, app.ropeway.EquipmentDefine.WeaponType, app.ropeway.gamemastering.Item.ID, System.Int32)", id, 0, 0, 1) end)
    L("open: StockItem.setStock(" .. id .. ", 0, 0, 1) -> " .. tostring(ok1) .. " " .. tostring(e1) .. "; blank " .. tostring(safe(function() return stock:call("isBlank") end)))
    local ok, r = pcall(function() return gm:call("openInventoryGetItemMode", stock, nil) end)
    L("open: openInventoryGetItemMode(stock, nil) -> " .. tostring(ok) .. " " .. tostring(r))
end

-- lua <code>: run a line of Lua here (the rest of the command file), so the probe can be steered without a script reload
local function run_lua(code)
    local f, err = load(code, "pickup_probe_cmd", "t", setmetatable({L = L, safe = safe, components = components, scene = scene, snapshot = snapshot, diff = diff, set_draw = set_draw}, {__index = _G}))
    if not f then L("lua: compile error: " .. tostring(err)); return end
    local ok, r = pcall(f)
    L("lua: " .. tostring(ok) .. " " .. tostring(r))
end


-- who hides the world, and does a write right before rendering win? Counts of baseline-drawn objects that read draw=false,
-- taken before and after the inventory's own update / lateUpdate, and a second restore at PrepareRendering (pre).
local pick_up_now = false
local who_logged = 0
local function hidden_count()
    if not baseline then return -1 end
    local n = 0
    for _, mc in ipairs(components("via.render.Mesh")) do
        local b = baseline[mc:get_address()]
        if b and b.draw then
            local go = safe(function() return mc:call("get_GameObject") end)
            if go and safe(function() return go:call("get_Draw") end) == false then n = n + 1 end
        end
    end
    return n
end
local function watch(tname, mname)
    local td = sdk.find_type_definition(tname); local m = td and td:get_method(mname)
    if not m then L("watch: no " .. tname .. "." .. mname); return end
    local before = -1
    sdk.hook(m, function(args) if pick_up_now and who_logged < 12 then before = hidden_count() end end,
                function(ret) if pick_up_now and who_logged < 12 then who_logged = who_logged + 1; L(string.format("who: %s.%s: hidden before %d, after %d", tname:match("[^.]+$"), mname, before, hidden_count())) end return ret end)
    L("watch: " .. tname .. "." .. mname)
end
watch("app.ropeway.gui.NewInventoryBehavior", "update")
watch("app.ropeway.gui.NewInventoryBehavior", "lateUpdate")
watch("app.ropeway.gui.NewInventoryDetailBehavior", "update")
watch("app.ropeway.gui.NewInventoryDetailBehavior", "lateUpdate")
local pr_logged = 0
re.on_pre_application_entry("PrepareRendering", function()
    if pick_up_now and restore_on then
        local n = restore_world()
        if n > 0 and pr_logged < 6 then pr_logged = pr_logged + 1; L("restore at PrepareRendering: " .. n .. " drawn again") end
    end
end)

re.on_frame(function()
    local t = os.clock()
    local mode = inventory_mode()
    pick_up_now = (mode == 4 or mode == 5) and in_pickup_frames >= 12
    if mode == nil then
        in_pickup_frames, diffed, restore_logged, who_logged, pr_logged = 0, false, 0, 0, 0
        if t - baseline_t > 2.0 then baseline, _ = snapshot(); baseline_t = t end
    elseif mode == 4 or mode == 5 then
        in_pickup_frames = in_pickup_frames + 1
        if in_pickup_frames == 12 and not diffed then diffed = true; L("pick-up (mode " .. mode .. ") up for 12 frames:"); diff("pick-up"); cam_report() end
        if restore_on and in_pickup_frames >= 12 then
            local n = restore_world()
            if n > 0 and restore_logged < 20 then restore_logged = restore_logged + 1; L(string.format("restore: frame %d of the pick-up: %d object(s) the game had switched off drawn again", in_pickup_frames, n)) end
        end
    end
    local cmd = safe(function() return fs.read(CMD) end)
    if cmd and cmd ~= "" then
        pcall(fs.write, CMD, "")
        cmd = cmd:gsub("^%s+", ""):gsub("%s+$", "")
        local word, arg = cmd:match("^(%S+)%s*(.*)$")
        if word == "lua" then run_lua(cmd:sub(4)); return end
        L("command: " .. cmd)
        if word == "snap" then baseline, _ = snapshot(); baseline_t = t; L("baseline taken")
        elseif word == "diff" then diff("asked")
        elseif word == "hide" then set_draw(arg, false)
        elseif word == "show" then set_draw(arg, true)
        elseif word == "cam" then cam_report()
        elseif word == "open" then try_open(arg)
        elseif word == "restore" then restore_on = arg ~= "off"; restore_logged = 0; L("restore " .. tostring(restore_on)) end
    end
end)
L("loaded (test only)")
