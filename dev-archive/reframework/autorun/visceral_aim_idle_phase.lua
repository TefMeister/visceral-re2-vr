-- Visceral (RE2 VR) -- keep the idle's phase across the aim press (v3, 2026-09-24)
--
-- Why: the press probe (runs 14-17) shows the pelvis sliding 1-7 cm over ~15 frames at every RG press,
-- before the IK pass, in a phase-dependent direction: layer 0 leaves the ordinary idle at frame N for the
-- hold bank's raise slot and then its idle slot, both starting at frame 0. Two earlier levers were
-- ignored by the FSM (changeMotion is never called for its transitions; ContinueFromPrevEnd is reset
-- every frame). This one acts after the switch, on the layer's own frame counter:
--   * every frame, remember layer 0's motion (bank, id, name) and frame;
--   * when layer 0 has just switched between two Gazing_Idle motions (any bank/slot), set its frame to the
--     previous motion's frame (mod the idle's length) -- the pose continues instead of restarting;
--   * layer 3 (upper-body action): when it starts a hold-bank raise slot that now holds the full idle,
--     set its frame to the last frame, so the raise state ends at once (it ends on motion end) and the
--     laser still switches on. Needs the hold bank with FULL idles in the raise slots (v4 base list, v2
--     light list).
-- Logs every correction. Menu checkbox toggles it. Read-only otherwise.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_phase]"
local NS = sdk.game_namespace
local HOLD_BANK = 2
local RAISE_IDS = { [140] = true, [141] = true, [143] = true, [150] = true, [151] = true, [153] = true }
local IDLE_LEN = { OFF = 3354, OLF = 1000, KFF = 3039 }

local cfg = { enabled = true }
local st = { fixed0 = 0, ended3 = 0, last_log = 0, prev = nil }

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
    local name = node and safe(function() return node:call("get_MotionName") end) or "-"
    return {
        bank = safe(function() return layer:call("get_MotionBankID") end) or -1,
        id = safe(function() return layer:call("get_MotionID") end) or -1,
        frame = safe(function() return layer:call("get_Frame") end) or 0,
        name = name,
    }
end

re.on_pre_application_entry("LateUpdateBehavior", function()
    if not cfg.enabled then st.prev = nil; return end
    local p = get_player(); if not p then st.prev = nil; return end
    local mo = component(p, "via.motion.Motion"); if not mo then return end
    local l0 = safe(function() return mo:call("getLayer", 0) end); if not l0 then return end
    local cur = layer_state(l0)
    local prev = st.prev
    if prev and (cur.bank ~= prev.bank or cur.id ~= prev.id) then
        local kind_now, kind_prev = idle_kind(cur.name), idle_kind(prev.name)
        if kind_now and kind_prev then
            local len = IDLE_LEN[kind_now] or 3354
            local target = prev.frame % len
            safe(function() l0:call("set_Frame", target) end)
            local after = safe(function() return l0:call("get_Frame") end) or -1
            st.fixed0 = st.fixed0 + 1
            log_line(string.format("layer0 %s bank %d id %d (frame %.0f) -> bank %d id %d: frame set to %.0f, reads %.0f",
                kind_prev, prev.bank, prev.id, prev.frame, cur.bank, cur.id, target, after))
            cur.frame = after
        end
    end
    st.prev = cur
    -- layer 3: end the raise at once
    local l3 = safe(function() return mo:call("getLayer", 3) end)
    if l3 then
        local s3 = layer_state(l3)
        local kind = idle_kind(s3.name)
        if kind and s3.bank == HOLD_BANK and RAISE_IDS[s3.id] then
            local len = IDLE_LEN[kind] or 3354
            if s3.frame < len - 2 then
                safe(function() l3:call("set_Frame", len - 1) end)
                st.ended3 = st.ended3 + 1
                if st.ended3 <= 20 then log_line(string.format("layer3 raise slot id %d (%s, frame %.0f) -> frame set to %d", s3.id, kind, s3.frame, len - 1)) end
            end
        end
    end
end)

re.on_frame(function()
    local now = os.clock()
    if now - st.last_log < 5.0 then return end
    st.last_log = now
    log_line(string.format("layer0 phase fixes: %d, layer3 raise ended: %d (totals)", st.fixed0, st.ended3))
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral aim idle phase (no restart at the press)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED", cfg.enabled)
    imgui.text(string.format("layer0 fixes=%d layer3 ended=%d", st.fixed0, st.ended3))
    imgui.tree_pop()
end)
