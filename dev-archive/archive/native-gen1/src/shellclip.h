// shellclip.h -- the game's own finger movement for a shell going in, played on the left hand (2026-11-02, Opus; ported from
// visceral-re8-vr src/shellclip.cpp, where it was proven in a window: the fingers matched the clip within 0.2-0.5 deg).
//
// Tefa, 2026-11-02 (Village first, then "please port the shotgun reloads over to both re2 and re3"): the hand docks at the
// shell port, LT puts the shells in, each with the game's own hand movement.
// RE2 plays a whole shotgun reload as one motion, so the clip is a time window of it (one push of the left fingers, cut by
// visceral-re8-vr tools/make_finger_clip.py --re2 --window) in reframework/data/visceral/clips/wp<NNNN>_round_in.csv (the
// game's animation data: on the PC only, never on GitHub). Only the 17 left finger joints' local rotations are written,
// blended in and out; the hand stays where the controller / the dock puts it. Written last at PrepareRendering, after
// handpose.cpp, so it wins while it plays. Test without a headset: write "wp1000" into clips/play.txt.
#pragma once

namespace vn::shellclip {

void play(int wp);       // reload.cpp: a shell went in (wp = the gun's WP number)
void frame();            // UpdateBehavior pre: the test file
void render_point();     // PrepareRendering post, after every other finger writer
bool playing();

} // namespace vn::shellclip
