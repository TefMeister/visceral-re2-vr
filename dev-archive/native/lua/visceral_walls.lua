-- visceral_walls.lua -- RE2 and RE3: the hands stop at walls, and a gun turns at the wrist away from one (2026-11-18, Tefa:
-- "give re2 and re3 hands, walls and guns the same collision treatment"). Ported from Visceral RE8's visceral_re8_walls.lua
-- (b141, checked on the simulator there). The same file runs in both games.
--
-- Each frame, for each hand: one ray from about that shoulder (the camera, 18 cm down, 17 cm out) to where the controller
-- puts the hand (our REFramework hands that to src/hands/walls.cpp, which writes it into bridge slots 608-625), a hand's
-- radius further; a hit = a wall plane for that hand (626-639). For the gun in the right hand: where its muzzle WOULD be with
-- the hand where the player holds it (the muzzle's place in the hand is learned from the drawn gun), a ray from the hand to
-- there; on a hit the gun turns at the wrist, away from the wall and a little up, just enough for the muzzle to stay 3 cm off
-- it (640-644); only past tilt_max does the hand also slide back. The drawn muzzle goes to the doors (645-648).
-- Doors are never walls: a hit whose name (or a parent's) says door is skipped, so hands and guns still push them.
-- Settings: reframework/data/visceral/walls.json, and the options book (if it lists them).

local GAME = reframework:get_game_name()
if GAME ~= "re2" and GAME ~= "re3" then return end
local NS = GAME == "re3" and "offline." or "app.ropeway."

local TAG = "[visceral-walls]"
local function L(s) log.info(TAG .. " " .. s) end
local told = {}
local function once(key, s) if not told[key] then told[key] = true; L(s) end end
local function safe(f) local ok, v = pcall(f); if ok then return v end end

local FILE = "visceral/walls.json"
local S_FRAME = 0
local S_RAW_R, S_OUT_R, S_RAW_L, S_HW_STAMP = 608, 615, 622, 625
local SLOTS = { { on = 626, p = 627, n = 630 }, { on = 633, p = 634, n = 637 } }   -- left, right
local S_TILT_Q, S_TILT_ON, S_MUZZLE_W, S_MUZZLE_ON = 640, 644, 645, 648
local cfg = {
    enabled = true, with_gun = true, muzzle = true, muzzle_tilt = true,
    hand_r = 0.05, muzzle_r = 0.03, shoulder_down = 0.18, shoulder_side = 0.17, min_reach = 0.20, max_reach = 1.50,
    max_push = 1.20, tilt_max = 80, tilt_up = 0.5, tilt_in_dps = 720, tilt_out_dps = 240,
}
local function load_cfg()
    local okf, fh = pcall(io.open, FILE, "r")
    if not okf or not fh then return end
    fh:close()
    local ok, d = pcall(json.load_file, FILE)
    if ok and type(d) == "table" then
        for k, v in pairs(cfg) do
            if type(v) == type(d[k]) then cfg[k] = d[k] end
        end
    end
end
local function save_cfg() pcall(json.dump_file, FILE, cfg) end
load_cfg()

-- ===== the ray (Village's, the game's own castRay; praydog's crosshair script uses app.CollisionManager.Layer.Bullet) =====
local cast_ray = safe(function() return sdk.find_type_definition("via.physics.System"):get_method("castRay(via.physics.CastRayQuery, via.physics.CastRayResult)") end)
local LAYER, LAYER_NAME = nil, "?"
-- Which collision layer holds the walls is found in the game, not assumed (2026-11-18, RE2 on the simulator: there is no
-- app.CollisionManager.Layer list, and rays from the head on each layer hit: 2 and 3 the room's "st4_101_0_Terrain_00ms",
-- 7 trigger boxes ("AdditionalNotice", "Env_..."), 6 the player himself ("pl5700"), 12 a "StaticCompounder" a little off).
-- So: once in play, twelve level rays around the head on layers 0-20; the layer with the most hits named "...Terrain..."
-- wins (ties: the lowest number). Tried again every 2 s while nothing is found; after 30 s, layer 2 (RE2's) is used.
local layer_try_t, layer_tries = 0, 0
local function raw_ray_name(a, b, layer)
    local sys = sdk.get_native_singleton("via.physics.System")
    local q = sdk.create_instance("via.physics.CastRayQuery")
    local r = sdk.create_instance("via.physics.CastRayResult")
    q:call("setRay(via.vec3, via.vec3)", a, b); q:call("clearOptions"); q:call("enableAllHits"); q:call("enableNearSort")
    local fi = q:call("get_FilterInfo"); fi:call("set_Group", 0); fi:call("set_MaskBits", 0xFFFFFFFF & ~1); fi:call("set_Layer", layer)
    q:call("set_FilterInfo", fi)
    cast_ray:call(sys, q, r)
    if (r:call("get_NumContactPoints") or 0) < 1 then return nil end
    local col = r:call("getContactCollidable(System.UInt32)", 0)
    return col and col:call("get_GameObject"):call("get_Name") or "?"
end
local function find_layer()
    if LAYER ~= nil or cast_ray == nil or os.clock() - layer_try_t < 2.0 then return LAYER ~= nil end
    layer_try_t = os.clock()
    layer_tries = layer_tries + 1
    local cp = safe(function() return sdk.get_primary_camera():call("get_GameObject"):call("get_Transform"):call("get_Position") end)
    if cp == nil then return false end
    local best, best_n = nil, 0
    for L = 0, 20 do
        local n = 0
        for k = 0, 11 do
            local a = math.rad(k * 30)
            local name = safe(function() return raw_ray_name(Vector3f.new(cp.x, cp.y - 0.4, cp.z), Vector3f.new(cp.x + math.sin(a) * 4, cp.y - 0.4, cp.z + math.cos(a) * 4), L) end)
            if name and tostring(name):lower():find("terrain", 1, true) then n = n + 1 end
        end
        if n > best_n then best, best_n = L, n end
    end
    if best ~= nil then
        LAYER, LAYER_NAME = best, tostring(best)
        L(string.format("the walls are on collision layer %d (%d of 12 rays round the head hit a Terrain there)", best, best_n))
    elseif layer_tries >= 15 then
        LAYER, LAYER_NAME = 2, "2 (no Terrain seen in 30 s: RE2's)"
        L("no wall layer found round the head in 30 s: layer 2 used (RE2's)")
    end
    return LAYER ~= nil
end

local seen_names, seen_count = {}, 0
local function names_up(go)
    local out = {}
    local tf = go and safe(function() return go:call("get_Transform") end)
    for _ = 1, 4 do
        if tf == nil then break end
        local g = safe(function() return tf:call("get_GameObject") end)
        local n = g and safe(function() return g:call("get_Name") end)
        if n then out[#out + 1] = tostring(n) end
        tf = safe(function() return tf:call("get_Parent") end)
    end
    return out
end
local function is_door(names)
    for _, n in ipairs(names) do
        local l = string.lower(n)
        if l:find("door", 1, true) or l:find("gate", 1, true) then return true end
    end
    return false
end
local function ray(a, b)
    if cast_ray == nil then return nil end
    return safe(function()
        local sys = sdk.get_native_singleton("via.physics.System")
        local q = sdk.create_instance("via.physics.CastRayQuery")
        local r = sdk.create_instance("via.physics.CastRayResult")
        q:call("setRay(via.vec3, via.vec3)", Vector3f.new(a.x, a.y, a.z), Vector3f.new(b.x, b.y, b.z))
        q:call("clearOptions")
        q:call("enableAllHits")
        q:call("enableNearSort")
        local fi = q:call("get_FilterInfo")
        fi:call("set_Group", 0)
        fi:call("set_MaskBits", 0xFFFFFFFF & ~1)   -- everything except the player
        fi:call("set_Layer", LAYER)
        q:call("set_FilterInfo", fi)
        cast_ray:call(sys, q, r)
        local count = r:call("get_NumContactPoints") or 0
        for i = 0, math.min(count, 4) - 1 do
            local cp = r:call("getContactPoint(System.UInt32)", i)
            local col = safe(function() return r:call("getContactCollidable(System.UInt32)", i) end)
            local go = col and safe(function() return col:call("get_GameObject") end)
            local names = go and names_up(go) or {}
            if go == nil then once("nocol", "a ray hit has no readable owner (getContactCollidable): doors cannot be told from walls") end
            local key = names[1] or "?"
            if seen_count < 20 and not seen_names[key] then
                seen_names[key] = true; seen_count = seen_count + 1
                L("a ray hit: " .. table.concat(names, " < ") .. (is_door(names) and "  (a door: skipped)" or ""))
            end
            if not is_door(names) and cp then
                local p, n = cp:get_field("Position"), cp:get_field("Normal")
                if p then return { p = { x = p.x, y = p.y, z = p.z }, n = n and { x = n.x, y = n.y, z = n.z } or nil } end
            end
        end
        return nil
    end)
end

-- ===== small maths =====
local function v(x, y, z) return { x = x, y = y, z = z } end
local function sub(a, b) return v(a.x - b.x, a.y - b.y, a.z - b.z) end
local function add(a, b) return v(a.x + b.x, a.y + b.y, a.z + b.z) end
local function mul(a, s) return v(a.x * s, a.y * s, a.z * s) end
local function dot(a, b) return a.x * b.x + a.y * b.y + a.z * b.z end
local function len(a) return math.sqrt(dot(a, a)) end
local function cross(a, b) return v(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x) end
local function qrot(q, p)
    local ux, uy, uz, w = q.x, q.y, q.z, q.w
    local tx, ty, tz = 2 * (uy * p.z - uz * p.y), 2 * (uz * p.x - ux * p.z), 2 * (ux * p.y - uy * p.x)
    return v(p.x + w * tx + (uy * tz - uz * ty), p.y + w * ty + (uz * tx - ux * tz), p.z + w * tz + (ux * ty - uy * tx))
end
local function qconj(q) return { x = -q.x, y = -q.y, z = -q.z, w = q.w } end
local function qnorm(q)
    local l = math.sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w)
    if l < 1e-6 then return nil end
    return { x = q.x / l, y = q.y / l, z = q.z / l, w = q.w / l }
end
local function facing(n, dir)
    if n == nil or len(n) < 0.5 then n = mul(dir, -1) end
    n = mul(n, 1 / len(n))
    if dot(n, dir) > 0 then n = mul(n, -1) end
    return n
end

-- ===== the gun in the right hand and its muzzle =====
local muzzle1 = safe(function() return sdk.find_type_definition("via.murmur_hash"):get_method("calc32"):call(nil, "vfx_muzzle1") end)
local muzzle2 = safe(function() return sdk.find_type_definition("via.murmur_hash"):get_method("calc32"):call(nil, "vfx_muzzle2") end)
local function gun_and_muzzle()
    local pm = sdk.get_managed_singleton(NS .. "PlayerManager")
    local pl = pm and safe(function() return pm:call("get_CurrentPlayer") end)
    local eq = pl and safe(function() return pl:call("getComponent(System.Type)", sdk.typeof(NS .. "survivor.Equipment")) end)
    local w = eq and safe(function() return eq:get_field("<EquipWeapon>k__BackingField") end)
    if w == nil then return nil end
    local tn = safe(function() return w:get_type_definition():get_full_name() end) or ""
    if not tn:find("Gun", 1, true) then return nil end   -- a knife or a grenade is no gun
    local tf = safe(function() return w:call("get_GameObject"):call("get_Transform") end)
    local j = tf and muzzle1 and safe(function() return tf:call("getJointByHash", muzzle1) end)
    if j == nil and tf and muzzle2 then j = safe(function() return tf:call("getJointByHash", muzzle2) end) end
    if j == nil then
        local mj = safe(function() return w:get_field("<MuzzleJoint>k__BackingField") end)
        j = mj and safe(function() return mj:get_field("_Parent") end) or nil
    end
    if j == nil then once("nomuzzle:" .. tn, "no muzzle joint found on " .. tn .. ": its muzzle cannot stop at walls"); return w, nil end
    local p = safe(function() return j:call("get_Position") end)
    return w, p
end

-- ===== each frame =====
local GEN = {}   -- this copy's own token: a copy loaded again by hand (dofile) takes over and the old one stops
rawset(_G, "__visceral_walls_gen", GEN)
local hit_log_t = { 0, 0 }
local muz = { gun = nil, local_ = nil, tilt = 0.0, axis = nil, prev_out_p = nil, prev_out_q = nil, t = nil }

local function update()
    if rawget(_G, "__visceral_walls_gen") ~= GEN then return end
    local b = rawget(_G, "visceral_bridge")
    if not cfg.enabled or b == nil or not b.ready() then return end
    if b.r(S_HW_STAMP) < b.r(S_FRAME) - 3 then once("nohook", "our REFramework is not placing the hands through visceral_hand_wall (yet)"); return end
    if not find_layer() then return end
    local cx = safe(function() return sdk.get_primary_camera():call("get_GameObject"):call("get_Transform") end)
    local cp, cr = cx and safe(function() return cx:call("get_Position") end), cx and safe(function() return cx:call("get_Rotation") end)
    if not (cp and cr) then return end
    local cam = v(cp.x, cp.y, cp.z)
    local right = qrot({ x = cr.x, y = cr.y, z = cr.z, w = cr.w }, v(1, 0, 0))
    right.y = 0
    if len(right) < 1e-3 then return end
    right = mul(right, 1 / len(right))
    local gun, mp = gun_and_muzzle()
    if gun ~= muz.gun then muz.gun, muz.local_, muz.tilt = gun, nil, 0.0 end
    for side = 1, 2 do
        local base = side == 1 and S_RAW_L or S_RAW_R
        local hand = v(b.r(base), b.r(base + 1), b.r(base + 2))
        local skip = not (hand.x == hand.x) or (not cfg.with_gun and gun and side == 2)
        if not skip then
            local best = nil
            local function offer(p, n, why)
                local d = dot(sub(hand, p), n)
                if best == nil or d < best.d then best = { p = p, n = n, d = d, why = why } end
            end
            local shoulder = add(add(cam, v(0, -cfg.shoulder_down, 0)), mul(right, (side == 1 and -1 or 1) * cfg.shoulder_side))
            local dvec = sub(hand, shoulder)
            local dl = len(dvec)
            if dl >= cfg.min_reach and dl <= cfg.max_reach then
                local dir = mul(dvec, 1 / dl)
                local h = ray(shoulder, add(hand, mul(dir, cfg.hand_r)))
                if h then local n = facing(h.n, dir); offer(add(h.p, mul(n, cfg.hand_r)), n, "hand") end
            end
            if side == 2 and gun and cfg.with_gun and cfg.muzzle then
                local now = os.clock()
                local dt = math.min(0.1, math.max(0.0, now - (muz.t or now)))
                muz.t = now
                local q = qnorm({ x = b.r(S_RAW_R + 3), y = b.r(S_RAW_R + 4), z = b.r(S_RAW_R + 5), w = b.r(S_RAW_R + 6) })
                -- learn the muzzle's place in the hand's frame from the drawn muzzle and the hand as last handed back
                if mp and muz.prev_out_p and muz.prev_out_q then
                    local l = qrot(qconj(muz.prev_out_q), sub(v(mp.x, mp.y, mp.z), muz.prev_out_p))
                    local ll = len(l)
                    if ll > 0.05 and ll < 1.3 then muz.local_ = muz.local_ and add(mul(muz.local_, 0.8), mul(l, 0.2)) or l end
                end
                local want = 0.0
                if q and muz.local_ then
                    local barrel = qrot(q, muz.local_)
                    local bl = len(barrel)
                    if bl > 0.05 then
                        local bdir = mul(barrel, 1 / bl)
                        local h = ray(hand, add(add(hand, barrel), mul(bdir, cfg.muzzle_r)))
                        if h then
                            local n = facing(h.n, bdir)
                            if cfg.muzzle_tilt then
                                local up = v(0, 1, 0)
                                local u = add(sub(n, mul(bdir, dot(n, bdir))), mul(sub(up, mul(bdir, dot(up, bdir))), cfg.tilt_up))
                                if len(u) > 1e-3 then
                                    u = mul(u, 1 / len(u))
                                    local need = cfg.muzzle_r - dot(sub(hand, h.p), n)
                                    local found = nil
                                    for deg = 0, cfg.tilt_max, 2 do
                                        local t = math.rad(deg)
                                        if dot(add(mul(barrel, math.cos(t)), mul(u, bl * math.sin(t))), n) >= need then found = t; break end
                                    end
                                    want = found or math.rad(cfg.tilt_max)
                                    local ax = cross(bdir, u)
                                    if len(ax) > 1e-3 then muz.axis = mul(ax, 1 / len(ax)) end
                                    if found == nil then
                                        local bt = add(mul(barrel, math.cos(want)), mul(u, bl * math.sin(want)))
                                        offer(sub(add(h.p, mul(n, cfg.muzzle_r)), bt), n, "muzzle")
                                    end
                                end
                            else
                                offer(sub(add(h.p, mul(n, cfg.muzzle_r)), barrel), n, "muzzle")
                            end
                        end
                    end
                end
                local rate = math.rad(want > muz.tilt and cfg.tilt_in_dps or cfg.tilt_out_dps)
                if want > muz.tilt then muz.tilt = math.min(want, muz.tilt + rate * dt) else muz.tilt = math.max(want, muz.tilt - rate * dt) end
                if muz.tilt > 1e-3 and muz.axis then
                    local hs = math.sin(muz.tilt * 0.5)
                    b.w(S_TILT_Q, muz.axis.x * hs); b.w(S_TILT_Q + 1, muz.axis.y * hs); b.w(S_TILT_Q + 2, muz.axis.z * hs); b.w(S_TILT_Q + 3, math.cos(muz.tilt * 0.5))
                    b.w(S_TILT_ON, b.r(S_FRAME) + 1.0)
                    if now - (muz.log_t or 0) > 2.0 then
                        muz.log_t = now
                        L(string.format("the gun's muzzle meets a wall: the gun turns %.0f deg at the wrist", math.deg(muz.tilt)))
                    end
                end
                muz.prev_out_p = v(b.r(S_OUT_R), b.r(S_OUT_R + 1), b.r(S_OUT_R + 2))
                muz.prev_out_q = qnorm({ x = b.r(S_OUT_R + 3), y = b.r(S_OUT_R + 4), z = b.r(S_OUT_R + 5), w = b.r(S_OUT_R + 6) })
            end
            local s = SLOTS[side]
            if best then
                b.w(s.p, best.p.x); b.w(s.p + 1, best.p.y); b.w(s.p + 2, best.p.z)
                b.w(s.n, best.n.x); b.w(s.n + 1, best.n.y); b.w(s.n + 2, best.n.z)
                b.w(s.on, b.r(S_FRAME) + 1.0)
                if os.clock() - hit_log_t[side] > 2.0 and best.d < 0 then
                    hit_log_t[side] = os.clock()
                    L(string.format("the %s %s meets a wall: the hand goes %.0f cm back", side == 1 and "left" or "right",
                        best.why == "muzzle" and "gun's muzzle" or "hand", -best.d * 100))
                end
            end
        end
    end
    if mp then
        b.w(S_MUZZLE_W, mp.x); b.w(S_MUZZLE_W + 1, mp.y); b.w(S_MUZZLE_W + 2, mp.z)
        b.w(S_MUZZLE_ON, b.r(S_FRAME) + 1.0)
    end
end

re.on_application_entry("LateUpdateBehavior", function() local ok, e = pcall(update); if not ok then once("err", "update failed: " .. tostring(e)) end end)

re.on_draw_ui(function()
    if imgui.tree_node("Visceral: hands and guns at walls") then
        local c1, v1 = imgui.checkbox("Hands stop at walls", cfg.enabled); if c1 then cfg.enabled = v1; save_cfg() end
        local c2, v2 = imgui.checkbox("...with a gun in the hand too", cfg.with_gun); if c2 then cfg.with_gun = v2; save_cfg() end
        local c3, v3 = imgui.checkbox("A gun turns away from a wall (off: the hand slides back)", cfg.muzzle_tilt); if c3 then cfg.muzzle_tilt = v3; save_cfg() end
        imgui.tree_pop()
    end
end)

L("loaded: hands stop at walls " .. (cfg.enabled and "ON" or "off") .. ", with a gun " .. (cfg.with_gun and "too" or "no"))
