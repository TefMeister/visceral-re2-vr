// head_hider.cpp -- the head hider: route A walk, route E take, scan, per-frame apply, the assignment hooks.
// Split out of Plugin.cpp 2026-09-27, move only (SPLIT-MAP-2026-09-24.md).
#include "visceral.h"

namespace visceral {


// ---------------------------------------------------------------------------
// v0.15: ROUTE E — CATCH THE MESHES AS THEY ARE HANDED OVER.
//
// 2026-09-12 flat run: `walkC 8 scene mesh / 0 player via scene.findComponents(via.render.Mesh)` at a moment the
// zombie census counted TEN live zombies, each holding a readable face mesh whose type is exactly via.render.Mesh
// `[verified-live 2026-09-12, n=1 flat run]`. So scene enumeration UNDER-REPORTS badly — it evidently does not
// reach objects inside instantiated prefabs — and no sweep can be trusted for meshes on this engine.
//
// The pattern that does work is the zombies' own: take the mesh from the component that OWNS it, at the moment it
// is handed over. The zombie moment is Em0000SimpleMontageBase.attachedMontageMesh(Face, Body, Shirt, Pants).
// The survivor-side analogues, from an exhaustive scan of il2cpp_dump.json for every app.* method that RECEIVES or
// RETURNS a via.render.Mesh `[inferred-static 2026-09-12]`:
//   app.ropeway.survivor.SurvivorCostumeChanger.set_Face / set_Hair / set_Body / set_Other / set_SheathKnife(value)
//   app.ropeway.survivor.SurvivorCostumeChanger.setPartsEnable(Mesh, …)   — fires during play, not only on load
//   app.ropeway.survivor.SurvivorCondition.set_Mesh(value)
//   app.ropeway.survivor.SurvivorMeshPartsController.set_Mesh(value) (declared on the MeshPartsController base) and
//       its .ScenarioPartsEnable / .VariablePartsEnable .applyPartsEnable(Mesh, …)
// Pre-hooks on those cache whatever via.render.Mesh goes past. Same machinery as the VR bridge's mailbox hook, and
// like it we scan EVERY argv slot by type rather than trusting a fixed argument position.
// ---------------------------------------------------------------------------
// v0.16: hide the hair along with the face. On by default — a floating hairstyle where the head was is
// exactly as wrong as a floating head, and the hair is a separate mesh handed over by its own setter.
// NUM- toggles it live so one run can judge both.
std::atomic<bool> g_head_hide_hair{true};

MeshCatcher g_catch;

void catch_mesh_args(int argc, void** argv, const char* how) {
    g_catch.calls++;
    if (!g_api_ok.load()) return;
    // argv[0] is the instance for an instance method; keep it as the owner, but only if it is NOT itself the
    // mesh (a static or oddly-shaped signature would otherwise record the mesh as its own owner).
    // 2026-09-12: is_managed() rejects argv[0] here, so the owner was coming out null and every catch was
    // skipped as "another character's". Keep the RAW pointer instead and compare it by identity — it is only
    // ever compared against another pointer, never dereferenced unless is_managed() vouches for it.
    API::ManagedObject* owner = nullptr;
    if (argc > 0 && argv[0] != nullptr) {
        auto* a0 = (API::ManagedObject*)argv[0];
        const bool a0_is_mesh = is_managed(a0) && tname(a0) == "via.render.Mesh";
        if (!a0_is_mesh) owner = a0;
    }
    for (int i = 0; i < argc && i < 8; ++i) {
        auto* o = (API::ManagedObject*)argv[i];
        if (!is_managed(o)) continue;
        if (tname(o) != "via.render.Mesh") continue;
        for (int k = 0; k < g_catch.n; ++k) if (g_catch.s[k].mesh == o) { g_catch.s[k].hits++; return; }
        if (g_catch.n < MESH_CATCH_MAX) g_catch.s[g_catch.n++] = {o, owner, how, 1};
        return;
    }
}


// ---------------------------------------------------------------------------
// v0.8: THE HEAD HIDER — head gone from the camera, shadow kept (roadmap v1 #4 / v2 phase H)
//
// Why not REFramework's HideJointMesh: it scales the "head" joint to zero, which collapses the face out of EVERY
// pass, shadow map included — a headless shadow. via.render.Mesh has independent per-pass flags (dossier §7,
// praydog's RE8VR.cpp fix_player_shadow): DrawDefault off + DrawShadowCast on keeps the shadow. Arcade Controls
// proved the technique live in Lua on 2026-08-20 (re2_vr_head_shadow.lua — knowledge, not code: this is our own).
//
// What it walks: every via.Transform under the player's, every via.render.Mesh component on each GameObject
// (getComponent returns only the first — eyelashes are often a second mesh on the same object), decided by name
// AND material patterns. The body's own mesh (pl1000_Body_Mat, cloth) is never matched.
//
// When it reveals the head again (mode 1): the cinematic gate says so (bridge slot 30), the player is grabbed
// (JackDominator.get_Jacked — grabs only, an ordinary hit never sets it), the FirstPerson mod is not driving the
// camera (bridge slot 31 — third person, menus), or the camera is > 0.35 m from the head joint. Each trigger holds
// for 0.3 s after it drops. Mode 2 ignores all of that: hidden whatever happens, to isolate the reveal logic.
//
// With the head hidden this way the face file goes with it — and the face file carries the NECK (05e note), so
// the collar opens and the v0.7 plug has something to fill. The two are one feature. [hypothesis until a flat run]
// ---------------------------------------------------------------------------

bool head_pattern(const std::string& lname) {
    static const char* pats[] = {"face", "hair", "head", "eye", "lash", "brow", "matsuge", "beard", "mustache", "hige",
                                 "tooth", "teeth", "tongue", "tear", "pl3050", "pl3070", "pl1050", "pl1070"};
    for (auto* p : pats) if (lname.find(p) != std::string::npos) return true;
    return false;
}

void head_restore_all(const char* why) {
    auto& h = g.head;
    int n = 0;
    for (auto& e : h.meshes) {
        if (!e.applied) continue;
        e.applied = false;
        if (!is_managed(e.mesh)) continue;
        call_void_direct(e.mesh, "set_DrawDefault", e.orig_default);
        call_void_direct(e.mesh, "set_DrawShadowCast", e.orig_shadow);
        if (e.has_rt) call_void_direct(e.mesh, "set_DrawRaytracing", e.orig_rt);
        ++n;
    }
    if (n > 0) LOGI("%s head: restored %d mesh(es) — %s", TAG, n, why);
    h.hidden_n = 0;
}

void head_walk(API::ManagedObject* tf, int depth, int& seen, bool verbose) {
    auto& h = g.head;
    if (tf == nullptr || depth > 12 || seen > 500) return;
    ++seen;
    if (auto* go = inv_ptr(tf, "get_GameObject"); go != nullptr) {
        const std::string name = sysstr(inv_ptr(go, "get_Name"));
        if (auto* comps = inv_ptr(go, "get_Components"); comps != nullptr) {
            const uint32_t n = arr_count(comps);
            if (n > 256) { LOGW("%s head: %s has %u components — array layout assumption wrong, skipping", TAG, name.c_str(), n); }
            else {
                int mesh_i = 0;
                for (uint32_t i = 0; i < n; ++i) {
                    auto* c = arr_ptr_at(comps, i);
                    if (!is_managed(c) || tname(c) != "via.render.Mesh") continue;
                    ++mesh_i;
                    State::Head::Entry e{};
                    e.mesh = c;
                    e.label = mesh_i > 1 ? name + " #" + std::to_string(mesh_i) : name;
                    e.mats = mesh_material_names(c);
                    e.has_rt = find_method_deep(c->get_type_definition(), "set_DrawRaytracing") != nullptr;
                    e.orig_default = inv_bool(c, "get_DrawDefault");
                    e.orig_shadow = inv_bool(c, "get_DrawShadowCast");
                    e.orig_rt = e.has_rt ? inv_bool(c, "get_DrawRaytracing") : true;
                    e.hide = head_pattern(lower(name)) || head_pattern(lower(e.mats));
                    // never the body itself, whatever else matched (its skin material is pl1000_Body_Mat; "body" is not a pattern)
                    if (depth == 0 && mesh_i == 1) e.hide = false;
                    if (verbose) LOGI("%s head:   mesh %-36s %s  DrawDefault=%d Shadow=%d%s -> %s", TAG, e.label.c_str(), e.mats.c_str(),
                                      (int)e.orig_default, (int)e.orig_shadow, e.has_rt ? (e.orig_rt ? " RT=1" : " RT=0") : "", e.hide ? "HIDE" : "keep");
                    h.meshes.push_back(std::move(e));
                }
            }
        }
    }
    // via.Transform children: first child, then the sibling chain (the walk Arcade Controls' probe proved on this build)
    int guard = 0;
    for (auto* ch = inv_ptr(tf, "get_Child"); ch != nullptr && guard < 500; ch = inv_ptr(ch, "get_Next"), ++guard)
        head_walk(ch, depth + 1, seen, verbose);
}


// v0.16b: WHICH CHARACTER DOES A CAUGHT MESH BELONG TO?
//
// argv[0] is NOT the costume changer. Measured 2026-09-12: Claire's Body, Hair and Face catches carried three
// DIFFERENT owner pointers (…1B0D3B60, …1AFB8760, …1B59F4E0) and none matched the player's own changer
// (…0C761520) `[verified-numerically 2026-09-12, n=3]`. So the hooks give us no usable owner.
//
// But we now HAVE the mesh, and that turns the original problem inside out. Walking DOWN from the player never
// reached the meshes (dossier §7g); walking UP from a mesh is a short, certain climb. If any ancestor is the
// player's own transform, the mesh is the player's. This is identity, not a material-name guess — Claire is
// pl1000/pl1050/pl1070 but Leon and the alternate costumes are not, and hiding an NPC's face would be far
// worse than failing to hide the player's.
bool mesh_belongs_to_player(API::ManagedObject* mesh) {
    if (mesh == nullptr || g.transform == nullptr) return false;
    auto* go = inv_ptr(mesh, "get_GameObject");
    if (go == nullptr) return false;
    auto* tf = inv_ptr(go, "get_Transform");
    for (int depth = 0; tf != nullptr && depth < 16; ++depth) {
        if (tf == g.transform) return true;
        if (auto* ago = inv_ptr(tf, "get_GameObject"); ago != nullptr && ago == g.player_go) return true;
        tf = inv_ptr(tf, "get_Parent");
    }
    return false;
}


// ---------------------------------------------------------------------------
// v0.16: ROUTE E FEEDS THE HIDER. This is the one that works, and it is now the source h.meshes is
// built from — walk A stays as the fallback and its count stays in the log for one more run.
//
// Why hooks and not a search: a character's meshes are not under its transform and the scene sweep
// under-reports (dossier 7g). The costume changer's SETTERS hand them over, and pre-hooks on them
// caught Claire's face, hair and body by material name in one flat run
// `[verified-live 2026-09-12, n=1]`:
//     set_Face -> "Face"  pl1050_Face_Mat, Eyelash, Tearline, Eyes_In, Eyes_Out
//     set_Hair -> "Hair"  pl1070_Hair_Mat, Hair2, Hair3
//     set_Body -> "Body"  pl1000_Jacket_Mat, Body, Trousers, Boots, Chain, Holster
//
// ⚠️ The hooks are GLOBAL, so the catch list also holds other characters. Everything below is
// filtered to the PLAYER's own costume changer, by object identity, never by material prefix —
// Claire is pl1000/pl1050/pl1070 but Leon and the alternate costumes are not, and hiding an NPC's
// face would be a far worse defect than failing to hide the player's.
int head_take_caught(bool verbose) {
    auto& h = g.head;
    auto* changer = g.cond != nullptr ? inv_ptr(g.cond, "get_CostumeChanger") : nullptr;
    if (changer == nullptr) {
        if (verbose) LOGI("%s head: routeE not used — no SurvivorCostumeChanger on the player this bind", TAG);
        return 0;
    }
    int taken = 0, skipped_other = 0;
    for (int k = 0; k < g_catch.n; ++k) {
        auto& c = g_catch.s[k];
        if (c.mesh == nullptr) continue;
        if (!mesh_belongs_to_player(c.mesh)) { ++skipped_other; continue; }
        bool dup = false;
        for (auto& e : h.meshes) if (e.mesh == c.mesh) { dup = true; break; }
        if (dup) continue;
        State::Head::Entry e{};
        e.mesh = c.mesh;
        e.label = std::string("routeE:") + (c.how ? c.how : "?");
        e.mats = mesh_material_names(c.mesh);
        e.has_rt = find_method_deep(c.mesh->get_type_definition(), "set_DrawRaytracing") != nullptr;
        e.orig_default = inv_bool(c.mesh, "get_DrawDefault");
        e.orig_shadow = inv_bool(c.mesh, "get_DrawShadowCast");
        e.orig_rt = e.has_rt ? inv_bool(c.mesh, "get_DrawRaytracing") : true;
        // The catch tells us WHICH part it is, which is far more reliable than matching names: hide what the
        // game handed over as the Face, and the Hair with it (a floating hairstyle is as wrong as a floating
        // head). Never the Body — that is the character you are supposed to see.
        const std::string how = lower(c.how ? c.how : "");
        const bool is_face = how.find("set_face") != std::string::npos;
        const bool is_hair = how.find("set_hair") != std::string::npos;
        e.hide = is_face || (is_hair && g_head_hide_hair.load());
        if (verbose || e.hide)
            LOGI("%s head:   routeE %-18s %s -> %s", TAG, e.label.c_str(), e.mats.c_str(), e.hide ? "HIDE" : "keep");
        h.meshes.push_back(std::move(e));
        ++taken;
    }
    static int last_reported = -1;
    if (verbose || taken > 0 || skipped_other != last_reported) {
        last_reported = skipped_other;
        LOGI("%s head: routeE took %d mesh(es) from the player's changer %p, skipped %d belonging to other characters (catch list %d)",
             TAG, taken, (void*)changer, skipped_other, g_catch.n);
    }
    return taken;
}

void head_scan(bool verbose) {
    auto& h = g.head;
    static int quiet_retries = 0;
    head_restore_all("rescan");
    h.meshes.clear();
    h.scanned = true;
    h.stale_logged = false;
    h.last_scan_t = now_s();
    if (g.transform == nullptr) return;
    int seen = 0;
    if (verbose) LOGI("%s head: scanning the player hierarchy for via.render.Mesh components", TAG);
    head_walk(g.transform, 0, seen, verbose);
    const size_t walkA_n = h.meshes.size();
    // v0.16: route E is the real source. Walk A's finds stay in the list (they are our own injected objects
    // plus the flashlight, and none of them is marked hide once route E has spoken), so its count is still
    // visible in the log for one more run as the control.
    const int caught = head_take_caught(verbose);
    if (caught > 0) for (size_t i = 0; i < walkA_n; ++i) h.meshes[i].hide = false;
    int to_hide = 0; for (auto& e : h.meshes) to_hide += e.hide ? 1 : 0;
    if (verbose || to_hide > 0)
        LOGI("%s head: %d transform(s) walked, %zu mesh(es) found, %d to hide — NUM. cycles off/on/forced, NUM+ rescans (%d quiet retries before this)",
             TAG, seen, h.meshes.size(), to_hide, quiet_retries);

    // v0.13: the A/B mesh-discovery comparison. One flat run decides which route reaches the player's head.
    // GOOD:  walkB names include Face/Hair/Body objects with pl1050_*/pl1070_*/pl1000_Body_Mat materials, and
    //        walkB's mesh count is clearly larger than walkA's 5.
    // BAD:   "via NO SurvivorCostumeChanger ..." (the component is not where the dump implies), or walkB 0 mesh
    //        with a route that DID resolve (the getters return null until a costume change — retry then).
    if (verbose || to_hide > 0 || quiet_retries < 5) {
        static int probe_runs = 0;
        HeadProbeB b{};
        head_probe_b(b);

        // ROUTE C: the whole-scene sweep — the ground truth, and the thing that FINDS the anchor. Capped per
        // player bind, because enumerating every mesh in the scene is not a per-frame route.
        const bool anchor_was_cached = is_managed(h.anchor);
        HeadProbeC c{};
        if (probe_runs < 8) {
            ++probe_runs;
            LOGI("%s head:   player go \"%s\" components: %s", TAG, sysstr(inv_ptr(g.player_go, "get_Name")).c_str(),
                 component_types(g.player_go).c_str());
            head_probe_c(c, verbose);
            head_probe_owners();
        } else c.route = "capped (8 sweeps per player bind)";
        if (c.anchor != nullptr) { h.anchor = c.anchor; h.anchor_name = c.anchor_name; }

        // ROUTE D: the cheap one — walk down from that anchor only. This is what would run per frame once the
        // anchor is known; on a rescan (NUM+) it runs off the CACHED anchor with no sweep behind it, which is the
        // real proof. The log says which of the two it was.
        HeadProbeB d{};
        if (is_managed(h.anchor)) {
            head_b_walk(h.anchor, 0, "anchor", d);
            d.route = "anchor \"" + h.anchor_name + "\"" + (anchor_was_cached ? " (cached)" : " (found this sweep)");
        } else {
            d.route = "no anchor yet (route C found none)";
        }

        // ROUTE E: whatever the assignment hooks have caught. This one cannot be defeated by hierarchy or by a
        // sweep that under-reports — but it only has something once the game has actually handed a mesh over.
        HeadProbeB e{};
        for (int k = 0; k < g_catch.n; ++k) head_b_add_mesh(g_catch.s[k].mesh, g_catch.s[k].how, e);
        if (g_catch.installed == 0)
            e.route = "NO HOOKS INSTALLED (" + g_catch.install + ")";
        else if (g_catch.calls.load() == 0)
            e.route = "hooks " + std::to_string(g_catch.installed) + "/" + std::to_string(g_catch.attempted) +
                      " installed but NEVER CALLED yet — the assignment happens before us, or on a costume change only";
        else if (e.meshes.empty())
            e.route = "hooks called " + std::to_string(g_catch.calls.load()) + "x but no via.render.Mesh in any argv slot";
        else
            e.route = "hooks " + std::to_string(g_catch.installed) + "/" + std::to_string(g_catch.attempted) +
                      ", " + std::to_string(g_catch.calls.load()) + " calls";

        // ROUTE F: walk from the TOP of the player's own hierarchy, not from the player object. If Claire's mesh
        // objects are siblings or cousins of the player object rather than its children, this reaches them, and it
        // is anchored on the player so it stays cheap. The chain is logged either way.
        HeadProbeB f{};
        {
            auto chain = ancestor_chain(g.transform);
            std::string cs;
            for (auto* t : chain) cs += (cs.empty() ? "" : " > ") + tf_go_name(t);
            LOGI("%s head:   F player chain (%zu deep): %s", TAG, chain.size(), cs.empty() ? "(none)" : cs.c_str());
            if (!chain.empty() && chain.front() != g.transform) {
                head_b_walk(chain.front(), 0, "plroot", f);
                f.route = "player root \"" + tf_go_name(chain.front()) + "\"";
            } else {
                f.route = chain.empty() ? "no player transform" : "player transform IS the root — same subtree as walk A";
            }
        }

        std::string an, bn, dn, cn, en, fn_;
        for (size_t i = 0; i < h.meshes.size() && i < 6; ++i) an += (i ? ", " : "") + h.meshes[i].label;
        for (size_t i = 0; i < b.meshes.size() && i < 6; ++i) bn += (i ? ", " : "") + b.meshes[i].first;
        for (size_t i = 0; i < d.meshes.size() && i < 6; ++i) dn += (i ? ", " : "") + d.meshes[i].first;
        for (size_t i = 0; i < c.hit_name.size() && i < 6; ++i) cn += (i ? ", " : "") + c.hit_name[i];
        if (h.meshes.size() > 6) an += ", …";
        if (b.meshes.size() > 6) bn += ", …";
        if (d.meshes.size() > 6) dn += ", …";
        if (c.hit_name.size() > 6) cn += ", …";
        for (size_t i = 0; i < e.meshes.size() && i < 6; ++i) en += (i ? ", " : "") + e.meshes[i].first;
        for (size_t i = 0; i < f.meshes.size() && i < 6; ++i) fn_ += (i ? ", " : "") + f.meshes[i].first;
        if (e.meshes.size() > 6) en += ", …";
        if (f.meshes.size() > 6) fn_ += ", …";
        LOGI("%s head: walkA %d tf / %zu mesh | walkB %zu mesh via %s | walkC %u scene mesh / %zu player (UNDER-REPORTS) | walkD %d tf / %zu mesh via %s | walkE %zu mesh via %s | walkF %d tf / %zu mesh via %s",
             TAG, seen, h.meshes.size(), b.meshes.size(), b.route.c_str(), c.scene_total, c.hit_mesh.size(),
             d.transforms, d.meshes.size(), d.route.c_str(), e.meshes.size(), e.route.c_str(),
             f.transforms, f.meshes.size(), f.route.c_str());
        LOGI("%s head:   walkA: %s", TAG, an.empty() ? "(none)" : an.c_str());
        LOGI("%s head:   walkB: %s", TAG, bn.empty() ? "(none)" : bn.c_str());
        LOGI("%s head:   walkC: %s", TAG, cn.empty() ? "(none)" : cn.c_str());
        LOGI("%s head:   walkD: %s", TAG, dn.empty() ? "(none)" : dn.c_str());
        LOGI("%s head:   walkE: %s", TAG, en.empty() ? "(none)" : en.c_str());
        LOGI("%s head:   walkF: %s", TAG, fn_.empty() ? "(none)" : fn_.c_str());
        for (auto& m : e.meshes) LOGI("%s head:   E mesh %-44s %s", TAG, m.first.c_str(), m.second.c_str());
        for (size_t i = 0; i < f.meshes.size() && i < 24; ++i)
            LOGI("%s head:   F mesh %-44s %s", TAG, f.meshes[i].first.c_str(), f.meshes[i].second.c_str());
        if (verbose)
            for (auto& m : b.meshes) LOGI("%s head:   B mesh %-44s %s", TAG, m.first.c_str(), m.second.c_str());
    }
    // Nothing head-ish yet (the face/hair objects may attach after the player binds): keep looking every 2 s, quietly.
    if (to_hide == 0) { h.scanned = false; ++quiet_retries; } else quiet_retries = 0;
    if (h.head_joint == nullptr && g.transform != nullptr) {
        auto* nm = API::get()->create_managed_string(L"head");
        h.head_joint = inv_ptr(g.transform, "getJointByName", {nm});
        if (h.head_joint == nullptr) LOGW("%s head: joint \"head\" not found on the player skeleton — camera-distance reveal unavailable", TAG);
    }
    if (h.jack == nullptr && g.player_go != nullptr) {
        h.jack = get_component(g.player_go, "app.ropeway.JackDominator");
        if (h.jack == nullptr) LOGW("%s head: JackDominator component not found — grab reveal unavailable", TAG);
    }
}

void head_update() {
    auto& h = g.head;
    if (g.player_go == nullptr || g.transform == nullptr) return;
    // v0.16: THE CATCHES ARRIVE AFTER THE SCAN. Measured 2026-09-12: the one scan this bind performs ran at
    // 16:15:17.829 and Claire's face was handed over at 16:15:17.943 — 114 ms later — so a route that only
    // reads the catch list at scan time finds it empty and silently hides nothing `[verified-live 2026-09-12, n=1]`.
    // So take new catches as they land. head_take_caught() de-duplicates, and the first time it takes anything
    // it clears the hide flags walk A set on our own injected objects and the flashlight.
    {
        static int last_taken = 0;
        static uint32_t take_gen = 0;
        if (take_gen != g_catch.gen.load()) { take_gen = g_catch.gen.load(); last_taken = 0; }
        if (h.scanned && g_catch.n > last_taken) {
            const size_t before = h.meshes.size();
            if (head_take_caught(false) > 0) {
                for (size_t i = 0; i < before; ++i)
                    if (h.meshes[i].label.rfind("routeE:", 0) != 0) h.meshes[i].hide = false;
            }
            last_taken = g_catch.n;
        }
        if (!h.scanned) last_taken = 0;      // a fresh bind re-takes everything
    }
    // v0.15: route E accumulates whenever the game hands a mesh over, which can be long after the one scan this
    // bind performs — so announce every new catch the moment it lands, rather than waiting for a NUM+ rescan.
    {
        static int last_catch = 0;
        static uint32_t announce_gen = 0;
        if (announce_gen != g_catch.gen.load()) { announce_gen = g_catch.gen.load(); last_catch = 0; }
        while (last_catch < g_catch.n) {
            auto& s = g_catch.s[last_catch++];
            // owner + the player's own changer on the same line: the whole question is whether they match,
            // and printing them apart cost a run on 2026-09-12.
            auto* pc = g.cond != nullptr ? inv_ptr(g.cond, "get_CostumeChanger") : nullptr;
            LOGI("%s head: MESH CAUGHT by %s -> \"%s\" owner=%p (%s) playerChanger=%p %s%s", TAG, s.how,
                 sysstr(inv_ptr(inv_ptr(s.mesh, "get_GameObject"), "get_Name")).c_str(),
                 (void*)s.owner, (s.owner != nullptr && is_managed(s.owner)) ? tname(s.owner).c_str() : "raw",
                 (void*)pc, (s.owner != nullptr && s.owner == pc) ? "MATCH " : "",
                 mesh_material_names(s.mesh).c_str());
        }
    }
    if (h.want_rescan) { h.want_rescan = false; head_scan(true); }
    if (h.mode == 0) { if (h.hidden_n > 0 || !h.reveal_why.empty()) { head_restore_all("NUM. off"); h.reveal_why.clear(); } h.revealed = false; return; }
    if (!h.scanned && now_s() - h.last_scan_t >= HEAD_RESCAN_MIN_S) head_scan(h.meshes.empty() && h.last_scan_t == 0.0);
    if (!h.scanned) return;

    // --- reveal triggers -------------------------------------------------
    h.head_cam_d = -1.f;
    // 2026-09-12: this used to read `if (!g.dock.cam_valid) update_camera();`, which called the
    // camera reader ONCE and then never again, because update_camera() sets cam_valid on its first
    // success. Every consumer downstream -- the head hider's reveal gate and the dock's re-basing --
    // was therefore comparing against the camera pose from the FIRST frame of the session. That is
    // the whole of the "the camera the plugin reads does not move" defect: cam stayed at
    // (-11.50 -3.20 4.20) across two level loads while the player walked 20 m away. It was never a
    // VR problem -- the same freeze appears in a flat log with no headset attached.
    // `camF` (this same call, re-made fresh every frame) tracked the player correctly through the
    // 2026-09-12 flat walk, so calling it unconditionally is the fix AND is already evidenced.
    update_camera();
    if (g.dock.cam_valid && h.head_joint != nullptr) {
        Vec3 hp{};
        if (inv_vec3(h.head_joint, "get_Position", hp)) h.head_cam_d = dist(g.dock.cam_t, hp);
    }
    std::string why;
    if (h.mode == 1) {
        const bool live = bridge_live();
        const float* f = live ? arr_f32(g.bridge) : nullptr;
        if (live && f[S_CINE] >= 0.5f) why = "cinematic";
        else if (h.jack != nullptr && inv_bool(h.jack, "get_Jacked")) why = "grabbed";
        else if (live && f[S_FP] < 0.5f) why = "not first person";
        else if (h.head_cam_d > HEAD_REVEAL_DIST_M) why = "camera off the head";
        const double t = now_s();
        if (!why.empty()) h.last_far_t = t;
        else if (t - h.last_far_t < HEAD_REVEAL_TAIL_S) why = "tail";
    }
    const bool reveal = !why.empty();
    if (reveal != h.revealed || (reveal && why != h.reveal_why && why != "tail")) {
        if (reveal) LOGI("%s head: REVEAL — %s (d=%.2f m)", TAG, why.c_str(), h.head_cam_d);
        else LOGI("%s head: HIDE again (d=%.2f m)", TAG, h.head_cam_d);
    }
    h.revealed = reveal;
    if (reveal) { h.reveal_why = why; head_restore_all(why.c_str()); return; }
    h.reveal_why.clear();

    // --- apply, with the read-back that catches a rebuilt player -----------
    int hidden = 0; bool stale = false;
    for (auto& e : h.meshes) {
        if (!e.hide) continue;
        if (!is_managed(e.mesh)) { stale = true; continue; }
        if (!e.applied) {
            call_void_direct(e.mesh, "set_DrawDefault", false);
            call_void_direct(e.mesh, "set_DrawShadowCast", true);
            if (e.has_rt) call_void_direct(e.mesh, "set_DrawRaytracing", false);
            e.applied = true;
        }
        // Death -> Continue and save loads rebuild the player while the OLD components stay writable (Arcade Controls
        // saw the write "succeed" on a head that was plainly visible). A flag that reads true right after we cleared it
        // means these are not the meshes on screen.
        if (inv_bool(e.mesh, "get_DrawDefault")) stale = true; else ++hidden;
    }
    if (hidden != h.hidden_n) LOGI("%s head: %d mesh(es) hidden, shadow kept", TAG, hidden);
    h.hidden_n = hidden;
    if (stale && now_s() - h.last_scan_t >= HEAD_RESCAN_MIN_S) {
        if (!h.stale_logged) { LOGW("%s head: stale mesh refs (flag would not stay cleared, or component gone) — rescanning", TAG); h.stale_logged = true; }
        h.scanned = false; h.last_scan_t = now_s() - HEAD_RESCAN_MIN_S;   // scan on the next frame
    }
}


// v0.15: the route-E hooks. One thin pre-hook per assignment moment so the log can say WHICH moment produced a
// mesh; each just caches any via.render.Mesh it sees and always calls the original.
#define MESH_HOOK(fn, label) \
    int fn(int argc, void** argv, REFrameworkTypeDefinitionHandle*, unsigned long long) { \
        catch_mesh_args(argc, argv, label); return REFRAMEWORK_HOOK_CALL_ORIGINAL; }
MESH_HOOK(pre_cc_face,   "CC.set_Face")
MESH_HOOK(pre_cc_hair,   "CC.set_Hair")
MESH_HOOK(pre_cc_body,   "CC.set_Body")
MESH_HOOK(pre_cc_other,  "CC.set_Other")
MESH_HOOK(pre_cc_knife,  "CC.set_SheathKnife")
MESH_HOOK(pre_cc_parts,  "CC.setPartsEnable")
MESH_HOOK(pre_cond_mesh, "Cond.set_Mesh")
MESH_HOOK(pre_mpc_mesh,  "MPC.set_Mesh")
MESH_HOOK(pre_mpc_scen,  "MPC.Scenario.applyPartsEnable")
MESH_HOOK(pre_mpc_var,   "MPC.Variable.applyPartsEnable")
#undef MESH_HOOK

void install_mesh_catch_hooks() {
    auto& api = API::get();
    struct Target { const char* type; const char* method; REFPreHookFn pre; };
    static const Target targets[] = {
        {"app.ropeway.survivor.SurvivorCostumeChanger", "set_Face", pre_cc_face},
        {"app.ropeway.survivor.SurvivorCostumeChanger", "set_Hair", pre_cc_hair},
        {"app.ropeway.survivor.SurvivorCostumeChanger", "set_Body", pre_cc_body},
        {"app.ropeway.survivor.SurvivorCostumeChanger", "set_Other", pre_cc_other},
        {"app.ropeway.survivor.SurvivorCostumeChanger", "set_SheathKnife", pre_cc_knife},
        {"app.ropeway.survivor.SurvivorCostumeChanger", "setPartsEnable", pre_cc_parts},
        {"app.ropeway.survivor.SurvivorCondition", "set_Mesh", pre_cond_mesh},
        {"app.ropeway.survivor.SurvivorMeshPartsController", "set_Mesh", pre_mpc_mesh},
        {"app.ropeway.survivor.SurvivorMeshPartsController.ScenarioPartsEnable", "applyPartsEnable", pre_mpc_scen},
        {"app.ropeway.survivor.SurvivorMeshPartsController.VariablePartsEnable", "applyPartsEnable", pre_mpc_var},
    };
    for (const auto& t : targets) {
        ++g_catch.attempted;
        API::Method* m = api->tdb()->find_method(t.type, t.method);
        // set_Mesh is declared on the generic base MeshPartsController`1<…>, so the concrete survivor type may not
        // carry it directly — walk up the parent chain the way every other lookup here does.
        if (m == nullptr) if (auto* td = api->tdb()->find_type(t.type); td != nullptr) m = find_method_deep(td, t.method);
        if (m == nullptr) {
            g_catch.install += std::string(g_catch.install.empty() ? "" : "; ") + t.method + " NOT FOUND";
            LOGW("%s meshcatch: %s.%s not found — that assignment moment is unhooked", TAG, t.type, t.method);
            continue;
        }
        const auto id = m->add_hook(t.pre, nullptr, false);
        ++g_catch.installed;
        LOGI("%s meshcatch: hook %s.%s id=%u fn=%p", TAG, t.type, t.method, id, m->get_function_raw());
    }
    LOGI("%s meshcatch: %d/%d assignment hooks installed", TAG, g_catch.installed, g_catch.attempted);
}

} // namespace visceral
