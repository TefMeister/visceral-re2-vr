-- Visceral (RE2 VR) -- PROBE: put the Story page's rain on the MAIN MENU. Built 2026-10-02 (/lm, home PC).
--
-- Reader 2026-10-02 (static, disassembly): MenuStoryBehavior.open, after toSubCamera, picks effectID from the latest
-- save's location (RPD -> EffectID(container 0, element 2)) and calls the PRIVATE
-- ObjectEffectManager.requestEffectInternal(effectID, null, -1) on its ObjectEffectManagerComponent field, keeping the
-- result in EffectContainer; close / fade-out set EffectContainer.KillAllRequest. The 09-27 probe hooked only the public
-- requestEffect wrapper (never called here) and its own request passed the menu object + index 0.
--
--   NUM7  request the Story effect on the main menu (requestEffectInternal(eid, nil, -1)); NUM7 again = KillAllRequest
-- Logs: how the Story object was found, the effectID it holds (or the one we built), the return value, and every
-- requestEffectInternal call the game makes (post-hook) so a Story visit shows the reference call.
-- Probe: once it answers, the lever moves into visceral_title_one_scene.lua and this file is archived.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_rain]"
local VK_NUMPAD7 = 0x67
local SIG = "requestEffectInternal(via.effect.script.EffectID, via.GameObject, System.Int32)"
local st = { menu = nil, k7 = false, ours = nil, dumped = false }

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local function dump_type(full)
    local t = sdk.find_type_definition(full)
    if not t then log_line("type NOT found: " .. full); return end
    local ms, fs = {}, {}
    for _, m in ipairs(t:get_methods()) do ms[#ms + 1] = m:get_name() .. "/" .. m:get_num_params() end
    for _, f in ipairs(t:get_fields()) do fs[#fs + 1] = f:get_name() end
    table.sort(ms); table.sort(fs)
    log_line(full .. " methods: " .. table.concat(ms, " "))
    log_line(full .. " fields: " .. table.concat(fs, " "))
end

local story_t = sdk.find_type_definition("app.ropeway.gui.MenuStoryBehavior")
if not story_t then log_line("MenuStoryBehavior NOT FOUND"); return end

-- catch the Story object from any of its methods (update runs while the title GUI lives)
for _, name in ipairs({ "update", "open", "close" }) do
    local m = story_t:get_method(name)
    if m then
        sdk.hook(m, function(args)
            local o = safe(function() return sdk.to_managed_object(args[2]) end)
            if o and not st.menu then st.menu = o; log_line("Story object caught via " .. name) end
        end, function(rv) return rv end)
    end
end

-- the reference: every requestEffectInternal the game makes
local oem_t = sdk.find_type_definition("via.effect.script.ObjectEffectManager")
local rei = oem_t and oem_t:get_method(SIG)
if rei then
    sdk.hook(rei, function(args)
        local id = safe(function() return sdk.to_managed_object(args[3]) end)
        local c = id and safe(function() return id:get_field("ContainerID") end)
        local e = id and safe(function() return id:get_field("ElementID") end)
        local par = safe(function() return sdk.to_managed_object(args[4]) end)
        if par then c = tostring(c) .. " parent=" .. tostring(safe(function() return par:call("get_Name") end)) end
        log_line(string.format("game -> requestEffectInternal(id container=%s element=%s, index=%s)", tostring(c), tostring(e),
            tostring(safe(function() return sdk.to_int64(args[5]) & 0xFFFFFFFF end))))
    end, function(rv) return rv end)
    log_line("hooked ObjectEffectManager." .. SIG)
else
    log_line("ObjectEffectManager." .. SIG .. " NOT FOUND")
end


-- ROUTE B: keep the game's own container alive after a Story visit. Pre-close: stash EffectContainer and clear the field,
-- so neither close(killEffect) nor update's fade-out end can kill it. Pre-open: kill the stash (the game makes a fresh one).
local function hook_pre(name, fn)
    local m = story_t:get_method(name)
    if m then sdk.hook(m, function(args) local o = safe(function() return sdk.to_managed_object(args[2]) end); if o then fn(o) end end, function(rv) return rv end) end
end
hook_pre("close", function(o)
    local c = safe(function() return o:get_field("EffectContainer") end)
    if c then
        st.kept = c
        safe(function() o:set_field("EffectContainer", nil) end)
        log_line("close: kept the Story effect container alive (" .. tostring(c) .. ")")
    end
end)
hook_pre("open", function(o)
    if st.kept then
        safe(function() st.kept:set_field("KillAllRequest", true) end)
        log_line("open: killed the kept container; the game requests its own")
        st.kept = nil
    end
end)

local function find_menu()
    if st.menu then return st.menu end
    -- no Story visit yet: look the component up in the scene
    local sm = sdk.get_native_singleton("via.SceneManager")
    local smt = sdk.find_type_definition("via.SceneManager")
    local scene = sm and safe(function() return sdk.call_native_func(sm, smt, "get_CurrentScene") end)
    local arr = scene and safe(function() return scene:call("findComponents(System.Type)", sdk.typeof("app.ropeway.gui.MenuStoryBehavior")) end)
    local n = arr and safe(function() return arr:call("get_Length") end) or 0
    if n and n > 0 then
        st.menu = safe(function() return arr:call("GetValue(System.Int32)", 0) end) or safe(function() return arr[0] end)
        log_line("Story object found in the scene (" .. tostring(n) .. ")")
    else
        log_line("Story object not found in the scene (" .. tostring(n) .. ")")
    end
    return st.menu
end

local function build_eids()
    -- three shapes; the first that the call accepts wins (09-27 lesson: log which)
    local out = {}
    local td = sdk.find_type_definition("via.effect.script.EffectID")
    log_line("EffectID is a value type: " .. tostring(safe(function() return td:is_value_type() end)))
    local vt = safe(function() return ValueType.new(td) end)
    if vt then
        safe(function() vt.ContainerID = 0 end); safe(function() vt.ElementID = 2 end); safe(function() vt.DataContainerIndex = -1 end)
        safe(function() vt:set_field("ContainerID", 0) end); safe(function() vt:set_field("ElementID", 2) end); safe(function() vt:set_field("DataContainerIndex", -1) end)
        out[#out + 1] = { "ValueType", vt }
    end
    local mo = safe(function() return sdk.create_instance("via.effect.script.EffectID", true) end) or safe(function() return sdk.create_instance("via.effect.script.EffectID") end)
    if mo then
        safe(function() mo:set_field("ContainerID", 0) end); safe(function() mo:set_field("ElementID", 2) end); safe(function() mo:set_field("DataContainerIndex", -1) end)
        out[#out + 1] = { "managed", mo }
        log_line("managed EffectID fields now container=" .. tostring(safe(function() return mo:get_field("ContainerID") end)) ..
            " element=" .. tostring(safe(function() return mo:get_field("ElementID") end)))
    end
    return out
end

re.on_frame(function()
    local k7 = reframework:is_key_down(VK_NUMPAD7)
    if k7 and not st.k7 then
        if not st.dumped then
            st.dumped = true
            dump_type("app.ropeway.gui.MenuStoryBehavior")
            dump_type("via.effect.script.EffectID")
        end
        if st.ours then
            local ok = pcall(function() st.ours:set_field("KillAllRequest", true) end)
            log_line("NUM7: KillAllRequest on our container (ok=" .. tostring(ok) .. ")")
            st.ours = nil
        else
            local m = find_menu()
            local oem = m and safe(function() return m:get_field("ObjectEffectManagerComponent") end)
            log_line("NUM7: ObjectEffectManagerComponent = " .. tostring(oem) .. ", IsCheckLatestLocation=" ..
                tostring(m and safe(function() return m:get_field("IsCheckLatestLocation") end)))
            if oem then
                for _, pair in ipairs(build_eids()) do
                    local sgo = safe(function() return m:call("get_GameObject") end)
                    for _, sig in ipairs({ SIG, "requestEffect(via.effect.script.EffectID, via.GameObject, System.Int32)" }) do
                        local ok, r = pcall(function() return oem:call(sig, pair[2], nil, -1) end)
                        if not ok and sgo then ok, r = pcall(function() return oem:call(sig, pair[2], sgo, -1) end); log_line("  (retried with the Story GameObject as parent)") end
                        log_line(string.format("try %s + %s: ok=%s result=%s", pair[1], sig:match("^[^(]+"), tostring(ok), tostring(r)))
                        if ok and r then st.ours = r; break end
                    end
                    if st.ours then break end
                end
                log_line("NUM7: " .. (st.ours and "a container came back -- look for falling rain" or "no call accepted"))
            end
        end
    end
    st.k7 = k7
end)

log_line("loaded: NUM7 = request / kill the Story page's rain effect on the main menu")
