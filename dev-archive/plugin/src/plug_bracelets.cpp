// plug_bracelets.cpp -- the neck plug and the forearm bracelets.
// Split out of Plugin.cpp 2026-09-27, move only (SPLIT-MAP-2026-09-24.md).
#include "visceral.h"

namespace visceral {


// ---------------------------------------------------------------------------
// v0.7: the neck plug (roadmap v2 H1). REFramework's first person scales the head joint to zero, so every
// face vertex weighted to `head` collapses to a point while the neck skin — weighted to neck_0 / neck_1 —
// stays behind as an open tube the camera looks straight down into: that is the "hollow" Claire. The plug is
// OUR OWN mesh (7 KB, a capped cylinder r=4 cm from the collar to just under the chin, built by
// tools/blender/build_neckplug.py in neck_0's bind frame from measurements of the real skeleton) on its own
// GameObject, pinned to neck_0's world pose every frame and wearing Claire's own skin material BY NAME
// (pl1000_Body_Mat out of the game's own pl1000.mdf2, which the engine loads from its pak; v0.8 said pl3000_Skin_Mat, which is
// SHERRY's — the plug drew in her skin material, colour-matched by luck). Nothing of the
// game's is shipped and no game file is replaced. Inside the neck it is invisible in third person; in first
// person it is the skin-coloured floor the collapsed head leaves behind. [hypothesis until the first flat run]
// ---------------------------------------------------------------------------
// "n=K: a, b, c" from via.render.Mesh.get_MaterialNum / getMaterialName(i); "n/a" if this build lacks them.
// Material names are the reliable identity of a mesh part (Claire: pl1000_Body_Mat; face pl1050_Face_Mat / pl1050_Eyelash_Mat /
// pl1050_Eyes_In_Mat / pl1050_Eyes_Out_Mat / pl1050_Tearline_Mat; hair pl1070_Hair_Mat/_Hair2_/_Hair3_ — measured from her MDFs
// 2026-09-06), where GameObject names are not known statically.
std::string mesh_material_names(API::ManagedObject* mesh) {
    if (mesh == nullptr) return "null";
    auto* td = mesh->get_type_definition();
    if (find_method_deep(td, "get_MaterialNum") == nullptr || find_method_deep(td, "getMaterialName") == nullptr) return "n/a (no get_MaterialNum/getMaterialName)";
    const uint32_t n = inv_u32(mesh, "get_MaterialNum");
    if (n == 0xFFFFFFFFu || n > 64) return "n/a (count implausible)";
    std::string out = "n=" + std::to_string(n) + ":";
    for (uint32_t i = 0; i < n; ++i) {
        auto* nm = inv_ptr(mesh, "getMaterialName", {(void*)(uintptr_t)i});
        out += (i ? ", " : " ") + (nm != nullptr ? sysstr(nm) : std::string("?"));
    }
    return out;
}

bool remat_if_empty(API::ManagedObject* mesh, const char* mdf_path, const char* what,
                    int& tries, double& last_t, bool manual) {
    constexpr int MAT_MAX_TRIES = 8;
    constexpr double MAT_RETRY_S = 0.5;
    if (mesh == nullptr || !is_managed(mesh)) return false;
    auto* td = mesh->get_type_definition();
    if (td == nullptr || find_method_deep(td, "get_MaterialNum") == nullptr) return false;
    if (inv_u32(mesh, "get_MaterialNum") > 0) return true;            // already has one, nothing to do
    if (tries >= MAT_MAX_TRIES) return false;
    if (now_s() - last_t < MAT_RETRY_S) return false;
    last_t = now_s();
    ++tries;
    auto* h = create_resource_holder("via.render.MeshMaterialResource", mdf_path, "via.render.MeshMaterialResourceHolder", manual);
    if (h == nullptr) {
        if (tries == MAT_MAX_TRIES) LOGW("%s %s: material resource still missing after %d tries (%s)", TAG, what, tries, mdf_path);
        return false;
    }
    const bool ok = inv(mesh, "set_Material", {h}).ok;
    const uint32_t n = inv_u32(mesh, "get_MaterialNum");
    if (n > 0) {
        LOGI("%s %s: material took on attempt %d (%u material(s): %s)", TAG, what, tries, n, mesh_material_names(mesh).c_str());
        return true;
    }
    if (tries >= MAT_MAX_TRIES)
        LOGW("%s %s: material STILL empty after %d attempts (set_Material %s) — it will draw nothing", TAG, what, tries, ok ? "ok" : "threw");
    return false;
}


// This run puts one of each in the game so a single launch decides: LEFT bracelet + the plug manual, RIGHT bracelet the old way.
API::ManagedObject* create_resource_holder(const char* res_type, const char* path, const char* holder_type, bool manual) {
    auto& api = API::get();
    auto* res = api->resource_manager()->create_resource(res_type, path);
    if (res == nullptr) { LOGW("%s create_resource(%s, %s) returned null", TAG, res_type, path); return nullptr; }
    if (manual) {
        auto* ht = api->tdb()->find_type(holder_type);
        auto* h = ht != nullptr ? ht->create_instance(1) : nullptr;      // flags 1 = allocate without running the ctor
        if (h != nullptr) {
            h->add_ref();
            res->add_ref();
            *(void**)((uintptr_t)h + 0x10) = (void*)res;                 // the holder's resource slot, as the Lua mod writes it
            LOGI("%s holder(%s) built MANUALLY for %s", TAG, holder_type, path);
            return h;
        }
        LOGW("%s holder(%s): create_instance failed, falling back to create_holder", TAG, holder_type);
    }
    auto* holder = (API::ManagedObject*)api->sdk()->resource->create_holder((REFrameworkResourceHandle)res, holder_type);
    if (holder == nullptr) LOGW("%s create_holder(%s) returned null", TAG, holder_type);
    else LOGI("%s holder(%s) built by create_holder for %s", TAG, holder_type, path);
    return holder;
}

void plug_create() {
    auto& p = g.plug;
    p.tried = true;
    auto& api = API::get();
    auto* go_t = api->tdb()->find_type("via.GameObject");
    auto* create = go_t != nullptr ? go_t->find_method("create(System.String)") : nullptr;
    if (create == nullptr) { LOGE("%s plug: via.GameObject.create(System.String) not found", TAG); return; }
    auto* name = api->create_managed_string(L"visceral_neckplug");
    auto r = create->invoke(nullptr, {name});
    if (r.exception_thrown || r.ptr == nullptr) { LOGE("%s plug: GameObject.create threw or returned null", TAG); return; }
    p.go = (API::ManagedObject*)r.ptr;
    p.go->add_ref();
    p.transform = inv_ptr(p.go, "get_Transform");
    auto* mesh_t = api->typeof("via.render.Mesh");
    auto* add = find_method_deep(p.go->get_type_definition(), "createComponent(System.Type)");
    if (mesh_t == nullptr || add == nullptr) { LOGE("%s plug: createComponent(System.Type) / typeof(via.render.Mesh) missing", TAG); return; }
    auto cr = add->invoke(p.go, {mesh_t});
    if (cr.exception_thrown || cr.ptr == nullptr) { LOGE("%s plug: createComponent(via.render.Mesh) failed", TAG); return; }
    p.mesh = (API::ManagedObject*)cr.ptr;
    p.mesh->add_ref();
    auto* mesh_holder = create_resource_holder("via.render.MeshResource", PLUG_MESH_PATH, "via.render.MeshResourceHolder", true);
    auto* mdf_holder  = create_resource_holder("via.render.MeshMaterialResource", PLUG_MDF_PATH, "via.render.MeshMaterialResourceHolder", true);
    if (mesh_holder == nullptr) {
        LOGE("%s plug: mesh resource missing — is natives/stm/%s.2109108288 in the game folder, and LooseFileLoader_Enabled=true in re2_fw_config.txt?", TAG, PLUG_MESH_PATH);
        return;
    }
    if (!inv(p.mesh, "setMesh", {mesh_holder}).ok) { LOGE("%s plug: setMesh threw", TAG); return; }
    if (mdf_holder != nullptr && !inv(p.mesh, "set_Material", {mdf_holder}).ok) LOGW("%s plug: set_Material threw — the plug will draw with no material", TAG);
    // v0.11: JOIN THE SCENE. A spawned GameObject that is parented to nothing is in no hierarchy the renderer walks; the one
    // REFramework mod found that provably draws a spawned mesh (Universal Lasers, RE4) parents its Transform to an existing
    // object's right after createComponent, then sets world position/rotation each frame exactly as we do. The plug has
    // never been seen since v0.7, and the bracelets were created but invisible on 2026-09-06 19:51 — this is the one
    // difference. [hypothesis until the next run]
    if (g.transform != nullptr) {
        const auto pr = inv(p.transform, "set_Parent", {g.transform});
        LOGI("%s plug: Transform.set_Parent(player) %s", TAG, pr.ok ? "ok" : "THREW / NOT FOUND");
    }
    p.created = true;
    p.enabled_sent = true;
    LOGI("%s PLUG CREATED: go=%p transform=%p mesh=%p (mesh %s, mdf %s) — NUM0 toggles it", TAG, (void*)p.go, (void*)p.transform, (void*)p.mesh, PLUG_MESH_PATH, PLUG_MDF_PATH);
    // v0.8: the 2026-09-06 run could not tell "drawing but occluded" from "not drawing" — say what the mesh itself reports.
    LOGI("%s PLUG STATE: DrawDefault=%d DrawShadowCast=%d materials: %s", TAG,
         (int)inv_bool(p.mesh, "get_DrawDefault"), (int)inv_bool(p.mesh, "get_DrawShadowCast"), mesh_material_names(p.mesh).c_str());
}

void plug_update() {
    auto& p = g.plug;
    if (g.player_go == nullptr || g.neck0 == nullptr) return;
    if (p.created && (!is_managed(p.go) || !is_managed(p.transform) || !is_managed(p.mesh))) {
        if (!p.lost) LOGW("%s plug: GameObject is no longer a managed object (scene wipe?) — recreating", TAG);
        p = State::Plug{}; p.lost = true;
    }
    if (!p.created) {
        if (p.tried) return;          // caller-owned budget as of v0.17; rebind_player() re-arms it
        plug_create();
        if (!p.created) return;
    }
    remat_if_empty(p.mesh, PLUG_MDF_PATH, "plug", p.mat_tries, p.last_mat_t, true);
    p.lost = false;
    Vec3 jp{};
    if (!inv_vec3(g.neck0, "get_Position", jp)) return;
    Quat jq{};
    { auto x = inv(g.neck0, "get_Rotation"); if (!x.ok) return; memcpy(&jq, x.r.bytes.data(), sizeof(Quat)); }
    // via.Transform.set_Position(via.vec3) / set_Rotation(via.Quaternion): 16-byte value types, passed by reference on
    // x64, so the direct route with a pointer is the real ABI (the reflection route mangles value-type args here).
    V4 pos{jp.x, jp.y, jp.z, 0.f};
    V4 rot{jq.x, jq.y, jq.z, jq.w};
    call_void_direct(p.transform, "set_Position", &pos);
    call_void_direct(p.transform, "set_Rotation", &rot);
    if (p.enabled != p.enabled_sent) {
        p.enabled_sent = p.enabled;
        call_void_direct(p.mesh, "set_DrawDefault", p.enabled);   // dossier §7: the per-pass draw flag, not the bone
        call_void_direct(p.mesh, "set_DrawShadowCast", p.enabled);
        p.readback_pending = true;
    } else if (p.readback_pending) {
        p.readback_pending = false;
        const bool rb = inv_bool(p.mesh, "get_DrawDefault");
        LOGI("%s plug read-back: DrawDefault=%d (wanted %d)%s", TAG, (int)rb, (int)p.enabled, rb == p.enabled ? "" : " — THE WRITE DID NOT STICK");
    }
    p.last_pos = jp;
}


// ---------------------------------------------------------------------------
// v0.10: FOREARM BRACELETS (Tefa, 2026-09-06): "cover over these exact straight lines so they can stay under the
// bracelets ... metal and leather combined, different on both hands". Two meshes of our own (tools/blender/
// build_bracelets.py, wrapped onto Claire's sampled arm cross-section, exported in the ARM_RADIUS joint's bind frame),
// four materials of our own (tools/re-engine/mdf_build_bracelets.py: leather tinted brown / dark red / red-purple and
// brushed steel, on our own 512^2 tiles) — the same runtime recipe as the neck plug: a GameObject each, a
// via.render.Mesh, a MeshResourceHolder on a loose file under natives/stm/visceral/, a MeshMaterialResourceHolder
// on our MDF, and the Transform pinned to a joint every frame.
//
// Which joint: the live skeleton has NO twist helpers (l_arm_radius -> l_arm_wrist -> l_weapon, verified in the
// 2026-09-06 joint dump), and the wrist joint carries the hand's flexion, which a bracelet must not follow. So the
// bracelet is pinned to the RADIUS joint (elbow end) and, optionally, rotated about the forearm axis by a FRACTION k
// of the wrist's twist relative to the radius. k = 0 is the safe baseline (rigid to the radius). Whether the radius
// joint already carries the pronation, and which quaternion convention the engine wants for the extra rotation, are
// unknown statically: `theta` is logged so the first run can read the split, NUM- cycles k, NUM/ cycles the
// convention. [hypothesis until the first run]
// ---------------------------------------------------------------------------

void bracelet_create(State::Bracelet& b, int side) {
    // v0.17: the caller owns the retry budget now; this used to set b.tried = true here and so could
    // never be attempted twice within one level.
    auto& api = API::get();
    auto* go_t = api->tdb()->find_type("via.GameObject");
    auto* create = go_t != nullptr ? go_t->find_method("create(System.String)") : nullptr;
    if (create == nullptr) { LOGE("%s bracelet: via.GameObject.create(System.String) not found", TAG); return; }
    auto* name = api->create_managed_string(side == 0 ? L"visceral_bracelet_l" : L"visceral_bracelet_r");
    auto r = create->invoke(nullptr, {name});
    if (r.exception_thrown || r.ptr == nullptr) { LOGE("%s bracelet %c: GameObject.create threw or returned null", TAG, side == 0 ? 'l' : 'r'); return; }
    b.go = (API::ManagedObject*)r.ptr; b.go->add_ref();
    b.transform = inv_ptr(b.go, "get_Transform");
    auto* mesh_t = api->typeof("via.render.Mesh");
    auto* add = find_method_deep(b.go->get_type_definition(), "createComponent(System.Type)");
    if (mesh_t == nullptr || add == nullptr) { LOGE("%s bracelet: createComponent(System.Type) / typeof(via.render.Mesh) missing", TAG); return; }
    auto cr = add->invoke(b.go, {mesh_t});
    if (cr.exception_thrown || cr.ptr == nullptr) { LOGE("%s bracelet: createComponent(via.render.Mesh) failed", TAG); return; }
    b.mesh = (API::ManagedObject*)cr.ptr; b.mesh->add_ref();
    const bool manual = (side == 0);                                   // v0.12 A/B in one launch: LEFT manual, RIGHT the old create_holder
    LOGI("%s bracelet %c: holder route = %s", TAG, side == 0 ? 'l' : 'r', manual ? "MANUAL (+0x10)" : "create_holder (the old one)");
    auto* mesh_holder = create_resource_holder("via.render.MeshResource", BRACELET_MESH_PATH[side], "via.render.MeshResourceHolder", manual);
    auto* mdf_holder  = create_resource_holder("via.render.MeshMaterialResource", BRACELET_MDF_PATH, "via.render.MeshMaterialResourceHolder", manual);
    if (mesh_holder == nullptr) { LOGE("%s bracelet %c: mesh resource missing — is natives/stm/%s.2109108288 in the game folder?", TAG, side == 0 ? 'l' : 'r', BRACELET_MESH_PATH[side]); return; }
    if (!inv(b.mesh, "setMesh", {mesh_holder}).ok) { LOGE("%s bracelet %c: setMesh threw", TAG, side == 0 ? 'l' : 'r'); return; }
    if (mdf_holder == nullptr) LOGW("%s bracelet: MDF resource missing (natives/stm/%s.21) — it will draw with no material", TAG, BRACELET_MDF_PATH);
    else if (!inv(b.mesh, "set_Material", {mdf_holder}).ok) LOGW("%s bracelet %c: set_Material threw", TAG, side == 0 ? 'l' : 'r');
    if (g.transform != nullptr) {                                            // v0.11: join the player's hierarchy (see plug_create)
        const auto pr = inv(b.transform, "set_Parent", {g.transform});
        LOGI("%s bracelet %c: Transform.set_Parent(player) %s", TAG, side == 0 ? 'l' : 'r', pr.ok ? "ok" : "THREW / NOT FOUND");
    }
    b.created = true; b.enabled_sent = true; b.created_frame = g.frame;
    LOGI("%s BRACELET %c CREATED: go=%p mesh=%p (%s + %s) — NUM* toggles both, NUM- cycles the twist blend, NUM/ the convention", TAG,
         side == 0 ? 'l' : 'r', (void*)b.go, (void*)b.mesh, BRACELET_MESH_PATH[side], BRACELET_MDF_PATH);
    LOGI("%s BRACELET %c STATE: DrawDefault=%d DrawShadowCast=%d materials: %s", TAG, side == 0 ? 'l' : 'r',
         (int)inv_bool(b.mesh, "get_DrawDefault"), (int)inv_bool(b.mesh, "get_DrawShadowCast"), mesh_material_names(b.mesh).c_str());
}

void bracelets_update() {
    auto& B = g.bracelets;
    if (g.player_go == nullptr) return;
    API::ManagedObject* rad[2] = {g.l_radius, g.r_radius};
    API::ManagedObject* wri[2] = {g.l_wrist, g.r_wrist};
    for (int side = 0; side < 2; ++side) {
        auto& b = side == 0 ? B.l : B.r;
        if (rad[side] == nullptr) continue;
        if (b.created && (!is_managed(b.go) || !is_managed(b.transform) || !is_managed(b.mesh))) {
            if (!b.lost) LOGW("%s bracelet %c: GameObject is no longer a managed object (scene wipe?) — recreating", TAG, side == 0 ? 'l' : 'r');
            b = State::Bracelet{}; b.lost = true;
        }
        if (!b.created) {
            // v0.17: RETRY, do not give up after one attempt. Tefa, 2026-09-12: the bracelets are absent on the
            // FIRST load after launching the game and present on every load after that, including a reload of the
            // very same save `[verified-live 2026-09-12, n=1 wearer]`. One attempt per bind, fired the moment the
            // player binds, is exactly that shape: the first one lands before the arm's joints are ready and there
            // was no second chance until the next level load. Six tries, half a second apart, then give up and say so.
            constexpr int BRACELET_MAX_TRIES = 6;
            constexpr double BRACELET_RETRY_S = 0.5;
            if (b.tried) continue;
            if (now_s() - b.last_try_t < BRACELET_RETRY_S) continue;
            b.last_try_t = now_s();
            ++b.attempts;
            bracelet_create(b, side);
            if (!b.created) {
                if (b.attempts >= BRACELET_MAX_TRIES) {
                    b.tried = true;
                    LOGW("%s bracelet %c: giving up after %d attempts — see the errors above", TAG, side == 0 ? 'l' : 'r', b.attempts);
                } else {
                    LOGI("%s bracelet %c: attempt %d did not take, retrying in %.1f s", TAG, side == 0 ? 'l' : 'r', b.attempts, BRACELET_RETRY_S);
                }
                continue;
            }
            LOGI("%s bracelet %c: created on attempt %d", TAG, side == 0 ? 'l' : 'r', b.attempts);
        }
        b.lost = false;
        remat_if_empty(b.mesh, BRACELET_MDF_PATH, side == 0 ? "bracelet l" : "bracelet r",
                       b.mat_tries, b.last_mat_t, side == 0);
        if (!b.relogged && g.frame > b.created_frame + 240) {               // ~4 s after creation: has the material resolved by now?
            b.relogged = true;
            Vec3 tp{}; inv_vec3(b.transform, "get_Position", tp);
            LOGI("%s BRACELET %c LATER: DrawDefault=%d materials: %s | transform @(%.2f %.2f %.2f)", TAG, side == 0 ? 'l' : 'r',
                 (int)inv_bool(b.mesh, "get_DrawDefault"), mesh_material_names(b.mesh).c_str(), tp.x, tp.y, tp.z);
        }
        Vec3 pr{};
        if (!inv_vec3(rad[side], "get_Position", pr)) continue;
        Quat qr{};
        { auto x = inv(rad[side], "get_Rotation"); if (!x.ok) continue; memcpy(&qr, x.r.bytes.data(), sizeof(Quat)); }
        Quat q = qr;
        float theta = 0.f;
        const float k = BRACELET_K[B.k_idx];
        Mat4 mr{}, mw{};
        if (wri[side] != nullptr && inv_mat4(rad[side], "get_WorldMatrix", mr) && inv_mat4(wri[side], "get_WorldMatrix", mw)) {
            // the wrist's twist about the forearm axis, relative to the radius joint: pure vector algebra on the two
            // world frames (rows = basis vectors in world, the convention the dock already relies on), so it does not
            // depend on any quaternion convention. Take the radius row least parallel to the axis, strip the axis
            // component, compare with the same row of the wrist.
            const Vec3 pw{mw.m[12], mw.m[13], mw.m[14]};
            Vec3 a = vsub(pw, pr); const float al = vlen(a);
            if (al > 1e-4f) {
                a = vscale(a, 1.f / al);
                const Rows Rr = rows_of(mr), Rw = rows_of(mw);
                int best = 0; float bd = 2.f;
                for (int i = 0; i < 3; ++i) { const Vec3 v = row(Rr, i); const float d = std::fabs(v.x * a.x + v.y * a.y + v.z * a.z); if (d < bd) { bd = d; best = i; } }
                auto perp = [&](Vec3 v) { const float d = v.x * a.x + v.y * a.y + v.z * a.z; v = vsub(v, vscale(a, d)); const float l = vlen(v); return l > 1e-5f ? vscale(v, 1.f / l) : Vec3{}; };
                const Vec3 ur = perp(row(Rr, best)), uw = perp(row(Rw, best));
                const Vec3 c{ur.y * uw.z - ur.z * uw.y, ur.z * uw.x - ur.x * uw.z, ur.x * uw.y - ur.y * uw.x};
                theta = std::atan2(c.x * a.x + c.y * a.y + c.z * a.z, ur.x * uw.x + ur.y * uw.y + ur.z * uw.z);
                if (k > 0.f) {
                    const float sign = (B.conv & 2) ? -1.f : 1.f;
                    const float h = 0.5f * k * theta * sign; const float sn = std::sin(h);
                    const Quat qa{a.x * sn, a.y * sn, a.z * sn, std::cos(h)};
                    q = quat_norm((B.conv & 1) ? quat_mul(qa, qr) : quat_mul(qr, qa));
                }
            }
        }
        b.theta = theta;
        V4 pos{pr.x, pr.y, pr.z, 0.f};
        V4 rot{q.x, q.y, q.z, q.w};
        call_void_direct(b.transform, "set_Position", &pos);
        call_void_direct(b.transform, "set_Rotation", &rot);
        if (B.enabled != b.enabled_sent) {
            b.enabled_sent = B.enabled;
            call_void_direct(b.mesh, "set_DrawDefault", B.enabled);
            call_void_direct(b.mesh, "set_DrawShadowCast", B.enabled);
        }
        b.last_pos = pr;
    }
}

} // namespace visceral
