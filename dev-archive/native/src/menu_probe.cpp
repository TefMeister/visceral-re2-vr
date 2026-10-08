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
    if (!bridge::live() || g_dumps >= 30) return;   // b105: was 6, which ran out before the inventory closes
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
    // b105: the picture's zoom (camera FOV) and the dark-edge effect (ToneMapping vignetting) per step, to see whether
    // the inventory close changes either for a frame (Tefa 2026-10-09: "a flicker of the world" on closing the inventory)
    // b107: b105 read the camera, a ToneMapping component (never found) and three values at every point of every frame;
    // Tefa then saw the view jitter on slow head turns. The tone mapping reads are gone and the camera is looked up
    // once per frame; FOV stays (one call). Cost check: the jitter must go with this build, or it was not the probe.
    static int cam_frame = -1;
    static MO* cam = nullptr;
    static API::Method* get_fov = nullptr;
    if (cam_frame != g_frame) { cam_frame = g_frame; cam = (MO*)menu_body::probe_camera(); if (cam && !get_fov) get_fov = find_method_deep(cam->get_type_definition(), "get_FOV"); }
    const float fov = (cam && get_fov) ? get_fov->call<float>(API::get()->get_vm_context(), (void*)cam) : -1.0f;
    char buf[320];
    std::snprintf(buf, sizeof buf, "f%d %-24s open=%d gui=%d fp=%d hidden=%d fov=%.1f pos=%s%.3f %.3f %.3f rot=%s%.3f %.3f %.3f %.3f",
                  g_frame, where, (int)open, menu_body::probe_gui_state(), (int)(bridge::view(bridge::S_FP_USED) > 0.5f),
                  (int)menu_body::probe_body_hidden(), fov, hp ? "" : "?", p.x, p.y, p.z, hq ? "" : "?", q.x, q.y, q.z, q.w);
    emit(buf);
}

} // namespace vn::menu_probe
