-- visceral_options_book.lua -- THE IN-WORLD OPTIONS BOOK, FIRST MILESTONE (2026-11-10, Fable). Shared by RE2 and RE3.
--
-- Tefa (2026-11-07): "if you can just create a notebook with leather covers for now and have that show up in the game,
-- that would be awesome" (RE2: the officer's notebook; RE3: the UBCS file folder). "Big, just in front" of the player,
-- in the game world, X + A held 0.5 s.
--
-- WHAT THIS IS: the book hanging in the world, brought out and put away. Until Tefa's own Blender model exists it is
-- THE GAME'S OWN officer's notebook (the 3D model from the scene where the officer hands it over, pages and leather
-- as the game made them), three times life size. No options on it yet.
--
-- WHAT WAS PROVED GETTING HERE [verified-live 2026-11-10, RE2 on the OpenXR Simulator, n=1 each]:
--   * a model of the game's own, made in play by script, IS drawn in the headset picture, in the world, with its real
--     materials: via.GameObject.create(name, <the camera's folder>) + a via.render.Mesh component + setMesh /
--     set_Material with holders from sdk.create_resource. Created with no folder it drew nothing.
--   * a page's picture can be changed while it hangs there: Mesh.updatableMaterial(), then
--     Mesh.setMaterialTexture(material, slot, holder); without the first call the second throws. Slot 0 = BaseMetalMap.
--   So the pages can carry our own pictures (Tefa's hand-written options) once those are texture files the game loads.
--
-- Open / close: X + A held 0.5 s (the A-button action on both hands), or the file
-- reframework/data/visceral/options_book_cmd.json {"cmd": "open" | "close" | "toggle"} for driving it from the desk.
-- Numbers (per game, below): size, distance, the model's own turn and the sideways / up nudge its pivot needs.

local TAG = "[visceral-book]"
local CMD_FILE = "visceral/options_book_cmd.json"
local HOLD_S = 0.5
local CMD_EVERY_FRAMES = 20

local game = reframework:get_game_name()
local MODELS = {
    re2 = {
        mesh = "objectroot/setmodel/sm6x_event/sm60/sm60_035_pocketbook01a/sm60_035_pocketbook01a_00md.mesh",
        mdf = "objectroot/setmodel/sm6x_event/sm60/sm60_035_pocketbook01a/sm60_035_pocketbook01a_00md_mat.mdf2",
        scale = 5.0, dist = 1.0, yaw = 90.0, pitch = 0.0, side = -0.58, up = -0.05,
    },
    -- RE3: the game's own UBCS binder (a set-dressing prop). Its numbers are guesses until seen.
    re3 = {
        mesh = "escape/setmodel/sm3x_artifactprops/sm36/sm36_465_es_ubcsbinder01a/sm36_465_es_ubcsbinder01a_00ms.mesh",
        mdf = "escape/setmodel/sm3x_artifactprops/sm36/sm36_465_es_ubcsbinder01a/sm36_465_es_ubcsbinder01a_00ms_mat.mdf2",
        scale = 2.0, dist = 0.8, yaw = 0.0, pitch = 90.0, side = 0.0, up = 0.0,
    },
}
local M = MODELS[game]
_G.VISCERAL_OPTIONS_BOOK = _G.VISCERAL_OPTIONS_BOOK or {}
local B = _G.VISCERAL_OPTIONS_BOOK
B.model = M
B.open = false
B.status = M and "closed" or ("no model set for " .. tostring(game))

local function L(s) log.info(TAG .. " " .. tostring(s)) end
local function safe(f, ...) local ok, v = pcall(f, ...); if ok then return v end; return nil end

local go, meshc = nil, nil
local holders = nil

local function yaw_quat(a) return Quaternion.new(math.cos(a * 0.5), 0.0, math.sin(a * 0.5), 0.0) end
local function pitch_quat(a) return Quaternion.new(math.cos(a * 0.5), math.sin(a * 0.5), 0.0, 0.0) end

local function destroy()
    if go ~= nil then pcall(function() go:call("destroy", go) end) end
    go, meshc, holders = nil, nil, nil
end

local function spawn()
    if M == nil then return false end
    local cam = sdk.get_primary_camera()
    local cam_go = cam and cam:call("get_GameObject")
    if cam_go == nil then B.status = "no camera"; return false end
    local folder = safe(function() return cam_go:call("get_Folder") end)
    local td = sdk.find_type_definition("via.GameObject")
    local g = folder and td:get_method("create(System.String, via.Folder)"):call(nil, "VisceralOptionsBook", folder)
              or td:get_method("create(System.String)"):call(nil, "VisceralOptionsBook")
    if g == nil then B.status = "could not make the object"; return false end
    g = g:add_ref()
    pcall(function() g:call(".ctor") end)
    local m = g:call("createComponent(System.Type)", sdk.typeof("via.render.Mesh"))
    if m == nil then B.status = "could not make the mesh part"; pcall(function() g:call("destroy", g) end); return false end
    m = m:add_ref()
    pcall(function() m:call(".ctor()") end)
    local mres = sdk.create_resource("via.render.MeshResource", M.mesh)
    local dres = sdk.create_resource("via.render.MeshMaterialResource", M.mdf)
    if mres == nil or dres == nil then
        B.status = "the model's files did not load (mesh " .. tostring(mres ~= nil) .. ", material " .. tostring(dres ~= nil) .. ")"
        pcall(function() g:call("destroy", g) end)
        return false
    end
    mres, dres = mres:add_ref(), dres:add_ref()
    holders = { mesh = mres:create_holder("via.render.MeshResourceHolder"):add_ref(),
                mdf = dres:create_holder("via.render.MeshMaterialResourceHolder"):add_ref(), made = 1 }
    m:call("setMesh", holders.mesh)
    m:call("set_Material", holders.mdf)
    pcall(function() m:call("set_DrawShadowCast", false) end)

    -- in front of the head, facing it, level; it then stays where it was put (in the world, not on the head)
    local ctf = cam_go:call("get_Transform")
    local cp, cq = ctf:call("get_Position"), ctf:call("get_Rotation")
    local f = cq * Vector3f.new(0, 0, -1)
    f.y = 0.0
    f = f:normalized()
    local r = cq * Vector3f.new(1, 0, 0)
    r.y = 0.0
    r = r:normalized()
    local pos = Vector3f.new(cp.x + f.x * M.dist + r.x * M.side, cp.y + M.up, cp.z + f.z * M.dist + r.z * M.side)
    local q = yaw_quat(math.atan(f.x, f.z)) * yaw_quat(math.rad(M.yaw)) * pitch_quat(math.rad(M.pitch))
    local tf = g:call("get_Transform")
    tf:call("set_Position", pos)
    tf:call("set_Rotation", q)
    tf:call("set_LocalScale", Vector3f.new(M.scale, M.scale, M.scale))
    go, meshc = g, m
    B.mesh = m
    return true
end

function B.set_open(want)
    if want == B.open then return end
    if want then
        if spawn() then B.open = true; B.status = "open"; L("out, in front of the player") else L("could not bring it out: " .. tostring(B.status)) end
    else
        destroy()
        B.open = false
        B.status = "closed"
        L("put away")
    end
end

-- a page's picture: slot 0 is the page's colour picture on the game's notebook (BaseMetalMap)
function B.set_page_texture(material, slot, tex_path)
    if meshc == nil then return false, "the book is not out" end
    local r = sdk.create_resource("via.render.TextureResource", tex_path)
    if r == nil then return false, "that picture did not load" end
    r = r:add_ref()
    meshc:call("updatableMaterial")
    local ok, e = pcall(function() meshc:call("setMaterialTexture", material, slot, r:create_holder("via.render.TextureResourceHolder"):add_ref()) end)
    return ok, e
end

local held_since, fired = nil, false
local tick = 0
local function both_held()
    local vr = vrmod
    if vr == nil or not safe(function() return vr:is_hmd_active() end) then return false end
    local a = safe(function() return vr:get_action_a_button() end)
    local lj = safe(function() return vr:get_left_joystick() end)
    local rj = safe(function() return vr:get_right_joystick() end)
    if a == nil or lj == nil or rj == nil then return false end
    return safe(function() return vr:is_action_active(a, lj) end) == true and safe(function() return vr:is_action_active(a, rj) end) == true
end

re.on_pre_application_entry("UpdateBehavior", function()
    if M == nil then return end
    -- X + A held
    if both_held() then
        held_since = held_since or os.clock()
        if not fired and os.clock() - held_since >= HOLD_S then fired = true; B.set_open(not B.open) end
    else
        held_since, fired = nil, false
    end
    -- the desk's command file
    tick = tick + 1
    if tick % CMD_EVERY_FRAMES == 0 then
        local c = json.load_file(CMD_FILE)
        if c ~= nil and c.cmd ~= nil and c.cmd ~= "" then
            json.dump_file(CMD_FILE, { cmd = "" })
            L("command: " .. tostring(c.cmd))
            if c.cmd == "open" then B.set_open(true) elseif c.cmd == "close" then B.set_open(false) elseif c.cmd == "toggle" then B.set_open(not B.open) end
        end
    end
    -- THE FIRST ASK FOR THE MODEL'S FILES IN A SESSION GIVES A BOOK THAT NEVER DRAWS (seen twice 2026-11-10: the object is
    -- there, MaterialReady false, 0 materials; handing the files to the same mesh part again did not help, a NEW object
    -- a moment later drew at once). So: not ready after a few frames = take it away and make it again.
    if B.open and meshc ~= nil and holders ~= nil and tick % 6 == 0 then
        if safe(function() return meshc:call("get_MaterialReady") end) == true and (safe(function() return meshc:call("get_MaterialNum") end) or 0) > 0 then
            L("drawn (made " .. tostring(holders.made) .. " time(s))")
            holders = nil
        else
            local made = holders.made
            if made < 12 then
                destroy()
                if spawn() then holders.made = made + 1 else B.open = false end
            else
                L("the book never drew after " .. tostring(made) .. " tries: put away")
                destroy(); B.open = false; B.status = "closed (it would not draw)"
            end
        end
    end
    -- the scene changed under it (a load, a cutscene): the object is gone, so is "open"
    if B.open and go ~= nil and safe(function() return go:call("get_Valid") end) == false then go, meshc = nil, nil; B.open = false; B.status = "closed (the scene changed)" end
end)

re.on_script_reset(function() destroy() end)
L("loaded for " .. tostring(game) .. ": " .. B.status)
