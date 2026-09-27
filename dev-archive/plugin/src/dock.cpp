// dock.cpp -- the dock: camera pose, blend, HOLD latch, the aid-target hooks.
// Split out of Plugin.cpp 2026-09-27, move only (SPLIT-MAP-2026-09-24.md).
#include "visceral.h"

namespace visceral {


// v0.3 hook call counters (the hooks themselves are defined with the bridge hook below).
std::atomic<uint32_t> g_calls_aid{0}, g_calls_ikl{0};

// v0.4.1: true while the plugin itself is calling the hooked getters (summary / dumps), so the hooks can tell
// the plugin's own reads apart from the game's per-frame read and never take them as the blend origin.
bool g_self_call = false;

void set_force(int kind, bool on) {
    auto* is = API::get()->get_managed_singleton("app.ropeway.InputSystem");
    if (is == nullptr) { LOGW("%s InputSystem singleton missing", TAG); return; }
    auto r = inv(is, "setForce", {(void*)(uintptr_t)kind, (void*)(uintptr_t)(on ? 1 : 0)});
    LOGI("%s setForce(kind=%d, %s) %s", TAG, kind, on ? "true" : "false", r.ok ? "ok" : "THREW/NOT FOUND");
}


// Game camera world pose. Under REFramework's VR the camera follows the HMD, so this is the anchor the
// controller pose is re-based on in space mode 0. Read through the TDB (via.SceneManager is a NATIVE
// singleton; via.SceneView / via.Camera are called through their type-definitions, as praydog's example
// plugin does) so no managed-header assumption is made on the view object.
void update_camera() {
    auto& d = g.dock;
    d.cam_valid = false;
    auto& api = API::get();
    auto* sm = api->get_native_singleton("via.SceneManager");
    if (sm == nullptr) return;
    static API::Method* m_view = nullptr; static API::Method* m_cam = nullptr; static API::Method* m_wm = nullptr; static bool looked = false;
    if (!looked) {
        looked = true;
        if (auto* t = api->tdb()->find_type("via.SceneManager"); t != nullptr) m_view = t->find_method("get_MainView");
        if (auto* t = api->tdb()->find_type("via.SceneView"); t != nullptr) m_cam = t->find_method("get_PrimaryCamera");
        if (auto* t = api->tdb()->find_type("via.Camera"); t != nullptr) m_wm = t->find_method("get_WorldMatrix");
        LOGI("%s camera path: get_MainView=%p get_PrimaryCamera=%p Camera.get_WorldMatrix=%p", TAG, (void*)m_view, (void*)m_cam, (void*)m_wm);
    }
    if (m_view == nullptr || m_cam == nullptr || m_wm == nullptr) return;
    auto* view = m_view->call<API::ManagedObject*>(api->get_vm_context(), sm);
    if (view == nullptr) return;
    auto* cam = m_cam->call<API::ManagedObject*>(api->get_vm_context(), (void*)view);
    if (cam == nullptr) return;
    auto r = m_wm->invoke(cam, std::vector<void*>{});
    if (r.exception_thrown) return;
    Mat4 cm{}; memcpy(&cm, r.bytes.data(), sizeof(Mat4));
    d.cam_t = Vec3{cm.m[12], cm.m[13], cm.m[14]}; d.cam_r = rows_of(cm); d.cam_valid = true;
}


// ---------------------------------------------------------------------------
// v0.11 (2026-09-12): THE CAMERA THAT ACTUALLY MOVES — diagnostic pass, reads only.
//
// Why a second reader rather than a fix to the first. `cam=(-11.50 -3.20 4.20)` was identical on every
// summary of the 2026-09-09 FLAT run and the 2026-09-10 VR run, including across two level loads with the
// player at x ~ +2 and then x ~ -18 (recon/2026-09-09-.../num7-motion-component-dump.txt logs hands at
// (-20.27 -10.50 20.66) beside that camera). The board read that as "REFramework VR replaced the camera",
// but the same freeze is in a flat run with no VR at all, which rules that out. Static cause, and it is
// mundane: update_camera() has exactly two callers — head_update()'s `if (!cam_valid) update_camera();`
// and update_dock()'s real-controller branch (which needs the dock engaged). It sets cam_valid = true on
// first success, so from frame 2 onward nothing ever calls it again. One read, at load, forever.
// Matching the dossier's own ranked cause (1), "a handle fetched once at script load". [inferred-static]
//
// So this function re-fetches everything every frame and reports THREE things, which between them decide
// the question in one flat run:
//   camF  the OLD call (via.Camera.get_WorldMatrix) on a FRESHLY fetched camera. praydog calls exactly this
//         in RE8VR.cpp:322, so it should be live; if camF moves while cam does not, the freeze was the
//         call-once bug and nothing else.
//   cam2  the camera's GameObject -> Transform -> joint 0 world matrix, which is the route REFramework's own
//         FreeCam uses (FreeCam.cpp:140 get_joint(*transform, 0), :163-165 joint rotation + position) and
//         which re8_vr.lua uses for the camera. Falls back to the Transform's own world matrix, then to the
//         Transform's position, and names which one it used.
//   camA  the primary-camera object's address. Dossier 5b.3: address constant across a scene transition
//         => stale handle upstream; address moving while the position does not => wrong node.
// Nothing here writes to cam_t / cam_r / cam_valid, so the dock's re-basing and the head hider's reveal gate
// behave exactly as they did — this pass only has to prove which reading tracks the player.
// ---------------------------------------------------------------------------
void update_camera2() {
    auto& d = g.dock;
    d.cam2_valid = false; d.camf_valid = false; d.cam2_src = "none";
    auto& api = API::get();
    auto* sm = api->get_native_singleton("via.SceneManager");
    if (sm == nullptr) { d.cam2_src = "no SceneManager"; return; }
    static API::Method* m_view = nullptr; static API::Method* m_cam = nullptr; static bool looked2 = false;
    if (!looked2) {
        looked2 = true;
        if (auto* t = api->tdb()->find_type("via.SceneManager"); t != nullptr) m_view = t->find_method("get_MainView");
        if (auto* t = api->tdb()->find_type("via.SceneView"); t != nullptr) m_cam = t->find_method("get_PrimaryCamera");
    }
    if (m_view == nullptr || m_cam == nullptr) { d.cam2_src = "no method"; return; }
    auto* view = m_view->call<API::ManagedObject*>(api->get_vm_context(), sm);
    if (view == nullptr) { d.cam2_src = "no view"; return; }
    auto* cam = m_cam->call<API::ManagedObject*>(api->get_vm_context(), (void*)view);
    if (cam == nullptr) { d.cam2_src = "no camera"; return; }

    // The identity half of the diagnostic: log the camera (and its GameObject) once, and again whenever the
    // engine hands us a different object — a level load that does NOT change this address is the stale-handle
    // answer, and one that does change it while the position stays put is the wrong-node answer.
    const auto addr = (uintptr_t)cam;
    if (addr != d.cam_addr) {
        d.cam_addr = addr;
        auto* go = inv_ptr(cam, "get_GameObject");
        LOGI("%s camera2: PRIMARY CAMERA -> 0x%llx type=%s go=\"%s\"", TAG, (unsigned long long)addr,
             tname(cam).c_str(), go != nullptr ? sysstr(inv_ptr(go, "get_Name")).c_str() : "?");
    }

    // camF: the old call, freshly fetched. Same method lookup the old path uses (declared on via.Camera).
    Mat4 fm{};
    if (inv_mat4(cam, "get_WorldMatrix", fm)) { d.camf_t = Vec3{fm.m[12], fm.m[13], fm.m[14]}; d.camf_valid = true; }

    // cam2: the transform route.
    auto* go = inv_ptr(cam, "get_GameObject");
    if (go == nullptr) { d.cam2_src = "no gameobject"; return; }
    auto* tf = inv_ptr(go, "get_Transform");
    if (tf == nullptr) { d.cam2_src = "no transform"; return; }

    Mat4 jm{};
    if (auto* joints = inv_ptr(tf, "get_Joints"); joints != nullptr) {
        const auto n = arr_count(joints);
        if (n >= 1 && n <= 2048) {
            auto* j0 = arr_ptr_at(joints, 0);
            // self-check: the default array offsets are only measured once the Lua bridge hands its sentinel
            // array over, so never trust element 0 without confirming it really is a managed via.Joint.
            if (is_managed(j0) && inv_mat4(j0, "get_WorldMatrix", jm)) {
                d.cam2_t = Vec3{jm.m[12], jm.m[13], jm.m[14]}; d.cam2_r = rows_of(jm);
                d.cam2_valid = true; d.cam2_src = "joint0";
                return;
            }
        }
    }
    if (inv_mat4(tf, "get_WorldMatrix", jm)) {
        d.cam2_t = Vec3{jm.m[12], jm.m[13], jm.m[14]}; d.cam2_r = rows_of(jm);
        d.cam2_valid = true; d.cam2_src = "transform";
        return;
    }
    Vec3 tp{};
    if (inv_vec3(tf, "get_Position", tp)) { d.cam2_t = tp; d.cam2_valid = true; d.cam2_src = "tf_pos"; return; }
    d.cam2_src = "transform unreadable";
}

void update_dock(double t, double dt) {
    auto& d = g.dock;
    const bool vr = bridge_live() && arr_f32(g.bridge)[S_USING_CTL] != 0.0f && arr_f32(g.bridge)[S_HMD_ACTIVE] != 0.0f;
    d.lg_held = vr && arr_f32(g.bridge)[S_LGRIP] > 0.5f;
    const bool was = d.docked;
    d.docked = d.lg_held || d.synthetic;
    if (d.docked != was) LOGI("%s DOCK %s (lg=%d synthetic=%d vr=%d) w=%.2f", TAG, d.docked ? "ENGAGED" : "RELEASED", (int)d.lg_held, (int)d.synthetic, (int)vr, d.w);

    const float step = (float)(dt / DOCK_BLEND_S);
    d.w = std::clamp(d.w + (d.docked ? step : -step), 0.f, 1.f);
    d.w_eased = d.w * d.w * (3.f - 2.f * d.w);

    // spec v2.3 req 2: HOLD rides the dock. One setForce per edge, reconciled with the NUM4 manual latch.
    const bool want_hold = g.force_hold || d.docked;
    if (want_hold != g.hold_sent) { g.hold_sent = want_hold; set_force(KIND_HOLD, want_hold); }

    d.target_valid = false;

    // v0.6: measure the arm's own length while it is doing nothing in particular. Bone lengths are rigid, so the
    // running max over plausible frames IS the length; the band rejects a garbage read rather than letting one
    // bad frame set the clamp forever. Reset on rebind, because Claire's arm is not Leon's.
    // Self-review, same session: this ran every frame, docked included, for three extra get_Position calls
    // per frame on the one path where latency matters. Bone lengths do not change, so measuring only while
    // UNDOCKED costs nothing and keeps the dock frame as cheap as it was in v0.5. If a dock somehow happens
    // before any measurement lands, reach_valid stays false and the clamp simply does not apply — the v0.5
    // behaviour, which is the right thing to fall back to.
    d.reach_valid = d.reach > 0.f;
    if (!d.docked && g.l_humerus != nullptr && g.l_radius != nullptr && g.l_hand != nullptr) {
        Vec3 hu{}, ra{}, wr{};
        if (inv_vec3(g.l_humerus, "get_Position", hu) && inv_vec3(g.l_radius, "get_Position", ra) &&
            inv_vec3(g.l_hand, "get_Position", wr)) {
            const float upper = dist(hu, ra), fore = dist(ra, wr);
            if (upper > 0.05f && upper < 1.0f && fore > 0.05f && fore < 1.0f) {
                const float sum = upper + fore;
                if (sum > d.reach_raw) {
                    d.reach_raw = sum; d.reach = sum * DOCK_REACH_FRAC;
                    LOGI("%s reach measured: upper=%.3f fore=%.3f arm=%.3f clamp at %.3f m from l_arm_humerus", TAG, upper, fore, sum, d.reach);
                }
            }
            d.shoulder_t = hu;
        } else {
            d.reach_valid = false;   // could not read the chain this frame: clamp about a stale shoulder is worse than no clamp
        }
    }

    // v0.6: M is measured EVERY frame, docked or not. It was only computed while the dock was active, so the
    // summary's angM was whatever the last dock left behind — misleading exactly when a headset run wants to
    // read it before docking. It costs one get_WorldMatrix per frame. A stale M is worse than none, so an
    // unreadable joint clears M_valid rather than leaving the previous frame's mapping to be read as this one's.
    Mat4 am{}; Vec3 a_t{}; Rows A{};
    const bool have_frame = g.aid_joint != nullptr && d.natural_valid && inv_mat4(g.aid_joint, "get_WorldMatrix", am);
    if (have_frame) {
        a_t = Vec3{am.m[12], am.m[13], am.m[14]};
        A = rows_of(am);
        d.M = rows_mul(rows_T(rows_of(d.natural)), A); d.M_valid = true;
    } else {
        d.M_valid = false;
    }

    if (!d.docked && d.w <= 0.f) return;      // idle: the hook leaves the matrix alone
    if (!have_frame) return;                  // no natural value or no aid joint — nothing to blend from
    Vec3 p_f{}; Rows C{};                     // the desired FINAL wrist pose, unblended

    if (vr && !d.synthetic) {
        // Real controller. Both HMD and controller poses come from the same vrmod space, so their DIFFERENCE is
        // space-independent; re-base it on the game camera (which REFramework pins to the HMD) to get world.
        // Space mode 1 trusts the bridge pose as world outright; mode 2 is mode 0 with the camera rows transposed
        // (the one convention ambiguity in the rows<->quaternion path). None of the three has run in a headset.
        update_camera();
        const float* f = arr_f32(g.bridge);
        const Vec3 lp = bridge_vec3(S_LPOS), hp = bridge_vec3(S_HPOS);
        const Quat lq = quat_norm(Quat{f[S_LROT], f[S_LROT + 1], f[S_LROT + 2], f[S_LROT + 3]});
        const Quat hq = quat_norm(Quat{f[S_HROT], f[S_HROT + 1], f[S_HROT + 2], f[S_HROT + 3]});
        if (d.space_mode != 1 && d.cam_valid) {
            Rows cr = d.cam_r;
            if (d.space_mode == 2) { Rows tr; for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) tr.r[i * 3 + j] = cr.r[j * 3 + i]; cr = tr; }
            const Quat fix = quat_mul(quat_of_rows(cr), quat_conj(hq));   // tracking -> world
            p_f = vadd(d.cam_t, quat_rotate(fix, vsub(lp, hp)));
            C = rows_of_quat(quat_mul(fix, lq));
        } else {
            p_f = lp;
            C = rows_of_quat(lq);
        }
    } else {
        // Flat stand-in: a point 10 cm from the FINAL aid joint, orbiting above it once every 4 s, facing 45 deg
        // (world yaw) off the joint's final frame. If the mapping is right the trace shows |Lw-tgt| -> 0 and
        // rotW-T -> 0 while docked.
        const float th = (float)((t - d.orbit_t0) * 2.0 * 3.14159265 / DOCK_ORBIT_PERIOD_S);
        const Vec3 dir{std::cos(th) * 0.7071f, 0.7071f, std::sin(th) * 0.7071f};
        p_f = vadd(a_t, vscale(dir, DOCK_ORBIT_RADIUS));
        C = rows_yaw(A, DOCK_ORBIT_YAW_DEG);
    }
    // v0.6 reach clamp, applied BEFORE the target is published so the trace compares the wrist against a pose the
    // arm can actually hold and |Lw-tgt| still goes to 0.000 when the mapping is right. The rotation is left
    // alone: a wrist orientation is always reachable, it is only the position that runs out of arm.
    d.clamped_m = 0.f;
    float want_L = 0.f;                       // how far the UNCLAMPED target sat from the shoulder
    if (d.reach_valid && d.use_reach_clamp) {
        const Vec3 sd = vsub(p_f, d.shoulder_t);
        want_L = vlen(sd);
        if (want_L > d.reach && want_L > 1e-4f) {
            d.clamped_m = want_L - d.reach;
            p_f = vadd(d.shoulder_t, vscale(sd, d.reach / want_L));
        }
    }
    const bool clamping = d.clamped_m > 0.001f;
    if (clamping != d.clamp_on) {
        d.clamp_on = clamping;
        LOGI("%s REACH CLAMP %s (target %.3f m from l_arm_humerus, arm reaches %.3f, pulled in %.3f)", TAG,
             clamping ? "ON" : "off", want_L, d.reach, d.clamped_m);
    }

    d.target_t = p_f; d.target_r = C; d.target_valid = true;

    // Blend in final space, then map into getter space for the hook.
    const Vec3 p_b = vlerp(a_t, p_f, d.w_eased);
    const Rows R_b = rows_slerp(A, C, d.w_eased);
    const Rows MT = rows_T(d.M);
    d.d_get = vec_mul_rows(vsub(p_b, a_t), MT);
    d.T_rows = rows_mul(R_b, MT);
}


// ---------------------------------------------------------------------------
// v0.3/v0.4: the aid-target hooks. Both getters return System.Nullable`1<via.mat4> through a hidden
// return-buffer pointer, so at return RAX holds that pointer: *ret_val -> { u8 HasValue @0, mat4 @+0x10 }.
// The post-hook edits the buffer before the caller reads it. v0.3 shifted it (+10 cm) and proved the wrist
// follows; v0.4 BLENDS it toward the dock target on the OUTER getter only (getIKLeftArmMatrix calls
// get_AidTargetWorldMatrix inside itself — the two counts were always equal — so editing the outer one
// is what the wrist solver sees, and the inner one stays honest for anything else that reads it).
// ---------------------------------------------------------------------------

int pre_passthrough(int, void**, REFrameworkTypeDefinitionHandle*, unsigned long long) { return REFRAMEWORK_HOOK_CALL_ORIGINAL; }

void post_aid_target(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    g_calls_aid++;
    if (g_self_call || ret_val == nullptr) return;
    auto* nb = (uint8_t*)*ret_val;
    if (nb == nullptr || nb[0] == 0) return;
    const float* m = (const float*)(nb + 0x10);
    g.dock.inner_t = Vec3{m[12], m[13], m[14]}; g.dock.inner_valid = true;
}

void post_ik_left_arm(void** ret_val, REFrameworkTypeDefinitionHandle, unsigned long long) {
    g_calls_ikl++;
    if (ret_val == nullptr) return;
    auto* nb = (uint8_t*)*ret_val;
    if (nb == nullptr) return;
    auto& d = g.dock;
    if (nb[0] == 0) { if (d.docked) d.no_value_frames++; return; }   // no value: do not invent one (counted, so the minigun case shows)
    float* m = (float*)(nb + 0x10);
    if (g_self_call) { memcpy(d.natural_self.m, m, sizeof(Mat4)); d.natural_self_valid = true; }
    else             { memcpy(d.natural.m, m, sizeof(Mat4)); d.natural_valid = true; }   // the game's un-hooked value: the blend origin
    if (d.w_eased <= 0.f || !d.target_valid || !d.M_valid) return;
    // v0.5: the blend already happened in final space (update_dock); write its getter-space form.
    m[12] += d.d_get.x;
    m[13] += d.d_get.y;
    m[14] += d.d_get.z;
    if (d.write_rot) {
        Mat4 tmp{}; memcpy(tmp.m, m, sizeof(Mat4));
        rows_into(tmp, d.T_rows);
        memcpy(m, tmp.m, sizeof(Mat4));
    }
}

void install_shift_hooks() {
    auto& api = API::get();
    auto* a = api->tdb()->find_method("app.ropeway.implement.Implement", "get_AidTargetWorldMatrix");
    auto* b = api->tdb()->find_method("app.ropeway.implement.Implement", "getIKLeftArmMatrix");
    if (a != nullptr) { const auto id = a->add_hook(pre_passthrough, post_aid_target, false); LOGI("%s hook get_AidTargetWorldMatrix id=%u fn=%p", TAG, id, a->get_function_raw()); }
    else LOGE("%s Implement.get_AidTargetWorldMatrix not found — no aid-target hook", TAG);
    if (b != nullptr) { const auto id = b->add_hook(pre_passthrough, post_ik_left_arm, false); LOGI("%s hook getIKLeftArmMatrix id=%u fn=%p", TAG, id, b->get_function_raw()); }
    else LOGE("%s Implement.getIKLeftArmMatrix not found — no IK-left-arm hook", TAG);
}

} // namespace visceral
