// ladder.cpp -- see ladder.h. A C++ re-write of Arcade Controls' v12.2 hold, same steps in the same order;
// only the experiments AC itself had switched off (comfort cone, camera clamp, head-yaw lock) are left out.
// Every yaw is radians, atan2(forward.x, forward.z); the view readings come from the bridge (bridge.h S_*_YAW).
#include "ladder.h"
#include "bridge.h"
#include "common.h"
#include "settings.h"

#include <cctype>
#include <chrono>
#include <vector>

namespace vn::ladder {

namespace {
constexpr float DEG = 3.14159265f / 180.0f;

float now_s() {
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
}

// ---- the player ------------------------------------------------------------------------------------------
MO* player() { return call_ptr(API::get()->get_managed_singleton("app.ropeway.PlayerManager"), "get_CurrentPlayer"); }
MO* player_transform() { return call_ptr(player(), "get_Transform"); }

bool body_rot(MO* tf, Quat& q) { return tf != nullptr && call_quat(tf, "get_Rotation", q); }
bool body_yaw(MO* tf, float& y) {
    Quat q;
    if (!body_rot(tf, q)) return false;
    y = yaw_of(rotate(q, {0.0f, 0.0f, 1.0f}));   // the body faces +Z (Arcade Controls)
    return true;
}
void set_body_rot(MO* tf, const Quat& q) {
    // a 16-byte value type goes by reference (the old plugin's proven transform write, read for the idea)
    auto* m = tf ? find_method_deep(tf->get_type_definition(), "set_Rotation") : nullptr;
    if (m != nullptr) m->call<void>(API::get()->get_vm_context(), (void*)tf, (void*)&q);
}

bool jacked() {
    return call_direct<bool>(component(player(), "app.ropeway.JackDominator"), "get_Jacked", false);
}

// ---- climbing: the strongest animation on each of the player's motion layers (Arcade Controls' read) -----
struct MotionCalls { API::Method *count = nullptr, *layer = nullptr, *node = nullptr, *name = nullptr; bool tried = false; };
MotionCalls g_mc;
std::string g_layer0;
int g_motion_logs = 0;
float g_motion_log_t = -10.0f;

bool motion_calls() {
    if (g_mc.tried) return g_mc.name != nullptr;
    g_mc.tried = true;
    auto* tdb = API::get()->tdb();
    g_mc.count = tdb->find_method("via.motion.Motion", "getLayerCount");
    if (g_mc.count == nullptr) g_mc.count = tdb->find_method("via.motion.Motion", "get_LayerCount");
    g_mc.layer = tdb->find_method("via.motion.Motion", "getLayer");
    auto* lt = g_mc.layer ? g_mc.layer->get_return_type() : nullptr;
    g_mc.node = lt ? find_method_deep(lt, "get_HighestWeightMotionNode") : nullptr;
    auto* nt = g_mc.node ? g_mc.node->get_return_type() : nullptr;
    g_mc.name = nt ? find_method_deep(nt, "get_MotionName") : nullptr;
    LOGI("%s ladder: motion reads count=%d layer=%d node=%d name=%d", TAG, g_mc.count != nullptr, g_mc.layer != nullptr,
         g_mc.node != nullptr, g_mc.name != nullptr);
    return g_mc.name != nullptr;
}

bool is_climb_name(const std::string& n) {
    std::string up(n);
    for (char& c : up) c = (char)std::toupper((unsigned char)c);
    for (const char* part : cfg::CLIMB_NAME_PARTS) if (up.find(part) != std::string::npos) return true;
    return false;
}

std::string climb_motion(float now) {
    auto* mc = component(player(), "via.motion.Motion");
    if (mc == nullptr || !motion_calls()) return "";
    auto* ctx = API::get()->get_vm_context();
    const uint32_t layers = g_mc.count ? g_mc.count->call<uint32_t>(ctx, (void*)mc) : 4u;
    std::string match;
    for (uint32_t i = 0; i < layers && i < 16; ++i) {
        void* layer = g_mc.layer->call<void*>(ctx, (void*)mc, i);
        void* node = layer ? g_mc.node->call<void*>(ctx, layer) : nullptr;
        const std::string name = node ? read_string(g_mc.name->call<void*>(ctx, node)) : "";
        if (name.empty()) continue;
        if (i == 0 && name != g_layer0) {
            if (g_motion_logs < cfg::MOTION_LOG_MAX && now - g_motion_log_t > cfg::MOTION_LOG_EVERY_S) {
                ++g_motion_logs;
                g_motion_log_t = now;
                LOGI("%s ladder: layer0 motion %s", TAG, name.c_str());
            }
            g_layer0 = name;
        }
        if (match.empty() && is_climb_name(name)) match = name;
    }
    return match;
}

struct Climb { bool on = false; float last_match_t = 0.0f; Quat snap{}; bool snap_valid = false; int restores = 0; };
Climb g_climb;

// ---- the view hold: the game's player camera controller -------------------------------------------------
struct Anchor {
    MO* ctrl = nullptr;
    API::Field *yaw = nullptr, *prev_yaw = nullptr, *rot = nullptr;
    MO* sync = nullptr;                          // SyncCameraRotation (app.ropeway.DampingQuat)
    std::vector<API::Field*> sync_q;             // its quaternion fields
    float acquire_log_t = -10.0f;
    bool hooks = false;
    int hook_count = 0;

    bool hold_valid = false;
    float hold_yaw = 0.0f; bool has_yaw = false;
    Quat hold_rot{}; bool has_rot = false;
    std::vector<Quat> hold_sync;
    float hmd0 = bridge::NO_VALUE, vt0 = bridge::NO_VALUE, body0 = bridge::NO_VALUE;
    float view_cal = 0.0f; bool has_cal = false;
    bool measured = false;                       // this jack's hold came from the measured view, not a formula
    int snap_wait = 0, snaps = 0;                // the jack-start snap: frames left to wait, snaps done
    float body_prev = 0.0f; bool has_body_prev = false;

    float jack_t = 0.0f, last_t = -1.0f;
    float vsign = 1.0f; bool v_flipped = false; float v_grow = -1.0f, v_last_abs = -1.0f;
    float k = 0.0f; bool has_k = false;          // learned anchor-to-view constant (lasts the whole game run)
    float last_verr = 0.0f, last_log_t = -10.0f;
    int writes = 0;
};
Anchor A;
bool g_jacked = false;

bool type_is(API::Field* f, const char* full) {
    auto* t = f ? f->get_type() : nullptr;
    return t != nullptr && t->get_full_name() == full;
}

void rotate_hold(float d) {
    if (A.has_yaw) A.hold_yaw += d;
    const Quat dq = yaw_quat(d);
    if (A.has_rot) A.hold_rot = qnorm(qmul(dq, A.hold_rot));
    for (Quat& q : A.hold_sync) q = qnorm(qmul(dq, q));
}

void write_anchor(float yaw, float delta_from_hold) {
    const Quat dq = yaw_quat(delta_from_hold);
    if (A.has_yaw) {
        *(float*)A.yaw->get_data_raw(A.ctrl, false) = yaw;
        if (A.prev_yaw) *(float*)A.prev_yaw->get_data_raw(A.ctrl, false) = yaw;
    }
    if (A.has_rot) *(Quat*)A.rot->get_data_raw(A.ctrl, false) = qnorm(qmul(dq, A.hold_rot));
    for (size_t i = 0; i < A.sync_q.size() && i < A.hold_sync.size(); ++i)
        *(Quat*)A.sync_q[i]->get_data_raw(A.sync, false) = qnorm(qmul(dq, A.hold_sync[i]));
}

// pin the controller to the hold; runs from LateUpdateBehavior and right after each controller update (the
// only moment our value reaches FirstPerson before the game's own per-frame write replaces it)
void apply() {
    if (!g_jacked || A.ctrl == nullptr || !A.hold_valid) return;
    write_anchor(A.hold_yaw, 0.0f);
    ++A.writes;
}

void post_apply(void**, REFrameworkTypeDefinitionHandle, unsigned long long) { apply(); }

void install_hooks(API::TypeDefinition* td) {
    if (A.hooks) return;
    A.hooks = true;
    for (int depth = 0; td != nullptr && depth < 12 && A.hook_count < cfg::MAX_UPDATE_HOOKS; ++depth) {
        for (auto* m : td->get_methods()) {
            if (A.hook_count >= cfg::MAX_UPDATE_HOOKS) break;
            std::string n = m->get_name();
            for (char& c : n) c = (char)std::tolower((unsigned char)c);
            if (n.find("update") == std::string::npos || m->get_num_params() != 0) continue;
            m->add_hook(nullptr, post_apply, false);
            ++A.hook_count;
            LOGI("%s ladder: post-hook on %s.%s", TAG, td->get_full_name().c_str(), m->get_name());
        }
        td = td->get_parent_type();
    }
    LOGI("%s ladder: %d camera-controller update hooks in", TAG, A.hook_count);
}

MO* acquire(float now) {
    if (A.ctrl != nullptr) return A.ctrl;
    auto* cs = API::get()->get_managed_singleton("app.ropeway.camera.CameraSystem");
    if (cs == nullptr) return nullptr;
    const bool log = now - A.acquire_log_t > 5.0f;
    if (log) A.acquire_log_t = now;
    std::string how;
    // the PLAYER controller from the CameraControllers list (as FirstPerson finds it), else BusyCameraController
    if (auto* list = field_obj(cs, "<CameraControllers>k__BackingField")) {
        const int n = call_direct<int>(list, "get_Count", 0);
        for (int i = 0; i < n && A.ctrl == nullptr; ++i) {
            auto* info = call_ptr(list, "get_Item", {(void*)(intptr_t)i});
            if (!is_managed(info)) continue;
            for (const char* f : {"Controller", "<Controller>k__BackingField", "CameraController", "<CameraController>k__BackingField"}) {
                auto* c = field_obj(info, f);
                if (c == nullptr) continue;
                const std::string go = read_string(call_ptr(call_ptr(c, "get_GameObject"), "get_Name"));
                if (log) LOGI("%s ladder: camera list[%d].%s = %s (%s)", TAG, i, f, type_name(c).c_str(), go.c_str());
                if (go.find("Player") != std::string::npos) { A.ctrl = c; how = "list[" + std::to_string(i) + "] " + go; }
                break;
            }
        }
    }
    if (A.ctrl == nullptr && (A.ctrl = field_obj(cs, "<BusyCameraController>k__BackingField")) != nullptr) how = "BusyCameraController";
    if (A.ctrl == nullptr) {
        if (log) LOGW("%s ladder: no player camera controller yet", TAG);
        return nullptr;
    }
    A.ctrl->add_ref();
    auto* td = A.ctrl->get_type_definition();
    A.yaw = find_field_deep(td, "<Yaw>k__BackingField");
    A.prev_yaw = find_field_deep(td, "PrevYaw");
    if (A.prev_yaw == nullptr) A.prev_yaw = find_field_deep(td, "<PrevYaw>k__BackingField");
    A.rot = find_field_deep(td, "<CameraRotation>k__BackingField");
    if (!type_is(A.yaw, "System.Single")) A.yaw = nullptr;
    if (!type_is(A.prev_yaw, "System.Single")) A.prev_yaw = nullptr;
    if (!type_is(A.rot, "via.Quaternion")) A.rot = nullptr;
    if ((A.sync = field_obj(A.ctrl, "<SyncCameraRotation>k__BackingField")) != nullptr) {
        A.sync->add_ref();
        for (auto* t = A.sync->get_type_definition(); t != nullptr; t = t->get_parent_type())
            for (auto* f : t->get_fields())
                if (!f->is_static() && type_is(f, "via.Quaternion")) A.sync_q.push_back(f);
    }
    LOGI("%s ladder: camera controller FOUND via %s (%s): yaw=%d prevyaw=%d rot=%d sync=%s quats=%zu", TAG, how.c_str(),
         type_name(A.ctrl).c_str(), A.yaw != nullptr, A.prev_yaw != nullptr, A.rot != nullptr,
         A.sync ? type_name(A.sync).c_str() : "none", A.sync_q.size());
    install_hooks(td);
    return A.ctrl;
}

void capture_hold(MO* tf, float now) {
    A.has_yaw = A.yaw != nullptr;
    if (A.has_yaw) A.hold_yaw = *(float*)A.yaw->get_data_raw(A.ctrl, false);
    A.has_rot = A.rot != nullptr;
    if (A.has_rot) A.hold_rot = *(Quat*)A.rot->get_data_raw(A.ctrl, false);
    A.hold_sync.clear();
    for (auto* f : A.sync_q) A.hold_sync.push_back(*(Quat*)f->get_data_raw(A.sync, false));

    A.hmd0 = bridge::view(bridge::S_HMD_YAW);
    const float offext = bridge::view(bridge::S_OFFEXT_YAW);
    A.vt0 = bridge::view(bridge::S_CAM_YAW);
    // learn this jack's rendered-view convention while the view still equals the camera
    const float raw_vy = bridge::view(bridge::S_RENDER_YAW);
    A.has_cal = bridge::has(A.vt0) && bridge::has(raw_vy);
    if (A.has_cal) A.view_cal = wrap_pi(A.vt0 - raw_vy);

    // aim at the BODY facing (it faces the ladder/cupboard at the start), never the approach view (Tefa, AC)
    float b0 = 0.0f;
    const bool has_b0 = body_yaw(tf, b0);
    A.body0 = has_b0 ? b0 : bridge::NO_VALUE;
    bool has_desired = false;
    float desired = 0.0f;
    const char* branch = "none";
    // measured: turn the held yaw by exactly the gap between the view on screen now and the body. The formula
    // and K branches below came out ~150 degrees off on the first worn test (2026-10-06: "camera teleports 180
    // and then slowly turns right"): they leave out the headset's own turn and recentre offset.
    A.measured = A.has_yaw && has_b0 && bridge::has(A.vt0);
    if (A.measured) { desired = A.hold_yaw + wrap_pi(b0 - A.vt0); has_desired = true; branch = "measured"; }
    else if (A.has_k && has_b0 && A.has_yaw) { desired = b0 - A.k; has_desired = true; branch = "K"; }
    else if (bridge::has(A.hmd0) && bridge::has(offext) && (has_b0 || bridge::has(A.vt0)) && A.has_yaw) {
        desired = (has_b0 ? b0 : A.vt0) - offext - A.hmd0;
        has_desired = true;
        branch = "formula";
    }
    if (has_desired) rotate_hold(desired - A.hold_yaw);
    A.has_body_prev = has_b0;
    A.body_prev = b0;
    A.hold_valid = A.has_yaw || A.has_rot;
    if (A.hold_valid)
        LOGI("%s ladder: HOLD captured yaw=%.4f hmd0=%.3f offext=%.3f cam=%.3f body=%.3f k=%s branch=%s", TAG, A.hold_yaw,
             A.hmd0, offext, A.vt0, A.body0, A.has_k ? std::to_string(A.k).c_str() : "none", branch);
    (void)now;
}

// jack end: hand the controller the normal-play balance for the view on screen now (yaw = view - headset), so
// the view carries on exactly where the hold left it (AC v11, chair-proof)
void release() {
    if (A.ctrl == nullptr || !A.has_yaw) return;
    const float hmd = bridge::view(bridge::S_HMD_YAW);
    if (!bridge::has(hmd)) return;
    const float vy = bridge::view(bridge::S_RENDER_YAW);
    const float cam = bridge::view(bridge::S_CAM_YAW), offext = bridge::view(bridge::S_OFFEXT_YAW);
    float desired;
    if (bridge::has(vy)) desired = vy + (A.has_cal ? A.view_cal : 0.0f) - hmd;
    else if (bridge::has(cam) && bridge::has(offext)) desired = cam + offext - hmd;
    else return;
    write_anchor(desired, desired - A.hold_yaw);
    LOGI("%s ladder: RELEASE yaw=%.4f (hold was %.4f, measured=%d, writes=%d)", TAG, desired, A.hold_yaw, bridge::has(vy), A.writes);
}

void update_hold(float now) {
    auto* ctrl = acquire(now);
    auto* tf = player_transform();
    float body = 0.0f;
    const bool has_body = body_yaw(tf, body);
    const float cam = bridge::view(bridge::S_CAM_YAW);

    if (ctrl != nullptr && now - A.last_log_t > cfg::STATUS_LOG_EVERY_S) {
        A.last_log_t = now;
        LOGI("%s ladder: held cam=%.1f body=%.1f hmd=%.1f ctrlyaw=%.3f verr=%.1f vsign=%.0f writes=%d", TAG, cam / DEG,
             has_body ? body / DEG : 999.0f, bridge::view(bridge::S_HMD_YAW) / DEG,
             A.yaw ? *(float*)A.yaw->get_data_raw(ctrl, false) : 999.0f, A.last_verr / DEG, A.vsign, A.writes);
    }
    if (ctrl == nullptr || !has_body || !bridge::has(cam)) { A.last_t = -1.0f; return; }

    const float dt = A.last_t < 0.0f ? 1.0f / 60.0f : std::fmax(0.0f, std::fmin(0.1f, now - A.last_t));
    A.last_t = now;
    if (!A.hold_valid) capture_hold(tf, now);

    // the first moments of a jack: re-measure and re-aim (the headset-side offset may settle a frame late)
    if (A.hold_valid && !A.measured && A.has_yaw && bridge::has(A.vt0) && bridge::has(A.hmd0) && now - A.jack_t < cfg::HOLD_REAIM_S) {
        const float offext = bridge::view(bridge::S_OFFEXT_YAW);
        if (bridge::has(offext)) {
            const float from = A.has_body_prev ? A.body_prev : bridge::has(A.body0) ? A.body0 : A.vt0;
            const float d = (from - offext - A.hmd0) - A.hold_yaw;
            if (std::fabs(d) > 0.002f) rotate_hold(d);
        }
    }

    // the body turns during the jack (the 180 at the top of a ladder): the held view turns with it, never on its own
    if (A.hold_valid && A.has_body_prev) {
        const float d = wrap_pi(body - A.body_prev);
        A.body_prev = body;
        if (std::fabs(d) > 1e-4f && A.has_yaw) rotate_hold(d);
    }

    // measured-view servo: nudge the hold until the view actually rendered faces the body
    if (A.hold_valid && A.has_cal && A.has_yaw) {
        const float raw = bridge::view(bridge::S_RENDER_YAW);
        if (bridge::has(raw)) {
            const float vy = raw + A.view_cal;
            const float verr = wrap_pi(body - vy);
            A.last_verr = verr;
            const float av = std::fabs(verr);
            if (A.v_last_abs >= 0.0f && av > A.v_last_abs + cfg::WATCHDOG_GROW_RAD) {
                if (A.v_grow < 0.0f) A.v_grow = now;
                if (!A.v_flipped && now - A.v_grow > cfg::WATCHDOG_S) {
                    A.vsign = -A.vsign;
                    A.v_flipped = true;
                    LOGI("%s ladder: view servo direction flipped (error kept growing)", TAG);
                }
            } else {
                A.v_grow = -1.0f;
            }
            A.v_last_abs = av;
            if (av < cfg::K_LEARN_ERR_DEG * DEG && now - A.jack_t > cfg::K_LEARN_AFTER_S) {
                const float kn = wrap_pi(vy - A.hold_yaw);
                if (!A.has_k || std::fabs(wrap_pi(kn - A.k)) > cfg::K_RELOG_RAD)
                    LOGI("%s ladder: K learned %.4f (was %s)", TAG, kn, A.has_k ? std::to_string(A.k).c_str() : "none");
                A.k = kn;
                A.has_k = true;
            }
            // the jack's own extra turn of the view differs every time and only shows once the jack has begun
            // (2026-10-06 wear: "it teleports there and turns to the right spot"), so in the first moments the
            // whole measured error is corrected at once, then the view is given a frame or two to show it
            if (now - A.jack_t < cfg::SNAP_WINDOW_S && av > cfg::SERVO_DEADBAND_DEG * DEG) {
                if (A.snap_wait > 0) {
                    --A.snap_wait;
                } else {
                    rotate_hold(verr * A.vsign);
                    A.snap_wait = cfg::SNAP_SETTLE_FRAMES;
                    if (++A.snaps <= 4)
                        LOGI("%s ladder: snap %d: view was %.1f deg off the body, turned in one go", TAG, A.snaps, verr / DEG);
                }
            } else if (av > cfg::SERVO_DEADBAND_DEG * DEG) {
                const float maxd = cfg::SERVO_MAX_DEG_S * DEG * dt;
                rotate_hold(std::fmax(-maxd, std::fmin(maxd, cfg::SERVO_GAIN * verr * A.vsign)));
            }
        }
    }
    apply();
}

void update_climb(float now) {
    const std::string match = climb_motion(now);
    if (!match.empty()) {
        g_climb.last_match_t = now;
        if (!g_climb.on) LOGI("%s ladder: CLIMB START (%s) -- body guard on", TAG, match.c_str());
        g_climb.on = true;
    } else if (g_climb.on && now - g_climb.last_match_t > cfg::CLIMB_HOLD_S) {
        g_climb.on = false;
        g_climb.snap_valid = false;
        LOGI("%s ladder: CLIMB END -- body guard off (put back %d frames)", TAG, g_climb.restores);
        g_climb.restores = 0;
    }
}
} // namespace

void late_update() {
    if (!bridge::live() || bridge::view(bridge::S_FP_USED) < 0.5f) {   // only while FirstPerson drives the view
        g_climb.on = false;
        g_climb.snap_valid = false;
        return;
    }
    const float now = now_s();
    const bool j = jacked();
    if (j != g_jacked) {
        g_jacked = j;
        A.last_t = -1.0f;
        if (j) {
            A.hold_valid = false;   // a fresh capture this frame
            A.jack_t = now;
            A.vsign = 1.0f;
            A.v_flipped = false;
            A.v_grow = A.v_last_abs = -1.0f;
            A.snap_wait = 1;        // the capture's own turn needs a frame to show
            A.snaps = 0;
        } else {
            if (A.hold_valid) release();
            A.hold_valid = false;
        }
        LOGI("%s ladder: jacked -> %d (layer0 %s)", TAG, (int)j, g_layer0.c_str());
    }
    if (j) update_hold(now);

    update_climb(now);
    if (!g_climb.on) { g_climb.snap_valid = false; return; }
    g_climb.snap_valid = body_rot(player_transform(), g_climb.snap);
}

void restore(bool final) {
    if (!g_climb.snap_valid) return;
    set_body_rot(player_transform(), g_climb.snap);
    if (final) { g_climb.snap_valid = false; ++g_climb.restores; }
}

} // namespace vn::ladder
