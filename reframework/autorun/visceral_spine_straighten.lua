-- Visceral (RE2 VR) -- spine straighten (torso-twist removal)
--
-- Rebuilt from our own Arcade Controls work (re2_vr_posture_spine_straighten_
-- override.lua) for the ground-up Visceral codebase. Removes the character's
-- twisted weapon-hold torso pose (Claire/Ada especially) so the upper body
-- faces forward, WITHOUT killing the live gait/breathing motion.
--
-- Soft-baseline method (the version AC settled on):
--   Per spine bone, keep a slowly-adapting EMA "rest" rotation (base) of the
--   ANIMATED local rotation. Straighten the rest pose toward identity, take the
--   difference (offset = corrected_base * inverse(base)), and apply that offset
--   on top of the live animation each frame. So the twist is removed but the
--   animation keeps moving. The baseline FREEZES while aiming so the aim
--   reference frame (red dot) can't drift mid-aim.
-- Written at the pre-IK hook AND re-applied on every IkArmFit.updateIk call,
-- because native arm IK runs several times per frame and would otherwise solve
-- the hands/gun against the un-corrected spine.

if reframework:get_game_name() ~= "re2" then
    return
end

local TAG = "[visceral_spine]"
local NS = sdk.game_namespace
local VK_NUMPAD4 = 0x64

local cfg = {
    enabled = true,
    strength = 1.0,                 -- 0 = no change, 1 = fully straight
    soft_baseline_tau_s = 0.4,      -- how slowly the rest pose follows animation
    freeze_while_aiming = true,     -- hold the rest pose steady while aiming
    correct_upper_chain = true,     -- spine_0 + spine_1 + spine_2 (else spine_0 only)
    cutscene_gate = true,           -- skip during cutscenes if a blocker flag is present
}

local UPPER_CHAIN = { "spine_0", "spine_1", "spine_2" }
local SETTERS = { "set_LocalRotation", "setLocalRotation" }

local state = { keys_ok = true, prev = false, setter = nil, status = "idle",
    applied = 0, jog = nil }
local soft = { baselines = {}, last_t = nil, alpha = 0.0 }

local function safe(fn) local ok, r = pcall(fn); if ok then return r end return nil end
local function log_line(m) log.info(TAG .. " " .. m) end

-- ---- quaternion helpers ----
local function qnorm(x, y, z, w)
    local l = math.sqrt(x * x + y * y + z * z + w * w)
    if l < 1e-8 then return 0, 0, 0, 1 end
    return x / l, y / l, z / l, w / l
end
local function qdot(a, b) return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w end
local function qmul(a, b) local ok, q = pcall(function() return a * b end); return ok and q or a end
local function qinv(q) local ok, i = pcall(function() return q:inverse() end); return ok and i or nil end

-- advance rest pose toward the animated rotation q by alpha (nlerp, hemisphere-fixed)
local function nlerp(base, q, alpha)
    local sx, sy, sz, sw = q.x, q.y, q.z, q.w
    if qdot(base, q) < 0 then sx, sy, sz, sw = -sx, -sy, -sz, -sw end
    base.x, base.y, base.z, base.w = qnorm(
        base.x + (sx - base.x) * alpha, base.y + (sy - base.y) * alpha,
        base.z + (sz - base.z) * alpha, base.w + (sw - base.w) * alpha)
end

-- blend a rotation toward identity by strength (cheap component lerp + renorm)
local function toward_identity(q, s)
    local x, y, z = q.x * (1 - s), q.y * (1 - s), q.z * (1 - s)
    local w = q.w * (1 - s) + 1.0 * s
    return qnorm(x, y, z, w)
end

local function update_alpha()
    local now = os.clock()
    local dt = soft.last_t and math.max(0.0, now - soft.last_t) or 0.0
    soft.last_t = now
    if dt > 1e-4 then
        soft.alpha = 1.0 - math.exp(-dt / math.max(0.05, cfg.soft_baseline_tau_s))
    else
        soft.alpha = 0.0   -- same-frame repeat (IK re-call): don't advance again
    end
end
local function reset_baselines() soft.baselines = {}; soft.last_t = nil; soft.alpha = 0.0 end

-- ---- game accessors ----
local function get_player()
    local pm = sdk.get_managed_singleton(NS("PlayerManager")); if not pm then return nil end
    return safe(function() return pm:call("get_CurrentPlayer") end)
end
local function get_transform(player) return safe(function() return player:call("get_Transform") end) end
local function get_joint(tf, name) return safe(function() return tf:call("getJointByName", name) end) end
local function is_aiming(player)
    local t = sdk.typeof(NS("survivor.SurvivorCondition")); if not t then return false end
    local c = safe(function() return player:call("getComponent(System.Type)", t) end); if not c then return false end
    return safe(function() return c:call("get_IsHold") end) == true
end
local function cutscene_blocking()
    -- honor Visceral's shared cinematic gate (cutscenes, camera events, enemy grab)
    if cfg.cutscene_gate and type(_G.__visceral_cinematic_blocking) == "function" then
        return _G.__visceral_cinematic_blocking() == true
    end
    return false
end

local function set_local_rotation(joint, x, y, z, w)
    if state.setter then
        if pcall(function() joint:call(state.setter, Quaternion.new(w, x, y, z)) end) then return true end
        state.setter = nil
    end
    for _, m in ipairs(SETTERS) do
        if pcall(function() joint:call(m, Quaternion.new(w, x, y, z)) end) then state.setter = m; return true end
    end
    return false
end

-- ---- core correction ----
local function apply(player)
    local tf = get_transform(player); if not tf then state.status = "no transform"; return end
    update_alpha()
    local s = cfg.strength
    local names = cfg.correct_upper_chain and UPPER_CHAIN or { "spine_0" }
    local freeze = cfg.freeze_while_aiming and is_aiming(player)
    local applied = 0

    for _, name in ipairs(names) do
        local joint = get_joint(tf, name)
        if joint then
            local r = safe(function() return joint:call("get_LocalRotation") end)
            if r and type(r.y) == "number" then
                local bl = soft.baselines[name]
                if not bl then
                    bl = { base = { x = r.x, y = r.y, z = r.z, w = r.w },
                           last_raw = { x = r.x, y = r.y, z = r.z, w = r.w }, last_written = nil }
                    soft.baselines[name] = bl
                end
                -- stale-read guard: if the joint still holds our last write, reuse last_raw
                local anim
                if bl.last_written and math.abs(qdot(r, bl.last_written)) > 0.999999 then
                    anim = bl.last_raw
                else
                    bl.last_raw = { x = r.x, y = r.y, z = r.z, w = r.w }
                    anim = bl.last_raw
                end
                if soft.alpha > 0.0 and not freeze then nlerp(bl.base, anim, soft.alpha) end

                local bx, by, bz, bw = toward_identity(bl.base, s)
                local q_base = Quaternion.new(bl.base.w, bl.base.x, bl.base.y, bl.base.z)
                local q_corr = Quaternion.new(bw, bx, by, bz)
                local q_off = qmul(q_corr, qinv(q_base))
                local q_anim = Quaternion.new(anim.w, anim.x, anim.y, anim.z)
                local q_out = qmul(q_off, q_anim)
                local nx, ny, nz, nw = qnorm(q_out.x, q_out.y, q_out.z, q_out.w)
                if set_local_rotation(joint, nx, ny, nz, nw) then
                    applied = applied + 1
                    bl.last_written = { x = nx, y = ny, z = nz, w = nw }
                end
            end
        end
    end
    state.applied = applied
    state.status = applied > 0
        and string.format("applied to %d/%d joints (strength %.2f)", applied, #names, s)
        or "no setter worked (joint may be read-only)"
end

local function gated_player()
    if not cfg.enabled then return nil end
    if cutscene_blocking() then state.status = "skipped (cutscene)"; return nil end
    return get_player()
end

-- ---- IK re-apply hook: native arm IK runs several times per frame ----
local ik = { installed = false, warned = false }
local function install_ik_hook()
    if ik.installed then return end
    local t = sdk.find_type_definition(NS("IkArmFit"))
    if not t then if not ik.warned then log_line("IkArmFit type not found"); ik.warned = true end return end
    local hooked = 0
    for _, m in ipairs(t:get_methods() or {}) do
        if m and safe(function() return m:get_name() end) == "updateIk" then
            pcall(function()
                sdk.hook(m, function() local p = gated_player(); if p then apply(p) end end,
                    function(rv) return rv end)
            end)
            hooked = hooked + 1
        end
    end
    if hooked > 0 then ik.installed = true; log_line("hooked IkArmFit.updateIk x" .. hooked) end
end
install_ik_hook()

re.on_pre_application_entry("LateUpdateBehavior", function()
    if not cfg.enabled then reset_baselines(); return end
    local p = gated_player(); if p then apply(p) end
end)

re.on_frame(function()
    if not state.keys_ok then return end
    local d = safe(function() return reframework:is_key_down(VK_NUMPAD4) end)
    if d == nil then state.keys_ok = false; return end
    if d and not state.prev then cfg.enabled = not cfg.enabled; log_line("enabled=" .. tostring(cfg.enabled)) end
    state.prev = d
end)

re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: spine straighten (torso-twist removal)") then return end
    local ch
    ch, cfg.enabled = imgui.checkbox("ENABLED (NUM4)", cfg.enabled)
    ch, cfg.strength = imgui.slider_float("strength", cfg.strength, 0.0, 1.0, "%.2f")
    ch, cfg.soft_baseline_tau_s = imgui.slider_float("rest-pose follow (tau s)", cfg.soft_baseline_tau_s, 0.1, 1.5, "%.2f")
    ch, cfg.freeze_while_aiming = imgui.checkbox("freeze rest pose while aiming", cfg.freeze_while_aiming)
    ch, cfg.correct_upper_chain = imgui.checkbox("correct spine_0+1+2 (off = spine_0 only)", cfg.correct_upper_chain)
    ch, cfg.cutscene_gate = imgui.checkbox("skip during cutscenes", cfg.cutscene_gate)
    imgui.separator()
    imgui.text("status: " .. state.status)
    imgui.text("IK re-apply hook: " .. (ik.installed and "installed" or "NOT installed"))
    imgui.tree_pop()
end)

rawset(_G, "__visceral_spine_enabled", cfg.enabled)
log_line("loaded (spine straighten, soft-baseline). NUM4 toggles.")
