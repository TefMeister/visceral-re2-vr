-- Visceral (RE2 VR) -- foot grounding: kill the aim-pose hover
--
-- The braced aim pose floats the body ~10cm because VR anchors the head to the
-- HMD and the braced legs leave the feet off the floor. The headset test proved
-- lowering the body plants the feet and bends the knees (the game's own leg IK).
-- This does that automatically: while aiming, lower the PELVIS bone by a tunable
-- amount so the feet come down to the floor. The braced pose stays; it just sits
-- onto the ground.
--
-- IMPORTANT: this lowers the SKELETON (pelvis + everything hanging off it), NOT
-- the world position and NOT the arms -- the hands/gun are pinned to the VR
-- controllers, so the muzzle (and thus where bullets go) is unaffected.
--
-- Applied while aiming (IsHold) only, at the pre-IK hook so it composes with the
-- game's leg IK (which should re-plant the feet + bend knees). Reversible.
--
-- Hotkeys: NUM2 = toggle    slider = drop amount (metres)

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_ground]"
local NS = sdk.game_namespace
local VK_NUMPAD2 = 0x62

-- pelvis/root candidates (first one that exists is used)
local PELVIS_CANDIDATES = { "pelvis", "hips", "hip", "cog", "waist", "root" }

local cfg = {
    enabled = true,       -- CONFIRMED good (2026-08-30, Claire + Leon): feet plant, view fine, bullets true
    drop = 0.175,         -- user-tuned sweet spot: feet touch ground while walking + looking down
    only_when_aiming = true,
}

local state = {
    keys_ok = true, prev = false,
    pelvis_name = nil, found = false,
    ui_aiming = "n/a", status = "idle",
}

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end
local function get_tf(player) return safe(function() return player:call("get_Transform") end) end
local function get_joint(tf, name) return safe(function() return tf:call("getJointByName", name) end) end
local function is_aiming(player)
    local t = sdk.typeof(NS("survivor.SurvivorCondition")); if not t then return false end
    local c = safe(function() return player:call("getComponent(System.Type)", t) end); if not c then return false end
    return safe(function() return c:call("get_IsHold") end) == true
end

local function find_pelvis(tf)
    for _, n in ipairs(PELVIS_CANDIDATES) do
        if get_joint(tf, n) then state.pelvis_name = n; state.found = true; log_line("pelvis joint = " .. n); return n end
    end
    state.found = false
    return nil
end

local function apply(player)
    local tf = get_tf(player); if not tf then return end
    if not state.pelvis_name then find_pelvis(tf) end
    local joint = state.pelvis_name and get_joint(tf, state.pelvis_name) or nil
    if not joint then state.status = "no pelvis joint"; return end
    local p = safe(function() return joint:call("get_Position") end)
    if not p then state.status = "no get_Position"; return end
    local x = safe(function() return p.x end); local y = safe(function() return p.y end); local z = safe(function() return p.z end)
    if x == nil then return end
    local ok = safe(function() joint:call("set_Position", Vector3f.new(x, y - cfg.drop, z)); return true end)
    state.status = ok and string.format("dropping pelvis %.3fm", cfg.drop) or "set_Position failed"
end

local function cinematic()
    return type(_G.__visceral_cinematic_blocking) == "function" and _G.__visceral_cinematic_blocking() == true
end

re.on_pre_application_entry("LateUpdateBehavior", function()
    if not cfg.enabled then return end
    if cinematic() then state.status = "paused (cinematic)"; return end   -- don't drop the body during cutscenes/grab
    local player = get_player(); if not player then return end
    local aiming = is_aiming(player); state.ui_aiming = tostring(aiming)
    if cfg.only_when_aiming and not aiming then return end
    apply(player)
end)

re.on_frame(function()
    if not state.keys_ok then return end
    local d = safe(function() return reframework:is_key_down(VK_NUMPAD2) end)
    if d == nil then state.keys_ok = false; return end
    if d and not state.prev then cfg.enabled = not cfg.enabled; log_line("enabled=" .. tostring(cfg.enabled)) end
    state.prev = d
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: foot grounding (kill aim hover)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED (NUM2)", cfg.enabled)
    ch, cfg.drop = imgui.slider_float("pelvis drop (m)", cfg.drop, 0.0, 0.30, "%.3f")
    ch, cfg.only_when_aiming = imgui.checkbox("only while aiming", cfg.only_when_aiming)
    imgui.separator()
    imgui.text("aiming: " .. state.ui_aiming .. "   pelvis: " .. (state.found and state.pelvis_name or "NOT FOUND"))
    imgui.text("status: " .. state.status)
    imgui.text("Lowers the pelvis/skeleton only -- hands, gun & aim are VR-pinned, so")
    imgui.text("bullets are unaffected. Watch: do feet plant + knees bend, and does your")
    imgui.text("view stay put (camera = HMD) or dip down (camera = body head)?")
    imgui.tree_pop()
end)

log_line("loaded (foot grounding). NUM2 toggle; tune the drop slider while aiming.")
