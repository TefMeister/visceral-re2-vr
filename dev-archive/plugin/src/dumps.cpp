// dumps.cpp -- NUM7/NUM9 dumps and the summary line.
// Split out of Plugin.cpp 2026-09-27, move only (SPLIT-MAP-2026-09-24.md).
#include "visceral.h"

namespace visceral {


// ---------------------------------------------------------------------------
// Dumps
// ---------------------------------------------------------------------------

API::ManagedObject** g_grab_named = nullptr;   // dump_joints side channel: capture the joint with this name

std::string g_grab_name;

void dump_joints(API::ManagedObject* transform, const char* who, bool all, API::ManagedObject** l_hand, API::ManagedObject** r_hand) {
    auto* joints = inv_ptr(transform, "get_Joints");
    if (joints == nullptr) { LOGW("%s %s: get_Joints failed", TAG, who); return; }
    if (!g_arr_measured) LOGW("%s %s: array layout not yet measured (no bridge hand-over seen) — reading with default offsets, expect a self-check warning if they are wrong", TAG, who);
    const auto n = arr_count(joints);
    LOGI("%s %s: %u joints (array type %s)", TAG, who, n, tname(joints).c_str());
    if (n > 2048) { LOGW("%s %s: joint count implausible, array layout assumption wrong?", TAG, who); return; }
    for (uint32_t i = 0; i < n; ++i) {
        auto* j = arr_ptr_at(joints, i);
        if (!is_managed(j)) { LOGW("%s %s: joint[%u] is not a managed object — array layout assumption wrong", TAG, who, i); return; }
        const auto name = joint_name(j);
        const auto ln = lower(name);
        const bool interesting = all || ln.find("hand") != std::string::npos || ln.find("wrist") != std::string::npos ||
                                 ln.find("arm") != std::string::npos || ln.find("weapon") != std::string::npos ||
                                 ln.find("wp") != std::string::npos || ln.find("grip") != std::string::npos ||
                                 ln.find("hold") != std::string::npos || ln.find("aid") != std::string::npos;
        if (interesting) {
            Vec3 p{}; inv_vec3(j, "get_Position", p);
            LOGI("%s   joint[%u] %-28s pos=(%.3f %.3f %.3f)", TAG, i, name.c_str(), p.x, p.y, p.z);
        }
        // RE2 pl1000 skeleton (verified-live 2026-09-04): no "l_hand"; the palm points are l_weapon / r_weapon, wrists l_arm_wrist / r_arm_wrist
        if (l_hand != nullptr && *l_hand == nullptr && (ln == "l_weapon" || ln == "l_hand" || ln == "l_arm_wrist")) *l_hand = j;
        // v0.6: the reach clamp needs the two joints above the wrist. Bound here rather than by a second walk of
        // the joint array, and only on the player's own skeleton (dump_joints is called for the weapon too).
        if (l_hand != nullptr && g.l_humerus == nullptr && ln == "l_arm_humerus") g.l_humerus = j;
        if (l_hand != nullptr && g.l_radius  == nullptr && ln == "l_arm_radius")  g.l_radius  = j;
        if (l_hand != nullptr && g.neck0     == nullptr && ln == "neck_0")        g.neck0     = j;   // v0.7: the plug's anchor
        if (l_hand != nullptr && g.r_radius  == nullptr && ln == "r_arm_radius")  g.r_radius  = j;   // v0.10: bracelets
        if (l_hand != nullptr && g.l_wrist   == nullptr && ln == "l_arm_wrist")   g.l_wrist   = j;
        if (l_hand != nullptr && g.r_wrist   == nullptr && ln == "r_arm_wrist")   g.r_wrist   = j;
        if (r_hand != nullptr && *r_hand == nullptr && (ln == "r_weapon" || ln == "r_hand" || ln == "r_arm_wrist")) *r_hand = j;
        if (g_grab_named != nullptr && *g_grab_named == nullptr && ln == g_grab_name) *g_grab_named = j;
    }
}

void log_nullable_mat(const char* label, bool ok, bool has, const Mat4& m) {
    if (!ok) { LOGI("%s   %s: call failed", TAG, label); return; }
    if (!has) { LOGI("%s   %s: (no value)", TAG, label); return; }
    LOGI("%s   %s: t=(%.3f %.3f %.3f) r0=(%.2f %.2f %.2f) r2=(%.2f %.2f %.2f)", TAG, label,
         m.m[12], m.m[13], m.m[14], m.m[0], m.m[1], m.m[2], m.m[8], m.m[9], m.m[10]);
}

void dump_weapon() {
    auto* w = g.weapon;
    if (w == nullptr) { LOGI("%s weapon: none equipped", TAG); return; }
    LOGI("%s ---- WEAPON DUMP: %s ----", TAG, tname(w).c_str());
    LOGI("%s   WeaponType=%u  MuzzleJointName=%s  AidJointType=%u (0 None,1 ExtraNarrow,2 Narrow,3 Wide)", TAG,
         inv_u32(w, "get_WeaponType"), sysstr(inv_ptr(w, "get_MuzzleJointName")).c_str(), inv_u32(g.equipment, "getAidJointType"));
    LOGI("%s   IsEquiped=%d  EnabledHoldMainWeapon=%d  ParentIkController=%p (cond IkController=%p)", TAG,
         (int)inv_bool(w, "get_IsEquiped"), (int)inv_bool(g.equipment, "get_EnabledHoldMainWeapon"), (void*)inv_ptr(w, "get_ParentIkController"), (void*)g.ik);

    // Aid joint — the engine's own "support hand goes here"
    g.aid_joint = inv_ptr(w, "get_AidJoint");
    if (g.aid_joint != nullptr) {
        Vec3 p{}; inv_vec3(g.aid_joint, "get_Position", p);
        auto* owner = inv_ptr(g.aid_joint, "get_Owner");
        LOGI("%s   AidJoint=%s pos=(%.3f %.3f %.3f) owner_transform=%p (weapon transform=%p, player transform=%p)", TAG,
             joint_name(g.aid_joint).c_str(), p.x, p.y, p.z, (void*)owner, (void*)g.weapon_transform, (void*)g.transform);
    } else {
        LOGI("%s   AidJoint=null", TAG);
    }
    const auto narrow = inv_u32(w, "get_LEFT_ARM_JOINT_NARROW");
    const auto wide = inv_u32(w, "get_LEFT_ARM_JOINT_WIDE");
    auto resolve = [&](API::ManagedObject* tr, uint32_t hash) -> std::string {
        if (tr == nullptr) return "(no transform)";
        auto* j = inv_ptr(tr, "getJointByHash", {(void*)(uintptr_t)hash});
        return j != nullptr ? joint_name(j) : "(not on this skeleton)";
    };
    LOGI("%s   LEFT_ARM_JOINT_NARROW=0x%08x -> weapon:%s player:%s", TAG, narrow, resolve(g.weapon_transform, narrow).c_str(), resolve(g.transform, narrow).c_str());
    LOGI("%s   LEFT_ARM_JOINT_WIDE  =0x%08x -> weapon:%s player:%s", TAG, wide, resolve(g.weapon_transform, wide).c_str(), resolve(g.transform, wide).c_str());

    // Attach joint (where the weapon hangs on the player) and aim joint
    if (auto* ci = inv_ptr(w, "get_AttachJoint"); ci != nullptr) {
        auto* j = field_at<API::ManagedObject*>(ci, 0x10);
        const auto op = field_at<Vec3>(ci, 0x20);
        LOGI("%s   AttachJoint=%s offset=(%.3f %.3f %.3f)", TAG, is_managed(j) ? joint_name(j).c_str() : "null", op.x, op.y, op.z);
    }
    if (auto* aj = inv_ptr(w, "get_AimJoint"); aj != nullptr) {
        Vec3 p{}; inv_vec3(aj, "get_Position", p);
        LOGI("%s   AimJoint (%s) pos=(%.3f %.3f %.3f)", TAG, tname(aj).c_str(), p.x, p.y, p.z);
    }
    Mat4 m{}; bool has = false;
    if (inv_mat4(w, "get_AimJointWorldMatrix", m)) LOGI("%s   AimJointWorldMatrix t=(%.3f %.3f %.3f)", TAG, m.m[12], m.m[13], m.m[14]);
    if (auto* mj = inv_ptr(w, "get_MuzzleJoint"); mj != nullptr) {
        Vec3 p{}; inv_vec3(mj, "get_Position", p);
        LOGI("%s   MuzzleJoint (%s) pos=(%.3f %.3f %.3f)", TAG, tname(mj).c_str(), p.x, p.y, p.z);
    }
    if (inv_mat4(w, "get_MuzzleJointWorldMatrix", m)) LOGI("%s   MuzzleJointWorldMatrix t=(%.3f %.3f %.3f)", TAG, m.m[12], m.m[13], m.m[14]);
    { SelfCall sc; log_nullable_mat("IKLeftArmMatrix", inv_nullable_mat4(w, "getIKLeftArmMatrix", has, m), has, m); }
    { SelfCall sc; log_nullable_mat("AidTargetWorldMatrix", inv_nullable_mat4(w, "get_AidTargetWorldMatrix", has, m), has, m); }

    g.w_narrow = g.weapon_transform != nullptr ? inv_ptr(g.weapon_transform, "getJointByHash", {(void*)(uintptr_t)narrow}) : nullptr;
    g.w_wide   = g.weapon_transform != nullptr ? inv_ptr(g.weapon_transform, "getJointByHash", {(void*)(uintptr_t)wide}) : nullptr;
    if (g.weapon_transform != nullptr) dump_joints(g.weapon_transform, "weapon skeleton", true, nullptr, nullptr);
    LOGI("%s ---- end weapon dump ----", TAG);
}


// Every method and field of an object's type (parent chain included) whose name contains one of the
// hunt words. For the two unread IK objects — what places the support hand may be in here.
void dump_type_surface(API::ManagedObject* o, const char* label,
                       const std::vector<const char*>& words) {
    if (o == nullptr) { LOGI("%s   %s: null", TAG, label); return; }
    LOGI("%s   %s: %s", TAG, label, tname(o).c_str());
    int lines = 0;
    for (auto* td = o->get_type_definition(); td != nullptr && lines < 120; td = td->get_parent_type()) {
        const auto tn = td->get_full_name();
        if (tn == "System.Object" || tn == "via.Component" || tn == "via.Base") break;
        for (auto* m : td->get_methods()) {
            const auto ln = lower(m->get_name());
            bool hit = false; for (auto* w : words) hit = hit || ln.find(w) != std::string::npos;
            if (!hit) continue;
            auto* rt = m->get_return_type();
            std::string ps;
            for (const auto& p : m->get_params()) {
                auto* pt = (API::TypeDefinition*)p.t;
                ps += (ps.empty() ? "" : ", ") + std::string(pt != nullptr ? pt->get_full_name() : "?") + " " + (p.name != nullptr ? p.name : "");
            }
            LOGI("%s     %s :: %s %s(%s)", TAG, tn.c_str(), rt != nullptr ? rt->get_full_name().c_str() : "void", m->get_name(), ps.c_str());
            if (++lines >= 120) break;
        }
        for (auto* f : td->get_fields()) {
            const auto ln = lower(f->get_name());
            bool hit = false; for (auto* w : words) hit = hit || ln.find(w) != std::string::npos;
            if (!hit) continue;
            auto* ft = f->get_type();
            LOGI("%s     %s :: field %s %s @+0x%x%s", TAG, tn.c_str(), ft != nullptr ? ft->get_full_name().c_str() : "?", f->get_name(), f->get_offset_from_base(), f->is_static() ? " (static)" : "");
            if (++lines >= 120) break;
        }
    }
    if (lines >= 120) LOGW("%s     (surface dump capped at 120 lines)", TAG);
}

void dump_ik() {
    auto* ik = g.ik;
    if (ik == nullptr) { LOGI("%s ik: none", TAG); return; }
    LOGI("%s ---- IK DUMP: %s ----", TAG, tname(ik).c_str());
    // The two unread candidates for what places the support hand (board, 2026-09-04 22:30).
    dump_type_surface(inv_ptr(ik, "getIkTwoArm"), "getIkTwoArm()");
    dump_type_surface(inv_ptr(ik, "getIkHand"), "getIkHand()");
    LOGI("%s   UseIkArm=%d UseIkWrist=%d UseIkArmFitAsWrist=%d WristKind=%u WristSolveMode=%u", TAG,
         (int)inv_bool(ik, "get_UseIkArm"), (int)inv_bool(ik, "get_UseIkWrist"), (int)inv_bool(ik, "get_UseIkArmFitAsWrist"),
         inv_u32(ik, "get_WristKind"), inv_u32(ik, "get_WristSolveMode"));
    static const char* kinds[] = {"LEG", "SPINE", "LOOKAT", "ARM", "ARMFIT", "HAND"};
    for (int k = 0; k < 6; ++k) {
        LOGI("%s   isEnabled(%s)=%d", TAG, kinds[k], (int)inv_bool_i(ik, "isEnabled", k));
    }
    auto* arms = inv_ptr(ik, "get_ArmStatusList");
    const auto n = arr_count(arms);
    auto* ctl = inv_ptr(ik, "get_ControlStatus");
    LOGI("%s   ArmStatusList: %u entries; ControlStatus: %u entries", TAG, arms != nullptr ? n : 0, ctl != nullptr ? arr_count(ctl) : 0);
    for (uint32_t i = 0; arms != nullptr && i < n && i < 8; ++i) {
        auto* st = arr_ptr_at(arms, i);
        if (!is_managed(st)) { LOGW("%s   arm[%u] not managed", TAG, i); break; }
        const auto has = field_at<uint8_t>(st, 0x20);
        const auto ap = field_at<Vec3>(st, 0x30);
        LOGI("%s   arm[%u] Index=%d AdjustMode=%d(0 NONE,1 CANCEL,2 FIT) ActivateTime=%.3f ResetTime=%.3f AdjustedPoint=%s(%.3f %.3f %.3f)", TAG, i,
             field_at<int32_t>(st, 0x10), field_at<int32_t>(st, 0x1c), field_at<float>(st, 0x14), field_at<float>(st, 0x18),
             has ? "" : "none ", ap.x, ap.y, ap.z);
        if (auto* m = find_method_deep(st->get_type_definition(), "get_TargetPosition"); m != nullptr) {
            auto r = m->invoke(st, std::vector<void*>{});
            const float* f = (const float*)r.bytes.data();
            LOGI("%s          get_TargetPosition -> %s raw=(%.3f %.3f %.3f %.3f) exc=%d", TAG,
                 m->get_return_type() != nullptr ? m->get_return_type()->get_full_name().c_str() : "?", f[0], f[1], f[2], f[3], (int)r.exception_thrown);
        }
        if (auto* m = find_method_deep(st->get_type_definition(), "get_DampingPosition"); m != nullptr) {
            auto r = m->invoke(st, std::vector<void*>{});
            const float* f = (const float*)r.bytes.data();
            LOGI("%s          get_DampingPosition -> %s raw=(%.3f %.3f %.3f %.3f) exc=%d", TAG,
                 m->get_return_type() != nullptr ? m->get_return_type()->get_full_name().c_str() : "?", f[0], f[1], f[2], f[3], (int)r.exception_thrown);
        }
    }
    LOGI("%s ---- end IK dump ----", TAG);
}

void dump_layers(bool force_table) {
    auto* mo = g.motion;
    if (mo == nullptr) return;
    const auto n = inv_u32(mo, "getLayerCount");
    if (n == 0xFFFFFFFFu || n > 64) { if (force_table) LOGW("%s motion: getLayerCount=%u", TAG, n); return; }
    if (g.layer_last.size() != n) g.layer_last.assign(n, "");
    const auto sec = (uint64_t)now_s();
    if (sec != g.layer_second) { g.layer_second = sec; g.layer_lines_this_second = 0; }
    if (force_table) LOGI("%s ---- MOTION LAYERS (%u) TargetBankType=%u ----", TAG, n, inv_u32(mo, "get_TargetBankType"));
    for (uint32_t i = 0; i < n; ++i) {
        auto* layer = inv_ptr(mo, "getLayer", {(void*)(uintptr_t)i});
        if (layer == nullptr) continue;
        auto* node = inv_ptr(layer, "get_HighestWeightMotionNode");
        const std::string name = node != nullptr ? sysstr(inv_ptr(node, "get_MotionName")) : "-";
        const bool changed = name != g.layer_last[i];
        if (changed || force_table) {
            if (!force_table && g.layer_lines_this_second++ > 24) { g.layer_last[i] = name; continue; }
            LOGI("%s   layer[%u] %s motion=%-36s speed=%.3f blend=%.2f bank=%u id=%u frame=%.0f/%.0f", TAG, i, changed ? "CHG" : "   ",
                 name.c_str(), inv_f32(layer, "get_Speed"), inv_f32(layer, "get_BlendRate"),
                 inv_u32(layer, "get_MotionBankID"), inv_u32(layer, "get_MotionID"), inv_f32(layer, "get_Frame"), inv_f32(layer, "get_EndFrame"));
            g.layer_last[i] = name;
        }
    }
}


// The req-4 speed hunt, from the /gr drop drained into dossier 8d on 2026-09-05. Public source
// witnesses a GETTER for a component-wide PlaySpeed on via.motion.Motion, above every layer; whether
// a SETTER exists is a hypothesis, and one method enumeration settles it. The same dump reads
// get_Weight per layer, which identifies the layer driving the pose by measurement instead of by
// matching motion-name strings. Both were asked for by the drop; neither has been run.
void dump_motion() {
    auto* mo = g.motion;
    if (mo == nullptr) { LOGI("%s motion component: none (no via.motion.Motion on the player)", TAG); return; }
    LOGI("%s ---- MOTION COMPONENT (req 4 speed hunt): %s ----", TAG, tname(mo).c_str());
    dump_type_surface(mo, "via.motion.Motion surface", {"speed", "play", "rate", "layer"});
    // Floats go through the DIRECT call route on this build; the reflection invoke returns 0 for every
    // float (dossier 8d). NaN here therefore means "no such method", not "the value is zero".
    LOGI("%s   get_PlaySpeed = %.4f   (NaN = method absent or not a float)", TAG, inv_f32(mo, "get_PlaySpeed"));
    const auto n = inv_u32(mo, "getLayerCount");
    LOGI("%s   getLayerCount = %u   (0xFFFFFFFF = absent; get_LayerCount is the fallback spelling)", TAG, n);
    for (uint32_t i = 0; i < 8 && n != 0xFFFFFFFFu && i < n; ++i) {
        auto* layer = inv_ptr(mo, "getLayer", {(void*)(uintptr_t)i});
        if (layer == nullptr) continue;
        auto* node = inv_ptr(layer, "get_HighestWeightMotionNode");
        const std::string name = node != nullptr ? sysstr(inv_ptr(node, "get_MotionName")) : "-";
        LOGI("%s   layer[%u] get_Weight=%.4f get_Speed=%.4f motion=%s", TAG, i,
             inv_f32(layer, "get_Weight"), inv_f32(layer, "get_Speed"), name.c_str());
    }
    LOGI("%s ---- end motion component ----", TAG);
}

void summary_line() {
    Vec3 lh{}, rh{}, aid{};
    const bool have_lh = g.l_hand != nullptr && inv_vec3(g.l_hand, "get_Position", lh);
    const bool have_rh = g.r_hand != nullptr && inv_vec3(g.r_hand, "get_Position", rh);
    if (g.aid_joint == nullptr && g.weapon != nullptr) {
        g.aid_joint = inv_ptr(g.weapon, "get_AidJoint");
        if (g.aid_joint != nullptr) LOGI("%s AidJoint appeared: %s", TAG, joint_name(g.aid_joint).c_str());
    }
    const bool have_aid = g.aid_joint != nullptr && inv_vec3(g.aid_joint, "get_Position", aid);
    Vec3 wn{}, ww{};
    const bool have_wn = g.w_narrow != nullptr && inv_vec3(g.w_narrow, "get_Position", wn);
    const bool have_ww = g.w_wide != nullptr && inv_vec3(g.w_wide, "get_Position", ww);
    const bool hold = inv_bool(g.cond, "get_IsHold");
    const auto aidtype = g.equipment != nullptr ? inv_u32(g.equipment, "getAidJointType") : 0;

    Mat4 ikm{}; bool ik_has = false;
    bool ik_ok = false;
    { SelfCall sc; ik_ok = g.weapon != nullptr && inv_nullable_mat4(g.weapon, "getIKLeftArmMatrix", ik_has, ikm); }

    char buf[1024];
    int o = snprintf(buf, sizeof buf, "%s f=%llu hold=%d aidT=%u", TAG, (unsigned long long)g.frame, (int)hold, aidtype);
    if (have_lh) o += snprintf(buf + o, sizeof buf - o, " Lhand=(%.2f %.2f %.2f)", lh.x, lh.y, lh.z);
    if (have_rh) o += snprintf(buf + o, sizeof buf - o, " Rhand=(%.2f %.2f %.2f)", rh.x, rh.y, rh.z);
    if (have_aid) o += snprintf(buf + o, sizeof buf - o, " aid=(%.2f %.2f %.2f)", aid.x, aid.y, aid.z);
    if (have_lh && have_aid) o += snprintf(buf + o, sizeof buf - o, " |Lhand-aid|=%.3f", dist(lh, aid));
    if (have_lh && have_wn) o += snprintf(buf + o, sizeof buf - o, " |Lhand-narrow|=%.3f", dist(lh, wn));
    if (have_lh && have_ww) o += snprintf(buf + o, sizeof buf - o, " |Lhand-wide|=%.3f", dist(lh, ww));
    if (have_rh && have_lh) o += snprintf(buf + o, sizeof buf - o, " |Lhand-Rhand|=%.3f", dist(lh, rh));
    if (ik_ok) {
        if (ik_has) o += snprintf(buf + o, sizeof buf - o, " ikL=(%.2f %.2f %.2f)", ikm.m[12], ikm.m[13], ikm.m[14]);
        else o += snprintf(buf + o, sizeof buf - o, " ikL=none");
    }
    // v0.4 dock report: where the wrist is against the target and against the natural (un-hooked) value, whether
    // its ROTATION followed (angle vs target and vs natural), and whether the RIGHT hand stayed on the muzzle.
    {
        auto& d = g.dock;
        o += snprintf(buf + o, sizeof buf - o, " | dock=%d(syn=%d lg=%d) w=%.2f rot=%d sp=%d hooks(aid=%u ikL=%u)",
                      (int)d.docked, (int)d.synthetic, (int)d.lg_held, d.w_eased, (int)d.write_rot, d.space_mode, g_calls_aid.exchange(0), g_calls_ikl.exchange(0));
        if (have_lh && d.target_valid) o += snprintf(buf + o, sizeof buf - o, " |Lw-tgt|=%.3f", dist(lh, d.target_t));
        if (have_lh && d.natural_valid) o += snprintf(buf + o, sizeof buf - o, " |Lw-nat|=%.3f", dist(lh, Vec3{d.natural.m[12], d.natural.m[13], d.natural.m[14]}));
        // v0.4.1: the four positions side by side — the game's natural (blend origin), the plugin's own read of the
        // same getter, the inner getter's value on the game's call, and the target — each as a distance from the joint.
        if (have_aid) {
            if (d.natural_valid)      o += snprintf(buf + o, sizeof buf - o, " |natG-aid|=%.3f", dist(Vec3{d.natural.m[12], d.natural.m[13], d.natural.m[14]}, aid));
            if (d.natural_self_valid) o += snprintf(buf + o, sizeof buf - o, " |natS-aid|=%.3f", dist(Vec3{d.natural_self.m[12], d.natural_self.m[13], d.natural_self.m[14]}, aid));
            if (d.inner_valid)        o += snprintf(buf + o, sizeof buf - o, " |inner-aid|=%.3f", dist(d.inner_t, aid));
            if (d.target_valid)       o += snprintf(buf + o, sizeof buf - o, " |tgt-aid|=%.3f", dist(d.target_t, aid));
        }
        if (d.M_valid) { Rows I; o += snprintf(buf + o, sizeof buf - o, " angM=%.0f", rows_angle_deg(d.M, I)); }
        // v0.6: the arm's measured length and whether this frame's target was outside it. clamp=0.000 with a
        // non-zero reach means the target was in range; a reach of 0 means the two arm joints were never bound.
        if (d.reach_valid) o += snprintf(buf + o, sizeof buf - o, " reach=%.3f clamp=%.3f%s", d.reach, d.clamped_m, d.use_reach_clamp ? "" : "(OFF)");
        else               o += snprintf(buf + o, sizeof buf - o, " reach=none");
        if (d.natural_valid && d.natural_self_valid)
            o += snprintf(buf + o, sizeof buf - o, " |natG-natS|=%.3f rotG-S=%.0f", dist(Vec3{d.natural.m[12], d.natural.m[13], d.natural.m[14]}, Vec3{d.natural_self.m[12], d.natural_self.m[13], d.natural_self.m[14]}),
                          rows_angle_deg(rows_of(d.natural), rows_of(d.natural_self)));
        Mat4 wm{};
        if (g.l_hand != nullptr && inv_mat4(g.l_hand, "get_WorldMatrix", wm)) {
            const Rows wr = rows_of(wm);
            if (d.natural_valid) o += snprintf(buf + o, sizeof buf - o, " rotW-N=%.0f", rows_angle_deg(wr, rows_of(d.natural)));
            if (d.target_valid)  o += snprintf(buf + o, sizeof buf - o, " rotW-T=%.0f", rows_angle_deg(wr, d.target_r));
            // once per second while the dock is active: the three frames in full, for the rotation-convention check offline
            static double last_rows_t = 0.0;
            if ((d.docked || d.w > 0.f) && d.natural_valid && d.target_valid && now_s() - last_rows_t >= 1.0) {
                last_rows_t = now_s();
                const Rows nr = rows_of(d.natural);
                LOGI("%s ROWS natG=[%.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f] t=(%.3f %.3f %.3f)", TAG, nr.r[0], nr.r[1], nr.r[2], nr.r[3], nr.r[4], nr.r[5], nr.r[6], nr.r[7], nr.r[8], d.natural.m[12], d.natural.m[13], d.natural.m[14]);
                LOGI("%s ROWS tgt =[%.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f] t=(%.3f %.3f %.3f)", TAG, d.target_r.r[0], d.target_r.r[1], d.target_r.r[2], d.target_r.r[3], d.target_r.r[4], d.target_r.r[5], d.target_r.r[6], d.target_r.r[7], d.target_r.r[8], d.target_t.x, d.target_t.y, d.target_t.z);
                LOGI("%s ROWS wrst=[%.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f] t=(%.3f %.3f %.3f)", TAG, wr.r[0], wr.r[1], wr.r[2], wr.r[3], wr.r[4], wr.r[5], wr.r[6], wr.r[7], wr.r[8], wm.m[12], wm.m[13], wm.m[14]);
                if (have_aid) LOGI("%s ROWS aid=(%.3f %.3f %.3f) Lwrist=(%.3f %.3f %.3f)", TAG, aid.x, aid.y, aid.z, lh.x, lh.y, lh.z);
            }
        }
        Mat4 mz{};
        if (have_rh && g.weapon != nullptr && inv_mat4(g.weapon, "get_MuzzleJointWorldMatrix", mz))
            o += snprintf(buf + o, sizeof buf - o, " |Rw-muz|=%.3f", dist(rh, Vec3{mz.m[12], mz.m[13], mz.m[14]}));
        if (d.no_value_frames != 0) { o += snprintf(buf + o, sizeof buf - o, " NOVALUE=%u", d.no_value_frames); d.no_value_frames = 0; }
        if (d.cam_valid) o += snprintf(buf + o, sizeof buf - o, " cam=(%.2f %.2f %.2f)", d.cam_t.x, d.cam_t.y, d.cam_t.z);
        // v0.11 camera diagnostic (see update_camera2). cam = the old read-once value; camF = the same call
        // re-made this frame; cam2 = the camera transform's joint 0, the route REFramework itself uses.
        // src names which fallback produced cam2, camA is the camera object's address.
        if (d.cam2_valid) o += snprintf(buf + o, sizeof buf - o, " cam2=(%.2f %.2f %.2f)", d.cam2_t.x, d.cam2_t.y, d.cam2_t.z);
        if (d.camf_valid) o += snprintf(buf + o, sizeof buf - o, " camF=(%.2f %.2f %.2f)", d.camf_t.x, d.camf_t.y, d.camf_t.z);
        o += snprintf(buf + o, sizeof buf - o, " src=%s camA=0x%llx", d.cam2_src, (unsigned long long)d.cam_addr);
    }
    if (bridge_live()) {
        const float* f = arr_f32(g.bridge);
        o += snprintf(buf + o, sizeof buf - o, " | vr f=%.0f hmd=%.0f ctl=%.0f", f[S_FRAME], f[S_HMD_ACTIVE], f[S_USING_CTL]);
        if (f[S_USING_CTL] != 0.0f) {
            const Vec3 lc = bridge_vec3(S_LPOS), rc = bridge_vec3(S_RPOS);
            o += snprintf(buf + o, sizeof buf - o, " Lctl=(%.2f %.2f %.2f) Rctl=(%.2f %.2f %.2f)", lc.x, lc.y, lc.z, rc.x, rc.y, rc.z);
            if (have_aid) o += snprintf(buf + o, sizeof buf - o, " |Lctl-aid|=%.3f", dist(lc, aid));
            if (have_lh) o += snprintf(buf + o, sizeof buf - o, " |Lctl-Lhand|=%.3f", dist(lc, lh));
            if (have_rh) o += snprintf(buf + o, sizeof buf - o, " |Rctl-Rhand|=%.3f", dist(rc, rh));
            const Vec3 hc = bridge_vec3(S_HPOS);
            o += snprintf(buf + o, sizeof buf - o, " Hctl=(%.2f %.2f %.2f) LG=%.2f RG=%.2f RT=%.2f", hc.x, hc.y, hc.z, f[S_LGRIP], f[S_RGRIP], f[S_RTRIG]);
        }
        o += snprintf(buf + o, sizeof buf - o, " stick=(%.2f %.2f)", f[S_LSTICK], f[S_LSTICK + 1]);
    } else {
        o += snprintf(buf + o, sizeof buf - o, " | vr bridge: not attached");
    }
    // v0.7: where the neck plug is (it should sit on neck_0, ~1.10 m over the feet when standing)
    if (g.plug.created) o += snprintf(buf + o, sizeof buf - o, " | plug=%s @(%.2f %.2f %.2f)", g.plug.enabled ? "on" : "off", g.plug.last_pos.x, g.plug.last_pos.y, g.plug.last_pos.z);
    else if (g.neck0 == nullptr) o += snprintf(buf + o, sizeof buf - o, " | plug: no neck_0");
    else o += snprintf(buf + o, sizeof buf - o, " | plug: %s", g.plug.tried ? "FAILED (see log)" : "pending");
    // v0.10: the bracelets — created or not, the twist blend, and the live wrist-vs-radius twist per arm (deg)
    if (g.bracelets.l.created || g.bracelets.r.created)
        o += snprintf(buf + o, sizeof buf - o, " | brac=%s k=%.1f conv=%d twist l=%.0f r=%.0f L@(%.2f %.2f %.2f)", g.bracelets.enabled ? "on" : "off", BRACELET_K[g.bracelets.k_idx], g.bracelets.conv,
                      g.bracelets.l.theta * 57.29578f, g.bracelets.r.theta * 57.29578f, g.bracelets.l.last_pos.x, g.bracelets.l.last_pos.y, g.bracelets.l.last_pos.z);
    else if (g.l_radius == nullptr) o += snprintf(buf + o, sizeof buf - o, " | brac: no radius joints");
    else o += snprintf(buf + o, sizeof buf - o, " | brac: %s", (g.bracelets.l.tried || g.bracelets.r.tried) ? "FAILED (see log)" : "pending");
    o += snprintf(buf + o, sizeof buf - o, " | caught=%d/%u", g_catch.n, g_catch.calls.load());   // v0.15 route E, live
    o += snprintf(buf + o, sizeof buf - o, " | head=%d hid=%d/%zu d=%.2f%s%s", g.head.mode, g.head.hidden_n, g.head.meshes.size(), g.head.head_cam_d,
                  g.head.revealed ? " REVEAL:" : "", g.head.revealed ? g.head.reveal_why.c_str() : "");
    LOGI("%s", buf);
}

} // namespace visceral
