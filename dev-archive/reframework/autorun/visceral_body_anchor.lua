-- Visceral (RE2 VR) -- keep the body anchor on the JOINT while aiming (2026-09-24)
--
-- Why: the aim-state diff probe shows exactly one body setting flipping at every RG press:
-- app.ropeway.survivor.SurvivorCharacterController.OffsetType goes Joint(0) -> CameraY(3) when aiming
-- and back on release (2026-09-24, 13 presses). CameraY anchors the character's capsule/body offset to
-- the CAMERA (the third-person shoulder offset); in VR the camera is the headset, so the body shifts at
-- the press and keeps being pulled after the head while aiming: the "twitch through the whole body" and
-- the "stiff and tense" feel. This keeps the anchor on the joint while aiming.
-- Two routes, both used: a pre-hook on set_OffsetType (rewrites CameraY -> Joint) and, in case the game
-- writes the field directly, a pre-hook on updateCharacterController that resets the field.
-- Logs OffsetType, the local offset and how often each route fired, once a second. NUM5 toggles.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_anchor]"
local NS = sdk.game_namespace
local VK_NUMPAD5 = 0x65
local JOINT, CAMERA_Y = 0, 3

local cfg = { enabled = true }
local st = { setter_hits = 0, setter_fixed = 0, field_fixed = 0, last_log = 0, prev_key = false, keys_ok = true }

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
local function is_aiming(player)
    local c = component(player, NS("survivor.SurvivorCondition")); if not c then return false end
    return safe(function() return c:call("get_IsHold") end) == true
end

local player_ctrl = nil   -- the player's own controller (enemies/NPCs have theirs too)
local function is_players(ctrl)
    if player_ctrl and ctrl == player_ctrl then return true end
    local p = get_player(); if not p then return false end
    local c = component(p, NS("survivor.SurvivorCharacterController"))
    if c then player_ctrl = c end
    return c ~= nil and ctrl == c
end

local function install()
    local t = sdk.find_type_definition(NS("survivor.SurvivorCharacterController"))
    if not t then log_line("type not found"); return end
    local hooked = 0
    for _, m in ipairs(t:get_methods() or {}) do
        local n = safe(function() return m:get_name() end)
        if n == "set_OffsetType" then
            pcall(function()
                sdk.hook(m, function(args)
                    st.setter_hits = st.setter_hits + 1
                    if not cfg.enabled then return end
                    local ctrl = sdk.to_managed_object(args[2]); if not ctrl or not is_players(ctrl) then return end
                    local v = sdk.to_int64(args[3])
                    if v == CAMERA_Y then args[3] = sdk.to_ptr(JOINT); st.setter_fixed = st.setter_fixed + 1 end
                end, function(rv) return rv end)
            end)
            hooked = hooked + 1
        elseif n == "updateCharacterController" then
            pcall(function()
                sdk.hook(m, function(args)
                    if not cfg.enabled then return end
                    local ctrl = sdk.to_managed_object(args[2]); if not ctrl or not is_players(ctrl) then return end
                    local v = safe(function() return ctrl:get_field("<OffsetType>k__BackingField") end)
                    if v == CAMERA_Y then
                        safe(function() ctrl:set_field("<OffsetType>k__BackingField", JOINT) end)
                        st.field_fixed = st.field_fixed + 1
                    end
                end, function(rv) return rv end)
            end)
            hooked = hooked + 1
        end
    end
    log_line("hooked " .. hooked .. " method(s)")
end
install()

re.on_frame(function()
    if st.keys_ok then
        local d = safe(function() return reframework:is_key_down(VK_NUMPAD5) end)
        if d == nil then st.keys_ok = false
        elseif d and not st.prev_key then cfg.enabled = not cfg.enabled; log_line("enabled=" .. tostring(cfg.enabled)) end
        st.prev_key = d == true
    end
    local now = os.clock()
    if now - st.last_log < 1.0 then return end
    st.last_log = now
    local p = get_player(); if not p then return end
    local c = component(p, NS("survivor.SurvivorCharacterController")); if not c then return end
    local ot = safe(function() return c:call("get_OffsetType") end)
    local lo = safe(function() return c:call("getLocalOffsetPosition") end)
    log_line(string.format("hold=%d OffsetType=%s localOffset=(%s) setter %d/%d fixed, field fixed %d",
        is_aiming(p) and 1 or 0, tostring(ot),
        lo and string.format("%.3f %.3f %.3f", lo.x, lo.y, lo.z) or "?",
        st.setter_fixed, st.setter_hits, st.field_fixed))
    st.setter_hits, st.setter_fixed, st.field_fixed = 0, 0, 0
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral body anchor (keep Joint while aiming)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED (NUM5)", cfg.enabled)
    imgui.tree_pop()
end)
