-- visceral_finger_rec.lua -- records the game's own finger animations in RE2 and RE3 (2026-11-02, ported from
-- visceral-re8-vr lua/visceral_re8_finger_rec.lua).
--
-- Tefa, 2026-11-02, after every Village gun was recorded: "do the same recordings on those games as well" (RE2, RE3).
-- While the game plays its own idle / reload animations, write every finger joint's LOCAL rotation, every frame,
-- labelled by gun and by the motion each layer is playing. Wrists are written too, as a reference only.
-- ⚠️ Visceral RE2 ships its own animation files (the relaxed one-handed pose); a take records what the INSTALLED files
-- play, so note which pose files were installed when the take was made.
--
-- Driven by a command file so nobody has to touch the game window:
--   reframework/data/finger_rec/command.txt, one line:
--     start <label>     begin a take (file finger_rec/<label>__<gun>__<n>.csv)
--     stop              end the take
--     probe             finger_rec/probe.txt: the finger joints found, the motion layers, the gun's ammo members
--     slot <n>          put inventory slot n in hand (Inventory.equipSlot)
--     give <WP name>    add and equip a gun the save does not have (InventoryManager.addAndEquipMainWeapon), e.g.
--                       give WP0100; memory only, the game saves at typewriters (testing only)
--     empty             the held gun's loaded rounds to 0 (tries the setters the probe lists)
--     fill              ...to the magazine size
-- The file is emptied once read. Status: finger_rec/status.txt and the REFramework log ("[finger-rec]").
-- Costs nothing until a take starts.

local game = reframework:get_game_name()
if game ~= "re2" and game ~= "re3" then return end
local NS = game == "re2" and "app.ropeway." or "offline."

local TAG = "[finger-rec]"
local DIR = "finger_rec"
local CMD = DIR .. "/command.txt"
local FINGERS = { "index_0", "index_1", "index_2", "middle_0", "middle_1", "middle_2", "ring_0", "ring_1", "ring_2", "ring_3",
                  "little_0", "little_1", "little_2", "little_3", "thumb_0", "thumb_1", "thumb_2" }
local WRISTS = { "l_arm_wrist", "r_arm_wrist" }
local motion_t = sdk.typeof("via.motion.Motion")

local rec = { on = false, f = nil, n = 0, frames = 0, t0 = 0, last_poll = 0 }

local function say(s) pcall(function() log.info(TAG .. " " .. s) end) end
local function safe(fn) local ok, v = pcall(fn); if ok then return v end return nil end
local function status(s)
    local f = safe(function() return io.open(DIR .. "/status.txt", "w") end)
    if f then f:write(os.date("%H:%M:%S") .. " " .. s .. "\n"); f:close() end
    say(s)
end

local function player()
    local pm = safe(function() return sdk.get_managed_singleton(NS .. "PlayerManager") end)
    return pm and safe(function() return pm:call("get_CurrentPlayer") end)
end
local function component(pl, tn)
    local t = safe(function() return sdk.typeof(tn) end)
    return t and pl and safe(function() return pl:call("getComponent(System.Type)", t) end)
end

local function gun_and_name(pl)
    local eq = component(pl, NS .. "survivor.Equipment")
    local w = eq and safe(function() return eq:get_field("<EquipWeapon>k__BackingField") end)
    local go = w and safe(function() return w:call("get_GameObject") end)
    local nm = go and safe(function() return go:call("get_Name") end)
    return w, nm and tostring(nm) or "none"
end

local function joint(tf, name) return safe(function() return tf:call("getJointByName", name) end) end

local cache = { pl = nil, list = nil, t = 0 }
local function joint_list(pl)
    if cache.pl == pl and cache.list and os.clock() - cache.t < 5 then return cache.list end
    local list = {}
    local tf = safe(function() return pl:call("get_Transform") end)
    if tf then
        for _, side in ipairs({ "l_hand_", "r_hand_" }) do
            for _, f in ipairs(FINGERS) do
                local j = joint(tf, side .. f)
                if j then list[#list + 1] = { j = j, name = side .. f } end
            end
        end
        for _, w in ipairs(WRISTS) do
            local j = joint(tf, w)
            if j then list[#list + 1] = { j = j, name = w } end
        end
    end
    cache.pl, cache.list, cache.t = pl, list, os.clock()
    return list
end

local function motions(pl)
    local out = {}
    local mo = motion_t and safe(function() return pl:call("getComponent(System.Type)", motion_t) end)
    if mo then
        local n = safe(function() return mo:call("getLayerCount") end) or 0
        for i = 0, math.min(n, 6) - 1 do
            local L = safe(function() return mo:call("getLayer", i) end)
            if L then
                out[#out + 1] = string.format("player.%d:%s:%s:%.1f", i, tostring(safe(function() return L:call("get_MotionBankID") end) or -1),
                    tostring(safe(function() return L:call("get_MotionID") end) or -1), tonumber(safe(function() return L:call("get_Frame") end)) or -1)
            end
        end
    end
    return table.concat(out, "|")
end

local function reloading(pl)
    local c = component(pl, NS .. "survivor.SurvivorCondition")
    return c and safe(function() return c:call("get_IsReload") end)
end

-- the gun's members that look like its loaded rounds: listed by probe, tried by empty / fill
local function ammo_members(w)
    local out = {}
    local td = w and safe(function() return w:get_type_definition() end)
    while td do
        for _, m in ipairs(safe(function() return td:get_methods() end) or {}) do
            local n = safe(function() return m:get_name() end) or ""
            if n:lower():find("bullet") then out[#out + 1] = "method " .. n .. "(" .. tostring(safe(function() return m:get_num_params() end)) .. ")" end
        end
        for _, fl in ipairs(safe(function() return td:get_fields() end) or {}) do
            local n = safe(function() return fl:get_name() end) or ""
            if n:lower():find("bullet") then out[#out + 1] = "field " .. n end
        end
        td = safe(function() return td:get_parent_type() end)
    end
    return out
end

local function set_loaded(w, n)
    if not w then return false, "no gun" end
    local max = safe(function() return w:call("getMaxBulletNumber") end) or safe(function() return w:call("get_MaxBulletNumber") end) or 15
    if n == nil then n = max end
    for _, m in ipairs({ "setBulletNumber", "set_BulletNumber", "setBulletNum" }) do
        if pcall(function() w:call(m, n) end) and safe(function() return w:call("getBulletNumber") end) == n then return true, m end
    end
    for _, fl in ipairs({ "<BulletNumber>k__BackingField", "_BulletNumber", "BulletNumber", "_BulletNum" }) do
        if pcall(function() w:set_field(fl, n) end) and safe(function() return w:call("getBulletNumber") end) == n then return true, fl end
    end
    local pl = player()
    local inv = pl and component(pl, NS .. "survivor.Inventory")
    local slot = inv and (safe(function() return inv:call("get_MainSlot") end) or safe(function() return inv:get_field("<MainSlot>k__BackingField") end))
    for _, m in ipairs({ "set_Number", "setNumber", "set_Count" }) do
        if slot and pcall(function() slot:call(m, n) end) and safe(function() return w:call("getBulletNumber") end) == n then return true, "main slot " .. m end
    end
    return false, "no setter worked (see probe)"
end

local function start(label)
    local pl = player()
    if not pl then status("no player: not started"); return end
    local _, gname = gun_and_name(pl)
    rec.n = rec.n + 1
    local path = string.format("%s/%s__%s__%s__%d.csv", DIR, game, label, gname, rec.n)
    rec.f = safe(function() return io.open(path, "w") end)
    if not rec.f then status("could not open " .. path); return end
    local list = joint_list(pl)
    local head = { "t", "gun", "reloading", "motions" }
    for _, e in ipairs(list) do head[#head + 1] = e.name .. ".x"; head[#head + 1] = e.name .. ".y"; head[#head + 1] = e.name .. ".z"; head[#head + 1] = e.name .. ".w" end
    rec.f:write(table.concat(head, ",") .. "\n")
    rec.on, rec.frames, rec.t0, rec.path = true, 0, os.clock(), path
    status(string.format("recording %s (%d joints)", path, #list))
end

local function stop()
    if rec.f then rec.f:close() end
    if rec.on then status(string.format("stopped %s: %d frames, %.1f s", rec.path or "?", rec.frames, os.clock() - rec.t0)) end
    rec.on, rec.f = false, nil
end

local function probe()
    local pl = player()
    local f = safe(function() return io.open(DIR .. "/probe.txt", "w") end)
    if not f then return end
    if not pl then f:write("no player\n"); f:close(); return end
    local w, gname = gun_and_name(pl)
    -- 2026-11-02: no inventory calls here: asking InventoryManager for the ammo while a save was still loading froze RE2
    -- at "NOW 10%" (two cores busy, no disk, the window still "responding"); those calls live in "ammo", gun in hand only
    f:write("game: " .. game .. ", gun: " .. gname .. ", loaded: " .. tostring(w and safe(function() return w:call("getBulletNumber") end)) .. "\n")
    local names = {}
    for _, e in ipairs(joint_list(pl)) do names[#names + 1] = e.name end
    f:write(#names .. " joints: " .. table.concat(names, " ") .. "\n")
    f:write("motions now: " .. motions(pl) .. "\n")
    f:write("gun ammo members:\n  " .. table.concat(ammo_members(w), "\n  ") .. "\n")
    local ctr = w and safe(function() return w:call("get_WpBulletCounter") end)
    local ctd = ctr and safe(function() return ctr:get_type_definition() end)
    if ctd then
        f:write("bullet counter " .. tostring(safe(function() return ctd:get_full_name() end)) .. ":\n")
        for _, m in ipairs(safe(function() return ctd:get_methods() end) or {}) do f:write("  method " .. tostring(safe(function() return m:get_name() end)) .. "(" .. tostring(safe(function() return m:get_num_params() end)) .. ")\n") end
        for _, fl in ipairs(safe(function() return ctd:get_fields() end) or {}) do f:write("  field " .. tostring(safe(function() return fl:get_name() end)) .. " = " .. tostring(safe(function() return ctr:get_field(fl:get_name()) end)) .. "\n") end
    end
    for _, tn in ipairs(w and { NS .. "survivor.Inventory", NS .. "gamemastering.InventoryManager" } or {}) do
        local td = sdk.find_type_definition(tn)
        f:write(tn .. ":\n")
        for _, m in ipairs(td and (safe(function() return td:get_methods() end) or {}) or {}) do
            local n = safe(function() return m:get_name() end) or ""
            local l = n:lower()
            if l:find("bullet") or l:find("ammo") or l:find("loaded") then f:write("  " .. n .. "(" .. tostring(safe(function() return m:get_num_params() end)) .. ")\n") end
        end
    end
    local inv = w and component(pl, NS .. "survivor.Inventory")
    local slot = inv and (safe(function() return inv:call("get_MainSlot") end) or safe(function() return inv:get_field("<MainSlot>k__BackingField") end))
    local std = slot and safe(function() return slot:get_type_definition() end)
    f:write("main slot " .. tostring(std and std:get_full_name()) .. ":\n")
    while std do
        for _, m in ipairs(safe(function() return std:get_methods() end) or {}) do
            local n = safe(function() return m:get_name() end) or ""
            if n:lower():find("number") or n:lower():find("count") or n:lower():find("weapon") or n:lower():find("item") then f:write("  " .. n .. "(" .. tostring(safe(function() return m:get_num_params() end)) .. ")\n") end
        end
        std = safe(function() return std:get_parent_type() end)
    end
    f:close()
    status("probe written: " .. #names .. " joints, gun " .. gname)
end

local function poll()
    local f = safe(function() return io.open(CMD, "r") end)
    if not f then return end
    local line = f:read("*l") or ""
    f:close()
    pcall(function() fs.write(CMD, "") end)
    local cmd, arg = line:match("^%s*(%S+)%s*(.-)%s*$")
    if not cmd or cmd == "" then return end
    local pl = player()
    if cmd == "start" then stop(); start(arg ~= "" and arg or "take")
    elseif cmd == "stop" then stop()
    elseif cmd == "probe" then probe()
    elseif cmd == "slot" then
        local inv = component(pl, NS .. "survivor.Inventory")
        local ok = inv and pcall(function() inv:call("equipSlot(System.Int32)", tonumber(arg)) end)
        status(string.format("slot %s: %s", arg, ok and "asked" or "FAILED"))
    elseif cmd == "give" then
        local td = sdk.find_type_definition(NS .. "EquipmentDefine.WeaponType")
        local fl = td and td:get_field(arg)
        local v = fl and safe(function() return fl:get_data(nil) end)
        local im = safe(function() return sdk.get_managed_singleton(NS .. "gamemastering.InventoryManager") end)
        local ok = v ~= nil and im ~= nil and pcall(function() im:call("addAndEquipMainWeapon", v, 1, 0) end)
        status(string.format("give %s (%s): %s", arg, tostring(v), ok and "asked" or "FAILED"))
    elseif cmd == "swap" then
        -- testing only (memory only, the game saves at typewriters): the held gun's inventory slot becomes another gun;
        -- take another slot out and this one again to see it in hand
        local td = sdk.find_type_definition(NS .. "EquipmentDefine.WeaponType")
        local fl = td and td:get_field(arg)
        local v = fl and safe(function() return fl:get_data(nil) end)
        local inv = component(pl, NS .. "survivor.Inventory")
        local slot = inv and (safe(function() return inv:call("get_MainSlot") end) or safe(function() return inv:get_field("<MainSlot>k__BackingField") end))
        local ok = v ~= nil and slot ~= nil and pcall(function() slot:call("set_WeaponType", v) end)
        status(string.format("swap %s (%s): %s, slot now %s", arg, tostring(v), ok and "set" or "FAILED", tostring(slot and safe(function() return slot:call("get_WeaponName") end))))
    elseif cmd == "itemids" then
        local names = {}
        for _, tn in ipairs({ NS .. "gamemastering.Item.ID", NS .. "gamemastering.Item.Id", NS .. "ItemID" }) do
            local td = sdk.find_type_definition(tn)
            for _, fl in ipairs(td and (safe(function() return td:get_fields() end) or {}) or {}) do
                if safe(function() return fl:is_static() end) then names[#names + 1] = tn .. " " .. tostring(fl:get_name()) .. "=" .. tostring(safe(function() return fl:get_data(nil) end)) end
            end
        end
        pcall(function() fs.write(DIR .. "/itemids.txt", table.concat(names, "\n") .. "\n") end)
        status("itemids: " .. #names)
    elseif cmd == "additem" then
        local id, n = arg:match("^(%S+)%s*(%d*)")
        local im = safe(function() return sdk.get_managed_singleton(NS .. "gamemastering.InventoryManager") end)
        local ok, r = pcall(function() return im:call("addItemCount", tonumber(id), tonumber(n) or 60) end)
        status(string.format("additem %s x%s: %s (%s)", tostring(id), tostring(n), ok and "asked" or "FAILED", tostring(r)))
    elseif cmd == "ammo" then
        local w = gun_and_name(pl)
        local im = w and safe(function() return sdk.get_managed_singleton(NS .. "gamemastering.InventoryManager") end)
        if not im then status("ammo: no gun in hand, not asked") else
            status(string.format("ammo: loaded %s, spare %s, reloadable %s, bullet item %s", tostring(safe(function() return w:call("getBulletNumber") end)),
                tostring(safe(function() return im:call("getMainWeaponRemainingBullet") end)), tostring(safe(function() return im:call("getMainWeaponReloadableBullet") end)),
                tostring(safe(function() return im:call("getWeaponBulletId") end))))
        end
    elseif cmd == "spare" then
        -- testing only: give the held gun spare rounds of its own ammo, two ways: as the inventory's surplus for the
        -- main slot, and (spare item) as an item of the gun's own bullet id
        local w = gun_and_name(pl)
        local inv = w and component(pl, NS .. "survivor.Inventory")
        local slot = inv and safe(function() return inv:call("get_MainSlot") end)
        local bid = slot and safe(function() return slot:call("get_BulletID") end)
        local n = tonumber(arg) or 60
        local a = bid and inv and pcall(function() inv:call("set_MainSlotSurplusBulletID", bid); inv:call("set_MainSlotSurplusBulletNumber", n) end)
        local im = safe(function() return sdk.get_managed_singleton(NS .. "gamemastering.InventoryManager") end)
        local r1 = im and safe(function() return im:call("getMainWeaponReloadableBullet") end)
        local b = nil
        if (r1 or 0) <= 0 and bid and im then b = safe(function() return im:call("addItemCount", bid, n) end) end
        local r2 = im and safe(function() return im:call("getMainWeaponReloadableBullet") end)
        status(string.format("spare: bullet id %s, surplus %s -> reloadable %s; item add %s -> reloadable %s", tostring(bid), tostring(a), tostring(r1), tostring(b), tostring(r2)))
    elseif cmd == "slotcall" then
        -- testing only: call one method on the held gun's inventory slot, e.g. "slotcall set_BulletID 15"
        local m, v = arg:match("^(%S+)%s*(%-?%d*)")
        local inv = component(pl, NS .. "survivor.Inventory")
        local slot = inv and safe(function() return inv:call("get_MainSlot") end)
        local ok, r = pcall(function() if v ~= "" then return slot:call(m, tonumber(v)) end return slot:call(m) end)
        status(string.format("slotcall %s %s: %s %s", tostring(m), tostring(v), ok and "ok" or "FAILED", tostring(r)))
    elseif cmd == "find" then
        -- list every method whose name holds the word, on the gun, its slot, the inventory and the inventory manager
        local w = gun_and_name(pl)
        local inv = component(pl, NS .. "survivor.Inventory")
        local slot = w and inv and safe(function() return inv:call("get_MainSlot") end)
        local out = {}
        for _, o in ipairs({ { "gun", w }, { "slot", slot }, { "inventory", inv },
                             { "manager", safe(function() return sdk.get_managed_singleton(NS .. "gamemastering.InventoryManager") end) } }) do
            local td = o[2] and safe(function() return o[2]:get_type_definition() end)
            while td do
                for _, m in ipairs(safe(function() return td:get_methods() end) or {}) do
                    local n = safe(function() return m:get_name() end) or ""
                    if n:lower():find(arg:lower(), 1, true) then out[#out + 1] = o[1] .. " " .. tostring(td:get_full_name()) .. "." .. n .. "(" .. tostring(safe(function() return m:get_num_params() end)) .. ")" end
                end
                td = safe(function() return td:get_parent_type() end)
            end
        end
        pcall(function() fs.write(DIR .. "/find.txt", table.concat(out, "\n") .. "\n") end)
        status("find " .. arg .. ": " .. #out)
    elseif cmd == "wptypes" then
        local td = sdk.find_type_definition(NS .. "EquipmentDefine.WeaponType")
        local names = {}
        for _, fl in ipairs(td and (safe(function() return td:get_fields() end) or {}) or {}) do
            local n = safe(function() return fl:get_name() end) or ""
            if n:match("^WP%d") then names[#names + 1] = n .. "=" .. tostring(safe(function() return fl:get_data(nil) end)) end
        end
        pcall(function() fs.write(DIR .. "/wptypes.txt", table.concat(names, "\n") .. "\n") end)
        status("wptypes: " .. #names)
    elseif cmd == "empty" or cmd == "fill" then
        local w = gun_and_name(pl)
        local ok, how = set_loaded(w, cmd == "empty" and 0 or nil)
        status(string.format("%s: %s (%s)", cmd, ok and "done" or "FAILED", tostring(how)))
    else status("unknown command: " .. line) end
end

re.on_frame(function()
    local now = os.clock()
    if now - rec.last_poll > 0.2 then rec.last_poll = now; pcall(poll) end
    if not rec.on or not rec.f then return end
    local pl = player()
    if not pl then return end
    local _, gname = gun_and_name(pl)
    local row = { string.format("%.4f", now - rec.t0), gname, tostring(reloading(pl)), motions(pl) }
    for _, e in ipairs(joint_list(pl)) do
        local q = safe(function() return e.j:call("get_LocalRotation") end)
        if q then
            row[#row + 1] = string.format("%.5f", q.x); row[#row + 1] = string.format("%.5f", q.y)
            row[#row + 1] = string.format("%.5f", q.z); row[#row + 1] = string.format("%.5f", q.w)
        else
            row[#row + 1] = ""; row[#row + 1] = ""; row[#row + 1] = ""; row[#row + 1] = ""
        end
    end
    rec.f:write(table.concat(row, ",") .. "\n")
    rec.frames = rec.frames + 1
    if rec.frames % 120 == 0 then rec.f:flush() end
end)

say("ready (" .. game .. "): write a command into reframework/data/" .. CMD)
