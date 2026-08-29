-- Visceral (RE2 VR) -- unarmed gait while armed (relaxed legs with weapon out)
--
-- Rebuilt from our own AC re2_vr_unarmed_gait.lua for the standalone Visceral
-- codebase. Makes the body/legs use the NO-WEAPON walk/idle animation while a
-- weapon is equipped, so the character doesn't do the twisted armed gait -- the
-- HoldUp motion-variable experiment did nothing, and AC's motlist-swap /
-- weapon-category spoof failed; THIS bank-poison method is what actually worked.
--
-- Mechanism (decoded in AC via the gait probe):
--   * Locomotion plays motion id 160/190/192 from bank id 1000. Which motlist
--     "bank 1000" resolves to depends on the rule
--     (Motion.TargetBankType & bank.BankTypeMaskBit) == bank.BankType.
--     Handgun drawn -> TargetBankType ~287; unarmed -> 0.
--   * Weapon-grip pose is in SEPARATE HOLD/FINGER banks by the same rule, so
--     forcing TargetBankType=0 would also drop the hand grip -- wrong fix.
--   * Fix: POISON only the weapon *_MOVE banks (BankID 1000, name HDG_MOVE_* /
--     STG_MOVE_* / MAG_MOVE_* etc., never CMN_MOVE*) by writing BankType=255 +
--     BankTypeMaskBit=255 -- a combo no real TargetBankType satisfies. Bank
--     1000 then falls through to the CMN_MOVE* (unarmed) family, which keeps its
--     own caution/danger/wet condition matching -> relaxed no-weapon gait, with
--     the weapon still properly gripped (HOLD/FINGER untouched).
--   * The active bank list rebuilds when the equipped weapon changes, so the
--     poison is re-applied on a short poll. Originals kept per name, restored on
--     disable/reset.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_gait]"
local NS = sdk.game_namespace
local VK_NUMPAD5 = 0x65
local VK_NUMPAD6 = 0x66  -- diagnostic dump

local motion_type = sdk.typeof("via.motion.Motion")
local POISON_TYPE, POISON_MASK = 255, 255
local SCAN_INTERVAL_S = 0.25

local cfg = {
    enabled = false,   -- OFF by default: superseded by the aim-state-escape approach (bank-swap was the wrong layer). NUM5 to try it.
    -- true: poison the ENTIRE hold bank (2000) incl CMN_HOLD, so the locomotion
    --   layer falls through to the walk bank (1000) CMN_MOVE = normal careful walk.
    -- false: only poison weapon *_HOLD -> CMN_HOLD (a hold-idle pose; looked like a
    --   sideways shuffle in testing).
    force_move_locomotion = true,
}
local state = {
    keys_ok = true, prev = false,
    status = "idle", last_scan_t = 0.0, poisoned = 0,
    originals = {},        -- [bank Name] = { bank_type, mask }
    setter_ok = nil,
}

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local function get_player_motion()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    local player = safe(function() return pm:call("get_CurrentPlayer") end)
    if not player or not motion_type then return nil end
    return safe(function() return player:call("getComponent(System.Type)", motion_type) end)
end

-- weapon locomotion banks to redirect: the *_MOVE (bank 1000, weapon lowered) AND
-- *_HOLD (bank 2000, weapon held -- what VR actually uses) families, but NEVER
-- the CMN_ (unarmed) fallbacks and NEVER the FINGER/grip bank (10000, untouched
-- so the gun stays gripped).
local function is_weapon_loco_bank(name)
    if type(name) ~= "string" then return false end
    if name:sub(1, 3) == "CMN" then return false end
    return name:find("_MOVE", 1, true) ~= nil or name:find("_HOLD", 1, true) ~= nil
end

local function write_bank(bank, bank_type, mask)
    local ok1 = pcall(function() bank:call("set_BankType", bank_type) end)
    local ok2 = pcall(function() bank:call("set_BankTypeMaskBit", mask) end)
    return ok1 and ok2
end

local function for_each_active_bank(fn)
    local mc = get_player_motion(); if not mc then return false end
    local count = safe(function() return mc:call("getActiveMotionBankCount") end)
    for i = 0, (tonumber(count) or 0) - 1 do
        local bank = safe(function() return mc:call("getActiveMotionBank", i) end)
        if bank then fn(bank) end
    end
    return true
end

local function apply_poison()
    local touched = 0
    local ok = for_each_active_bank(function(bank)
        local bid = safe(function() return bank:call("get_BankID") end)
        if bid ~= 1000 and bid ~= 2000 then return end   -- move + hold locomotion (not FINGER/grip 10000)
        local name = tostring(safe(function() return bank:call("get_Name") end) or "")
        local do_poison
        if bid == 1000 then
            -- weapon *_MOVE only; keep CMN_MOVE eligible (it's the walk we want to land on)
            do_poison = is_weapon_loco_bank(name)
        else -- bid == 2000 (hold locomotion)
            if cfg.force_move_locomotion then
                do_poison = true            -- poison ALL of the hold bank -> fall through to bank 1000 CMN_MOVE
            else
                do_poison = is_weapon_loco_bank(name)  -- weapon HOLD only -> CMN_HOLD
            end
        end
        if not do_poison then return end
        local bt = safe(function() return bank:call("get_BankType") end)
        if bt == POISON_TYPE then return end
        if not state.originals[name] then
            state.originals[name] = { bank_type = bt,
                mask = safe(function() return bank:call("get_BankTypeMaskBit") end) }
        end
        local wrote = write_bank(bank, POISON_TYPE, POISON_MASK)
        if state.setter_ok == nil then state.setter_ok = wrote end
        if wrote then touched = touched + 1 end
    end)
    if not ok then state.status = "no player/motion"; return end
    if state.setter_ok == false then
        state.status = "set_BankType not writable -- approach dead"; return
    end
    if touched > 0 then
        state.poisoned = state.poisoned + touched
        state.status = string.format("active (%d banks redirected)", state.poisoned)
    elseif state.status == "idle" then
        state.status = "active (nothing to redirect yet)"
    end
    if state.status ~= state.last_logged then
        state.last_logged = state.status
        log_line("status: " .. state.status)
    end
end

local function restore_originals()
    for_each_active_bank(function(bank)
        local bid = safe(function() return bank:call("get_BankID") end)
        if bid ~= 1000 and bid ~= 2000 then return end
        local name = tostring(safe(function() return bank:call("get_Name") end) or "")
        local orig = state.originals[name]
        if orig and safe(function() return bank:call("get_BankType") end) == POISON_TYPE then
            write_bank(bank, orig.bank_type, orig.mask)
        end
    end)
    state.status = "restored/disabled"; state.poisoned = 0
end

-- diagnostic: log the full live active-bank list so we can see what's there
local function dump_banks()
    local mc = get_player_motion()
    if not mc then log_line("DUMP: no player/motion component"); return end
    local count = safe(function() return mc:call("getActiveMotionBankCount") end)
    local tbt = safe(function() return mc:call("get_TargetBankType") end)
    log_line(string.format("DUMP: activeBankCount=%s  TargetBankType=%s", tostring(count), tostring(tbt)))
    local n = tonumber(count) or 0
    local shown = 0
    for i = 0, n - 1 do
        local bank = safe(function() return mc:call("getActiveMotionBank", i) end)
        if bank then
            local bid = safe(function() return bank:call("get_BankID") end)
            local name = tostring(safe(function() return bank:call("get_Name") end) or "?")
            local bt = safe(function() return bank:call("get_BankType") end)
            local mask = safe(function() return bank:call("get_BankTypeMaskBit") end)
            -- show ALL banks this pass (find where the playing bank-2000 HG_ legs come from)
            log_line(string.format("  [%d] id=%s type=%s mask=%s  %s",
                i, tostring(bid), tostring(bt), tostring(mask), name))
            shown = shown + 1
        end
    end
    log_line("DUMP: " .. shown .. " MOVE/id1000 banks shown of " .. n .. " total")

    -- what is ACTUALLY playing per layer (the resolved motion name = the truth)
    local lc = safe(function() return mc:call("getLayerCount") end)
        or safe(function() return mc:call("get_LayerCount") end)
    local maxl = tonumber(lc) or 8
    log_line("DUMP layers (playing motion per layer), layerCount=" .. tostring(lc))
    for li = 0, maxl - 1 do
        local layer = safe(function() return mc:call("getLayer", li) end)
        if layer then
            local lw = safe(function() return layer:call("get_Weight") end)
            local node = safe(function() return layer:call("get_HighestWeightMotionNode") end)
            if node then
                local nm = safe(function() return node:call("get_MotionName") end)
                local w = safe(function() return node:call("get_Weight") end)
                local bank = safe(function() return node:call("get_MotionBankID") end)
                local mid = safe(function() return node:call("get_MotionID") end)
                if nm then
                    log_line(string.format("  L%d lw=%s  name=%s w=%.2f bank=%s id=%s",
                        li, tostring(lw), tostring(nm), tonumber(w) or -1, tostring(bank), tostring(mid)))
                end
            end
        end
    end
end

local function cinematic()
    return type(_G.__visceral_cinematic_blocking) == "function" and _G.__visceral_cinematic_blocking() == true
end

re.on_pre_application_entry("LateUpdateBehavior", function()
    if not cfg.enabled then return end
    -- during cutscenes / enemy grab (3rd person), stop redirecting and restore
    if cinematic() then
        if state.poisoned > 0 then restore_originals(); state.status = "paused (cinematic)" end
        return
    end
    local now = os.clock()
    if now - state.last_scan_t < SCAN_INTERVAL_S then return end
    state.last_scan_t = now
    apply_poison()
end)

re.on_frame(function()
    if not state.keys_ok then return end
    local d = safe(function() return reframework:is_key_down(VK_NUMPAD5) end)
    if d == nil then state.keys_ok = false; return end
    if d and not state.prev then
        cfg.enabled = not cfg.enabled
        if not cfg.enabled then restore_originals() end
        log_line("enabled=" .. tostring(cfg.enabled))
    end
    state.prev = d
    local d6 = safe(function() return reframework:is_key_down(VK_NUMPAD6) end)
    if d6 and not state.prev6 then dump_banks() end
    state.prev6 = d6
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: unarmed gait (relaxed legs while armed)") then return end
    local c, v = imgui.checkbox("Use no-weapon walk/idle legs while a weapon is equipped (NUM5)", cfg.enabled)
    if c then cfg.enabled = v; if not v then restore_originals() end end
    local c2, v2 = imgui.checkbox("force MOVE locomotion (poison whole hold bank -> normal walk)", cfg.force_move_locomotion)
    if c2 then cfg.force_move_locomotion = v2; restore_originals() end
    imgui.text("status: " .. state.status)
    imgui.text("Weapon grip/fingers unaffected; caution/danger/wet gait variants still work.")
    imgui.tree_pop()
end)

re.on_script_reset(function() restore_originals() end)
log_line("loaded (unarmed gait while armed). NUM5 toggles.")
