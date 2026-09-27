// head_probes.cpp -- the diagnostic head routes B, C and owners, and the scene helpers they use.
// Split out of Plugin.cpp 2026-09-27, move only (SPLIT-MAP-2026-09-24.md).
#include "visceral.h"

namespace visceral {

void head_b_add_mesh(API::ManagedObject* mesh, const std::string& how, HeadProbeB& b) {
    if (!is_managed(mesh)) return;
    for (auto* s : b.seen_meshes) if (s == mesh) return;
    b.seen_meshes.push_back(mesh);
    std::string label = sysstr(inv_ptr(inv_ptr(mesh, "get_GameObject"), "get_Name"));
    if (label.empty()) label = "?";
    b.meshes.emplace_back(label + " [" + how + "]", mesh_material_names(mesh));
}

void head_b_add_go(API::ManagedObject* go, const std::string& how, HeadProbeB& b) {
    if (go == nullptr) return;
    ++b.objects;
    auto* comps = inv_ptr(go, "get_Components");
    if (comps == nullptr) return;
    const uint32_t n = arr_count(comps);
    if (n > 256) return;
    for (uint32_t i = 0; i < n; ++i) {
        auto* c = arr_ptr_at(comps, i);
        if (!is_managed(c) || tname(c) != "via.render.Mesh") continue;
        head_b_add_mesh(c, how, b);
    }
}

void head_b_walk(API::ManagedObject* tf, int depth, const std::string& how, HeadProbeB& b) {
    if (tf == nullptr || depth > 12 || b.transforms > 500) return;
    ++b.transforms;
    head_b_add_go(inv_ptr(tf, "get_GameObject"), how, b);
    int guard = 0;
    for (auto* ch = inv_ptr(tf, "get_Child"); ch != nullptr && guard < 500; ch = inv_ptr(ch, "get_Next"), ++guard)
        head_b_walk(ch, depth + 1, how, b);
}

void head_probe_b(HeadProbeB& b) {
    auto* changer = inv_ptr(g.cond, "get_CostumeChanger");
    if (changer != nullptr) {
        b.route = "cond.get_CostumeChanger";
    } else {
        changer = get_component(g.player_go, "app.ropeway.survivor.SurvivorCostumeChanger");
        if (changer != nullptr) b.route = "getComponent(SurvivorCostumeChanger)";
    }
    head_b_add_mesh(inv_ptr(g.cond, "get_Mesh"), "cond.Mesh", b);
    if (changer == nullptr) {
        b.route = "NO SurvivorCostumeChanger (cond=" + tname(g.cond) + ", getComponent also null)";
        return;
    }
    b.route += " (" + tname(changer) + ")";
    static const char* mesh_getters[] = {"get_Face", "get_Hair", "get_Body", "get_Other", "get_SheathKnife"};
    for (auto* gn : mesh_getters) head_b_add_mesh(inv_ptr(changer, gn), gn + 4, b);
    // Then the part GameObjects, walked — catches second mesh components and child mesh objects.
    static const char* obj_getters[] = {"get_FaceObject", "get_HairObject", "get_BodyObject", "get_OtherObject",
                                        "get_SheathKnifeObject", "get_AccessoryObject"};
    for (auto* gn : obj_getters) {
        auto* go = inv_ptr(changer, gn);
        if (go == nullptr) continue;
        auto* tf = inv_ptr(go, "get_Transform");
        if (tf != nullptr) head_b_walk(tf, 0, std::string(gn + 4) + "\\", b);
        else head_b_add_go(go, std::string(gn + 4) + "!", b);
    }
    // Which is it — the getters are MISSING on this build, or they exist and return null? head_b_add_mesh drops
    // both silently, and they mean completely different things, so say it outright.
    {
        std::string st;
        auto tell = [&](API::ManagedObject* owner, const char* gn) {
            st += std::string(st.empty() ? "" : " ") + (gn + 4) + "=";
            if (owner == nullptr) { st += "noowner"; return; }
            if (find_method_deep(owner->get_type_definition(), gn) == nullptr) { st += "MISSING"; return; }
            st += inv_ptr(owner, gn) != nullptr ? "ok" : "null";
        };
        tell(g.cond, "get_Mesh");
        for (auto* gn : mesh_getters) tell(changer, gn);
        for (auto* gn : obj_getters) tell(changer, gn);
        LOGI("%s head:   B getters: %s", TAG, st.c_str());
    }
    // Does the changer only fill those slots DURING a costume change? Read the cheap scalars that would say so.
    LOGI("%s head:   B changer state: SurvivorType=%u CurrentCostume=%u NowChanging=%d DrawHair=%d PartsNames=%u", TAG,
         inv_u32(changer, "get_SurvivorType"), inv_u32(changer, "get_CurrentCostume"),
         (int)inv_bool(changer, "get_NowChanging"), (int)inv_bool(changer, "get_DrawHair"),
         arr_count(inv_ptr(changer, "get_PartsNames")));

    // v0.15: the changer has a private gatherPartsMesh() — no arguments, returns void, id 28377 fn 0x14136ff60
    // `[inferred-static 2026-09-12]`. Its whole job is to fill the very slots that came back null, so ask it to,
    // ONCE per player bind, and re-read. Cheap, argument-free and idempotent by name; if it throws we say so and
    // never try again this bind. `[hypothesis — not run yet]`
    if (b.meshes.empty() && !g.head.gathered) {
        g.head.gathered = true;
        auto r = inv(changer, "gatherPartsMesh");
        if (find_method_deep(changer->get_type_definition(), "gatherPartsMesh") == nullptr) LOGW("%s head:   B gatherPartsMesh MISSING on this build", TAG);
        else if (!r.ok) LOGW("%s head:   B gatherPartsMesh THREW — not retried this bind", TAG);
        else {
            for (auto* gn : mesh_getters) head_b_add_mesh(inv_ptr(changer, gn), std::string(gn + 4) + "@gather", b);
            head_b_add_mesh(inv_ptr(g.cond, "get_Mesh"), "cond.Mesh@gather", b);
            LOGI("%s head:   B gatherPartsMesh() called — %zu mesh(es) after it", TAG, b.meshes.size());
        }
    }
}


// ---------------------------------------------------------------------------
// v0.14: ROUTE C — BRUTE FORCE, the ground truth, and it cannot come back empty.
//
// Route B resolved its component and still handed back nothing (`walkB 0 go 0 tf / 0 mesh via
// cond.get_CostumeChanger (app.ropeway.survivor.SurvivorCostumeChanger)`) `[verified-live 2026-09-12, n=1 flat run,
// RPD save]`, so the costume changer's Face/Hair/Body slots are empty outside a costume change — a plausible
// reading the state line below now tests directly `[hypothesis]`.
//
// So stop guessing at hierarchy and ask the SCENE for every via.render.Mesh there is:
//   via.SceneManager (native singleton) -> get_CurrentScene() / get_MainScene() -> via.Scene
//   via.Scene.findComponents(System.Type) -> via.Component[]   (a real native function in this build,
//   il2cpp_dump.json id 135955 fn 0x14018db70) `[inferred-static 2026-09-12]`
// findComponents matches the EXACT type only, so we ask for via.render.Mesh itself, which is concrete.
//
// Claire's meshes are identified by MATERIAL, not by hierarchy: pl1000_* (body/jacket atlas), pl1050_* (face),
// pl1070_* (hair) — dossier §7b `[measured 2026-09-06]`. The filter is the general shape "pl" + four digits, so a
// different survivor (Leon, Sherry pl3000) is caught too and we learn the real names either way.
//
// Then it answers the two questions a cheap route needs:
//   - the ANCESTOR: every hit's transform parent chain, root-first, intersected down to the deepest node they all
//     share. That node, with its component list, is the anchor route D will use.
//   - the SHAPE: the full component list of the first hits' GameObjects and of that anchor — which is how we find
//     the survivor-side equivalent of Em0000SimpleMontageBase (a component that OWNS the mesh) by shape, not name.
// Still discovery only: route C and D do not feed h.meshes and nothing that gets hidden changes.
// ---------------------------------------------------------------------------
API::ManagedObject* current_scene() {
    auto& api = API::get();
    auto* sm = api->get_native_singleton("via.SceneManager");
    if (sm == nullptr) return nullptr;
    auto* t = api->tdb()->find_type("via.SceneManager");
    if (t == nullptr) return nullptr;
    for (const char* getter : {"get_CurrentScene", "get_MainScene"}) {
        auto* m = t->find_method(getter);
        if (m == nullptr) continue;
        auto* s = m->call<API::ManagedObject*>(api->get_vm_context(), sm);
        if (is_managed(s)) return s;
    }
    return nullptr;
}


// via.Scene.findComponents(System.Type) -> via.Component[]. ⚠️ It matches the EXACT type only, so ask for a
// concrete type, never a base class `[reported 2026-09-12, /lm]`.
API::ManagedObject* scene_find_components(API::ManagedObject* scene, const char* type) {
    if (scene == nullptr) return nullptr;
    auto* t = API::get()->typeof(type);
    if (t == nullptr) { LOGW("%s typeof(%s) failed", TAG, type); return nullptr; }
    auto* m = find_method_deep(scene->get_type_definition(), "findComponents(System.Type)");
    if (m == nullptr) { LOGW("%s via.Scene.findComponents(System.Type) not found", TAG); return nullptr; }
    auto r = m->invoke(scene, {t});
    return r.exception_thrown ? nullptr : (API::ManagedObject*)r.ptr;
}


// Cheap identity for a scene-wide sweep: the FIRST material name only (mesh_material_names invokes once per
// material, which is far too much over a whole scene).
std::string mesh_first_material(API::ManagedObject* mesh) {
    auto* td = mesh != nullptr ? mesh->get_type_definition() : nullptr;
    if (td == nullptr || find_method_deep(td, "get_MaterialNum") == nullptr || find_method_deep(td, "getMaterialName") == nullptr) return "n/a";
    const uint32_t n = inv_u32(mesh, "get_MaterialNum");
    if (n == 0 || n > 64) return "-";
    auto* nm = inv_ptr(mesh, "getMaterialName", {(void*)(uintptr_t)0});
    return nm != nullptr ? sysstr(nm) : "?";
}


// "pl" followed by four digits, anywhere — pl1000/pl1050/pl1070 (Claire), pl3000 (Sherry), pl0000 (Leon), …
bool player_like(const std::string& s) {
    for (size_t i = 0; i + 5 < s.size() + 1 && i + 6 <= s.size(); ++i) {
        if (s[i] != 'p' || s[i + 1] != 'l') continue;
        bool digits = true;
        for (int k = 2; k < 6; ++k) if (!isdigit((unsigned char)s[i + k])) { digits = false; break; }
        if (digits) return true;
    }
    return false;
}

std::string component_types(API::ManagedObject* go, int cap) {
    if (go == nullptr) return "(no gameobject)";
    auto* comps = inv_ptr(go, "get_Components");
    if (comps == nullptr) return "(no components)";
    const uint32_t n = arr_count(comps);
    if (n > 256) return "(component count implausible)";
    std::string out;
    for (uint32_t i = 0; i < n && (int)i < cap; ++i) {
        auto* c = arr_ptr_at(comps, i);
        out += (i ? ", " : "") + (is_managed(c) ? tname(c) : std::string("?"));
    }
    if ((int)n > cap) out += ", …(" + std::to_string(n) + " total)";
    return out;
}


// Ancestor chain of a transform, ROOT FIRST.
std::vector<API::ManagedObject*> ancestor_chain(API::ManagedObject* tf) {
    std::vector<API::ManagedObject*> up;
    for (auto* t = tf; t != nullptr && up.size() < 24; t = inv_ptr(t, "get_Parent")) up.push_back(t);
    std::reverse(up.begin(), up.end());
    return up;
}

std::string tf_go_name(API::ManagedObject* tf) {
    auto* go = inv_ptr(tf, "get_GameObject");
    auto s = sysstr(inv_ptr(go, "get_Name"));
    return s.empty() ? "?" : s;
}

void head_probe_c(HeadProbeC& c, bool verbose) {
    auto& api = API::get();
    auto* scene = current_scene();
    if (scene == nullptr) { c.route = "NO SCENE (via.SceneManager get_CurrentScene/get_MainScene both null)"; return; }
    auto* arr = scene_find_components(scene, "via.render.Mesh");
    if (arr == nullptr) { c.route = "via.Scene.findComponents(via.render.Mesh) missing / threw / null"; return; }
    c.scene_total = arr_count(arr);
    c.route = "scene.findComponents(via.render.Mesh)";
    if (c.scene_total > 20000) { c.route += " — count implausible, aborted"; c.scene_total = 0; return; }

    std::string sample;
    int sampled = 0;
    for (uint32_t i = 0; i < c.scene_total; ++i) {
        auto* comp = arr_ptr_at(arr, i);
        if (!is_managed(comp)) continue;
        auto* go = inv_ptr(comp, "get_GameObject");
        const std::string name = sysstr(inv_ptr(go, "get_Name"));
        const std::string mat = mesh_first_material(comp);
        if (player_like(name) || player_like(mat)) {
            if (c.hit_mesh.size() < 64) { c.hit_mesh.push_back(comp); c.hit_name.push_back(name); c.hit_mat.push_back(mat); }
        } else if (sampled < 20) {
            sample += (sampled++ ? ", " : "") + name + "/" + mat;
        }
    }
    LOGI("%s head: C — %u via.render.Mesh in the scene, %zu look like a player model (pl####)", TAG, c.scene_total, c.hit_mesh.size());
    for (size_t i = 0; i < c.hit_mesh.size() && i < 40; ++i)
        LOGI("%s head:   C hit %-30s %s", TAG, c.hit_name[i].c_str(), mesh_material_names(c.hit_mesh[i]).c_str());
    if (c.hit_mesh.empty() || verbose)
        LOGI("%s head:   C non-player sample: %s", TAG, sample.empty() ? "(none)" : sample.c_str());
    if (c.hit_mesh.empty()) return;

    // The anchor: intersect every hit's root-first parent chain.
    std::vector<API::ManagedObject*> common;
    bool first = true;
    for (auto* mesh : c.hit_mesh) {
        auto* tf = inv_ptr(inv_ptr(mesh, "get_GameObject"), "get_Transform");
        if (tf == nullptr) continue;
        auto chain = ancestor_chain(tf);
        if (first) { common = chain; first = false; continue; }
        size_t k = 0;
        while (k < common.size() && k < chain.size() && common[k] == chain[k]) ++k;
        common.resize(k);
    }
    if (!common.empty()) { c.anchor = common.back(); c.anchor_name = tf_go_name(c.anchor); }

    // The chain of the FIRST hit, and the player's own, so the relationship between the two is on one screen.
    {
        auto* tf0 = inv_ptr(inv_ptr(c.hit_mesh[0], "get_GameObject"), "get_Transform");
        std::string chain_s;
        for (auto* t : ancestor_chain(tf0)) chain_s += (chain_s.empty() ? "" : " > ") + tf_go_name(t);
        LOGI("%s head:   C hit[0] chain: %s", TAG, chain_s.c_str());
        std::string pl_s;
        for (auto* t : ancestor_chain(g.transform)) pl_s += (pl_s.empty() ? "" : " > ") + tf_go_name(t);
        LOGI("%s head:   C player  chain: %s", TAG, pl_s.c_str());
        LOGI("%s head:   C anchor \"%s\" (%zu deep) components: %s", TAG, c.anchor_name.c_str(), common.size(),
             c.anchor != nullptr ? component_types(inv_ptr(c.anchor, "get_GameObject")).c_str() : "(none)");
        // The shape search: what OWNS the head mesh. One of these is RE2's Em0000SimpleMontageBase.
        LOGI("%s head:   C hit[0] go components: %s", TAG, component_types(inv_ptr(c.hit_mesh[0], "get_GameObject")).c_str());
        if (auto* par = inv_ptr(tf0, "get_Parent"); par != nullptr)
            LOGI("%s head:   C hit[0] parent \"%s\" components: %s", TAG, tf_go_name(par).c_str(), component_types(inv_ptr(par, "get_GameObject")).c_str());
    }
}


// The BY-SHAPE search, done statically first: of every app.ropeway type in il2cpp_dump.json, only three hand out a
// via.render.Mesh — SurvivorCondition (get_Mesh), SurvivorCostumeChanger (the five part getters route B already
// tried), and the MeshPartsController family, whose survivor member is
// app.ropeway.survivor.SurvivorMeshPartsController with get_Mesh() `[inferred-static 2026-09-12]`.
// That last one is RE2's Em0000SimpleMontageBase: it sits on the object whose mesh it controls, so enumerating it
// scene-wide hands us the player's mesh objects AND the component that owns each. Few instances, so it is cheap.
void head_probe_owners() {
    auto* scene = current_scene();
    if (scene == nullptr) return;
    static const char* owner_types[] = {"app.ropeway.survivor.SurvivorMeshPartsController",
                                        "app.ropeway.survivor.SurvivorCostumeChanger",
                                        "app.ropeway.survivor.SurvivorGpuClothController"};
    for (auto* ot : owner_types) {
        auto* arr = scene_find_components(scene, ot);
        const uint32_t n = arr_count(arr);
        LOGI("%s head:   OWN %s: %u in the scene", TAG, ot, n);
        for (uint32_t i = 0; i < n && i < 24; ++i) {
            auto* comp = arr_ptr_at(arr, i);
            if (!is_managed(comp)) continue;
            auto* go = inv_ptr(comp, "get_GameObject");
            auto* mesh = inv_ptr(comp, "get_Mesh");
            LOGI("%s head:     on \"%s\" get_Mesh=%s %s", TAG, sysstr(inv_ptr(go, "get_Name")).c_str(),
                 mesh != nullptr ? "ok" : "null", mesh != nullptr ? mesh_material_names(mesh).c_str() : "");
        }
    }
}

} // namespace visceral
