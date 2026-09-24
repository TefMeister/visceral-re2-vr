-- Visceral (RE2 VR) -- stop the game "directing" the body while aiming (2026-09-24)
--
-- Why: the aim-state diff probe shows app.ropeway.survivor.SurvivorCondition.IsDirectingBody flip
-- false -> true at every RG press (2026-09-24, run 9), alongside the hold flags. "Directing body" is the
-- hold-state control that steers the character's body toward the aim direction (the third-person aim
-- facing). With the ordinary walk/idle already in the hold bank, this flag is the next candidate for
-- the "stiff and tense" body and the careful, straight-legged steps while aim-walking.
-- Lever: post-hook on get_IsDirectingBody, answering false while aiming. Logs how often it answered
-- and what the game would have said. NUM6 toggles. Read-only otherwise.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_direct]"
local NS = sdk.game_namespace
local VK_NUMPAD6 = 0x66

local cfg = { enabled = true }
local st = { calls = 0, overridden = 0, game_true = 0, last_log = 0, prev_key = false, keys_ok = true }

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

local player_cond = nil
local function is_players(cond)
    if player_cond and cond == player_cond then return true end
    local p = get_player(); if not p then return false end
    local c = component(p, NS("survivor.SurvivorCondition"))
    if c then player_cond = c end
    return c ~= nil and cond == c
end

local pending = nil   -- set by the pre-hook when the call is the player's, read by the post-hook
local function install()
    local t = sdk.find_type_definition(NS("survivor.SurvivorCondition"))
    if not t then log_line("type not found"); return end
    local hooked = 0
    for _, m in ipairs(t:get_methods() or {}) do
        if safe(function() return m:get_name() end) == "get_IsDirectingBody" then
            pcall(function()
                sdk.hook(m,
                    function(args)
                        st.calls = st.calls + 1
                        pending = nil
                        if not cfg.enabled then return end
                        local cond = sdk.to_managed_object(args[2]); if not cond or not is_players(cond) then return end
                        pending = cond
                    end,
                    function(rv)
                        if not pending then return rv end
                        local cond = pending; pending = nil
                        local game_says = (sdk.to_int64(rv) & 0xFF) ~= 0
                        if game_says then st.game_true = st.game_true + 1 end
                        local hold = safe(function() return cond:call("get_IsHold") end) == true
                        if hold and game_says then st.overridden = st.overridden + 1; return sdk.to_ptr(0) end
                        return rv
                    end)
            end)
            hooked = hooked + 1
        end
    end
    log_line("hooked get_IsDirectingBody x" .. hooked)
end
install()

re.on_frame(function()
    if st.keys_ok then
        local d = safe(function() return reframework:is_key_down(VK_NUMPAD6) end)
        if d == nil then st.keys_ok = false
        elseif d and not st.prev_key then cfg.enabled = not cfg.enabled; log_line("enabled=" .. tostring(cfg.enabled)) end
        st.prev_key = d == true
    end
    local now = os.clock()
    if now - st.last_log < 1.0 then return end
    st.last_log = now
    log_line(string.format("calls/s=%d game_true/s=%d overridden/s=%d", st.calls, st.game_true, st.overridden))
    st.calls, st.game_true, st.overridden = 0, 0, 0
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral body direct (off while aiming)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED (NUM6)", cfg.enabled)
    imgui.tree_pop()
end)
