#pragma once

#include <atomic>
#include <cstdint>

namespace azs {

class god_mode_tests;

class god_mode_events final {
public:
    /** @brief Queue a scoped spell-event suppression hook for an independently verified dispatcher.
     * @param dispatcher_address Native entry already certified by god_mode_bindings and compared
     * against its complete live image bytes by god_mode before this call.
     * @return True after the exact dispatcher hook is queued; false leaves God Mode unavailable.
     * MinHook must already be initialized,
     * and the caller must apply the shared hook queue before gameplay features are enabled.
     * Other native events always execute through the retained original trampoline.
     */
    static bool initialize(std::uintptr_t dispatcher_address) noexcept;

    /** @brief Change spell-event suppression only for the current game thread.
     * @param active True while invoking the God Mode native damage/death pipeline.
     * @return Previous thread-local flag, which the caller must restore in its finally block.
     * This operation performs no allocation or native call and supports nested scopes.
     */
    static bool set_active(bool active) noexcept;

private:
    friend class god_mode_tests;

    using dispatcher_function = void(__fastcall*)(void*, const void*, std::uintptr_t);

    /** @brief Suppress talent spell dispatch only inside the current God Mode damage scope.
     * @param run_address RCX: native run supplied by the original caller.
     * @param event_address RDX: borrowed native damage event, never read by this bridge.
     * @param source_context R8: unchanged native optional source context.
     * @return None, matching the original Rust unit return. Outside the local scope the
     * original dispatcher executes exactly once with all arguments unchanged.
     */
    static void __fastcall dispatcher_hook(void* run_address, const void* event_address,
        std::uintptr_t source_context) noexcept;

    inline static std::atomic<dispatcher_function> original_dispatcher_{};
    inline static std::uintptr_t dispatcher_address_{};
    inline static bool queued_{};
    inline static thread_local bool active_{};
};

} // namespace azs
