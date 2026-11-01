#include "god_mode_events.hpp"
#include <Windows.h>
#include <MinHook.h>

namespace azs {

bool god_mode_events::initialize(std::uintptr_t dispatcher_address) noexcept {
    if (queued_) return dispatcher_address_ == dispatcher_address;
    if (dispatcher_address == 0) return false;
    auto* target = reinterpret_cast<void*>(dispatcher_address);
    void* trampoline{};
    if (MH_CreateHook(target, reinterpret_cast<void*>(&dispatcher_hook), &trampoline) != MH_OK) return false;
    original_dispatcher_.store(reinterpret_cast<dispatcher_function>(trampoline), std::memory_order_release);
    if (MH_QueueEnableHook(target) != MH_OK) {
        MH_RemoveHook(target);
        original_dispatcher_.store(nullptr, std::memory_order_release);
        return false;
    }
    dispatcher_address_ = dispatcher_address;
    queued_ = true;
    return true;
}

bool god_mode_events::set_active(bool active) noexcept {
    const bool previous = active_;
    active_ = active;
    return previous;
}

void __fastcall god_mode_events::dispatcher_hook(void* run_address, const void* event_address,
    std::uintptr_t source_context) noexcept {
    if (active_) return;
    const auto original = original_dispatcher_.load(std::memory_order_acquire);
    if (original != nullptr) original(run_address, event_address, source_context);
}

} // namespace azs
