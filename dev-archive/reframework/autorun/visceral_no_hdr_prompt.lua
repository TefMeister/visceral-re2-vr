-- Visceral (RE2 VR) -- no HDR question at start-up (2026-10-03, Tefa's ask)
--
-- Why: when the game's system save has no HDR answer yet (fresh or downloaded saves reset it), RE2 asks at launch
-- whether to play in HDR. Answering yes in VR freezes the game for about a minute and then says HDR is unavailable;
-- once it seemed to hang for good (Tefa, 2026-10-03). VR draws to the headset, so monitor HDR does nothing for us.
--
-- Lever: app.ropeway.OptionManager asks "is an HDR display connected?" (get_IsHDRDisplayConnect /
-- get_IsHDRDisplayConnected, il2cpp dump 2026-09-25) before it offers HDR. This answers false, so the game takes its
-- ordinary SDR path. The engine's own display settings are left alone. Logs once per method, then once a minute.

local TAG = "[visceral_nohdr] "
local function L(s) log.info(TAG .. s) end

local METHODS = { "get_IsHDRDisplayConnect", "get_IsHDRDisplayConnected" }
local LOG_EVERY_SEC = 60.0

local st = { answered = 0, game_said_true = 0, last_log = 0.0 }

local function on_post(retval)
    st.answered = st.answered + 1
    if (sdk.to_int64(retval) & 1) == 1 then st.game_said_true = st.game_said_true + 1 end
    return sdk.to_ptr(0)
end

local td = sdk.find_type_definition("app.ropeway.OptionManager")
local hooked = 0
if td ~= nil then
    for _, name in ipairs(METHODS) do
        local m = td:get_method(name)
        if m ~= nil then
            sdk.hook(m, function(args) end, on_post)
            hooked = hooked + 1
        else
            L(name .. " not found on this build")
        end
    end
else
    L("app.ropeway.OptionManager not found -- nothing hooked")
end
L(string.format("hooked %d of %d HDR display checks; the game will see no HDR display", hooked, #METHODS))

re.on_frame(function()
    local t = os.clock()
    if t - st.last_log >= LOG_EVERY_SEC and st.answered > 0 then
        st.last_log = t
        L(string.format("answered 'no HDR display' %d time(s); the game would have said yes %d time(s)", st.answered, st.game_said_true))
    end
end)
