#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>

namespace azs {

class preview_host final {
public:
    /** @brief Run an isolated synthetic game window for visual/input verification.
     * @param instance Process instance used to register/create the preview window.
     * @return A Windows exit status after the preview is closed or times out.
     */
    static int run(HINSTANCE instance);

private:
    /** @brief Dispatch paint, timer updates and close events for the synthetic host.
     * @param window Host HWND; @param message Win32 message identifier.
     * @param wparam First message argument; @param lparam Second message argument.
     * @return Appropriate Win32 message result.
     */
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    /** @brief Populate a private allocation with representative verified game fields.
     * @return None; initializes only this test process's synthetic buffer.
     */
    static void initialize_data();

    /** @brief Apply queued commands and simulate a pure lifesteal getter on the host thread.
     * @return None; publishes the current-layout synthetic scalar/computed snapshots.
     * Uses only the owned test allocation and never calls any game function.
     */
    static void update_data();

    static inline std::byte* data_{};
    static inline std::uint64_t started_ms_{};
};

} // namespace azs
