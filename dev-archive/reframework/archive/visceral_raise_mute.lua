-- Visceral (RE2 VR) -- raise mute (2026-09-24)
--
-- At the aim press the game plays a short "raise" clip on layer 3 (upper-body action) from the hold bank's
-- Hold_Start slots (140/141/143/150/151/153). The aim state only ends its raise phase when that clip ends, so
-- the clip has to play. But while it plays and fades out (~20 frames) it pulls the hips 3-5 cm toward its own
-- pose, which is idle frame 0, while the body underneath is somewhere else in the idle. Measured tonight with the
-- press probe: hips bulge ~3.5 cm and come back after ~20 frames, head/camera still -> the torso-and-legs nudge Tefa
-- sees in VR.
--
-- Fix: while layer 3 is on a raise slot, set that layer's BlendRate to 0. The clip still runs and ends (the aim
-- state, laser and firing are untouched); it just contributes nothing to the pose. Any other layer-3 motion
-- (reload, weapon change, ...) is left exactly as the game plays it. The previous BlendRate is put back the
-- moment the layer leaves the raise slot.
--
-- Proof of effect: one log line per raise ("muted, rate was X"), and the press probe's hips numbers.

if reframework:get_game_name() ~= "re2" then return end

local TAG = "[visceral_raise_mute]"
local NS = sdk.game_namespace
local RAISE_BANK = 2
local RAISE_SLOTS = { [140] = true, [141] = true, [143] = true, [150] = true, [151] = true, [153] = true }
local UPPER_LAYER = 3

local cfg = { enabled = true }
local st = { muted = false, saved = nil, count = 0 }

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end
local function component(player, full)
    local t = sdk.typeof(full); if not t then return nil end
    return safe(function() return player:call("getComponent(System.Type)", t) end)
end

local function step()
    local p = get_player(); if not p then st.muted = false; st.saved = nil; return end
    local mo = component(p, "via.motion.Motion"); if not mo then return end
    local layer = safe(function() return mo:call("getLayer", UPPER_LAYER) end); if not layer then return end
    local bank = safe(function() return layer:call("get_MotionBankID") end)
    local id = safe(function() return layer:call("get_MotionID") end)
    local raise = cfg.enabled and bank == RAISE_BANK and RAISE_SLOTS[id] == true
    if raise then
        if not st.muted then
            st.saved = safe(function() return layer:call("get_BlendRate") end) or 1.0
            st.muted = true; st.count = st.count + 1
            log_line(string.format("raise slot %d on layer %d: muted (rate was %.2f) #%d", id, UPPER_LAYER, st.saved, st.count))
        end
        safe(function() layer:call("set_BlendRate", 0.0) end)
    elseif st.muted then
        local back = st.saved or 1.0
        safe(function() layer:call("set_BlendRate", back) end)
        st.muted = false; st.saved = nil
        log_line(string.format("layer %d left the raise (now bank %s slot %s): rate back to %.2f", UPPER_LAYER, tostring(bank), tostring(id), back))
    end
end

-- before the motion update reads the layer, and again at the end of behaviour updates in case the FSM rewrote it
re.on_pre_application_entry("UpdateMotion", step)
re.on_pre_application_entry("LateUpdateBehavior", step)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: raise mute") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("mute layer 3 while the raise clip plays", cfg.enabled)
    imgui.text(string.format("raises muted so far: %d, muted now: %s", st.count, tostring(st.muted)))
    imgui.tree_pop()
end)

log_line("loaded (layer 3 BlendRate 0 while a Hold_Start raise slot plays)")
