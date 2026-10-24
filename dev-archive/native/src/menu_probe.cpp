// menu_probe.cpp -- see menu_probe.h. A PROBE: archive it (archive/ + a README line) once the menu flicker is solved.
#include "menu_probe.h"
#include "bridge.h"
#include "common.h"
#include "menu_body.h"
#include "ladder.h"

#include <cstdio>
#include <deque>
#include <string>

namespace vn::menu_probe {

namespace {
constexpr int KEEP_BEFORE = 4;   // frames logged before a menu opens or closes
constexpr int KEEP_AFTER = 6;    // and after
constexpr int KEEP_AFTER_JACK = 20;   // b188: a jack's first third of a second (the switching frames and our hold's snaps)

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

// b188: CameraSystem.MainCameraController.SwitchingCamera, the flag FirstPerson reads at a jack start (its switching-frame
// path, patched in b185): 1/0, or -1 when it cannot be read (the field names are tried both plain and as backing fields)
int switching_camera() {
    auto* cs = API::get()->get_managed_singleton("app.ropeway.camera.CameraSystem");
    for (const char* n : {"<MainCameraController>k__BackingField", "MainCameraController"}) {
        auto* mc = field_obj(cs, n);
        if (mc == nullptr) continue;
        for (const char* f : {"SwitchingCamera", "<SwitchingCamera>k__BackingField"}) {
            auto* p = field_ptr<bool>(mc, f);
            if (p != nullptr) return *p ? 1 : 0;
        }
    }
    return -1;
}

void point(const char* where) {
    if (!bridge::live() || g_dumps >= 160) return;   // b105: was 6, which ran out before the inventory closes; b182: 30 -> 80 (30 ran out after two inventory closes); b188: 160, jacks dump too
    const bool frame_start = std::string(where) == "UpdateBehavior.pre";
    if (frame_start) ++g_frame;
    const bool open = menu_body::probe_menu_open();
    if (open != g_last_open) {
        g_last_open = open;
        LOGI("%s menuprobe ---- menu %s seen at frame %d %s ----", TAG, open ? "OPEN" : "CLOSED", g_frame, where);
        for (auto& s : g_ring) LOGI("%s menuprobe %s", TAG, s.c_str());
        g_ring.clear();
        g_dump_until = g_frame + KEEP_AFTER;
        ++g_dumps;
    }
    // b188: the same dump around a jack's start and end (ladder, cupboard, switch), for the flick at the start of a
    // climb (Tefa 2026-10-16: "ladder still flickers"); the extra fields on every line say whether the game was
    // switching cameras that frame and where our hold had put the controller's yaw
    static bool last_jacked = false;
    if (frame_start) {
        const bool jk = ladder::jacked_now();
        if (jk != last_jacked) {
            last_jacked = jk;
            LOGI("%s menuprobe ---- jack %s seen at frame %d ----", TAG, jk ? "START" : "END", g_frame);
            for (auto& s : g_ring) LOGI("%s menuprobe %s", TAG, s.c_str());
            g_ring.clear();
            g_dump_until = g_frame + (jk ? KEEP_AFTER_JACK : KEEP_AFTER);
            ++g_dumps;
        }
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
    // b182: the raw headset turn (bridge, one pose old) on every line, so the inventory's closing jump can be compared
    // with how far the head is turned: is the jump the headset turn counted twice? [hypothesis 2026-10-17]
    const Quat hq_ = bridge::view_quat(bridge::S_HMD_Q);
    // b188: jk = the game has the body (jack), sw = SwitchingCamera, ct = BusyCameraType, cy = the player camera
    // controller's Yaw as our ladder hold left it (999 = not acquired)
    const int cam_type = call_direct<int>(API::get()->get_managed_singleton("app.ropeway.camera.CameraSystem"), "get_BusyCameraType", -1);
    char buf[480];
    std::snprintf(buf, sizeof buf, "f%d %-24s open=%d gui=%d fp=%d hidden=%d jk=%d sw=%d ct=%d cy=%.3f fov=%.1f pos=%s%.3f %.3f %.3f rot=%s%.3f %.3f %.3f %.3f hmd=%.3f %.3f %.3f %.3f",
                  g_frame, where, (int)open, menu_body::probe_gui_state(), (int)(bridge::view(bridge::S_FP_USED) > 0.5f),
                  (int)menu_body::probe_body_hidden(), (int)last_jacked, switching_camera(), cam_type, ladder::probe_ctrl_yaw(),
                  fov, hp ? "" : "?", p.x, p.y, p.z, hq ? "" : "?", q.x, q.y, q.z, q.w, hq_.x, hq_.y, hq_.z, hq_.w);
    emit(buf);
}

} // namespace vn::menu_probe
