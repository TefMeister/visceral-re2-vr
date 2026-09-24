-- Visceral (RE2 VR) -- keep the idle's phase across the aim press (2026-09-24)
--
-- Why: the press probe (runs 14/15) shows the pelvis sliding 1-7 cm over ~15 frames at every RG press,
-- BEFORE the IK pass (pre-IK == post-IK), in a direction that varies press to press. That is the
-- animation restarting: layer 0 leaves the ordinary idle (frame N of 3354) for the hold bank's
-- raise slot and then its idle slot, both starting at frame 0, so the body blends from the
-- breathing/sway pose at frame N to the pose at frame 0. With the hold bank already holding the
-- ordinary idle (item 22 splices), the only thing left to remove is the restart.
--
-- What it does, with a pre-hook on via.motion.TreeLayer.changeMotion:
--   * layer 0, entering one of the hold bank's raise slots (ids 140/141/143/150/151/153) or its idle
--     slot (160) while an ordinary Gazing_Idle is playing: redirect the raise slot to the idle slot
--     (the raise copies are 20-frame excerpts, useless on the full-body layer) and start the new
--     motion at the CURRENT frame (mod the new clip's length), so the pose does not move.
--   * layer 3 (upper-body action) is left alone: its 20-frame raise copy is what ends the raise state.
-- Logs each redirect. Menu checkbox toggles it. Read-only otherwise.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_phase]"
local NS = sdk.game_namespace
local HOLD_BANK = 2
local RAISE_IDS = { [140] = true, [141] = true, [143] = true, [150] = true, [151] = true, [153] = true }
local IDLE_ID = 160
local IDLE_LEN = { OFF = 3354, OLF = 1000, KFF = 3039 }   -- frames of the ordinary idles the hold bank now carries

local cfg = { enabled = true }
local st = { redirects = 0, kept = 0, hooked = 0 }

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

local layer0 = nil
local function is_player_layer0(layer)
    if layer0 and layer == layer0 then return true end
    local p = get_player(); if not p then return false end
    local mo = component(p, "via.motion.Motion"); if not mo then return false end
    local l0 = safe(function() return mo:call("getLayer", 0) end)
    if l0 then layer0 = l0 end
    return l0 ~= nil and layer == l0
end

local function current_idle_kind(layer)
    local node = safe(function() return layer:call("get_HighestWeightMotionNode") end); if not node then return nil end
    local name = safe(function() return node:call("get_MotionName") end) or ""
    if not name:find("Gazing_Idle") then return nil end
    for k, _ in pairs(IDLE_LEN) do if name:find("_" .. k .. "_") then return k end end
    return "OFF"
end

-- args: [2]=layer, [3]=bankID (u32), [4]=motionID (u32), [5]=startFrame (float), [6..] interpolation (long form)
local diag = { calls = 0, last = 0, seen = {} }
local function pre_change(args)
    diag.calls = diag.calls + 1
    if not cfg.enabled then return end
    local layer = sdk.to_managed_object(args[2]); if not layer then return end
    local bank = sdk.to_int64(args[3]) & 0xFFFFFFFF
    local id = sdk.to_int64(args[4]) & 0xFFFFFFFF
    -- diagnostic (run 16 saw zero redirects): record every hold-bank change we are shown, on any layer
    if bank == HOLD_BANK and diag.seen[id] == nil then diag.seen[id] = true; log_line(string.format("seen changeMotion bank %d id %d (player layer0=%s)", bank, id, tostring(is_player_layer0(layer)))) end
    if not is_player_layer0(layer) then return end
    if bank ~= HOLD_BANK or not (RAISE_IDS[id] or id == IDLE_ID) then return end
    local kind = current_idle_kind(layer); if not kind then return end
    local frame = safe(function() return layer:call("get_Frame") end) or 0
    local len = IDLE_LEN[kind] or 3354
    local start = frame % len
    if RAISE_IDS[id] then args[4] = sdk.to_ptr(IDLE_ID); st.redirects = st.redirects + 1 else st.kept = st.kept + 1 end
    args[5] = sdk.float_to_ptr(start)
    log_line(string.format("layer0 %s -> bank %d id %d: start at frame %.0f (was %s idle at %.0f)", RAISE_IDS[id] and ("raise " .. id) or "idle", bank, IDLE_ID, start, kind, frame))
end

-- second lever (run 16: changeMotion was never called for the FSM's own transitions): ask layer 0 to CONTINUE
-- the next motion from the previous one's frame instead of frame 0 (TreeLayer.ContinueFromPrevEnd). The
-- 20-frame raise copy then starts past its own end and finishes at once; the idle slot continues in phase.
local cont = { set = 0, was = nil }
re.on_pre_application_entry("LateUpdateBehavior", function()
    if not cfg.enabled then return end
    local p = get_player(); if not p then return end
    local mo = component(p, "via.motion.Motion"); if not mo then return end
    local l0 = safe(function() return mo:call("getLayer", 0) end); if not l0 then return end
    layer0 = l0
    if cont.was == nil then cont.was = safe(function() return l0:call("get_ContinueFromPrevEnd") end); log_line("layer0 ContinueFromPrevEnd was " .. tostring(cont.was)) end
    if safe(function() return l0:call("get_ContinueFromPrevEnd") end) ~= true then
        safe(function() l0:call("set_ContinueFromPrevEnd", true) end)
        cont.set = cont.set + 1
    end
end)

re.on_frame(function()
    local now = os.clock()
    if now - diag.last >= 5.0 then
        diag.last = now
        log_line(string.format("changeMotion calls in last 5 s: %d | ContinueFromPrevEnd writes: %d", diag.calls, cont.set))
        diag.calls = 0; cont.set = 0
    end
end)

local function install()
    local t = sdk.find_type_definition("via.motion.TreeLayer")
    if not t then log_line("TreeLayer type not found"); return end
    for _, m in ipairs(t:get_methods() or {}) do
        if safe(function() return m:get_name() end) == "changeMotion" then
            local p1 = safe(function() return m:get_param_types()[1]:get_full_name() end)
            if p1 == "System.UInt32" then
                if pcall(function() sdk.hook(m, pre_change, function(rv) return rv end) end) then st.hooked = st.hooked + 1 end
            else
                if pcall(function() sdk.hook(m, function(args) diag.calls = diag.calls + 1 end, function(rv) return rv end) end) then st.hooked = st.hooked + 1 end
            end
        end
    end
    log_line("hooked changeMotion x" .. st.hooked)
end
install()

-- no hotkey: every numpad key is taken (NUM. is the plugin's head hider); the menu checkbox toggles it
re.on_draw_ui(function()
    if not imgui.tree_node("Visceral aim idle phase (no restart at the press)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED", cfg.enabled)
    imgui.text(string.format("redirects=%d kept-phase=%d hooks=%d", st.redirects, st.kept, st.hooked))
    imgui.tree_pop()
end)
