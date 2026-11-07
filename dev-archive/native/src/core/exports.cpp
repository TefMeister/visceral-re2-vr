// exports.cpp -- two of the four functions our patched REFramework looks up in this DLL (design section 7). The other two,
// visceral_vr_buttons and visceral_sub_weapon_out, live in body/subweapon.cpp as before. Names and layouts must never
// change: the loader's patch (dev-archive/reframework-patch/, newest 2026-11-02) declares the same structs.
#include "exports.h"

namespace {
vn::exports::VisceralDockSocket g_socket{2, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0};   // version 2, not active: the loader docks by itself
} // namespace

namespace vn::exports {
VisceralDockSocket& dock_socket() { return g_socket; }
} // namespace vn::exports

// The left hand's socket on the gun while our rack holds it (layer 3 fills it; inactive until then).
extern "C" __declspec(dllexport) vn::exports::VisceralDockSocket* visceral_dock_socket() { return &g_socket; }

// The gen-1 tuning sliders' "hold both hands still" box. The sliders are gone (design item 31): always 0.
extern "C" __declspec(dllexport) int visceral_hands_frozen() { return 0; }
