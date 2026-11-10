// gun_data.h -- what the gun code knows about each gun: the shapes of the per-gun numbers and the lookups. The numbers
// themselves are each game's own (src/game/guns_table.cpp). Guns a lookup does not know keep the game's own reload.
#pragma once
#include "common.h"

namespace vn::guns {

// where the magazine sits in the left hand: offset from the left wrist joint in the wrist's axes (metres), and a turn
// (degrees: yaw about Z, then pitch about Y, then roll about X)
struct Hold { float ox, oy, oz, yaw, pitch, roll; };

// a magazine gun: the joint that IS the magazine, where it sits just out of the magwell (the joint's local space), how close
// a magazine must come to go in, the sound folder under reframework/data/custom_sfx/ and its volume, the hold per character
struct MagGun { int wp; const char* joint; Vec3 exit; float dock; const char* sfx; float vol; Hold leon, claire; };

// a slide or a pump: the joint that moves and its local-Z travel: rest = closed, parked = locked open, back = fully pulled
struct SlideGun { int wp; const char* joint; float rest_z, parked_z, back_z; };

// a pump gun's shells: the shell is the gun's own hidden mesh part on a joint; the port is where it goes in (the gun's
// local space); his insert distance (ours is capped by SHELL_PORT_DOCK_M). A gun with no shell of its own to show: joint ""
// and mesh_part -1 (the hand docks and LT loads; nothing is drawn in the hand yet)
struct ShellGun { int wp; const char* joint; int mesh_part; Vec3 port; float dock; const char* sfx; float vol; };

// a gun that takes more than one kind of round: the kinds as the game's item numbers, the gun's normal round first (0 = unused)
// A drum the hand loads chamber by chamber: the joint that IS the drum (it turns about its own Z), which way it turns as
// it fires (+1 / -1), and the chamber joints in the order they come to the barrel, the one at the barrel at rest first.
// No chamber names: a plain list, first in first out.
struct RoundGun { int wp; int items[4]; const char* drum; float turn_sign; const char* chamber[6]; };

const MagGun* mag_gun(int wp);
const RoundGun* round_gun(int wp);
const SlideGun* slide_gun(int wp);
const ShellGun* shell_gun(int wp);
bool is_pump(int wp);
const char* pump_sfx(int wp);
bool long_mag(int wp);    // the magazine sits ahead of the grip: LG on it + LT pulls it out, B does nothing

} // namespace vn::guns
