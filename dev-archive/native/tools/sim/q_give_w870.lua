-- q_give_w870.lua (RE2, rv.py -f): put the W-870 pump shotgun in hand, whoever the save's character is (memory only: the
-- game saves at typewriters). 2026-11-19.
local inv = sdk.get_managed_singleton("app.ropeway.gamemastering.InventoryManager")
local v = sdk.find_type_definition("app.ropeway.EquipmentDefine.WeaponType"):get_field("WP1000"):get_data(nil)
return "W-870 given: " .. tostring(inv:call("addAndEquipMainWeapon", v, 4, 0) ~= nil)
