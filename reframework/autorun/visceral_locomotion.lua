-- Visceral (RE2 VR) -- locomotion prototype, v4 (collision-safe aim speed, de-jittered)
--
-- v3 amplified the game's own per-frame movement while aiming (collision-safe, no
-- clipping). But near a wall/door it JITTERED: the game shoves you back out of the
-- wall (a corrective delta), and v3 amplified that bounce too -> fore/aft
-- oscillation.
--
-- v4 fix: only amplify the part of the game's movement that goes the way you're
-- PUSHING (the camera-relative stick direction). The collision push-back is
-- opposite to your intent, so it is NOT amplified -> no jitter. Plus light EMA
-- smoothing on the added amount.
--   intended = camera-relative stick direction (unit)
--   forward  = dot(native_delta, intended)          -- how much of the game's move
--                                                       went the way you asked
--   extra    = intended * max(forward,0) * (mult-1)  -- amplify only forward intent
-- Against a wall forward -> 0 (game blocked you), so extra -> 0 = no clip, no push.
-- Non-aim movement stays vanilla.
--
-- Hotkeys: NUM1 enable   NUM7 mult -0.1   NUM9 mult +0.1   NUM0 panic

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_loco]"
local NS = sdk.game_namespace
local VK_NUMPAD0, VK_NUMPAD1, VK_NUMPAD7, VK_NUMPAD9 = 0x60, 0x61, 0x67, 0x69

local cfg = {
    enabled = false,
    aim_speed_mult = 1.5,
    smoothing = 0.4,   -- EMA on the added amount (0 = none, higher = smoother/laggier)
}

local state = {
    keys_ok = true, prev_keys = {},
    have_last = false, last_x = 0, last_y = 0, last_z = 0,
    ema_fwd = 0.0,
    ui_player = false, ui_aiming = false, ui_input = "none",
    ui_native = 0.0, ui_fwd = 0.0,
}

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end
local gamepad_t = sdk.find_type_definition("via.hid.GamePad")

local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end
local function get_is_aiming(player)
    local t = sdk.typeof(NS("survivor.SurvivorCondition")); if not t then return false end
    local c = safe(function() return player:call("getComponent(System.Type)", t) end); if not c then return false end
    return safe(function() return c:call("get_IsHold") end) == true
end
local function get_transform(player) return safe(function() return player:call("get_Transform") end) end
local function get_pos(tr)
    local p = safe(function() return tr:call("get_Position") end); if not p then return nil end
    return safe(function() return p.x end), safe(function() return p.y end), safe(function() return p.z end)
end

local function get_left_axis()
    if _G.vrmod then
        local using = safe(function() return vrmod:is_using_controllers() end)
        if using then
            local a = safe(function() return vrmod:get_left_stick_axis() end)
            if a and safe(function() return a:length() end) and a:length() > 0.0 then state.ui_input = "vr"; return a end
        end
    end
    local gp = sdk.get_native_singleton("via.hid.GamePad")
    if not gp or not gamepad_t then return nil end
    local pad = safe(function() return sdk.call_native_func(gp, gamepad_t, "get_LastInputDevice") end)
    if not pad then return nil end
    local ax = safe(function() return pad:call("get_AxisL") end)
    if ax then state.ui_input = "pad" end
    return ax
end

-- camera-relative unit direction the stick is pushing (xz)
local function intended_dir(sx, sy)
    local cam = sdk.get_primary_camera(); if not cam then return nil end
    local cgo = safe(function() return cam:call("get_GameObject") end); if not cgo then return nil end
    local ctr = safe(function() return cgo:call("get_Transform") end); if not ctr then return nil end
    local crot = safe(function() return ctr:call("get_Rotation") end); if not crot then return nil end
    local fwd = safe(function() return crot * Vector3f.new(0, 0, 1) end); if not fwd then return nil end
    local flat = Vector3f.new(fwd.x, 0.0, fwd.z); if flat:length() <= 0.0 then return nil end
    flat = flat:normalized()
    local move = flat:to_quat() * Vector3f.new(sx, 0.0, -sy)
    if move:length() <= 0.0 then return nil end
    move = move:normalized()
    return move.x, move.z
end

local function tick()
    local player = get_player()
    state.ui_player = player ~= nil
    if not player then state.have_last = false; return end
    local tr = get_transform(player); if not tr then state.have_last = false; return end
    local x, y, z = get_pos(tr); if x == nil then state.have_last = false; return end
    local aiming = get_is_aiming(player); state.ui_aiming = aiming

    if not (cfg.enabled and aiming) then
        state.last_x, state.last_y, state.last_z = x, y, z
        state.have_last = true; state.ema_fwd = 0.0; state.ui_native = 0.0; state.ui_fwd = 0.0
        return
    end
    if not state.have_last then
        state.last_x, state.last_y, state.last_z = x, y, z; state.have_last = true; return
    end

    local dx, dz = x - state.last_x, z - state.last_z
    local native = math.sqrt(dx * dx + dz * dz)
    state.ui_native = native
    if native > 5.0 then state.last_x, state.last_y, state.last_z = x, y, z; return end   -- teleport guard

    local ax = get_left_axis()
    local sx = ax and safe(function() return ax.x end) or 0.0
    local sy = ax and safe(function() return ax.y end) or 0.0
    local ix, iz = intended_dir(sx, sy)

    local extra = cfg.aim_speed_mult - 1.0
    if extra <= 0.0 or not ix then
        state.last_x, state.last_y, state.last_z = x, y, z; return
    end

    -- forward = component of the game's move along the intended direction
    local forward = dx * ix + dz * iz
    if forward < 0.0 then forward = 0.0 end            -- never amplify the push-back
    -- EMA-smooth the amplified amount to kill residual oscillation
    state.ema_fwd = state.ema_fwd + (forward - state.ema_fwd) * (1.0 - cfg.smoothing)
    state.ui_fwd = state.ema_fwd

    local add = state.ema_fwd * extra
    local nx = x + ix * add
    local nz = z + iz * add
    local ok = safe(function() tr:call("set_Position", Vector3f.new(nx, y, nz)); return true end)
    if ok then state.last_x, state.last_y, state.last_z = nx, y, nz
    else state.last_x, state.last_y, state.last_z = x, y, z end
end

re.on_frame(function()
    tick()
    if not state.keys_ok then return end
    local function pressed(vk)
        local d = safe(function() return reframework:is_key_down(vk) end)
        if d == nil then state.keys_ok = false; return false end
        local w = state.prev_keys[vk]; state.prev_keys[vk] = d; return d and not w
    end
    if pressed(VK_NUMPAD1) then cfg.enabled = not cfg.enabled; log_line("enabled=" .. tostring(cfg.enabled)) end
    if pressed(VK_NUMPAD7) then cfg.aim_speed_mult = math.max(1.0, cfg.aim_speed_mult - 0.1) end
    if pressed(VK_NUMPAD9) then cfg.aim_speed_mult = math.min(5.0, cfg.aim_speed_mult + 0.1) end
    if pressed(VK_NUMPAD0) then cfg.enabled = false; log_line("PANIC off") end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: locomotion (v4, smooth collision-safe aim speed)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED (amplify aim-walk, collision-safe)", cfg.enabled)
    ch, cfg.aim_speed_mult = imgui.slider_float("aim speed multiplier", cfg.aim_speed_mult, 1.0, 5.0, "%.2fx")
    ch, cfg.smoothing = imgui.slider_float("smoothing (higher = smoother)", cfg.smoothing, 0.0, 0.9, "%.2f")
    imgui.text("hotkeys: NUM1=enable NUM7=-0.1 NUM9=+0.1 NUM0=panic"
        .. (state.keys_ok and "" or "  [KEYS UNAVAILABLE]"))
    imgui.separator()
    imgui.text("player: " .. (state.ui_player and "found" or "NO") .. "  aiming: " .. tostring(state.ui_aiming)
        .. "  input: " .. state.ui_input)
    imgui.text(string.format("game move: %.4f   forward(EMA): %.4f   mult %.2fx",
        state.ui_native, state.ui_fwd, cfg.aim_speed_mult))
    imgui.text("Only amplifies motion along your stick push -> collision push-back")
    imgui.text("is not amplified, so no wall jitter. Non-aim movement is vanilla.")
    if imgui.button("PANIC off") then cfg.enabled = false end
    imgui.tree_pop()
end)

log_line("loaded (v4 smooth collision-safe aim speed). NUM1 enable; tune multiplier + smoothing live.")
