// magazine_internal.h -- the magazine's state, shared by magazine.cpp (which owns and changes it) and magazine_pose.cpp
// (which draws it and works out where the hand goes). Not for any other file.
#pragma once
#include "gun_data.h"
#include "magazine.h"

namespace vn::guns::magazine::detail {

enum class St { SEATED, SLIDE_OUT, FALL, OUT, IN_HAND, HAND_FALL, INSERT };

extern St g_st;
extern const MagGun* g_w;     // the gun in hand, if it is a magazine gun
extern MO* g_joint;           // its magazine joint this frame
extern float g_t0;            // when the state began
extern Vec3 g_rest_pos, g_rest_scale, g_slide_from, g_fall_from;
extern Quat g_rest_rot, g_fall_rot;
extern bool g_scale_restore;  // seated again: the rest scale is written once
extern bool g_hand_on_mag;    // the left hand is on the seated magazine
extern bool g_out_to_hand;    // this slide-out ends in the left hand

// magazine_pose.cpp
void pose_joint();                                  // write the joint for the current state
bool hand_pose(Vec3& pos, Quat& rot);               // where a magazine in the left hand is (world)
bool mag_local_now(Vec3& p, Quat& q);               // the magazine's place in the gun's frame this frame
bool dock_wrist(Vec3 lp, Quat lq, float down, Vec3& wp, Quat& wq);   // the wrist holding it there
float hand_to_dock();                               // the drawn left wrist to its spot on the seated magazine (m; 99 = unknown)
void write_scale(Vec3 s);

} // namespace vn::guns::magazine::detail
