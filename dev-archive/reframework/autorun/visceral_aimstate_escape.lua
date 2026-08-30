-- Visceral (RE2 VR) -- aim-state escape (option 1): drop the weapon-ready brace
--
-- The braced weapon-hold stance (butt out, shoulders down, feet floated off the
-- ground because VR anchors the body to the HMD) is the root of the twist, the
-- hover, the wrong stance AND the slow aim speed. It's selected by the animation
-- "TargetBankType" value, which encodes "weapon ready / combat". Handgun out it
-- reads ~0x300009; unarmed it's 0.
--
-- This forces TargetBankType = 0 (unarmed) every frame -- on the Motion component
-- and the WeaponTargetBankController that drives it -- so the WHOLE body resolves
-- to the unarmed banks: normal grounded walk/idle. The weapon itself is held by
-- the VR mod's hand IK regardless, so it should stay in your hand.
--
-- EXPERIMENT: reversible toggle + live readback so we can see if the force sticks
-- and what the legs actually play. Known risk (from AC notes): unarmed also drops
-- the FINGER grip bank to bare-hand -- in VR the weapon is attached to the hand,
-- so we test whether that looks acceptable.
--
-- Hotkeys: NUM3 = toggle escape    NUM8 = dump current state to log

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_escape]"
local NS = sdk.game_namespace
local VK_NUMPAD3, VK_NUMPAD8 = 0x63, 0x68

local cfg = { enabled = false, force_value = 0 }
local state = {
    keys_ok = true, prev = {},
    motion = nil, wtbc = nil, have = false,
    ui_tbt = "n/a", ui_wtbc = "n/a", ui_l0 = "n/a", ui_ishold = "n/a",
    writes = 0,
}

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end
local function get_comp(go, tn)
    local t = sdk.typeof(tn); if not t then return nil end
    return safe(function() return go:call("getComponent(System.Type)", t) end)
end

local function acquire(player)
    state.motion = get_comp(player, "via.motion.Motion")
    -- WeaponTargetBankController may be on the player or the equipped weapon GO
    state.wtbc = get_comp(player, NS("survivor.motion.WeaponTargetBankController"))
    if not state.wtbc then
        local eq = get_comp(player, NS("survivor.Equipment"))
        local wp = eq and safe(function() return eq:get_field("<EquipWeapon>k__BackingField") end) or nil
        local wgo = wp and safe(function() return wp:call("get_GameObject") end) or nil
        if wgo then state.wtbc = get_comp(wgo, NS("survivor.motion.WeaponTargetBankController")) end
    end
    state.have = state.motion ~= nil
    log_line(string.format("acquire: Motion=%s WeaponTargetBankController=%s",
        tostring(state.motion ~= nil), tostring(state.wtbc ~= nil)))
end

local function force_unarmed()
    local v = cfg.force_value
    if state.motion then
        if safe(function() state.motion:call("set_TargetBankType", v); return true end) then state.writes = state.writes + 1 end
    end
    if state.wtbc then
        safe(function() state.wtbc:call("set_TargetBankType", v) end)
        safe(function() state.wtbc:call("set_CurrentTargetBankType", v) end)
        safe(function() state.wtbc:call("set_LastAppliedTargetBankType", v) end)
    end
end

local function read_state()
    if state.motion then state.ui_tbt = tostring(safe(function() return state.motion:call("get_TargetBankType") end)) end
    if state.wtbc then state.ui_wtbc = tostring(safe(function() return state.wtbc:call("get_CurrentTargetBankType") end)) end
    -- L0 playing motion (confirm unarmed CMN vs armed HG)
    if state.motion then
        local layer0 = safe(function() return state.motion:call("getLayer", 0) end)
        local node = layer0 and safe(function() return layer0:call("get_HighestWeightMotionNode") end) or nil
        if node then state.ui_l0 = tostring(safe(function() return node:call("get_MotionName") end)) end
    end
    local cond = get_comp(get_player() or {}, NS("survivor.SurvivorCondition"))
    if cond then state.ui_ishold = tostring(safe(function() return cond:call("get_IsHold") end)) end
end

re.on_pre_application_entry("LateUpdateBehavior", function()
    local player = get_player()
    if not player then state.have = false; return end
    if not state.have then acquire(player) end
    if cfg.enabled then force_unarmed() end
end)

re.on_frame(function()
    read_state()
    if not state.keys_ok then return end
    local function pressed(vk)
        local d = safe(function() return reframework:is_key_down(vk) end)
        if d == nil then state.keys_ok = false; return false end
        local w = state.prev[vk]; state.prev[vk] = d; return d and not w
    end
    if pressed(VK_NUMPAD3) then cfg.enabled = not cfg.enabled; log_line("enabled=" .. tostring(cfg.enabled)) end
    if pressed(VK_NUMPAD8) then
        log_line(string.format("STATE: enabled=%s TargetBankType=%s wtbc=%s IsHold=%s L0=%s",
            tostring(cfg.enabled), state.ui_tbt, state.ui_wtbc, state.ui_ishold, state.ui_l0))
    end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: aim-state escape (drop weapon brace)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED -- force unarmed stance (NUM3)", cfg.enabled)
    ch, cfg.force_value = imgui.slider_int("force TargetBankType value", cfg.force_value, 0, 16)
    imgui.separator()
    imgui.text("Motion.TargetBankType (live): " .. state.ui_tbt .. "   wtbc.Current: " .. state.ui_wtbc)
    imgui.text("IsHold(game aim): " .. state.ui_ishold)
    imgui.text("L0 playing: " .. state.ui_l0 .. "   (want CMN_ unarmed, not HG_)")
    imgui.text("acquired: " .. (state.have and "yes" or "NO") .. string.format("  writes=%d", state.writes))
    imgui.text("Weapon is held by VR hand IK; grip finger pose may go bare-hand (test).")
    imgui.tree_pop()
end)

log_line("loaded (aim-state escape). NUM3 toggle, NUM8 dump. Off until enabled.")
