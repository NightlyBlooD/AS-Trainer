#include "dll_host.hpp"
#include "localization.hpp"
#include "game_runtime.hpp"
#include "overlay_window.hpp"
#include "config_store.hpp"

namespace azs {

bool dll_host::attach(HMODULE module) noexcept {
    const auto thread = CreateThread(nullptr, 0, worker_thread, module, 0, nullptr);
    if (!thread) {
        return false;
    }
    CloseHandle(thread);
    return true;
}

DWORD WINAPI dll_host::worker_thread(void* context) noexcept {
    HMODULE pinned_module{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&worker_thread), &pinned_module)) {
        return GetLastError();
    }
    localization::initialize();
    game_runtime::initialize();
    if (config_store::initialize(pinned_module)) {
        try {
            for (const auto& filename : config_store::list_configs()) {
                if (filename == "default.ini") {
                    config_store::load(filename);
                    break;
                }
            }
        } catch (...) {
            trainer_state::instance().set_message(localization::translate("Could not read the preset list."));
        }
    }
    return overlay_window::thread_main(context);
}

} // namespace azs

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        return azs::dll_host::attach(module) ? TRUE : FALSE;
    }
    return TRUE;
}
