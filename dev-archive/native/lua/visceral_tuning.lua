-- visceral_tuning.lua -- the hand tuning sliders (2026-10-13, Tefa: "build the tuning sliders for all guns in the game,
-- also Leon, Claire, Ada and Hunk must all have different save files for them, because all their hands are different").
--
-- REFramework menu "Visceral: hand tuning". The plugin tells us the gun in hand and the character playing (bridge slots
-- S_WHO, S_WHO + 1); the sliders shown are always for THAT gun and THAT character, and are saved per character in
-- reframework/data/visceral_tuning_<character>.json, per gun (WP number) and per hold. Every number is added on top of
-- what is built in, so 0 everywhere = as built. Each frame the numbers for the gun in hand go to the plugin (src/tune.h).
-- b168: a fourth hold for the RIGHT hand on the gun (fingers, wrist, and the gun in the hand), live whenever a gun is
-- in hand. Uses the bridge array through _G.visceral_bridge (visceral_bridge.lua); the slot map must match src/bridge.h.

local TAG = "[visceral-tuning]"
local S_WHO, S_HT_ON, S_HT, STRIDE = 84, 86, 88, 64
local N_VALUES = 12 + 17 * 3   -- item move 3, item turn 3, hand move 3, hand turn 3, 17 finger joints x (twist, spread, curl)

local CHARACTERS = {[0] = "leon", [1] = "claire", [2] = "ada", [3] = "sherry", [4] = "hunk", [5] = "tofu"}   -- pl digit; 2-5 not yet seen in a log
local GUNS = {
    [0] = "Matilda", [100] = "M19", [200] = "JMB Hp3", [300] = "Quickdraw Army", [400] = "Glock 17", [600] = "MUP",
    [700] = "Broom Hc", [800] = "SLS 60", [3000] = "Lightning Hawk", [3200] = "Chief Irons Revolver", [7000] = "Samurai Edge",
    [7010] = "Samurai Edge (Chris)", [7020] = "Samurai Edge (Jill)", [7030] = "Samurai Edge (Albert)", [1000] = "W-870",
    [1100] = "Remington 870", [1200] = "M3 Shotgun", [1300] = "GM 79", [1500] = "Lightning Hawk (shotgun)", [2000] = "MQ 11",
    [2200] = "LE 5", [4100] = "GM 79", [4200] = "Chemical Flamethrower", [4300] = "Spark Shot", [4400] = "ATM-4",
    [4600] = "Anti-Tank Rocket", [4700] = "Minigun", [4500] = "Combat Knife", [6200] = "Hand Grenade", [6300] = "Flash Grenade",
}
local HOLDS = {
    {key = "mag", title = "Magazine in the hand", item = "The magazine"},
    {key = "shell", title = "Shell in the hand", item = "The shell"},
    {key = "slide", title = "Hand on the slide / pump", item = nil},
    {key = "gun", title = "RIGHT hand on the gun", item = "The gun in the hand"},   -- b168 (src/tune.cpp right_point)
}
local FINGERS = {   -- the order of the plugin's 17 finger joints (handpose_data.h kBones)
    {"Index", {"base", "middle", "tip"}},
    {"Middle", {"base", "middle", "tip"}},
    {"Ring", {"palm", "base", "middle", "tip"}},
    {"Little", {"palm", "base", "middle", "tip"}},
    {"Thumb", {"base", "middle", "tip"}},
}

local settings = json.load_file("visceral_tuning_settings.json") or {}
if settings.on == nil then settings.on = true end
local files = {}   -- character name -> {guns = {WPxxxx = {mag = {...}, shell = {...}, slide = {...}}}}

local function char_name(ch) return CHARACTERS[ch] or ("pl" .. tostring(ch)) end
local function file_of(name) return "visceral_tuning_" .. name .. ".json" end
local function data_for(name)
    if not files[name] then
        local t = json.load_file(file_of(name)) or {}
        t.guns = t.guns or {}
        files[name] = t
    end
    return files[name]
end
local function hold_values(name, wp, key)
    local guns = data_for(name).guns
    local g = guns[string.format("WP%04d", wp)]
    if not g then g = {}; guns[string.format("WP%04d", wp)] = g end
    local v = g[key]
    if not v then v = {}; g[key] = v end
    for i = 1, N_VALUES do if type(v[i]) ~= "number" then v[i] = 0.0 end end
    return v
end

local function who()
    local b = _G.visceral_bridge
    if not b or not b.ready() then return nil end
    local wp, ch = b.r(S_WHO), b.r(S_WHO + 1)
    if ch < 0 then return nil end
    return math.floor(wp + 0.5), math.floor(ch + 0.5)
end

-- every frame: the numbers for the gun in hand and the character playing, to the plugin
re.on_pre_application_entry("LateUpdateBehavior", function()
    local b = _G.visceral_bridge
    if not b or not b.ready() then return end
    local wp, ch = who()
    if not wp or not settings.on then b.w(S_HT_ON, 0); return end
    local name = char_name(ch)
    for h, hold in ipairs(HOLDS) do
        local v = hold_values(name, wp, hold.key)
        local base = S_HT + (h - 1) * STRIDE
        for i = 1, N_VALUES do b.w(base + i - 1, v[i]) end
    end
    b.w(S_HT_ON, 1)
end)

local function save(name) json.dump_file(file_of(name), files[name]) end

local function drag(label, v, i, metres)
    local changed
    if metres then changed, v[i] = imgui.drag_float(label, v[i], 0.001, -0.3, 0.3, "%.3f m")
    else changed, v[i] = imgui.drag_float(label, v[i], 0.5, -180.0, 180.0, "%.1f deg") end
    return changed
end

local function three(v, first, labels, metres, id)
    local any = false
    for k = 1, 3 do any = drag(labels[k] .. "##" .. id .. k, v, first + k - 1, metres) or any end
    return any
end

local function hold_ui(name, wp, hold)
    if not imgui.tree_node(hold.title) then return false end
    local v = hold_values(name, wp, hold.key)
    local id = hold.key
    local any = false
    if hold.item and imgui.tree_node(hold.item .. "##" .. id) then
        any = three(v, 1, {"Move 1", "Move 2", "Move 3"}, true, id .. "im") or any
        any = three(v, 4, {"Turn 1", "Turn 2", "Turn 3"}, false, id .. "it") or any
        imgui.tree_pop()
    end
    if imgui.tree_node("The hand##" .. id) then
        any = three(v, 7, {"Move 1", "Move 2", "Move 3"}, true, id .. "hm") or any
        any = three(v, 10, {"Turn 1", "Turn 2", "Turn 3"}, false, id .. "ht") or any
        imgui.tree_pop()
    end
    if imgui.tree_node("Fingers##" .. id) then
        local j = 0
        for _, f in ipairs(FINGERS) do
            if imgui.tree_node(f[1] .. "##" .. id) then
                for k, joint in ipairs(f[2]) do
                    local first = 13 + (j + k - 1) * 3
                    imgui.text(f[1] .. " " .. joint)
                    local tag = id .. f[1] .. k
                    any = drag("curl##" .. tag, v, first + 2, false) or any
                    any = drag("spread##" .. tag, v, first + 1, false) or any
                    any = drag("twist##" .. tag, v, first, false) or any
                end
                imgui.tree_pop()
            end
            j = j + #f[2]
        end
        imgui.tree_pop()
    end
    if imgui.button("Reset this hold##" .. id) then for i = 1, N_VALUES do v[i] = 0.0 end; any = true end
    imgui.tree_pop()
    return any
end

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: hand tuning") then return end
    local changed
    changed, settings.on = imgui.checkbox("Use the tuning numbers", settings.on)
    if changed then json.dump_file("visceral_tuning_settings.json", settings) end
    local wp, ch = who()
    if not wp then
        imgui.text("Waiting for the game: no character yet (or the bridge is not attached).")
        imgui.tree_pop()
        return
    end
    local name = char_name(ch)
    imgui.text(string.format("Character: %s (pl%d), saved in %s", name, ch, file_of(name)))
    imgui.text(string.format("Gun in hand: %s (WP%04d)", GUNS[wp] or "unknown", wp))
    imgui.text("Equip another gun to tune that one. 0 everywhere = as built.")
    local any = false
    for _, hold in ipairs(HOLDS) do any = hold_ui(name, wp, hold) or any end
    if any then save(name) end
    imgui.tree_pop()
end)

log.info(TAG .. " loaded")
