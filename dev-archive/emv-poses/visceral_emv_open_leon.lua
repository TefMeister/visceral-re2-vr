-- visceral_emv_open_leon.lua - a one-click button that opens the player in alphaZomega's
-- Enhanced Model Viewer, so its Poser can be used on the hands. Dev tool only, never shipped.
-- Needs EMV Engine + Enhanced Model Viewer in autorun. (2026-10-11, /ms hand-pose session)
local ok, EMV = pcall(require, "EMV Engine")
local msg = ""
re.on_draw_ui(function()
    if not imgui.tree_node("Visceral: pose Leon's hands (EMV)") then return end
    if imgui.button("Open the player in the Enhanced Model Viewer") then
        local pm = sdk.get_managed_singleton("app.ropeway.PlayerManager")
        local go = pm and pm:call("get_CurrentPlayer")
        local xf = go and go:call("get_Transform")
        if not xf then
            msg = "No player found (load a save first)."
        elseif not (ok and EMV and EMV.GameObject) then
            msg = "EMV Engine is not loaded."
        else
            _G.forced_object = (held_transforms and held_transforms[xf]) or EMV.GameObject:new{xform = xf}
            msg = "Sent. The Enhanced Model Viewer window should open."
        end
    end
    if msg ~= "" then imgui.text(msg) end
    imgui.tree_pop()
end)
