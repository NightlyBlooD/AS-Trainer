#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace azs {

class dll_host final {
public:
    /** @brief Schedule initialization after Windows releases the loader lock.
     * @param module The HMODULE supplied to DllMain for this DLL.
     * @return True when the initialization thread was created; false otherwise.
     */
    static bool attach(HMODULE module) noexcept;

private:
    /** @brief Pin the DLL, validate/install hooks and run the independent ImGui UI.
     * @param context This DLL's HMODULE supplied by attach().
     * @return A Windows thread exit status; no game state is written until validation.
     */
    static DWORD WINAPI worker_thread(void* context) noexcept;
};

} // namespace azs
