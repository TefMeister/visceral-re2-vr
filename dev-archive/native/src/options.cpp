// options.cpp -- see options.h.
#include "options.h"
#include "bridge.h"
#include "common.h"
#include "settings.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <windows.h>

namespace vn::options {

namespace {
constexpr uint32_t ON = 0, OFF = 1;   // app.ropeway.OptionManager.OnOff

enum class Step { WAIT, CHECK, VERIFY, DONE };
Step g_step = Step::WAIT;
int g_wait = 0;
int g_tries = 0;

// beside re2.exe, whatever the working folder (a launch from elsewhere once gave RE2 the wrong folder, 2026-10-03)
std::filesystem::path game_dir() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
}
std::filesystem::path marker() { return game_dir() / "reframework" / "data" / "visceral_options_set.txt"; }

MO* option_manager() { return API::get()->get_managed_singleton("app.ropeway.OptionManager"); }

bool in_game() {
    auto* pm = API::get()->get_managed_singleton("app.ropeway.PlayerManager");
    return call_ptr(pm, "get_CurrentPlayer") != nullptr;
}

struct Values { uint32_t run, reload, aim; };

Values read(MO* om) {
    return {call_direct<uint32_t>(om, "get_ControllerRunType", 99u), call_direct<uint32_t>(om, "get_ControllerAutoReloadValue", 99u),
            call_direct<uint32_t>(om, "get_CameraAimAssistLevel", 99u)};
}

bool wanted(const Values& v) { return v.run == OFF && v.reload == OFF && v.aim == cfg::OPTIONS_AIM_ASSIST_LEVEL; }

void call_void(MO* o, const char* method, uint32_t value) {
    auto* m = o ? find_method_deep(o->get_type_definition(), method) : nullptr;
    if (m == nullptr) { LOGW("%s options: %s not found", TAG, method); return; }
    m->call<void>(API::get()->get_vm_context(), (void*)o, value);
}

void apply(MO* om) {
    call_void(om, "set_ControllerRunType", OFF);              // Run Type: Hold
    call_void(om, "set_ControllerAutoReloadValue", OFF);      // auto reload off
    call_void(om, "set_CameraAimAssistLevel", cfg::OPTIONS_AIM_ASSIST_LEVEL);
    call_void(API::get()->get_managed_singleton("app.ropeway.InputSystem"), "setOptionToggleRunType", OFF);
}

void save(MO* om) {
    auto* m = find_method_deep(om->get_type_definition(), "saveSystemSaveData_PC");
    if (m == nullptr) { LOGW("%s options: saveSystemSaveData_PC not found, the game saves them on its own next time", TAG); return; }
    m->call<void>(API::get()->get_vm_context(), (void*)om);
    LOGI("%s options: system save written (saveSystemSaveData_PC)", TAG);
}

void write_marker(const Values& v) {
    std::error_code ec;
    std::filesystem::create_directories(marker().parent_path(), ec);
    std::ofstream f(marker());
    f << "Visceral set the game options once (Run Type Hold, auto reload off, aim assist off).\n"
      << "Delete this file to have them set again at the next start.\n"
      << "run " << v.run << " reload " << v.reload << " aim " << v.aim << "\n";
}
} // namespace

void frame() {
    if (g_step == Step::DONE) return;
    if (!bridge::live() || !in_game()) { g_wait = 0; return; }
    if (++g_wait < cfg::OPTIONS_SETTLE_FRAMES) return;   // let the game finish loading into play first
    auto* om = option_manager();
    if (om == nullptr) return;
    const Values v = read(om);

    if (g_step == Step::WAIT) {
        const bool done_before = std::filesystem::exists(marker());
        LOGI("%s options at start: run type %u, auto reload %u, aim assist %u (OnOff: 0 on, 1 off)%s", TAG, v.run, v.reload,
             v.aim, done_before ? "; set before, left alone" : "");
        g_step = done_before ? Step::DONE : Step::CHECK;
        return;
    }
    if (g_step == Step::CHECK) {
        if (wanted(v)) { LOGI("%s options: already as wanted", TAG); write_marker(v); g_step = Step::DONE; return; }
        apply(om);
        g_step = Step::VERIFY;
        g_wait = 0;
        return;
    }
    // VERIFY: read back a few frames later; save + marker only once they stuck
    if (wanted(v)) {
        LOGI("%s options SET: run type %u, auto reload %u, aim assist %u", TAG, v.run, v.reload, v.aim);
        save(om);
        write_marker(v);
        g_step = Step::DONE;
    } else if (++g_tries >= cfg::OPTIONS_TRIES) {
        LOGW("%s options: did not stick after %d tries (run %u reload %u aim %u), given up for this launch", TAG, g_tries,
             v.run, v.reload, v.aim);
        g_step = Step::DONE;
    } else {
        g_step = Step::CHECK;
        g_wait = 0;
    }
}

} // namespace vn::options
