-- Visceral (RE2 VR) -- locomotion prototype, v3 (collision-safe aim speed-up)
--
-- The AC system (position override while aiming) felt great but CLIPPED through
-- doors, because it wrote an absolute target position that ignored walls.
-- Jog-style kept collision but ran instantly (no analog) and couldn't touch aim.
--
-- v3 keeps the game's OWN movement system (analog + collision intact) and simply
-- AMPLIFIES it while aiming:
--   native_delta = how far the GAME actually moved you this frame (already
--     collision-resolved -> it's ~0 when you're against a wall/door).
--   extra = native_delta * (mult - 1);  new_pos = current + extra.
-- So total per-frame movement = native_delta * mult, in the game's own
-- collision-safe direction. Against a wall native_delta is 0, so extra is 0 ->
-- NO CLIPPING (we never add motion the game already blocked). Analog is
-- preserved because native_delta scales with stick push. Non-aim movement is
-- left completely vanilla.
--
-- `AIM_SPEED_MULT` is tunable live (start 1.5). Only applied while aiming.
--
-- Hotkeys: NUM1 enable/disable   NUM7 mult -0.1   NUM9 mult +0.1   NUM0 panic

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_loco]"
local NS = sdk.game_namespace
local VK_NUMPAD0, VK_NUMPAD1, VK_NUMPAD7, VK_NUMPAD9 = 0x60, 0x61, 0x67, 0x69

local cfg = {
    enabled = false,
    aim_speed_mult = 1.5,   -- multiply the game's own aim-walk movement; tune live
}

local state = {
    keys_ok = true, prev_keys = {},
    have_last = false, last_x = 0, last_y = 0, last_z = 0,
    ui_player = false, ui_aiming = false,
    ui_native = 0.0, ui_result = 0.0,
}

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

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

local function tick()
    local player = get_player()
    state.ui_player = player ~= nil
    if not player then state.have_last = false; return end
    local tr = get_transform(player); if not tr then state.have_last = false; return end
    local x, y, z = get_pos(tr); if x == nil then state.have_last = false; return end

    local aiming = get_is_aiming(player); state.ui_aiming = aiming

    -- only amplify while enabled AND aiming; otherwise just track position
    if not (cfg.enabled and aiming) then
        state.last_x, state.last_y, state.last_z = x, y, z
        state.have_last = true
        state.ui_native = 0.0; state.ui_result = 0.0
        return
    end

    if not state.have_last then
        state.last_x, state.last_y, state.last_z = x, y, z
        state.have_last = true
        return
    end

    -- how far the GAME moved us since we last set position (collision-resolved)
    local dx, dz = x - state.last_x, z - state.last_z
    local native = math.sqrt(dx * dx + dz * dz)
    state.ui_native = native

    -- guard against teleports/loads
    if native > 5.0 then
        state.last_x, state.last_y, state.last_z = x, y, z
        return
    end

    local extra = cfg.aim_speed_mult - 1.0
    if extra <= 0.0 or native <= 0.0 then
        state.last_x, state.last_y, state.last_z = x, y, z
        state.ui_result = native
        return
    end

    local nx = x + dx * extra
    local nz = z + dz * extra
    local ok = safe(function() tr:call("set_Position", Vector3f.new(nx, y, nz)); return true end)
    if ok then
        state.last_x, state.last_y, state.last_z = nx, y, nz
        state.ui_result = native * cfg.aim_speed_mult
    else
        state.last_x, state.last_y, state.last_z = x, y, z
    end
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
    if pressed(VK_NUMPAD7) then cfg.aim_speed_mult = math.max(1.0, cfg.aim_speed_mult - 0.1); log_line("mult=" .. string.format("%.2f", cfg.aim_speed_mult)) end
    if pressed(VK_NUMPAD9) then cfg.aim_speed_mult = math.min(5.0, cfg.aim_speed_mult + 0.1); log_line("mult=" .. string.format("%.2f", cfg.aim_speed_mult)) end
    if pressed(VK_NUMPAD0) then cfg.enabled = false; log_line("PANIC off") end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: locomotion (v3, collision-safe aim speed)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED (amplify aim-walk, collision-safe)", cfg.enabled)
    ch, cfg.aim_speed_mult = imgui.slider_float("aim speed multiplier", cfg.aim_speed_mult, 1.0, 5.0, "%.2fx")
    imgui.text("hotkeys: NUM1=enable  NUM7=-0.1  NUM9=+0.1  NUM0=panic"
        .. (state.keys_ok and "" or "  [KEYS UNAVAILABLE - use controls]"))
    imgui.separator()
    imgui.text("player: " .. (state.ui_player and "found" or "NO") .. "   aiming: " .. tostring(state.ui_aiming))
    imgui.text(string.format("game move/frame: %.4f   amplified: %.4f   (mult %.2fx)",
        state.ui_native, state.ui_result, cfg.aim_speed_mult))
    imgui.text("Only affects AIMING. Non-aim movement is vanilla. Against walls the")
    imgui.text("game move is 0, so nothing is added -> no clipping.")
    if imgui.button("PANIC off") then cfg.enabled = false end
    imgui.tree_pop()
end)

log_line("loaded (v3 collision-safe aim speed). NUM1 enable; tune the multiplier live.")
