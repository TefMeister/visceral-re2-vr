-- Visceral (RE2 VR) -- cinematic / no-control gate (shared)
--
-- Publishes _G.__visceral_cinematic_blocking() -> true when the player is NOT in
-- normal first-person control: cutscenes, scripted camera events, and (best
-- effort) enemy grabs where the view jumps to 3rd person. Other Visceral scripts
-- (spine straighten, unarmed gait, ...) call it to switch their body overrides
-- OFF during these moments so the twist/gait hacks don't corrupt cinematic or
-- 3rd-person shots.
--
-- Rebuilt from our own AC re2_vr_holster.lua is_cinematic_blocking(): checks
-- TimelineEventManager.InCameraEvent, PlayerCondition.IsEvent,
-- CameraSystem.BusyCameraType == EVENT(6) (ACTION=5 is normal aim -- do NOT
-- block on it), and CutSceneManager.TimelineOwnerObject. Plus a grab/reaction
-- check for the enemy-grab 3rd-person case.

if reframework:get_game_name() ~= "re2" then
    return
end

local NS = sdk.game_namespace
local CAM_EVENT = 6   -- BusyCameraType EVENT (ACTION=5 = normal aim, not blocking)

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function bool_call(obj, getter)
    if not obj then return false end
    return safe(function() return obj:call(getter) end) == true
end
local function enum_val(o)
    if o == nil then return nil end
    if type(o) == "number" then return o end
    return safe(function() return o:get_field("value__") end)
end

local function get_player_condition()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayerCondition") end)
end

local last = { blocking = false, reason = "", t = 0.0 }

local function is_cinematic_blocking()
    -- scripted camera / timeline event
    local tem = sdk.get_managed_singleton("app.ropeway.gamemastering.TimelineEventManager")
    if tem and bool_call(tem, "get_InCameraEvent") then last.reason = "InCameraEvent"; return true end

    local cond = get_player_condition()
    if cond then
        -- cutscene/event control
        if bool_call(cond, "get_IsEvent") then last.reason = "IsEvent"; return true end
        -- enemy grab / damage reaction (3rd-person-ish): best-effort, only if the
        -- getter exists on this build (all pcall'd, so harmless if absent)
        if bool_call(cond, "get_IsDamage") then last.reason = "IsDamage/grab"; return true end
        if bool_call(cond, "get_IsCaught") then last.reason = "IsCaught/grab"; return true end
    end

    local cam = sdk.get_managed_singleton("app.ropeway.camera.CameraSystem")
    if cam then
        local n = enum_val(safe(function() return cam:call("get_BusyCameraType") end))
        if n == CAM_EVENT then last.reason = "BusyCamera=EVENT"; return true end
    end

    local csm = sdk.get_managed_singleton("app.ropeway.CutSceneManager")
    if csm and safe(function() return csm:call("get_TimelineOwnerObject") end) ~= nil then
        last.reason = "CutSceneOwner"; return true
    end

    last.reason = ""
    return false
end

-- cache within a frame so many callers don't each re-query the singletons
local function cached_blocking()
    local now = os.clock()
    if now - last.t > 0.03 then
        last.t = now
        last.blocking = is_cinematic_blocking()
    end
    return last.blocking
end

rawset(_G, "__visceral_cinematic_blocking", cached_blocking)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: cinematic gate (shared)") then return end
    imgui.text("blocking now: " .. tostring(cached_blocking()))
    imgui.text("reason: " .. (last.reason == "" and "(in control)" or last.reason))
    imgui.text("Other Visceral body scripts switch OFF while this is true")
    imgui.text("(cutscenes, camera events, enemy grab / 3rd-person).")
    imgui.tree_pop()
end)

log.info("[visceral_cinegate] loaded — publishes __visceral_cinematic_blocking()")
