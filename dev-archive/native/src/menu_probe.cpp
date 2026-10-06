// menu_probe.cpp -- see menu_probe.h. A PROBE: archive it (archive/ + a README line) once the menu flicker is solved.
#include "menu_probe.h"
#include "bridge.h"
#include "common.h"
#include "menu_body.h"

#include <cstdio>
#include <deque>
#include <string>

namespace vn::menu_probe {

namespace {
constexpr int KEEP_BEFORE = 4;   // frames logged before a menu opens or closes
constexpr int KEEP_AFTER = 6;    // and after

std::deque<std::string> g_ring;  // the last few frames' lines, dumped when a change is seen
int g_frame = 0;
int g_dump_until = -1;
bool g_last_open = false;
int g_dumps = 0;

void emit(const std::string& s) {
    if (g_frame <= g_dump_until) {
        LOGI("%s menuprobe %s", TAG, s.c_str());
        return;
    }
    g_ring.push_back(s);
    while ((int)g_ring.size() > KEEP_BEFORE * 12) g_ring.pop_front();
}
} // namespace

void point(const char* where) {
    if (!bridge::live() || g_dumps >= 6) return;
    if (std::string(where) == "UpdateBehavior.pre") ++g_frame;
    const bool open = menu_body::probe_menu_open();
    if (open != g_last_open) {
        g_last_open = open;
        LOGI("%s menuprobe ---- menu %s seen at frame %d %s ----", TAG, open ? "OPEN" : "CLOSED", g_frame, where);
        for (auto& s : g_ring) LOGI("%s menuprobe %s", TAG, s.c_str());
        g_ring.clear();
        g_dump_until = g_frame + KEEP_AFTER;
        ++g_dumps;
    }
    auto* tf = (MO*)menu_body::probe_camera_tf();
    Vec3 p{0, 0, 0};
    Quat q{0, 0, 0, 1};
    const bool hp = tf && call_vec3(tf, "get_Position", p);
    const bool hq = tf && call_quat(tf, "get_Rotation", q);
    char buf[256];
    std::snprintf(buf, sizeof buf, "f%d %-24s open=%d hidden=%d cam=%p pos=%s%.3f %.3f %.3f rot=%s%.3f %.3f %.3f %.3f",
                  g_frame, where, (int)open, (int)menu_body::probe_body_hidden(), (void*)tf, hp ? "" : "?", p.x, p.y, p.z,
                  hq ? "" : "?", q.x, q.y, q.z, q.w);
    emit(buf);
}

} // namespace vn::menu_probe
