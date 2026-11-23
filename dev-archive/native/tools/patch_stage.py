"""patch_stage.py -- one-off (2026-11-23): wires guns/stage.cpp in (the left hand's stage number and the gun for Hand Poser).
Run once from anywhere; it refuses when a place it expects is not there."""
import io, os, sys
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

def sub(path, old, new):
    s = io.open(path, encoding="utf-8", newline="").read()
    if "\r\n" in s: old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
    if new in s: print("already:", path); return
    if old not in s: sys.exit("NOT FOUND in %s: %r" % (path, old[:70]))
    io.open(path, "w", encoding="utf-8", newline="").write(s.replace(old, new, 1)); print("patched:", path)

sub("src/core/bridge.h", "    S_HT2 = 540,",
    "    // 2026-11-23 (src/guns/stage.h), WRITTEN BY THE PLUGIN for Hand Poser: the left hand's stage (Village's spot numbers), how far a\n"
    "    // magazine riding the hand is out (0 = not riding, else 1 + 0..1); 651 / 652 kept for the left hand's area and the right hand's spot\n"
    "    S_LEFT_STAGE = 649, S_MAG_PULL = 650,\n"
    "    S_HT2 = 540,")
sub("src/guns/rack.h", "bool active();                // the left hand is on the slide / fore-end\n",
    "bool active();                // the left hand is on the slide / fore-end\n"
    "bool at_back();               // ... and the part reached the back this cycle (stage.cpp)\n"
    "bool gripping();              // a pump gun's fore-end held with LG alone (stage.cpp)\n")
sub("src/guns/rack.cpp", "bool active() { return g_hand_on; }\n",
    "bool active() { return g_hand_on; }\n"
    "bool at_back() { return g_hand_on && g_pulled; }\n"
    "bool gripping() { return g_grip && !g_hand_on; }\n")
sub("src/guns/magazine.h", "bool hand_docked();",
    "int  left_stage(float& pull);   // stage.cpp: 0 = the magazine has no say, 4 = the hand on it seated or riding it in, 13 = in the hand or sliding out into it; pull = 0, or 1 + how far out\n"
    "bool hand_docked();")
sub("src/guns/magazine.cpp", "bool hand_docked() {\n",
    "int left_stage(float& pull) {\n"
    "    pull = 0.0f;\n"
    "    if (!ours() || g_joint == nullptr) return 0;\n"
    "    if (g_st == St::IN_HAND) return 13;\n"
    "    if (g_st == St::SLIDE_OUT && g_out_to_hand) { pull = 1.0f + ease((now_s() - g_t0) / cfg::RELOAD_SLIDE_SEC); return 13; }\n"
    "    if (g_st == St::INSERT) { pull = 2.0f - ease((now_s() - g_t0) / cfg::RELOAD_INSERT_SEC); return 4; }\n"
    "    if (g_st == St::SEATED && g_hand_on_mag) { pull = 1.0f; return 4; }\n"
    "    return 0;\n"
    "}\n"
    "bool hand_docked() {\n")
sub("src/guns/cylinder.h", "bool is_open();", "bool hand_on_drum();     // LG held with the left wrist at the drum (stage.cpp)\nbool is_open();")
sub("src/guns/cylinder.cpp", "bool is_open() {",
    "bool hand_on_drum() { return ours() && bridge::held(bridge::S_LGRIP) && hand_at_drum(); }\nbool is_open() {")
sub("src/guns/guns.cpp", '#include "shells.h"\n', '#include "shells.h"\n#include "stage.h"\n')
sub("src/guns/guns.cpp", "    left_hand::frame();\n}", "    left_hand::frame();\n    stage::publish();\n}")
sub("src/guns/guns.cpp", "shells::late_point(); rack::late_point(); }", "shells::late_point(); rack::late_point(); stage::publish(); }")
sub("lua/visceral_bridge.lua", "local N = 649   --", "local N = 653   -- 2026-11-23: 649 / 650 the left hand's stage and the magazine pull for Hand Poser (src/guns/stage.h), 651 / 652 kept;")
sub("lua/visceral_bridge.lua", "    w(S_SENTINEL, SENTINEL)\n", "    w(84, -1.0)   -- the gun in hand (src/guns/stage.cpp writes it): no gun until the plugin says so (Hand Poser read 0 = the first gun)\n    w(S_SENTINEL, SENTINEL)\n")
sub("lua/visceral_bridge.lua", "    w = function(slot, v) w(slot, v) end,\n", "    w = function(slot, v) w(slot, v) end,\n    n = N,\n")
