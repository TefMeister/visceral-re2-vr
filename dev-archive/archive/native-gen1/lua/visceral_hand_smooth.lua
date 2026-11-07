-- visceral_hand_smooth.lua -- the menu for hand smoothing in RE2 and RE3 (2026-11-02).
--
-- Tefa, 2026-11-02: "please add smoothing to re2 and 3" (Village has had it since 2026-11-01), then "hand smoothing must
-- always be off for melee weapons in any game, only option for guns. smooth melee feels so bad in vr".
--
-- The filter itself lives in the VR layer (our REFramework patch, src/mods/VR.cpp update_hmd_state, right after the
-- controllers are read), so the arms, the gun, the aim and the plugin all see the steadier hand. It reads
-- reframework/data/visceral/hand_smooth.json once a second; this script only draws the menu and writes that file.
-- The VR layer smooths ONLY while a real gun is in hand: never with the knife, a grenade, bare hands or in a menu.
-- Its log lines start "[Visceral] hand smoothing" / "[Visceral] smoothing" (every 10 s: how much shake was taken out).

local FILE = "visceral/hand_smooth.json"
local TAG = "[visceral-hand-smooth]"

local cfg = { on = true, strength = 4, right = true, left = true }   -- Village's defaults (strength 4 = 2.1 Hz when still)

local function save() pcall(function() json.dump_file(FILE, cfg) end) end

local saved = nil
pcall(function() saved = json.load_file(FILE) end)
if type(saved) == "table" then
    for k, v in pairs(saved) do if cfg[k] ~= nil and type(v) == type(cfg[k]) then cfg[k] = v end end
else
    save()   -- first run: the VR layer finds the file and starts smoothing
end

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: hand smoothing (guns only)") then return end
    local c, v
    c, v = imgui.checkbox("Hand smoothing on", cfg.on)
    if c then cfg.on = v; save() end
    c, v = imgui.slider_int("Strength (0 = light, 10 = heavy)", math.floor(tonumber(cfg.strength) or 4), 0, 10)
    if c then cfg.strength = v; save() end
    c, v = imgui.checkbox("Right hand", cfg.right)
    if c then cfg.right = v; save() end
    c, v = imgui.checkbox("Left hand", cfg.left)
    if c then cfg.left = v; save() end
    imgui.text("Only while a gun is in hand: never the knife or grenades.")
    imgui.tree_pop()
end)

log.info(TAG .. " loaded (" .. (cfg.on and ("on, strength " .. tostring(cfg.strength)) or "off") .. ")")
