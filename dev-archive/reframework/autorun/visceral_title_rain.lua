-- Visceral (RE2 VR) -- the Story page's rain on the MAIN MENU too. 2026-10-02 (/lm, home PC).
--
-- Why it only rained on the Story page: MenuStoryBehavior.open requests one effect itself, picked by the latest save's
-- location (RPD -> EffectID(container 0, element 2)), through the private ObjectEffectManager.requestEffectInternal(id,
-- null, -1), and close / the fade-out kill it (reader 2026-10-02, disassembly; dossier title section). So the main menu
-- never has it.
--
-- What this does, on the title only (the Story object lives in the title scene and nowhere else):
--   * main menu: request the same effect ourselves, retrying every RETRY_S until the call is accepted -- the first
--     attempt can be refused ("Invoke threw") while the effect data is still loading [verified-live 2026-10-02: one press
--     accepted, two refused, same code]
--   * Story page opens: keep ours running, cancel the copy the game requests and give ours to the Story object (killing
--     ours and letting the game start fresh blipped: a new effect begins with no drops in the air, Tefa 2026-10-03)
--   * Story page closes: keep the game's container alive (clear the field before close / fade-out can kill it) and adopt
--     it as ours, so backing out keeps the rain
-- The element is the game's own choice: the Story object's effectID once filled, else the same latest-save lookup the game
-- does (RAIN_BY_LOCATION); a location with no rain (the underground parking) leaves the main menu dry, as the Story page is.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_title_rain]"
local SIG = "requestEffectInternal(via.effect.script.EffectID, via.GameObject, System.Int32)"
local RETRY_S = 0.5            -- seconds between request attempts on the main menu
local MAX_ATTEMPTS = 120       -- one minute of tries, then give up quietly until the next title visit
local FIND_S = 2.0             -- seconds between looks for the title's Story object
-- the game's own choice (MenuStoryBehavior.open, reader 2026-10-02): latest save's Location.ID -> rain element in container 0;
-- any location not listed gets NO rain (e.g. the underground parking), and OrphanAsylum none for survivor type 3
local RAIN_CONTAINER = 0
local RAIN_BY_LOCATION = { [16] = 2, [23] = 0, [24] = 4, [25] = 3, [30] = 1 }   -- RPD, GasStation2, OrphanAsylum, OrphanApproach, Opening3
local ORPHAN_ASYLUM, NO_RAIN_SURVIVOR = 24, 3

local st = { menu = nil, ours = nil, story_open = false, attempts = 0, next_try = 0, next_find = 0, gave_up = false, logged_fail = false, nudged = false }

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local story_t = sdk.find_type_definition("app.ropeway.gui.MenuStoryBehavior")
if not story_t then log_line("MenuStoryBehavior NOT FOUND -- off"); return end

local function reset(why)
    if st.menu then log_line("title left (" .. why .. ")") end
    st.menu, st.ours, st.story_open, st.attempts, st.gave_up, st.logged_fail, st.logged_el, st.nudged = nil, nil, false, 0, false, false, false, false
    st.preloaded = false   -- the held resources (st.held) stay referenced: loading them again next visit is free
end

local function kill(c) if c then safe(function() c:set_field("KillAllRequest", true) end) end end

local function hook_pre(name, fn, post)
    local m = story_t:get_method(name)
    if not m then log_line(name .. " not found"); return end
    local last
    sdk.hook(m, function(args)
        local o = safe(function() return sdk.to_managed_object(args[2]) end)
        last = o
        if o then fn(o) end
    end, function(rv)
        if post and last then post(last) end
        return rv
    end)
end
-- Story opens: keep OUR rain running and cancel the copy the game just asked for, then hand ours to the Story object so
-- close / the fade-out treat it as the game's. Killing ours and letting the game start its own made a visible blip:
-- a fresh effect starts with no drops in the air (Tefa, 2026-10-03).
hook_pre("open", function(o)
    st.menu = st.menu or o
    st.story_open = true
end, function(o)
    if not st.ours then return end
    local theirs = safe(function() return o:get_field("EffectContainer") end)
    if theirs and theirs ~= st.ours then kill(theirs) end
    safe(function() o:set_field("EffectContainer", st.ours) end)
    st.ours = nil
    log_line("Story opened: kept our rain running, the game's copy cancelled")
end)
hook_pre("close", function(o)
    st.story_open = false
    local c = safe(function() return o:get_field("EffectContainer") end)
    if c then
        safe(function() o:set_field("EffectContainer", nil) end)
        st.ours = c
        log_line("Story closed: kept the game's rain for the main menu")
    end
end)

-- 2026-10-17 (Fable, x64dbg live on the home PC, 22:27): WHY THE REQUEST CRASHED AT RANDOM, AND THE CURE. The game's
-- createEffectInternal keeps its one-entry list of effect objects in the calling thread's SCRATCH PAGE; inside the same
-- call, the first managed-object allocation runs the context's scratch CLEANUP when the context's allocation-nesting
-- counter ([ctx+0x78]) drops back to 0, which rewinds and zeroes the page under the list, so the loop reads junk
-- (length 0xNN0000, entry 1 = UTF-16 "ffec"). Caught in the act with a hardware write-watch on the page offset. The
-- cure holds that counter one higher for the length of every requestEffectInternal call; it lives in the native
-- plugin (dev-archive/native/src/rainhold.cpp) because Lua cannot write the thread context. Lua only logs here.

local function find_menu()
    local sm = sdk.get_native_singleton("via.SceneManager")
    local smt = sdk.find_type_definition("via.SceneManager")
    local scene = sm and safe(function() return sdk.call_native_func(sm, smt, "get_CurrentScene") end)
    local arr = scene and safe(function() return scene:call("findComponents(System.Type)", sdk.typeof("app.ropeway.gui.MenuStoryBehavior")) end)
    local n = arr and safe(function() return arr:call("get_Length") end) or 0
    if n > 0 then return safe(function() return arr:call("GetValue(System.Int32)", 0) end) end
    return nil
end

-- which rain the game would show for the latest save; nil = none
local function rain_element()
    local sdm = sdk.get_managed_singleton("app.ropeway.gamemastering.SaveDataManager")
        or sdk.get_managed_singleton(sdk.game_namespace("SaveDataManager"))
    if not sdm then return nil, "no SaveDataManager" end
    local slot = safe(function() return sdm:call("getLastTimeStampSlotIndex") end)
    if slot == nil then return nil, "no latest slot" end
    local loc = safe(function() return sdm:call("getGameDataLocation", slot) end)
    if loc == nil then return nil, "no location for slot " .. tostring(slot) end
    local el = RAIN_BY_LOCATION[loc]
    if loc == ORPHAN_ASYLUM and safe(function() return sdm:call("getLatestGameDataSurvivorType") end) == NO_RAIN_SURVIVOR then el = nil end
    return el, "slot " .. tostring(slot) .. ", location " .. tostring(loc)
end

local function make_id(m)
    local own = safe(function() return m:get_field("effectID") end)
    if own then return own end
    local el, why = rain_element()
    if el == nil then
        if not st.gave_up then log_line("no rain for the latest save (" .. why .. ") -- the main menu stays dry, as the Story page would") end
        st.gave_up = true
        return nil
    end
    if not st.logged_el then st.logged_el = true; log_line("rain element " .. el .. " for the latest save (" .. why .. ")") end
    local id = safe(function() return sdk.create_instance("via.effect.script.EffectID", true) end)
    if not id then return nil end
    safe(function() id:set_field("ContainerID", RAIN_CONTAINER) end)
    safe(function() id:set_field("ElementID", el) end)
    safe(function() id:set_field("DataContainerIndex", -1) end)
    return id
end

-- 2026-10-17 (Fable, desk read of re2.exe + the 22 crash dumps in the 10-16 log, then one flat launch): WHY THE REQUEST
-- WAS REFUSED AT RANDOM. The game's createEffectInternal builds a one-entry list of effect objects and keeps it in a
-- GameObject[] that it takes from the calling thread's SCRATCH PAGE (allocator 0x141f79930: a 4000-byte page hanging
-- off the thread's VM context, objects stamped 0x80000000 = temporary, never ref-counted). In every dump that array
-- sat exactly 0x60 bytes after the event-args object made the same way, its length read as 0xNN0000 and entry 1 held
-- UTF-16 text ("ffec"): the page had been rewritten under the call before the loop read it again. The game's own request
-- (Story open) runs on the main thread, whose page is reset every frame; ours ran in re.on_frame, i.e. on the render
-- thread, whose page is never reset and wraps at a moment that depends only on how much was allocated before, hence
-- "random". So the whole tick now runs in the game's own update (pre UpdateBehavior, main thread), nothing else changed.
local function tick()
    local now = os.clock()
    if not st.menu then
        if now < st.next_find then return end
        st.next_find = now + FIND_S
        st.menu = find_menu()
        if st.menu then st.armed_at = now; st.last_reading = nil; st.kicked = false; log_line("title found; rain on the main menu armed") end
        return
    end
    -- still on the title? (the object is destroyed with the scene)
    if not safe(function() return st.menu:call("get_GameObject") end) then reset("Story object gone"); return end
    if st.ours or st.story_open or st.gave_up or now < st.next_try then return end
    st.next_try = now + RETRY_S
    st.attempts = st.attempts + 1
    local oem = safe(function() return st.menu:get_field("ObjectEffectManagerComponent") end)
    local id = oem and make_id(st.menu)
    if not (oem and id) then return end
    -- 2026-10-10 (b144): WHY the start time was random. The rain is an effect of the Story menu object's
    -- ObjectEffectManager, and requestEffectInternal throws until that manager has built its effect data
    -- (EPVDataContainerObj, made from the DataContainer prefab in its own update -> updateDataContainer). On the title
    -- the Story object is not updating (UpdateSelf=false) until the game wakes it, at a moment that depends on the title
    -- flow, so the first accepted request came anywhere from 0 to 10+ s [inferred-static 2026-10-10: type database].
    -- Now: as soon as the prefab is Ready, we build the data ourselves (updateDataContainer, the manager's own step), so
    -- the request is accepted on the next try. Each change of the three readings is logged once.
    local dc = safe(function() return oem:get_field("DataContainer") end)
    local ready = dc and safe(function() return dc:call("get_Ready") end)
    local built = safe(function() return oem:get_field("EPVDataContainerObj") end) ~= nil
    if ready and not built then
        safe(function() oem:call("updateDataContainer") end)
        built = safe(function() return oem:get_field("EPVDataContainerObj") end) ~= nil
    end
    -- b145: built at 0.66 s, still refused until 3.17 s (worn 21:05). The manager's other start-up steps also only run
    -- in its own update: the current GameObject and the effect providers. Run them once ourselves when the data is built.
    if built and not st.kicked then
        st.kicked = true
        for _, m in ipairs({ "setDefaultCurrentGameObject", "updatetCurrentGameObjectRef", "createStandardProvider", "createProviders" }) do
            local okm = pcall(function() oem:call(m) end)
            log_line(string.format("start-up step %s: %s", m, okm and "ran" or "threw"))
        end
    end
    -- 2026-10-14 (desk read of requestEffectInternal @0x141783b40 and what it calls, Fable): the request comes back EMPTY
    -- (no throw) when the manager's StandardProvider is missing, when its StandardDataMap has no container for our ID,
    -- or when the EffectManager singleton says IsDisableAllEffect; it THROWS when the EffectManager singleton itself is
    -- missing (the static read before createEffectAutoDestroy). Everything deeper is inside the effect creation. These
    -- four readings go into the same once-per-change line, so the next launch names the gate.
    local provider = safe(function() return oem:get_field("StandardProvider") end) ~= nil
    local map = safe(function() return oem:get_field("StandardDataMap") end)
    local has_c0 = map and safe(function() return map:call("ContainsKey", RAIN_CONTAINER) end)
    local em = safe(function() return sdk.get_managed_singleton("via.effect.script.EffectManager") end)
    local em_off = em and safe(function() return em:get_field("IsDisableAllEffect") end)
    -- 2026-10-15 (/pd, home PC, read from the 2026-10-14 23:01 log + re2.exe): the "Invoke threw" is NOT a throw. Every
    -- refused attempt is an ACCESS VIOLATION (c0000005) that REFramework catches, inside EffectManager.createEffectInternal
    -- @0x140f7fd90 at +0x473 (0x140f80203): the effect GameObjects it has just made from the element's ProviderData come
    -- back as a list holding a junk pointer (RSI = 0x0063006500660066, UTF-16 text "ffec"), read before any of them is
    -- used. All four 10-14 readings were fine at the refused attempts ("provider set, container 0 in the map true,
    -- EffectManager present" at 0.05 s, refused until later), so the gate is the element's own data: most likely its
    -- effect file (EffectResourceHolder) still streaming in [hypothesis]. Lever: read the element's resource paths the
    -- moment the data is built and load them ourselves (sdk.create_resource + add_ref), once per title visit; log how many
    -- loaded and how long it took, and each resource's isValid reading, so the next launch proves or kills it by itself.
    local rid = safe(function() return id:get_field("ElementID") end)
    if built and not st.preloaded and rid then
        st.preloaded = true
        local el = safe(function() return oem:call("getProviderData", RAIN_CONTAINER, rid) end)
        local n = el and safe(function() return el:call("getResourcesCount") end) or 0
        local t0, loaded, names = os.clock(), 0, {}
        for i = 0, n - 1 do
            local h = safe(function() return el:call("getResources", i) end)
            local path = h and safe(function() return h:call("get_ResourcePath") end)
            if path and path ~= "" then
                local res = safe(function() return sdk.create_resource("via.effect.EffectResource", path) end)
                if res then safe(function() res:add_ref() end); st.held = st.held or {}; table.insert(st.held, res); loaded = loaded + 1 end
                table.insert(names, path .. (res and "" or " (NOT loaded)"))
            end
        end
        log_line(string.format("preloaded %d of %d rain resources in %.1f ms: %s", loaded, n, (os.clock() - t0) * 1000,
            #names > 0 and table.concat(names, ", ") or (el and "none listed" or "element NOT found")))
    end
    local rvalid = "?"
    if built and rid then
        local el = safe(function() return oem:call("getProviderData", RAIN_CONTAINER, rid) end)
        local n = el and safe(function() return el:call("getResourcesCount") end)
        if n then
            local v = 0
            for i = 0, n - 1 do if safe(function() return el:call("isValid", i) end) then v = v + 1 end end
            rvalid = string.format("%d/%d", v, n)
        end
    end
    local reading = string.format("prefab %s, ready %s, effect data %s | provider %s, container %d in the map %s, EffectManager %s%s | resources isValid %s",
        dc and "set" or "none", tostring(ready), built and "BUILT" or "missing",
        provider and "set" or "MISSING", RAIN_CONTAINER, tostring(has_c0), em and "present" or "MISSING", em_off and " (all effects DISABLED)" or "",
        rvalid)
    if reading ~= st.last_reading then
        st.last_reading = reading
        log_line(string.format("attempt %d at %.2f s: %s", st.attempts, now - (st.armed_at or now), reading))
    end
    local ok, r = pcall(function() return oem:call(SIG, id, nil, -1) end)
    if ok and r then
        st.ours = r
        log_line(string.format("rain requested for the main menu (attempt %d, %.2f s after the title was found, from UpdateBehavior)", st.attempts, now - (st.armed_at or now)))
    elseif st.attempts >= MAX_ATTEMPTS then
        st.gave_up = true
        log_line(string.format("gave up after %d attempts (last: %s)", st.attempts, tostring(r)))
    elseif not st.logged_fail or st.attempts % 10 == 0 then
        -- 2026-10-10: say WHY (the 2026-10-03 note's first step): the launch-time request is refused until the Story
        -- page has been opened once; the exception text names what is missing
        st.logged_fail = true
        local go = safe(function() return st.menu:call("get_GameObject") end)
        local active = go and safe(function() return go:call("get_UpdateSelf") end)
        log_line(string.format("request refused (attempt %d): ok=%s result=%s | Story object UpdateSelf=%s", st.attempts,
            tostring(ok), tostring(r), tostring(active)))
        -- 2026-10-10 00:40: the refusals come while the Story object is NOT updating (UpdateSelf=false: at boot for ~2 s,
        -- and on the press-any-button screen after a game until A is pressed). Switch its update on once and retry.
        -- 2026-10-10 01:25: NO wake-up any more. Forcing UpdateSelf on made the boot rain slower (11 s instead of 2-5 s) and did not speed up the after-game case (about 10 s with or without it: the title scene is still loading its effect data, and the game accepts the request the moment it is there). Removed; the refusal line stays for the log.
    end
end

-- main thread, inside the engine's own update (2026-10-17, see the note above tick); was re.on_frame since 2026-10-02
re.on_pre_application_entry("UpdateBehavior", function()
    local ok, err = pcall(tick)
    if not ok and not st.tick_err then st.tick_err = true; log_line("tick error: " .. tostring(err)) end
end)

log_line("loaded")
