// rounds.cpp -- see rounds.h. Each gun's drum is kept while the session lasts (a weapon swap does not lose it); after a save
// is loaded every round in the gun is of the kind the game saved, in the chambers from the barrel on.
#include "game.h"
#include "rounds.h"
#include "ammo.h"
#include "gun_data.h"
#include "gun_frame.h"
#include "joints.h"
#include "settings.h"

#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace vn::guns::rounds {

using namespace joints;

namespace {
constexpr float PI = 3.14159265f;
constexpr int SPENT = -1;

struct Drum {
    // chambered gun: one kind per chamber (0 = empty, SPENT = a fired case still in it); else the kinds in order, front =
    // fires next
    std::vector<int> ch;
    int up = 0;            // chambered gun: the chamber at the barrel
};

const RoundGun* g_w = nullptr;
int g_chambers = 0;                 // chamber joints named for this gun (0 = a plain list)
std::map<int, Drum> g_drums;        // per WP
MO* g_drum_joint = nullptr;
bool g_have_rest = false;
Quat g_rest{0, 0, 0, 1};

MO* slot() { return call_ptr(gf().inventory, "get_MainSlot"); }
int current() { return call_direct<int>(slot(), "get_BulletID", 0); }
void set_current(int item) { call_direct<void*>(slot(), "set_BulletID", nullptr, (int32_t)item); }

Drum& drum() {
    Drum& d = g_drums[gf().wp];
    if (g_chambers > 0 && (int)d.ch.size() != g_chambers) d.ch.assign(g_chambers, 0);
    return d;
}

int count(const Drum& d) {
    int n = 0;
    for (int it : d.ch) if (it > 0) ++n;
    return n;
}

// the chamber that fires next: the one at the barrel, or the next loaded one round the drum (-1 = none)
int next_loaded(const Drum& d) {
    for (int i = 0; i < g_chambers; ++i) {
        const int c = (d.up + i) % g_chambers;
        if (d.ch[c] > 0) return c;
    }
    return -1;
}

std::string text(const Drum& d) {
    std::string s;
    for (size_t i = 0; i < d.ch.size(); ++i) {
        if (!s.empty()) s += " ";
        if (g_chambers > 0 && (int)i == d.up) s += ">";
        s += d.ch[i] == 0 ? "-" : d.ch[i] == SPENT ? "x" : std::to_string(d.ch[i]);
    }
    return s.empty() ? "empty" : s;
}

bool known(int item) {
    for (int it : g_w->items) if (it != 0 && it == item) return true;
    return false;
}

bool has_crane() { return g_w != nullptr && g_w->crane != nullptr && g_w->crane[0] != 0; }

MO* joint(const char* name) { return gf().gun_tf != nullptr ? joint_by_name(gf().gun_tf, widen(name).c_str()) : nullptr; }

// a chamber's place in the gun's own space, as the drum is turned now
bool chamber_pos(int c, Vec3& out) {
    Vec3 wp;
    MO* j = joint(g_w->chamber[c]);
    return j != nullptr && get_v3(JOINT, j, "get_Position", wp) && to_gun_local(wp, Quat{0, 0, 0, 1}, out, nullptr);
}

// the drum turned so that the chamber at the barrel is `up` (the game never turns it: rounds.h)
void draw() {
    if (g_chambers <= 0 || g_w->drum[0] == '\0') return;
    g_drum_joint = joint(g_w->drum);
    if (g_drum_joint == nullptr) return;
    if (!g_have_rest) g_have_rest = get_q(JOINT, g_drum_joint, "get_LocalRotation", g_rest);
    if (!g_have_rest) return;
    const float a = g_w->turn_sign * (2.0f * PI / (float)g_chambers) * (float)drum().up * 0.5f;
    Quat q = qnorm(qmul(g_rest, Quat{0, 0, std::sin(a), std::cos(a)}));
    set_any(JOINT, g_drum_joint, "set_LocalRotation", &q);
    // each chamber's own round: there when loaded or spent, gone when empty
    const Drum& d = drum();
    for (int c = 0; c < g_chambers; ++c) {
        MO* j = joint(g_w->chamber[c]);
        const float k = d.ch[c] != 0 ? 1.0f : 0.0f;
        Vec3 s{k, k, k};
        if (j != nullptr) set_any(JOINT, j, "set_LocalScale", &s);
    }
}

void say(const char* what) { LOGI("%s rounds: WP%04d %s; the drum: %s", TAG, gf().wp, what, text(drum()).c_str()); }
} // namespace

void on_gun_changed(int, int new_wp) {
    g_w = round_gun(new_wp);
    g_chambers = 0;
    if (g_w != nullptr) for (const char* c : g_w->chamber) if (c != nullptr && c[0] != '\0') ++g_chambers;
    g_drum_joint = nullptr;
    g_have_rest = false;
}

bool ours() { return g_w != nullptr && gf().gun != nullptr && gf().inventory != nullptr; }
bool chambered() { return ours() && g_chambers > 0; }

int carried(int item) {
    if (!ours() || item == 0) return 0;
    if (item == current()) return ammo::reserve();
    auto* im = API::get()->get_managed_singleton(game::type("gamemastering.InventoryManager").c_str());
    auto* m = API::get()->tdb()->find_method(game::type("gamemastering.InventoryManager").c_str(),
                                             ("getMainWeaponChangeBulletNumber(" + game::type("gamemastering.Item.ID") + ")").c_str());
    return (im != nullptr && m != nullptr) ? m->call<int>(API::get()->get_vm_context(), (void*)im, (int32_t)item) : 0;
}

// the left hip: the gun's normal round (the first of its list) if any is carried, else the first kind that is; the right
// hip: the first carried kind that is not the left hip's
int hip_item(bool right) {
    if (!ours()) return 0;
    int left = 0;
    for (int it : g_w->items) if (it != 0 && carried(it) > 0) { left = it; break; }
    if (!right) return left;
    for (int it : g_w->items) if (it != 0 && it != left && carried(it) > 0) return it;
    return 0;
}

namespace {
int nearest_empty_chamber(const Vec3* in_gun, float& best) {
    Drum& d = drum();
    int pick = -1;
    best = 1e9f;
    for (int i = 0; i < g_chambers; ++i) {
        const int c = (d.up + i) % g_chambers;
        if (d.ch[c] != 0) continue;
        if (in_gun == nullptr) { best = 0.0f; return c; }       // no hand to ask: the next empty one round the drum
        Vec3 p;
        if (!chamber_pos(c, p)) continue;
        const float dd = dist(p, *in_gun);
        if (dd < best) { best = dd; pick = c; }
    }
    return pick;
}
} // namespace

bool nearest_empty(Vec3 in_gun, float& d) { return chambered() && nearest_empty_chamber(&in_gun, d) >= 0; }

bool insert(int item, const Vec3* in_gun) {
    if (!ours() || !known(item)) return false;
    Drum& d = drum();
    float dd = 0.0f;
    const int c = g_chambers > 0 ? nearest_empty_chamber(in_gun, dd) : -1;
    if (g_chambers > 0 && c < 0) return false;
    set_current(item);                       // the game takes the round from this kind's stack
    const bool ok = ammo::insert_one();
    if (ok && g_chambers > 0) d.ch[c] = item;
    else if (ok) d.ch.push_back(item);
    if (g_chambers > 0) { const int n = next_loaded(d); if (n >= 0) set_current(d.ch[n]); }
    else if (!d.ch.empty()) set_current(d.ch.front());
    if (g_chambers > 0) LOGI("%s rounds: WP%04d %s item %d in chamber %d (%.3f m from the hand); the drum: %s", TAG, gf().wp, ok ? "took" : "DID NOT TAKE", item, c, dd, text(d).c_str());
    else LOGI("%s rounds: WP%04d %s item %d; the drum: %s", TAG, gf().wp, ok ? "took" : "DID NOT TAKE", item, text(d).c_str());
    return ok;
}

void late_point() {
    if (!ours()) return;
    const int n = ammo::loaded();
    if (n < 0) return;
    Drum& d = drum();
    bool changed = false;
    if (g_chambers > 0) {
        while (count(d) > n) {                                    // fired: the chamber at the barrel, then the drum turns on
            const int c = next_loaded(d);
            d.ch[c] = has_crane() ? SPENT : 0;      // a drum that swings out keeps the case until it is tipped out
            d.up = (c + 1) % g_chambers;
            changed = true;
        }
        while (count(d) < n && count(d) < g_chambers) {           // put in by the game itself (a save, its own reload)
            float dd;
            int c = nearest_empty_chamber(nullptr, dd);
            if (c < 0) for (int i = 0; i < g_chambers && c < 0; ++i) if (d.ch[i] == SPENT) c = i;   // no empty one: over a spent case
            if (c < 0) break;
            d.ch[c] = current();
            changed = true;
        }
        const int c = next_loaded(d);
        if (c >= 0 && current() != d.ch[c]) { set_current(d.ch[c]); changed = true; }
        draw();
    } else {
        while ((int)d.ch.size() > n) { d.ch.erase(d.ch.begin()); changed = true; }
        while ((int)d.ch.size() < n) { d.ch.push_back(current()); changed = true; }
        if (!d.ch.empty() && current() != d.ch.front()) { set_current(d.ch.front()); changed = true; }
    }
    if (changed) say("now");
}

void render_point() {
    if (chambered()) draw();
}

int dump_spent() {
    if (!chambered()) return 0;
    Drum& d = drum();
    int n = 0;
    for (int& it : d.ch) if (it == SPENT) { it = 0; ++n; }
    if (n > 0) say("spent cases out");
    return n;
}

void show_hand(int n, Vec3 world, Quat wrot) {
    if (!chambered() || n <= 0) return;
    const Drum& d = drum();
    int shown = 0;
    for (int i = 0; i < g_chambers && shown < n; ++i) {
        const int c = (d.up + g_chambers - 1 - i) % g_chambers;   // the last chambers to come up lend first
        if (d.ch[c] != 0) continue;
        MO* j = joint(g_w->chamber[c]);
        if (j == nullptr) continue;
        Vec3 p = world + rotate(wrot, Vec3{cfg::HAND_ROUND_GAP_M * (float)shown, 0, 0});
        Vec3 one{1, 1, 1};
        set_any(JOINT, j, "set_Position", &p);
        set_any(JOINT, j, "set_Rotation", &wrot);
        set_any(JOINT, j, "set_LocalScale", &one);
        ++shown;
    }
}

} // namespace vn::guns::rounds
