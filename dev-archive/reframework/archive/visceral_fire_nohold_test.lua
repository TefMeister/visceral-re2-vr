-- Visceral (RE2 VR) -- TEMPORARY test (2026-09-24, idea 7 / idea 1): can the player fire WITHOUT the aim state?
-- The reader found that PlayerActionOrderer.checkOrder(ATTACK) is the one decision tying firing to aiming, and that
-- its first test is the orderer's Force bits (SurvivorActionOrderer.setForcePrecede). Scroll Lock toggles the force.
-- Logs: every FireShot.onStart (a shot actually started), IsHold at that moment, and once a second the force
-- state, IsHold and Precede. Remove after the test.
if reframework:get_game_name() ~= "re2" then return end
local TAG = "[visceral_firetest]"
local NS = sdk.game_namespace
local ATTACK = 4
local st = { force = true, prev_key = false, shots = 0, last_t = 0 }
local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function L(m) log.info(TAG .. " " .. m) end

local function player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end
local function condition(p)
    local t = sdk.typeof(NS("survivor.SurvivorCondition")); if not t or not p then return nil end
    return safe(function() return p:call("getComponent(System.Type)", t) end)
end
local function orderer(c) return c and safe(function() return c:get_field("<ActionOrderer>k__BackingField") end) end
local function is_hold(c) return c and safe(function() return c:call("get_IsHold") end) == true end

local fs = sdk.find_type_definition(NS("fsmv2.player.FireShot"))
local m = fs and fs:get_method("onStart")
if m then
    sdk.hook(m, function()
        st.shots = st.shots + 1
        local c = condition(player())
        L(string.format("FIRESHOT #%d started: force=%s IsHold=%s", st.shots, tostring(st.force), tostring(is_hold(c))))
    end, function(rv) return rv end)
    L("hooked FireShot.onStart")
else
    L("FireShot.onStart NOT found")
end

re.on_pre_application_entry("UpdateBehavior", function()
    local c = condition(player()); local o = orderer(c)
    local d = safe(function() return reframework:is_key_down(0x91) end)
    if d and not st.prev_key then st.force = not st.force; L("force -> " .. tostring(st.force))
        if o and not st.force then safe(function() o:call("setForcePrecede", false, ATTACK) end) end
    end
    st.prev_key = d
    -- only while the fire button (the game's ATTACK input, 0x100) is down; on release clear the force again
    local inp = sdk.get_managed_singleton(NS("InputSystem"))
    local fire = inp and safe(function() return inp:call("isOn(System.UInt64, System.Boolean)", 0x100, false) end) == true
    if st.fire_seen == nil and inp then st.fire_seen = true; L("InputSystem found; fire button read = " .. tostring(fire)) end
    if fire ~= st.prev_fire then L("fire button " .. (fire and "DOWN" or "up") .. " IsHold=" .. tostring(is_hold(c))); st.prev_fire = fire end
    if o and st.force then safe(function() o:call("setForcePrecede", fire and true or false, ATTACK) end) end
    local now = os.clock()
    if now - st.last_t > 1.0 and o then
        st.last_t = now
        local pre = safe(function() return o:get_field("<Precede>k__BackingField") end)
        L(string.format("tick force=%s IsHold=%s Precede=%s shots=%d", tostring(st.force), tostring(is_hold(c)), tostring(pre), st.shots))
    end
end)
L("loaded (Scroll Lock toggles setForcePrecede(true, ATTACK))")
