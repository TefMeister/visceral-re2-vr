-- Visceral (RE2 VR) -- keep the left-hand hold ON while aiming (2026-09-24)
--
-- Why: app.ropeway.survivor.SurvivorIKLeftArmController pins the left hand to the gun. Its
-- lateUpdate() calls updateBlendRate() then updateIKEnable() (re2.exe 0x1405c6900, read
-- statically 2026-09-24). updateBlendRate sets IKBlendRate._Target to the LAST
-- SurvivorIkLeftArmTrack.IKBlendRatio carried by the playing motion, or to 0.0 when the motion
-- carries no such track; updateIKEnable then sets IKEnable = (IKBlendRate.Current > 0.01).
-- The stock aim loops carry the track; the ordinary walk/idle loops our splice puts in the hold
-- bank do not, so while aiming + walking the hold switches off and the hand is only re-solved
-- now and then: the once-a-second flicker Tefa saw on 2026-09-24.
--
-- Lever: while aiming with a weapon in hand, replace updateBlendRate with "blend = 1.0". The
-- original runs again the moment aiming stops, so the game's own damping takes the hand off.
-- Proof of effect: this file logs IKEnable / Current / Target once a second with the aim state,
-- and the plugin's 1 Hz `hooks(aid= ikL=)` count should sit near the frame rate again while
-- walking aimed (it read ~11/s, often 1/s, under the splice without this).
-- Native port belongs in visceral_core.dll once Plugin.cpp is split (2,632 lines > 1,500 hard line).

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_lefthand]"
local NS = sdk.game_namespace

local cfg = { enabled = true, blend = 1.0 }
local st = { hooked = 0, calls = 0, forced = 0, last_log = 0, status = "idle" }

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end
local function component(player, tname)
    local t = sdk.typeof(NS(tname)); if not t then return nil end
    return safe(function() return player:call("getComponent(System.Type)", t) end)
end
local function is_aiming(player)
    local c = component(player, "survivor.SurvivorCondition"); if not c then return false end
    return safe(function() return c:call("get_IsHold") end) == true
end

-- pre-hook on SurvivorIKLeftArmController.updateBlendRate: args[2] is `this`
local function pre_update_blend(args)
    st.calls = st.calls + 1
    if not cfg.enabled then return end
    local ctrl = sdk.to_managed_object(args[2]); if not ctrl then return end
    local target = safe(function() return ctrl:get_field("<CurrentTarget>k__BackingField") end)
    if not target then return end                       -- no weapon target: let the game decide
    local player = get_player(); if not player or not is_aiming(player) then return end
    local df = safe(function() return ctrl:get_field("IKBlendRate") end); if not df then return end
    safe(function() df:set_field("_Target", cfg.blend) end)
    safe(function() df:set_field("<Current>k__BackingField", cfg.blend) end)
    safe(function() df:set_field("_valueChanging", false) end)
    st.forced = st.forced + 1
    return sdk.PreHookResult.SKIP_ORIGINAL
end

local function install()
    local t = sdk.find_type_definition(NS("survivor.SurvivorIKLeftArmController"))
    if not t then log_line("type not found"); return end
    for _, m in ipairs(t:get_methods() or {}) do
        if m and safe(function() return m:get_name() end) == "updateBlendRate" then
            local ok = pcall(function() sdk.hook(m, pre_update_blend, function(rv) return rv end) end)
            if ok then st.hooked = st.hooked + 1 end
        end
    end
    log_line("hooked updateBlendRate x" .. st.hooked)
end
install()

re.on_frame(function()
    local now = os.clock()
    if now - st.last_log < 1.0 then return end
    st.last_log = now
    local p = get_player(); if not p then return end
    local ctrl = component(p, "survivor.SurvivorIKLeftArmController")
    if not ctrl then st.status = "no controller"; return end
    local en = safe(function() return ctrl:get_field("IKEnable") end)
    local df = safe(function() return ctrl:get_field("IKBlendRate") end)
    local cur = df and safe(function() return df:get_field("<Current>k__BackingField") end) or nil
    local tgt = df and safe(function() return df:get_field("_Target") end) or nil
    local hold = is_aiming(p) and 1 or 0
    st.status = string.format("hold=%d IKEnable=%s cur=%s tgt=%s calls/s=%d forced/s=%d",
        hold, tostring(en), cur and string.format("%.2f", cur) or "?", tgt and string.format("%.2f", tgt) or "?",
        st.calls, st.forced)
    log_line(st.status)
    st.calls, st.forced = 0, 0
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral left-hand hold") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("keep left hand on the gun while aiming", cfg.enabled)
    ch, cfg.blend = imgui.slider_float("blend", cfg.blend, 0.0, 1.0)
    imgui.text(st.status)
    imgui.tree_pop()
end)
