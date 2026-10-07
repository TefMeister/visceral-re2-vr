-- Visceral (RE2 VR) -- TEMPORARY probe (2026-10-07, Fable, home PC): WHICH SWITCH TURNS THE FORCED SHOT INTO A DRY FIRE?
--
-- Known [measured 2026-09-25, n=3]: SurvivorActionOrderer.setForcePrecede(true, ATTACK) while the fire input is on makes the
-- game ACCEPT the shot without the aim state (Precede=4), but layer 4 plays pl00_1120_HG_Hold_Shoot_NoAmmo: a dry fire with a
-- loaded gun. The choice Shoot / Shoot_NoAmmo is made by the motion FSM data from the variables the game itself writes every
-- frame: the weapon's Wep_common set (Arm.get_CommonVariablesHold/Fire/Empty/NoEmptyFire/Equip) and the player's
-- SurvivorUserVariablesUpdater set (Fire, FireSlur, HoldUp, Relax, Jog). This probe reads all of them around a shot, aimed
-- and unaimed, so the run names the one that differs -- and offers one lever per candidate to flip it the game's own way.
--
-- Keys (flat, numpad; nothing runs by itself):
--   NUM1  toggle: setForcePrecede(ATTACK) while the game's ATTACK input is on (the 09-25 lever)
--   NUM2  one shot through the game's own input: InputSystem.setForce(ATTACK, true) for 8 frames
--   NUM3  dump every switch now (one line)
--   NUM4  toggle lever A: weapon Hold variable forced TRUE while the ATTACK input is on
--   NUM5  toggle lever B: player HoldUp variable forced TRUE while the ATTACK input is on
--   NUM6  toggle lever C: weapon Empty forced FALSE + NoEmptyFire forced FALSE while the ATTACK input is on
--   NUM7  toggle: the game's own HOLD (aim) input latched -- the AIMED reference shot
-- Log prefix [visceral_firevars]; every change of the switch set is one line, for 60 frames after each shot request.
-- Remove after the test (archive/ + README line).
if reframework:get_game_name() ~= "re2" then return end
local TAG = "[visceral_firevars]"
local NS = sdk.game_namespace
local KIND_ATTACK, KIND_HOLD = 256, 64
local PRECEDE_ATTACK = 4
local ATTACK_FRAMES, FOLLOW_FRAMES = 8, 60
local VK = { n1 = 0x61, n2 = 0x62, n3 = 0x63, n4 = 0x64, n5 = 0x65, n6 = 0x66 }

local st = { force = false, leverA = false, leverB = false, leverC = false, attack_left = 0, follow = 0, shots = 0,
             last = "", keys = {}, last_tick = 0, fireshots = 0 }
local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function L(m) log.info(TAG .. " " .. m) end

local function player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end
local function component(go, tn)
    local t = sdk.typeof(tn); if not t or not go then return nil end
    return safe(function() return go:call("getComponent(System.Type)", t) end)
end
local function input()
    return sdk.get_managed_singleton(NS("InputSystem"))
end
local function input_on(kind)
    local inp = input(); if not inp then return nil end
    return safe(function() return inp:call("isOn(System.UInt64, System.Boolean)", kind, false) end) == true
end
local function set_force_input(kind, on)
    local inp = input(); if not inp then return false end
    return safe(function() inp:call("setForce", kind, on) return true end) == true
end

-- ---- readers ---------------------------------------------------------------------------------------------
local function b(v) if v == nil then return "?" end if v == true then return "1" end if v == false then return "0" end return tostring(v) end
local function motions(p)
    local mo = component(p, "via.motion.Motion"); if not mo then return "nomotion" end
    local n = safe(function() return mo:call("get_LayerCount") end) or 0
    local parts = { "layers=" .. tostring(n) }
    for i = 0, 7 do   -- run 2: get_LayerCount read 0 while getLayer(0) still answers (the 10-02 probe saw the same), so ask 0..7

        local layer = safe(function() return mo:call("getLayer", i) end)
        local node = layer and safe(function() return layer:call("get_HighestWeightMotionNode") end)
        local name = node and safe(function() return node:call("get_MotionName") end)
        local id = layer and safe(function() return layer:call("get_MotionID") end)
        local bank = layer and safe(function() return layer:call("get_MotionBankID") end)
        -- run 1 (2026-10-07 22:23): the names came back empty on every layer, so the ids are printed too
        if (name and name ~= "") or id ~= nil then parts[#parts + 1] = string.format("L%d=%s/%s:%s", i, tostring(bank), tostring(id), tostring(name or "-")) end
    end
    return table.concat(parts, " ")
end
local function state_text(p)
    local cond = component(p, NS("survivor.SurvivorCondition"))
    local eq = cond and safe(function() return cond:call("get_Equipment") end)
    local upd = cond and safe(function() return cond:call("get_UserVariablesUpdater") end)
    local ord = cond and safe(function() return cond:call("get_ActionOrderer") end)
    local arm = eq and (safe(function() return eq:call("get_EquipWeapon") end) or safe(function() return eq:call("get_MainWeapon") end))
    local function c(o, m) return o and safe(function() return o:call(m) end) end
    local bullets = arm and c(arm, "getBulletNumber")
    return string.format("IsHold=%s EnableAttack=%s Precede=%s inATTACK=%s inHOLD=%s | player Fire=%s FireSlur=%s HoldUp=%s Relax=%s Jog=%s | weapon Hold=%s Fire=%s Empty=%s EnableExecuteFire=%s Equip=%s bullets=%s | %s",
        b(c(cond, "get_IsHold")), b(c(cond, "get_EnableAttack")), tostring(c(ord, "get_Precede")), b(input_on(KIND_ATTACK)), b(input_on(KIND_HOLD)),
        b(c(upd, "get_Fire")), tostring(c(upd, "get_FireSlur")), b(c(upd, "get_HoldUp")), b(c(upd, "get_Relax")), b(c(upd, "get_Jog")),
        b(c(arm, "get_CommonVariablesHold")), b(c(arm, "get_CommonVariablesFire")), b(c(arm, "get_CommonVariablesEmpty")),
        b(c(arm, "get_EnableExecuteFire")), b(c(arm, "get_CommonVariablesEquip")), tostring(bullets), motions(p)), cond, eq, upd, ord, arm
end

-- ---- the observe-only hook: a shot that really started -----------------------------------------------------
do
    local fs = sdk.find_type_definition(NS("fsmv2.player.FireShot"))
    local m = fs and fs:get_method("onStart")
    if m then
        sdk.hook(m, function() st.fireshots = st.fireshots + 1
            local p = player(); local txt = p and state_text(p) or "no player"
            L(string.format("FIRESHOT.onStart #%d: %s", st.fireshots, txt)) end, function(rv) return rv end)
        L("hooked FireShot.onStart (observe only)")
    else L("FireShot.onStart NOT found") end
    -- run 2 (22:30): the forced order + our Fire trigger put the FSM into the HOLD-tagged shot state for a few frames, but no
    -- bullet left. Does the shot pipeline run and refuse (code), or does the FSM play the no-ammo clip (data)? Observe the
    -- three doorbells; which one is missing names the gate.
    local function observe(tn, mn, label)
        local td = sdk.find_type_definition(NS(tn)); local m = td and td:get_method(mn)
        if m == nil then L(label .. " NOT found") return end
        sdk.hook(m, function(args) L(label .. " CALLED") end, function(rv) return rv end)
        L("hooked " .. label .. " (observe only)")
    end
    observe("survivor.Equipment", "requestFire", "Equipment.requestFire")
    observe("survivor.Equipment", "executeFire", "Equipment.executeFire")
    observe("implement.Gun", "executeFire", "Gun.executeFire")
    -- run 3 (22:45): with the forced order + our Fire trigger the FSM plays the REAL shoot clip (pl00_1100_..._Hold_Shoot) and
    -- Gun.executeFire IS called -- and still spends no round. Its first lines are the inlined enableFire: owner->enableAttack
    -- (Equipment.enableAttack(WeaponType)) and a byte at +0x194; false = return at once. Lever E (NUM9) answers those two
    -- questions YES while one of our forced shots is in flight, nothing else.
    local function answer_yes(tn, mn, label)
        local td = sdk.find_type_definition(NS(tn)); local m = td and td:get_method(mn)
        if m == nil then L(label .. " NOT found (lever E)") return end
        sdk.hook(m, function(args) end, function(rv)
            if st.leverE and st.shot_live then st.leverE_hits = (st.leverE_hits or 0) + 1 return sdk.to_ptr(1) end
            return rv
        end)
        L("hooked " .. label .. " (lever E: YES while our shot is in flight)")
    end
    answer_yes("survivor.Equipment", "enableAttack", "Equipment.enableAttack")
    answer_yes("implement.Gun", "enableFire", "Gun.enableFire")
    answer_yes("implement.Arm", "enableAttack", "Arm.enableAttack")
end

-- ---- per frame ------------------------------------------------------------------------------------------
local function edge(name, vk)
    local d = reframework:is_key_down(vk)
    local was = st.keys[name]; st.keys[name] = d
    return d and not was
end

re.on_pre_application_entry("UpdateBehavior", function()
    local p = player(); if not p then return end
    if edge("n1", VK.n1) then st.force = not st.force; L("NUM1: forced ATTACK order while the fire input is on -> " .. tostring(st.force)) end
    if edge("n4", VK.n4) then st.leverA = not st.leverA; L("NUM4: lever A (weapon Hold variable TRUE at the press) -> " .. tostring(st.leverA)) end
    if edge("n5", VK.n5) then st.leverB = not st.leverB; L("NUM5: lever B (player HoldUp variable TRUE at the press) -> " .. tostring(st.leverB)) end
    if edge("n6", VK.n6) then st.leverC = not st.leverC; L("NUM6: lever C (weapon Empty + NoEmptyFire FALSE at the press) -> " .. tostring(st.leverC)) end
    -- run 1 (2026-10-07 22:23, flat, handgun, 7 rounds): AIMED shot = Precede 4 + player Fire=1 for one frame + bullets 7->6;
    -- UNAIMED forced order = Precede 4 for one frame, player Fire stays 0, nothing else moves. So the order is accepted but the
    -- player's Fire TRIGGER (the updater's) is never raised outside the hold. Lever D raises it ourselves at the press.
    if edge("n9", 0x69) then st.leverE = not st.leverE; L("NUM9: lever E (enableAttack/enableFire answered YES during our shot) -> " .. tostring(st.leverE) .. " hits so far " .. tostring(st.leverE_hits or 0)) end
    if edge("n8", 0x68) then st.leverD = not st.leverD; L("NUM8: lever D (player Fire trigger raised at the press) -> " .. tostring(st.leverD)) end
    if edge("n7", 0x67) then   -- NUM7: the game's own aim input, latched (the reference: an AIMED shot for comparison)
        st.hold = not st.hold; set_force_input(KIND_HOLD, st.hold); L("NUM7: forced HOLD (aim) input -> " .. tostring(st.hold))
    end
    if edge("n2", VK.n2) and st.attack_left == 0 then
        st.attack_left = ATTACK_FRAMES; st.shots = st.shots + 1; st.follow = FOLLOW_FRAMES; st.last = ""
        set_force_input(KIND_ATTACK, true)
        L(string.format("SHOT #%d requested through the game's ATTACK input (force=%s A=%s B=%s C=%s)", st.shots, tostring(st.force), tostring(st.leverA), tostring(st.leverB), tostring(st.leverC)))
    end
    if st.attack_left > 0 then
        st.attack_left = st.attack_left - 1
        if st.attack_left == 0 then set_force_input(KIND_ATTACK, false) end
    end

    local txt, cond, eq, upd, ord, arm = state_text(p)
    local fire_on = input_on(KIND_ATTACK) == true
    -- the levers, applied before the game's own frame work reads them
    if ord and st.force then safe(function() ord:call("setForcePrecede", fire_on, PRECEDE_ATTACK) end) end
    if fire_on then
        if st.leverD and upd and not st.leverD_done then st.leverD_done = true; safe(function() upd:call("set_Fire", true) end) L("lever D: player Fire trigger set TRUE") end
        if st.leverA and arm then safe(function() arm:call("set_CommonVariablesHold", true) end) end
        if st.leverB and upd then safe(function() upd:call("set_HoldUp", true) end) end
        if st.leverC and arm then safe(function() arm:call("set_CommonVariablesEmpty", false) end) safe(function() arm:call("set_CommonVariablesNoEmptyFire", false) end) end
    end

    if not fire_on then st.leverD_done = nil end
    if edge("n3", VK.n3) then L("NUM3 dump: " .. txt) end
    st.shot_live = st.follow > FOLLOW_FRAMES - 20   -- the first 20 frames after a NUM2 request
    if st.follow > 0 then
        st.follow = st.follow - 1
        if txt ~= st.last then st.last = txt; L(string.format("f+%d: %s", FOLLOW_FRAMES - st.follow, txt)) end
    end
    local now = os.clock()
    if now - st.last_tick > 5.0 then st.last_tick = now; L("tick: " .. txt) end
end)
L("loaded -- NUM1 force ATTACK order | NUM2 one shot | NUM3 dump | NUM4/5/6 levers A/B/C. Nothing runs by itself.")
