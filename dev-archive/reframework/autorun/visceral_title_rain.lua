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

local st = { menu = nil, ours = nil, story_open = false, attempts = 0, next_try = 0, next_find = 0, gave_up = false, logged_fail = false }

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local story_t = sdk.find_type_definition("app.ropeway.gui.MenuStoryBehavior")
if not story_t then log_line("MenuStoryBehavior NOT FOUND -- off"); return end

local function reset(why)
    if st.menu then log_line("title left (" .. why .. ")") end
    st.menu, st.ours, st.story_open, st.attempts, st.gave_up, st.logged_fail, st.logged_el = nil, nil, false, 0, false, false, false
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

re.on_frame(function()
    local now = os.clock()
    if not st.menu then
        if now < st.next_find then return end
        st.next_find = now + FIND_S
        st.menu = find_menu()
        if st.menu then log_line("title found; rain on the main menu armed") end
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
    local ok, r = pcall(function() return oem:call(SIG, id, nil, -1) end)
    if ok and r then
        st.ours = r
        log_line(string.format("rain requested for the main menu (attempt %d)", st.attempts))
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
    end
end)

log_line("loaded")
