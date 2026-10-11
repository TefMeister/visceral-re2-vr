// handpose.h -- the left hand's fingers take a saved pose while it holds a magazine or a shell (2026-10-11, Tefa:
// "let's try doing a first hand pose on the gun when reloading"). The poses are RELOADED's left_hand_pose data from
// the Arcade Controls era config (credit: Andyalpa), per weapon and per character, in handpose_data.h. Like his
// ext_5: each finger joint's LocalEulerAngle is written at LateUpdate and again at PrepareRendering, blended in and
// out over cfg::HANDPOSE_BLEND_SEC. Shells use the gun's magazine pose (his fallback when no shell pose is saved).
#pragma once

namespace vn::handpose {
void point();   // after reload::late_point / reload::render_point
} // namespace vn::handpose
