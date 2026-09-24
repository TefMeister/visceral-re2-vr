-- Visceral (RE2 VR) -- TEMPORARY measurement aid (2026-09-24): freeze layer 0 (body/locomotion) playback so the
-- idle's own sway stops; then any body movement at the aim press is caused by the press. Remove after the run.
if reframework:get_game_name() ~= "re2" then return end
local NS = sdk.game_namespace
local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local logged = false
local function step()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return end
    local p = safe(function() return pm:call("get_CurrentPlayer") end); if not p then return end
    local mo = safe(function() return p:call("getComponent(System.Type)", sdk.typeof("via.motion.Motion")) end); if not mo then return end
    local l0 = safe(function() return mo:call("getLayer", 0) end); if not l0 then return end
    safe(function() l0:call("set_Speed", 0.0) end)
    if not logged then logged = true; log.info("[visceral_freeze] layer 0 speed -> 0 (was frozen for the press test)") end
end
re.on_pre_application_entry("UpdateMotion", step)
re.on_pre_application_entry("LateUpdateBehavior", step)
