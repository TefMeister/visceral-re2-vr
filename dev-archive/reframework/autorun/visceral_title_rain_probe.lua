-- Visceral (RE2 VR) -- PROBE: why the title rain only falls after Story (2026-09-27)
--
-- With visceral_title_one_scene.lua the last-save scene shows behind the main menu, but only raindrop splashes
-- show there; the falling rain appears once the Story menu opens (flow state 11), although the camera and the
-- scene do not change any more. So something is switched on by the Story state itself.
--
-- This takes a snapshot of the title scene (every GameObject's draw/update switches, and every effect player's
-- enabled switch) on the main menu (state 10) and on the Story menu (state 11), and logs what differs.
-- NUM8 (on the main menu): switch on everything that was on in the Story snapshot but off on the main menu.
--
-- Probe: archive it once it has answered (standing rule), keep only the fix.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_rain]"
local VK_NUMPAD8 = 0x68
local MAX_LINES = 80          -- per diff, so the log stays readable
local SNAP_DELAY = 1.0        -- seconds after entering a state before the snapshot (menus animate in)

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

local function scene()
    local sm = sdk.get_native_singleton("via.SceneManager")
    if not sm then return nil end
    return sdk.call_native_func(sm, sdk.find_type_definition("via.SceneManager"), "get_CurrentScene")
end

local function components(type_name)
    local td = sdk.find_type_definition(type_name)
    local sc = scene()
    if not td or not sc then return {} end
    local arr = safe(function() return sc:call("findComponents(System.Type)", td:get_runtime_type()) end)
    local out, n = {}, 0
    if not arr then return out end
    n = safe(function() return arr:get_size() end) or 0
    for i = 0, n - 1 do
        local c = safe(function() return arr:get_element(i) end)
        if c then out[#out + 1] = c end
    end
    return out
end

-- snapshot: key -> { obj, kind, on }
local function snapshot()
    local snap, count = {}, 0
    for _, tr in ipairs(components("via.Transform")) do
        local go = safe(function() return tr:call("get_GameObject") end)
        if go then
            local nm = safe(function() return go:call("get_Name") end) or "?"
            local addr = safe(function() return go:get_address() end) or 0
            local key = string.format("%s@%x", tostring(nm), addr)
            local draw = safe(function() return go:call("get_DrawSelf") end)
            local upd = safe(function() return go:call("get_UpdateSelf") end)
            snap[key .. " draw"] = { obj = go, kind = "draw", on = draw }
            snap[key .. " update"] = { obj = go, kind = "update", on = upd }
            count = count + 1
        end
    end
    local eff = 0
    for _, ep in ipairs(components("via.effect.EffectPlayer")) do
        local go = safe(function() return ep:call("get_GameObject") end)
        local nm = go and safe(function() return go:call("get_Name") end) or "?"
        local addr = safe(function() return ep:get_address() end) or 0
        snap[string.format("%s@%x effect", tostring(nm), addr)] = { obj = ep, kind = "effect",
            on = safe(function() return ep:call("get_Enabled") end) }
        eff = eff + 1
    end
    return snap, count, eff
end

local snaps = {}
local pending = nil     -- { state, at }
local last_flow = -1
local key_prev = false

local function diff(a, b, label)
    local n = 0
    for k, v in pairs(b) do
        local av = a[k]
        if av == nil then
            if v.on then
                n = n + 1
                if n <= MAX_LINES then log_line(label .. " NEW and on: " .. k) end
            end
        elseif av.on ~= v.on then
            n = n + 1
            if n <= MAX_LINES then log_line(label .. " " .. k .. ": " .. tostring(av.on) .. " -> " .. tostring(v.on)) end
        end
    end
    log_line(label .. ": " .. n .. " differences" .. (n > MAX_LINES and (" (first " .. MAX_LINES .. " shown)") or ""))
end

local flow_t = sdk.find_type_definition("app.ropeway.gamemastering.TitleFlow")
local flow_set = flow_t and flow_t:get_method("set_StateValue")
if flow_set then
    sdk.hook(flow_set, function(args)
        local v = safe(function() return sdk.to_int64(args[3]) & 0xFFFFFFFF end)
        if v and v ~= last_flow and v ~= 100 then
            if v == 10 or v == 11 then pending = { state = v, at = os.clock() + SNAP_DELAY } end
        end
        if v then last_flow = v end
    end, function(retval) return retval end)
end

re.on_frame(function()
    if pending and os.clock() >= pending.at then
        local st = pending.state
        pending = nil
        local s, count, eff = snapshot()
        snaps[st] = s
        log_line(string.format("snapshot on state %d: %d objects, %d effect players", st, count, eff))
        if snaps[10] and snaps[11] then
            if st == 11 then diff(snaps[10], snaps[11], "main->Story") else diff(snaps[11], snaps[10], "Story->main") end
        end
    end
    local d = safe(function() return reframework:is_key_down(VK_NUMPAD8) end)
    if d and not key_prev then
        if not snaps[11] then
            log_line("NUM8: no Story snapshot yet -- open Story once first")
        else
            local cur = snapshot()
            local n = 0
            for k, v in pairs(snaps[11]) do
                local c = cur[k]
                if v.on and c and not c.on then
                    n = n + 1
                    if c.kind == "draw" then safe(function() c.obj:call("set_DrawSelf", true) end)
                    elseif c.kind == "update" then safe(function() c.obj:call("set_UpdateSelf", true) end)
                    else safe(function() c.obj:call("set_Enabled", true) end) end
                    if n <= MAX_LINES then log_line("NUM8 switched on: " .. k) end
                end
            end
            log_line("NUM8: switched on " .. n .. " things that the Story menu has on")
        end
    end
    key_prev = d or false
end)

log_line("rain probe loaded")
