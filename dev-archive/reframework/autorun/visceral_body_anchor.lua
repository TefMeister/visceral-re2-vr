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

local cfg = { enabled = true, keep_default_shape = true, keep_relaxed_offset = true }
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
        elseif n == "register" then
            -- the hold state asks for its own capsule SHAPE (category Hold=2: other radius/height/offset);
            -- with the anchor kept on the joint the leftover offset still pushed the body ~5 cm forward
            -- (Tefa, run 10). Drop the Hold shape request so the Default shape stays while aiming.
            pcall(function()
                sdk.hook(m, function(args)
                    st.reg_calls = (st.reg_calls or 0) + 1
                    if not cfg.enabled or not cfg.keep_default_shape then return end
                    local ctrl = sdk.to_managed_object(args[2]); if not ctrl or not is_players(ctrl) then return end
                    local req = sdk.to_managed_object(args[3]); if not req then return end
                    local cat = safe(function() return req:get_field("_ShapeCategory") end)
                    if cat == 2 then st.reg_dropped = (st.reg_dropped or 0) + 1; return sdk.PreHookResult.SKIP_ORIGINAL end
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
                    -- run 11: even with the Hold shape request dropped, the capsule's anchor JOINT and
                    -- OFFSET still change while aiming (local offset (0.08, 1.24, -0.15) vs (0.06, 0, 0.06)):
                    -- the body sits ~5 cm off. Remember the relaxed joint + offset and re-apply them while aiming.
                    if cfg.keep_relaxed_offset then
                        local p = get_player()
                        local hold = p and is_aiming(p)
                        local joint = safe(function() return ctrl:get_field("<ConstJoint>k__BackingField") end)
                        local off = safe(function() return ctrl:get_field("<Offset>k__BackingField") end)
                        if not hold then
                            if joint then st.relaxed_joint = joint end
                            if off then
                                local c = safe(function() return off:get_field("<Current>k__BackingField") end)
                                local t = safe(function() return off:get_field("_Target") end)
                                if c and t then st.relaxed_off = { c = Vector3f.new(c.x, c.y, c.z), t = Vector3f.new(t.x, t.y, t.z) } end
                            end
                        elseif st.relaxed_joint and st.relaxed_off then
                            if joint ~= st.relaxed_joint then
                                safe(function() ctrl:set_field("<ConstJoint>k__BackingField", st.relaxed_joint) end)
                                st.joint_fixed = (st.joint_fixed or 0) + 1
                            end
                            if off then
                                safe(function() off:call("set_Target", st.relaxed_off.t) end)
                                safe(function() off:call("set_Current", st.relaxed_off.c) end)
                                st.off_fixed = (st.off_fixed or 0) + 1
                            end
                        end
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
    local joint = safe(function() return c:get_field("<ConstJoint>k__BackingField") end)
    local jname = joint and safe(function() return joint:call("get_Name") end) or "?"
    log_line(string.format("hold=%d OffsetType=%s joint=%s localOffset=(%s) setter %d/%d fixed, field fixed %d, shape requests %d (hold dropped %d), joint fixed %d, offset fixed %d",
        is_aiming(p) and 1 or 0, tostring(ot), tostring(jname),
        lo and string.format("%.3f %.3f %.3f", lo.x, lo.y, lo.z) or "?",
        st.setter_fixed, st.setter_hits, st.field_fixed, st.reg_calls or 0, st.reg_dropped or 0, st.joint_fixed or 0, st.off_fixed or 0))
    st.setter_hits, st.setter_fixed, st.field_fixed, st.reg_calls, st.reg_dropped, st.joint_fixed, st.off_fixed = 0, 0, 0, 0, 0, 0, 0
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral body anchor (keep Joint while aiming)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED (NUM5)", cfg.enabled)
    ch, cfg.keep_default_shape = imgui.checkbox("keep the Default capsule shape while aiming (drop Hold shape requests)", cfg.keep_default_shape)
    ch, cfg.keep_relaxed_offset = imgui.checkbox("keep the relaxed anchor joint + offset while aiming", cfg.keep_relaxed_offset)
    imgui.tree_pop()
end)
