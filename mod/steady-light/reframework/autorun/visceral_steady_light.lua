-- visceral_steady_light.lua -- both eyes the same brightness under Single Frame Multipass (2026-10-30, Opus, home PC;
-- Tefa: "please look at making a single frame multipass fix for all the other re games we have").
--
-- Why: under Single Frame Multipass the RIGHT eye's pass is drawn without the game's adapted exposure and the left eye's
-- with it, so one eye is brighter (Village 2026-10-29: "right eye open had the golden veil"; RE2 and RE7 show the same)
-- `[reported 2026-10-29]`. Village was cured by holding one exposure for both eyes `[verified-live 2026-10-29, headset]`.
-- This is the same cure, written for any RE Engine game that has no native fix of ours (RE7 first; RE2 and RE3 carry it
-- in visceral_native's exposure.cpp, so do NOT install this beside those).
--
-- What it writes, every frame, on the main camera's via.render.ToneMapping, right after the game's own tone-map
-- controller has written its values (a post-hook on its lateUpdate) and again before drawing (BeginRendering):
--   mode "area" (default): the adaptation's limits pinned to the area's own EV (RE2's b192 shape) and AutoExposure off.
--                          Adapted or not, a pass then gets the area's EV, so the eyes match by construction.
--                          `[inferred-static]` for RE7 until it is seen in the headset.
--   mode "hold":           AutoExposure off and one EV of our own for everywhere (Village's cure; for a game whose
--                          area EV looks too bright, as Village's EV 3 did).
--   mode "game":           nothing written (to compare).
-- Grading: "game" leaves the per-area colour grading alone (default); "off" switches it off as RE2 and Village do.
-- Contrast (2026-10-31, from RE2's b193, Tefa in the headset: "contrast looks right" at 1.2 with the grading off): 0 = the
-- game's own (nothing written, the default here); any other value is written every frame. RE2's package ships 1.2.
-- Settings: reframework/data/visceral_steady_light.json and the REFramework menu "Visceral: steady light".
--
-- 1.1 (2026-10-31, RE2, Tefa: "the flicker in re2, that just randomly occurs with SFM"): four Flickers BeG0nE catches
-- showed the LEFT eye alone brightening for one frame and settling over the next few (26.8 -> 30.2 -> 28.5 -> 27.6,
-- the right eye flat at 26.8) `[measured 2026-10-31, RE2, n=4 catches]`: automatic brightness, one eye. Under Single Frame
-- Multipass that eye is drawn by REFramework's CLONE of the camera ("<name> (Clone)", CameraDuplicator), which gets the
-- main camera's tone-map settings copied over a few frames at a time, sometimes the game's "AutoExposure Enable" from
-- before our write `[hypothesis]`. So the clone's tone map is held too, with the main camera's area EV.
local TAG = "[visceral-steady-light]"
local CFG_FILE = "visceral_steady_light.json"
local MODES = { "area", "hold", "game" }
local GRADINGS = { "game", "off" }
local AE_DISABLE = { 2, 1 }   -- Disable is 2 in RE2 and Village [verified-live]; 1 tried if 2 does not read back off
local LOG_EVERY_S = 10.0
local cfg = { mode = "area", ev = 6.4, grading = "game", contrast = 0.0 }

local function save_cfg() pcall(json.dump_file, CFG_FILE, cfg) end
local function load_cfg()
    local ok, d = pcall(json.load_file, CFG_FILE)
    if not ok or type(d) ~= "table" then save_cfg(); return end   -- no file yet: write the defaults (a missing file logs an error every read)
    do
        if type(d.mode) == "string" then cfg.mode = d.mode end
        if type(d.grading) == "string" then cfg.grading = d.grading end
        cfg.ev = tonumber(d.ev) or cfg.ev
        cfg.contrast = tonumber(d.contrast) or cfg.contrast
    end
end
load_cfg()

-- the game's own type names differ per game (app.ropeway.posteffect.* in RE2, app.* in Village); try both shapes
local function find_type(short)
    for _, name in ipairs({ sdk.game_namespace("posteffect." .. short), sdk.game_namespace(short), "app." .. short, "app.posteffect." .. short }) do
        local td = sdk.find_type_definition(name)
        if td then return td, name end
    end
end

local function camera_go()
    local cam = sdk.get_primary_camera()
    return cam and cam:call("get_GameObject") or nil
end
local function component(go, tname)
    local ok, c = pcall(function() return go:call("getComponent(System.Type)", sdk.typeof(tname)) end)
    return ok and c or nil
end

local st = { disable = nil, tries = 0, writes = 0, last_log = 0, area_ev = -1, said_grading = nil }

local function hold(tm, ev_in)
    if cfg.mode == "game" then return end
    local ev = cfg.ev
    if cfg.mode == "area" then
        -- the area's EV, read before we ever write it (hold mode writes EV; area mode never does); the clone takes the main's
        ev = ev_in or tm:call("get_EV") or ev
        if not ev_in then st.area_ev = ev end
        pcall(function() tm:call("set_AutoExposureMinEV", ev); tm:call("set_AutoExposureMaxEV", ev) end)
    else
        pcall(function() tm:call("set_EV", ev); tm:call("set_AutoExposureMinEV", ev); tm:call("set_AutoExposureMaxEV", ev) end)
    end
    local v = st.disable or AE_DISABLE[(st.tries % #AE_DISABLE) + 1]
    pcall(function() tm:call("setAutoExposure", v) end)
    if not st.disable then
        st.tries = st.tries + 1
        local ok, on = pcall(function() return tm:call("get_UseAutoExposure") end)
        if ok and on == false then st.disable = v; log.info(TAG .. " AutoExposure Disable = " .. v .. ", kept")
        elseif st.tries >= 2 * #AE_DISABLE then st.disable = AE_DISABLE[1]; log.warn(TAG .. " no value read back as off; keeping " .. st.disable .. " (the pinned limits still hold the light)") end
    end
    if cfg.contrast and cfg.contrast > 0 then pcall(function() tm:call("set_Contrast", cfg.contrast) end) end
    st.writes = st.writes + 1
end

local cc_td = find_type("ColorCorrectController")
local function grading(go)
    if cfg.grading ~= "off" and st.said_grading ~= "off" then return end
    local off = cfg.grading == "off"
    local ctl = cc_td and component(go, cc_td:get_full_name()) or nil
    local ldr = component(go, "via.render.LDRPostProcess")
    local cc = ldr and ldr:call("get_ColorCorrect") or nil
    if ctl then pcall(function() ctl:call("set_Enabled", not off) end) end
    if cc then pcall(function() cc:call("set_Enabled", not off) end) end
    if st.said_grading ~= cfg.grading then st.said_grading = cfg.grading; log.info(TAG .. " colour grading " .. (off and "OFF" or "back to the game's own")) end
end

-- 1.1: REFramework's Single Frame Multipass clone of the main camera (the other eye), found by name, looked up every 2 s
local function scene()
    local sm = sdk.get_native_singleton("via.SceneManager")
    local td = sdk.find_type_definition("via.SceneManager")
    if not sm or not td then return nil end
    return sdk.call_native_func(sm, td, "get_CurrentScene")
end
local clone = { name = nil, go = nil, t = -10, said = nil }
local function clone_go(main_go)
    local ok, name = pcall(function() return main_go:call("get_Name") end)
    if not ok or not name then return nil end
    local now = os.clock()
    if clone.name ~= name or now - clone.t > 2.0 then
        local sc = scene()
        local ok2, c = pcall(function() return sc and sc:call("findGameObject(System.String)", name .. " (Clone)") or nil end)
        clone.name, clone.go, clone.t = name, ok2 and c or nil, now
        local found = clone.go ~= nil
        if clone.said ~= found then clone.said = found; log.info(TAG .. (found and " the other eye's camera (REFramework's clone) found: held too" or " no clone camera (Single Frame Multipass off?): the main camera only")) end
    end
    return clone.go
end

local function apply()
    local go = camera_go()
    if not go then return end
    local tm = component(go, "via.render.ToneMapping")
    if tm then hold(tm) end
    grading(go)
    local cg = clone_go(go)
    local ctm = cg and component(cg, "via.render.ToneMapping") or nil
    if ctm then hold(ctm, st.area_ev >= -50 and st.area_ev or nil); grading(cg) end
end

local tm_td, tm_name = find_type("ToneMapController")
if tm_td and tm_td:get_method("lateUpdate") then
    sdk.hook(tm_td:get_method("lateUpdate"), function(args) end, function(retval) apply(); return retval end)
    log.info(TAG .. " hooked " .. tm_name .. ".lateUpdate (our values land after the game's)")
else
    log.warn(TAG .. " no tone-map controller found; writing at BeginRendering only")
end

local frame = 0
re.on_pre_application_entry("BeginRendering", function()
    frame = frame + 1
    if frame % 120 == 0 then load_cfg() end
    apply()
    local now = os.clock()
    if now - st.last_log >= LOG_EVERY_S then
        st.last_log = now
        local go = camera_go()
        local tm = go and component(go, "via.render.ToneMapping")
        if tm then
            local ok, msg = pcall(function()
                return string.format("%s mode=%s: EV %.2f, limits %.2f..%.2f, UseAutoExposure %s, grading %s (%d writes)", TAG, cfg.mode,
                    tm:call("get_EV") or -99, tm:call("get_AutoExposureMinEV") or -99, tm:call("get_AutoExposureMaxEV") or -99,
                    tostring(tm:call("get_UseAutoExposure")), cfg.grading, st.writes)
            end)
            log.info(ok and msg or (TAG .. " log read failed: " .. tostring(msg)))
        end
    end
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: steady light (both eyes the same)") then return end
    local mi = 1
    for i, m in ipairs(MODES) do if m == cfg.mode then mi = i end end
    local c1, n1 = imgui.combo("Light", mi, MODES)
    if c1 and MODES[n1] then cfg.mode = MODES[n1]; save_cfg(); log.info(TAG .. " mode -> " .. cfg.mode) end
    local c2, n2 = imgui.slider_float("Held EV (mode 'hold')", cfg.ev, 0.0, 12.0, "%.2f")
    if c2 then cfg.ev = n2; save_cfg() end
    local gi = 1
    for i, m in ipairs(GRADINGS) do if m == cfg.grading then gi = i end end
    local c3, n3 = imgui.combo("Per-area colour grading", gi, GRADINGS)
    if c3 and GRADINGS[n3] then cfg.grading = GRADINGS[n3]; save_cfg() end
    local c4, n4 = imgui.slider_float("Contrast (0 = the game's own)", cfg.contrast, 0.0, 2.0, "%.2f")
    if c4 then cfg.contrast = n4; save_cfg() end
    imgui.text(string.format("area EV %.2f, %d frames written", st.area_ev, st.writes))
    imgui.tree_pop()
end)
