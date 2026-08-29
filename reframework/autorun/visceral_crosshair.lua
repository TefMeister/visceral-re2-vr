-- Visceral (RE2 VR) -- crosshair control
--
-- Hides the game's 2D crosshair (the "GUI_Reticle" HUD element) so it doesn't
-- float in the VR view. Rebuilt fresh from our own AC re2_vr_crosshair.lua (the
-- disable half — not the VR world-crosshair raycasting, which we don't need here).
--
-- Mechanism: REFramework's on_pre_gui_draw_element fires per HUD element before it
-- draws; returning false skips it. We skip GUI_Reticle when disabled.
--
-- Settings live in reframework/data/visceral/crosshair_config.json so they can be
-- shipped/edited without touching the script; also a menu toggle.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_crosshair]"
local cfg_path = "visceral/crosshair_config.json"

local cfg = {
    disable_crosshair = true,   -- default: crosshair off (Visceral default)
}

local function load_cfg()
    local loaded = json.load_file(cfg_path)
    if loaded == nil then json.dump_file(cfg_path, cfg); return end
    for k, v in pairs(loaded) do cfg[k] = v end
end
load_cfg()

local function save_cfg() pcall(function() json.dump_file(cfg_path, cfg) end) end
pcall(function() re.on_config_save(save_cfg) end)

local RETICLE = { ["GUI_Reticle"] = true }

re.on_pre_gui_draw_element(function(element, context)
    if not cfg.disable_crosshair then return true end
    local ok, go = pcall(function() return element:call("get_GameObject") end)
    if not ok or not go then return true end
    local ok2, name = pcall(function() return go:call("get_Name") end)
    if not ok2 or not name then return true end
    if RETICLE[name] then return false end   -- skip drawing = crosshair hidden
    return true
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: crosshair") then return end
    local changed
    changed, cfg.disable_crosshair = imgui.checkbox("Disable game crosshair", cfg.disable_crosshair)
    if changed then save_cfg() end
    imgui.text("Hides the game's 2D reticle (GUI_Reticle). Config: data/" .. cfg_path)
    imgui.tree_pop()
end)

log.info(TAG .. " loaded (disable_crosshair=" .. tostring(cfg.disable_crosshair) .. ")")
