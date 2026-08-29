-- Visceral (RE2 VR) -- aim-pose experiment: swap aim-walk anim -> walk anim
--
-- Goal (roadmap "aim body animation banned"): make the body/legs use the NORMAL
-- WALK animation while aiming, instead of the twisted aim-walk gait. In VR the
-- arms/gun follow the controllers (IK) regardless of the body animation, so if
-- we can get the body out of the aim locomotion, we get "walk stance + legs,
-- gun held up by hands" -- which is the goal.
--
-- EMV Engine is only a bone-poser/observer (no anim-swap), and our AC history
-- shows motlist-swap + weapon-category spoof BOTH FAILED. So this tries the
-- motion-VARIABLE lever instead: force `HoldUp` (the "weapon up" animation flag)
-- OFF while aiming and see if the body reverts to walk locomotion while the game
-- still knows we're aiming (IsHold stays true) and the hands still aim.
--
-- Also lets you force a couple of other candidate flags to A/B, and shows the
-- live values, so we can find whichever flag actually drives the aim gait.
--
-- READ + limited WRITE (documented writable motion bools). Fully reversible.
-- Hotkeys: NUM5 = toggle force HoldUp=false   NUM6 = cycle extra flag to force
--          NUM8 = panic (stop all forcing)

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_aimpose]"
local NS = sdk.game_namespace
local VK_NUMPAD5, VK_NUMPAD6, VK_NUMPAD8 = 0x65, 0x66, 0x68

-- candidate motion bools that might drive the aim gait (force to false to test)
local EXTRA_CANDIDATES = { "none", "Relax", "LowAttack", "CanRapidFire" }

local cfg = { force_holdup_off = false, extra_idx = 1 }

local state = {
    keys_ok = true, prev = {},
    vars = {},              -- name -> Variable object
    names_wanted = { "HoldUp", "Jog", "Relax", "LowAttack", "CanRapidFire" },
    have = false,
    ui_ishold = "n/a", ui_holdup = "n/a", ui_jog = "n/a",
    status = "idle",
}

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end
local function is_aiming(player)
    local t = sdk.typeof(NS("survivor.SurvivorCondition")); if not t then return false end
    local c = safe(function() return player:call("getComponent(System.Type)", t) end); if not c then return false end
    return safe(function() return c:call("get_IsHold") end) == true
end

local function acquire(player)
    state.vars = {}; state.have = false
    local motion = safe(function() return player:call("getComponent(System.Type)", sdk.typeof("via.motion.Motion")) end)
    if not motion then return end
    local hub = safe(function() return motion:call("get_VariablesHub") end); if not hub then return end
    local count = tonumber(safe(function() return hub:call("get_VariableSum") end)) or 0
    local want = {}
    for _, n in ipairs(state.names_wanted) do want[n] = true end
    for i = 0, math.min(count, 512) - 1 do
        local v = safe(function() return hub:call("getVariableFromIndex", i) end)
        if v then
            local nm = safe(function() return v:call("get_Name") end)
            if nm and want[nm] then state.vars[nm] = v end
        end
    end
    state.have = state.vars["HoldUp"] ~= nil
    log_line("acquire: HoldUp=" .. tostring(state.vars["HoldUp"] ~= nil))
end

local function read_bool(v)
    local b = safe(function() return v:call("get_Bool") end)
    if type(b) == "boolean" then return b end
    local f = safe(function() return v:call("get_F") end)
    if type(f) == "number" then return f ~= 0 end
    return nil
end
local function force_false(v)
    if not v then return end
    if not safe(function() v:call("set_Bool", false); return true end) then
        safe(function() v:call("set_F", 0.0) end)
    end
end

local function tick()
    local player = get_player()
    if not player then state.have = false; return end
    if not state.have then acquire(player) end
    local aiming = is_aiming(player)
    state.ui_ishold = tostring(aiming)
    if state.vars["HoldUp"] then state.ui_holdup = tostring(read_bool(state.vars["HoldUp"])) end
    if state.vars["Jog"] then state.ui_jog = tostring(read_bool(state.vars["Jog"])) end

    if cfg.force_holdup_off and state.vars["HoldUp"] then
        force_false(state.vars["HoldUp"])
    end
    local extra = EXTRA_CANDIDATES[cfg.extra_idx]
    if extra and extra ~= "none" and state.vars[extra] then
        force_false(state.vars[extra])
    end
    state.status = string.format("HoldUp force=%s  extra=%s", tostring(cfg.force_holdup_off), extra or "none")
end

re.on_frame(function()
    tick()
    if not state.keys_ok then return end
    local function pressed(vk)
        local d = safe(function() return reframework:is_key_down(vk) end)
        if d == nil then state.keys_ok = false; return false end
        local w = state.prev[vk]; state.prev[vk] = d; return d and not w
    end
    if pressed(VK_NUMPAD5) then cfg.force_holdup_off = not cfg.force_holdup_off; log_line("force_holdup_off=" .. tostring(cfg.force_holdup_off)) end
    if pressed(VK_NUMPAD6) then cfg.extra_idx = (cfg.extra_idx % #EXTRA_CANDIDATES) + 1; log_line("extra=" .. EXTRA_CANDIDATES[cfg.extra_idx]) end
    if pressed(VK_NUMPAD8) then cfg.force_holdup_off = false; cfg.extra_idx = 1; log_line("PANIC: forcing off") end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: aim-pose experiment (aim-walk -> walk anim)") then return end
    local ch
    ch, cfg.force_holdup_off = imgui.checkbox("force HoldUp = false (NUM5)", cfg.force_holdup_off)
    imgui.text("extra flag to also force false (NUM6): " .. EXTRA_CANDIDATES[cfg.extra_idx])
    if imgui.button("cycle extra flag") then cfg.extra_idx = (cfg.extra_idx % #EXTRA_CANDIDATES) + 1 end
    imgui.separator()
    imgui.text("IsHold(game aim): " .. state.ui_ishold .. "   HoldUp(anim): " .. state.ui_holdup .. "   Jog: " .. state.ui_jog)
    imgui.text("vars acquired: " .. (state.have and "yes" or "NO"))
    imgui.text("status: " .. state.status)
    imgui.text("TEST: aim, then NUM5. Watch if legs/stance become normal WALK while")
    imgui.text("the hands still aim the gun. If partial, cycle the extra flag (NUM6).")
    if imgui.button("PANIC off (NUM8)") then cfg.force_holdup_off = false; cfg.extra_idx = 1 end
    imgui.tree_pop()
end)

log_line("loaded (aim-pose experiment). NUM5 force HoldUp off, NUM6 extra flag, NUM8 panic.")
