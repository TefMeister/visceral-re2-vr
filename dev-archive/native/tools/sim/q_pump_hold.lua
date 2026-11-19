-- q_pump_hold.lua (RE2 / RE3; RV_GAME_DIR set, rv.py -f): where the drawn left hand sits against the pump shotgun's fore-end.
-- In the GUN's own axes (x right, y up, z to the muzzle), metres: the wrist and the palm's middle from the fore-end joint
-- (y below 0 = UNDER the handle), and which way the palm faces (the side the curled fingers are on): toward the handle or
-- away. 2026-11-19, for "the left hand docks above the pump handle, not under it".
local NS = reframework:get_game_name() == "re3" and "offline." or "app.ropeway."
local pl = sdk.get_managed_singleton(NS .. "PlayerManager"):call("get_CurrentPlayer")
local w = pl:call("getComponent(System.Type)", sdk.typeof(NS .. "survivor.Equipment")):get_field("<EquipWeapon>k__BackingField")
if w == nil then return "no gun in hand" end
local gtf = w:call("get_GameObject"):call("get_Transform")
local ptf = pl:call("get_Transform")
local h = sdk.find_type_definition("via.murmur_hash"):get_method("calc32")
local function J(tf, n) return tf:call("getJointByHash", h:call(nil, n)) end
local fore = J(gtf, "_01")
if fore == nil then return "no fore-end joint _01" end
local gq = gtf:call("get_Rotation")
local X, Y, Z = gq * Vector3f.new(1, 0, 0), gq * Vector3f.new(0, 1, 0), gq * Vector3f.new(0, 0, 1)
local function loc(v) return v:dot(X), v:dot(Y), v:dot(Z) end
local jp = fore:call("get_Position")
local wr = J(ptf, "l_arm_wrist") or J(ptf, "l_hand") or J(ptf, "L_Hand")
local i0, l0, m0, m2 = J(ptf, "l_hand_index_0"), J(ptf, "l_hand_little_0"), J(ptf, "l_hand_middle_0"), J(ptf, "l_hand_middle_2")
if not (wr and i0 and l0 and m0 and m2) then return "left hand joints missing" end
local wp, ip, lp, mp, tp = wr:call("get_Position"), i0:call("get_Position"), l0:call("get_Position"), m0:call("get_Position"), m2:call("get_Position")
local palm = (wp + ip + lp) * (1.0 / 3.0)
local n = (ip - wp):cross(lp - wp); n = n * (1.0 / n:length())
if n:dot(tp - mp) < 0 then n = n * -1.0 end        -- the palm is on the side the fingers curl to
local to_fore = jp - palm; local d = to_fore:length()
local wx, wy, wz = loc(wp - jp)
local px, py, pz = loc(palm - jp)
local nx, ny, nz = loc(n)
local fx, fy, fz = loc(ip - wp)
return string.format("wrist from the fore-end: right %.3f up %.3f fwd %.3f | palm middle: right %.3f up %.3f fwd %.3f (%.3f m away) | palm faces: right %.2f up %.2f fwd %.2f (toward the handle %.2f) | fingers point: right %.2f up %.2f fwd %.2f | gun up in world y %.2f",
  wx, wy, wz, px, py, pz, d, nx, ny, nz, n:dot(to_fore) / math.max(d, 1e-4), fx, fy, fz, Y.y)
