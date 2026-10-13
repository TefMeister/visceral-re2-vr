-- visceral_tuning.lua -- the hand tuning sliders (2026-10-13, Tefa: "build the tuning sliders for all guns in the game,
-- also Leon, Claire, Ada and Hunk must all have different save files for them, because all their hands are different").
--
-- REFramework menu "Visceral: hand tuning". The plugin tells us the gun in hand and the character playing (bridge slots
-- S_WHO, S_WHO + 1); the sliders shown are always for THAT gun and THAT character, and are saved per character in
-- reframework/data/visceral_tuning_<character>.json, per gun (WP number) and per hold. Every number is added on top of
-- what is built in, so 0 everywhere = as built. Each frame the numbers for the gun in hand go to the plugin (src/tune.h).
-- b168: a fourth hold for the RIGHT hand on the gun (fingers, wrist, and the gun in the hand), live whenever a gun is
-- in hand. Uses the bridge array through _G.visceral_bridge (visceral_bridge.lua); the slot map must match src/bridge.h.
-- b169 (Tefa 2026-10-13: "it has to work with a countdown, so i tick the box, comfortably hold the controllers and in
-- 10 seconds they stop moving"): the tick box "Hold my hands still" counts down 10 s (a light buzz each second), then
-- writes S_FREEZE = 1 with a strong buzz; the VR layer then keeps both controllers where they were (src/tune.cpp
-- visceral_hands_frozen). Untick to let the hands follow again. Never saved: every game starts with the hands free.

local TAG = "[visceral-tuning]"
local S_WHO, S_HT_ON, S_HT, STRIDE = 84, 86, 88, 64
local S_FREEZE, FREEZE_AFTER_SEC = 536, 10   -- b169; b172/b173: moved from 344 (src/bridge.h)
local S_RUMBLE_L_AMP, S_RUMBLE_R_AMP = 10, 12   -- amp, then seconds at +1 (visceral_bridge.lua fires them)
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
    -- b172 (Tefa 2026-10-14): the left hand opening as it nears the slide / pump (fingers only), and the left hand on
    -- its own (no button held, not near a gun): one per character, whatever gun is out
    {key = "open", title = "LEFT hand opening to grab the slide / pump", item = nil, nohand = true},
    {key = "free", title = "LEFT hand on its own (no buttons, away from the gun)", item = nil, free = true},
    -- b173 (Tefa: "hand on the pistol handle is different than that hand going on the slider"): its own opening shape
    {key = "open_handle", title = "LEFT hand opening to hold the handle", item = nil, nohand = true},
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

-- b169: hold the hands still. os.time() counts whole seconds of the real clock, enough for a 10 s countdown.
local freeze = {ticked = false, started = 0, frozen = false, last_left = -1}
local function buzz(b, amp, sec)
    b.w(S_RUMBLE_L_AMP, amp); b.w(S_RUMBLE_L_AMP + 1, sec)
    b.w(S_RUMBLE_R_AMP, amp); b.w(S_RUMBLE_R_AMP + 1, sec)
end
local function freeze_frame(b)
    if not freeze.ticked then
        freeze.frozen = false
    elseif not freeze.frozen then
        local left = FREEZE_AFTER_SEC - (os.time() - freeze.started)
        if left <= 0 then
            freeze.frozen = true
            buzz(b, 1.0, 0.4)
            log.info(TAG .. " hands held still")
        elseif left ~= freeze.last_left then
            buzz(b, 0.3, 0.05)
        end
        freeze.last_left = left
    end
    b.w(S_FREEZE, freeze.frozen and 1 or 0)
end

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
-- b170 (Tefa 2026-10-13: "can you differentiate a long extended clip and a regular one?"): each set of upgrades on a
-- gun (the game's WeaponParts bits: long magazine, stock, muzzle brake ...) gets its own numbers. The plain gun keeps
-- the old key WPxxxx, so earlier tuning stays where it was; an upgraded gun is WPxxxx_parts<bits>, and the first time
-- one is seen it starts as a copy of the plain gun's numbers.
local cur_parts = 0
local function parts_now()
    local ok, v = pcall(function()
        local go = sdk.get_managed_singleton("app.ropeway.PlayerManager"):call("get_CurrentPlayer")
        local eq = go:call("getComponent(System.Type)", sdk.typeof("app.ropeway.survivor.Equipment"))
        return eq:call("get_MainWeapon"):call("get_WeaponParts")
    end)
    return (ok and type(v) == "number") and math.floor(v) or 0
end
local function gun_key(wp)
    if cur_parts == 0 then return string.format("WP%04d", wp) end
    return string.format("WP%04d_parts%d", wp, cur_parts)
end
local function copy(t)
    if type(t) ~= "table" then return t end
    local c = {}
    for k, v in pairs(t) do c[k] = copy(v) end
    return c
end
local last_logged = nil

local function hold_values(name, wp, key)
    if key == "free" then   -- b172: one per character, not per gun
        local d = data_for(name)
        d.free = d.free or {}
        for i = 1, N_VALUES do if type(d.free[i]) ~= "number" then d.free[i] = 0.0 end end
        return d.free
    end
    local guns = data_for(name).guns
    local gk = gun_key(wp)
    local g = guns[gk]
    if not g then g = copy(guns[string.format("WP%04d", wp)]) or {}; guns[gk] = g end
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
    freeze_frame(b)
    local wp, ch = who()
    if not wp or not settings.on then b.w(S_HT_ON, 0); return end
    local name = char_name(ch)
    cur_parts = parts_now()
    if gun_key(wp) ~= last_logged then
        last_logged = gun_key(wp)
        log.info(TAG .. " tuning set " .. name .. " " .. last_logged)
    end
    for h, hold in ipairs(HOLDS) do
        local base = S_HT + (h - 1) * STRIDE
        if wp < 0 and not hold.free then   -- b172: no gun out: only the left hand's own numbers
            for i = 1, N_VALUES do b.w(base + i - 1, 0.0) end
        else
            local v = hold_values(name, wp, hold.key)
            for i = 1, N_VALUES do b.w(base + i - 1, v[i]) end
        end
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
    if not hold.nohand and imgui.tree_node("The hand##" .. id) then
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
    changed, freeze.ticked = imgui.checkbox("Hold my hands still (starts after 10 seconds)", freeze.ticked)
    if changed then
        freeze.started, freeze.frozen, freeze.last_left = os.time(), false, -1
        if not freeze.ticked then log.info(TAG .. " hands follow the controllers again") end
    end
    if freeze.ticked and not freeze.frozen then
        imgui.text(string.format("Hold the controllers where you want the hands: still in %d s", math.max(0, FREEZE_AFTER_SEC - (os.time() - freeze.started))))
    elseif freeze.frozen then
        imgui.text("Hands held still: you can put the controllers down. Untick to let go.")
    end
    local wp, ch = who()
    if not wp then
        imgui.text("Waiting for the game: no character yet (or the bridge is not attached).")
        imgui.tree_pop()
        return
    end
    local name = char_name(ch)
    imgui.text(string.format("Character: %s (pl%d), saved in %s", name, ch, file_of(name)))
    if wp < 0 then   -- b172: no gun out: the left hand on its own can still be tuned
        imgui.text("No gun in hand: only the left hand on its own can be tuned now.")
        local any = false
        for _, hold in ipairs(HOLDS) do if hold.free then any = hold_ui(name, wp, hold) or any end end
        if any then save(name) end
        imgui.tree_pop()
        return
    end
    imgui.text(string.format("Gun in hand: %s (WP%04d)", GUNS[wp] or "unknown", wp))
    if cur_parts == 0 then imgui.text("Upgrades: none (these numbers are for the plain gun)")
    else imgui.text(string.format("Upgrades fitted (set %d): these numbers are for this gun with these upgrades only", cur_parts)) end
    imgui.text("Equip another gun to tune that one. 0 everywhere = as built.")
    local any = false
    for _, hold in ipairs(HOLDS) do any = hold_ui(name, wp, hold) or any end
    if any then save(name) end
    imgui.tree_pop()
end)

log.info(TAG .. " loaded")
