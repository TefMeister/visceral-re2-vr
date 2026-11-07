-- Visceral (RE2 VR) -- keep the idle's phase across the aim press (v4, 2026-09-24)
--
-- Why: at every RG press layer 0 restarts the ordinary idle from frame 0 (hold-bank slot), sliding the
-- pelvis 1-7 cm depending on where the loop was. Dead levers so far: changeMotion is never called for
-- FSM transitions; ContinueFromPrevEnd is reset every frame; set_Frame on layer 3 is reset every frame
-- (the raise never ended: frozen torso, laser off, movement locked -- run 18); set_Frame on layer 0 reads
-- back but the nudge stayed.
-- v4 tries the layer's "next start" parameters instead: every frame while an ordinary idle plays on
-- layer 0, write the CURRENT frame into NextStartFrame / ResetStartFrame / NextStartToFrame, so whatever
-- the FSM starts next on this layer starts there. The raise slots are the 20-frame copies again (v5 base,
-- v1 light lists); a start frame past their end just ends them early. Logs the getters once a second and
-- the frame the new motion actually shows right after a switch. Menu checkbox toggles it. Writes nothing else.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_phase]"
local NS = sdk.game_namespace
local IDLE_LEN = { OFF = 3354, OLF = 1000, KFF = 3039 }

local cfg = { enabled = true }
local st = { writes = 0, switches = 0, last_log = 0, prev = nil }

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
local function idle_kind(name)
    if not name or not name:find("Gazing_Idle") then return nil end
    for k, _ in pairs(IDLE_LEN) do if name:find("_" .. k .. "_") then return k end end
    return "OFF"
end
local function layer_state(layer)
    local node = safe(function() return layer:call("get_HighestWeightMotionNode") end)
    return {
        bank = safe(function() return layer:call("get_MotionBankID") end) or -1,
        id = safe(function() return layer:call("get_MotionID") end) or -1,
        frame = safe(function() return layer:call("get_Frame") end) or 0,
        name = node and safe(function() return node:call("get_MotionName") end) or "-",
    }
end

re.on_pre_application_entry("LateUpdateBehavior", function()
    if not cfg.enabled then st.prev = nil; return end
    local p = get_player(); if not p then st.prev = nil; return end
    local mo = component(p, "via.motion.Motion"); if not mo then return end
    local l0 = safe(function() return mo:call("getLayer", 0) end); if not l0 then return end
    local cur = layer_state(l0)
    local prev = st.prev
    if prev and (cur.bank ~= prev.bank or cur.id ~= prev.id) and idle_kind(cur.name) and idle_kind(prev.name) then
        st.switches = st.switches + 1
        log_line(string.format("layer0 switch bank %d id %d (frame %.0f) -> bank %d id %d: new motion shows frame %.0f",
            prev.bank, prev.id, prev.frame, cur.bank, cur.id, cur.frame))
    end
    st.prev = cur
    if idle_kind(cur.name) then
        local f = cur.frame
        safe(function() l0:call("set_NextStartFrame", f) end)
        safe(function() l0:call("set_ResetStartFrame", f) end)
        safe(function() l0:call("set_NextStartToFrame", f) end)
        st.writes = st.writes + 1
    end
    local now = os.clock()
    if now - st.last_log >= 1.0 then
        st.last_log = now
        local a = safe(function() return l0:call("get_NextStartFrame") end)
        local b = safe(function() return l0:call("get_ResetStartFrame") end)
        local c = safe(function() return l0:call("get_NextStartToFrame") end)
        log_line(string.format("layer0 %s frame %.0f | NextStartFrame=%s ResetStartFrame=%s NextStartToFrame=%s | writes %d switches %d",
            cur.name, cur.frame, tostring(a), tostring(b), tostring(c), st.writes, st.switches))
        st.writes = 0
    end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral aim idle phase (no restart at the press)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED", cfg.enabled)
    imgui.tree_pop()
end)
