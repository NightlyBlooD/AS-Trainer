#include "game_runtime.hpp"
#include "localization.hpp"
#include "trainer_state.hpp"
#include "auto_choice.hpp"
#include "legendary_loot.hpp"
#include "simulation_clock.hpp"
#include "party_runtime.hpp"
#include "game_layout.hpp"
#include "native_addresses.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <MinHook.h>

#include <cmath>
#include <bit>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <limits>

namespace azs {
namespace {

constexpr std::uintptr_t tick_rva = 0x5cbb3e0;
constexpr std::uintptr_t recalculate_rva = 0x5caaf00;
constexpr std::uintptr_t speed_packet_rva = 0x5b54d60;
constexpr std::uintptr_t packet_grow_rva = 0x60c5a60;
constexpr std::size_t maximum_packet_capacity = 1u << 20;
constexpr std::uintptr_t swap_player_rva = 0x5ce2c10;
constexpr std::uintptr_t flush_updates_rva = 0x5ca6240;
constexpr std::uintptr_t route_queues_rva = 0x5c9e280;
constexpr std::uintptr_t set_object_field_rva = 0x5b4cc40;

constexpr unsigned char swap_player_prologue[] = {
    0x0f,0x10,0x81,0xb0,0x01,0x00,0x00,0x0f,0x10,0x89,0xc0,0x01,
    0x00,0x00,0x0f,0x10,0x92,0xb0,0x01,0x00,0x00,0x0f,0x10,0x9a
};
constexpr unsigned char flush_updates_prologue[] = {
    0x55,0x56,0x57,0x48,0x81,0xec,0xb0,0x00,0x00,0x00,0x48,0x8d,
    0xac,0x24,0x80,0x00,0x00,0x00,0x48,0xc7,0x45,0x28,0xfe,0xff
};
constexpr unsigned char route_queues_prologue[] = {
    0x56,0x57,0x48,0x83,0xec,0x58,0x80,0xb9,0x9e,0x0d,0x00,0x00,
    0x01,0x0f,0x85,0x45,0x01,0x00,0x00,0x48,0x89,0xce,0x48,0x8b
};
constexpr unsigned char set_object_field_prologue[] = {
    0x56,0x57,0x53,0x48,0x83,0xec,0x70,0x44,0x89,0xc7,0x89,0xd3,
    0x48,0x89,0xce,0x48,0x8b,0x01,0x48,0x85,0xc0,0x74,0x62,0x48
};

constexpr unsigned char tick_prologue[] = {
    0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x54, 0x56,
    0x57, 0x53, 0x48, 0x81, 0xec, 0x70, 0x01, 0x00,
    0x00, 0x48, 0x8d, 0xac, 0x24, 0x80, 0x00, 0x00,
    0x00, 0x44, 0x0f, 0x29, 0xb5, 0xe0, 0x00, 0x00
};
constexpr unsigned char recalculate_prologue[] = {
    0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41,
    0x54, 0x56, 0x57, 0x53, 0x48, 0x81, 0xec, 0x18,
    0x02, 0x00, 0x00, 0x48, 0x8d, 0xac, 0x24, 0x80,
    0x00, 0x00, 0x00, 0x44, 0x0f, 0x29, 0xbd, 0x80
};
constexpr unsigned char speed_packet_prologue[] = {
    0x55, 0x56, 0x57, 0x53, 0x48, 0x81, 0xec, 0x98,
    0x00, 0x00, 0x00, 0x48, 0x8d, 0xac, 0x24, 0x80,
    0x00, 0x00, 0x00, 0x0f, 0x29, 0x75, 0x00, 0x48,
    0xc7, 0x45, 0xf8, 0xfe, 0xff, 0xff, 0xff, 0x0f
};
constexpr unsigned char packet_grow_prologue[] = {
    0x56, 0x57, 0x48, 0x83, 0xec, 0x48, 0x48, 0x89,
    0xce, 0x48, 0x8b, 0x11, 0x48, 0x8d, 0x04, 0x12,
    0x48, 0x83, 0xf8, 0x05, 0xbf, 0x04, 0x00, 0x00
};
constexpr unsigned char luck_reader[] = {
    0xf3, 0x44, 0x0f, 0x10, 0xb6, 0xf4, 0x04, 0x00, 0x00
};
constexpr unsigned char area_reader[] = {
    0xf3, 0x0f, 0x10, 0x8e, 0xf0, 0x04, 0x00, 0x00
};
constexpr unsigned char pickup_reader[] = {
    0xf3, 0x44, 0x0f, 0x10, 0x89, 0xe0, 0x04, 0x00, 0x00
};

} // namespace

bool game_runtime::hash_executable(const wchar_t* file_path,
    std::array<unsigned char, 32>& digest, const char*& error) noexcept {
    const HANDLE file_handle = CreateFileW(file_path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file_handle == INVALID_HANDLE_VALUE) {
        error = localization::translate("Cannot open the game executable for SHA-256 verification.");
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm_handle = nullptr;
    BCRYPT_HASH_HANDLE hash_handle = nullptr;
    std::array<unsigned char, 4096> hash_object{};
    std::array<unsigned char, 65536> read_buffer{};
    bool success = false;
    do {
        if (BCryptOpenAlgorithmProvider(&algorithm_handle, BCRYPT_SHA256_ALGORITHM,
                nullptr, 0) < 0) {
            error = localization::translate("Windows SHA-256 provider is unavailable.");
            break;
        }
        DWORD object_size = 0;
        DWORD result_size = 0;
        if (BCryptGetProperty(algorithm_handle, BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
                &result_size, 0) < 0 || object_size == 0 || object_size > hash_object.size()) {
            error = localization::translate("Windows SHA-256 context has an unsupported size.");
            break;
        }
        if (BCryptCreateHash(algorithm_handle, &hash_handle, hash_object.data(),
                object_size, nullptr, 0, 0) < 0) {
            error = localization::translate("Cannot initialize the executable SHA-256 calculation.");
            break;
        }
        bool read_complete = false;
        for (;;) {
            DWORD bytes_read = 0;
            if (!ReadFile(file_handle, read_buffer.data(),
                    static_cast<DWORD>(read_buffer.size()), &bytes_read, nullptr)) {
                error = localization::translate("Cannot read the complete game executable.");
                break;
            }
            if (bytes_read == 0) {
                read_complete = true;
                break;
            }
            if (BCryptHashData(hash_handle, read_buffer.data(), bytes_read, 0) < 0) {
                error = localization::translate("Windows SHA-256 rejected an executable data block.");
                break;
            }
        }
        if (!read_complete) {
            break;
        }
        if (BCryptFinishHash(hash_handle, digest.data(),
                static_cast<ULONG>(digest.size()), 0) < 0) {
            error = localization::translate("Cannot finalize the executable SHA-256 calculation.");
            break;
        }
        success = true;
    } while (false);

    if (hash_handle != nullptr) {
        BCryptDestroyHash(hash_handle);
    }
    if (algorithm_handle != nullptr) {
        BCryptCloseAlgorithmProvider(algorithm_handle, 0);
    }
    CloseHandle(file_handle);
    return success;
}

bool game_runtime::matching_code(std::uintptr_t address,
    const unsigned char* expected, std::size_t size) noexcept {
    MEMORY_BASIC_INFORMATION region{};
    if (size == 0 || VirtualQuery(reinterpret_cast<const void*>(address), &region,
            sizeof(region)) != sizeof(region) || region.State != MEM_COMMIT ||
        region.Type != MEM_IMAGE || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD protection = region.Protect & 0xff;
    if (protection != PAGE_EXECUTE_READ && protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY) {
        return false;
    }
    const auto region_start = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    if (address < region_start || size > region.RegionSize ||
        address - region_start > region.RegionSize - size) {
        return false;
    }
    __try {
        return std::memcmp(reinterpret_cast<const void*>(address), expected, size) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool game_runtime::verify_code(std::uintptr_t module_base) noexcept {
    if (native_addresses::uses_patterns()) return native_addresses::verify_all();
    return matching_code(module_base + tick_rva, tick_prologue, sizeof(tick_prologue)) &&
        matching_code(module_base + recalculate_rva, recalculate_prologue, sizeof(recalculate_prologue)) &&
        matching_code(module_base + speed_packet_rva, speed_packet_prologue, sizeof(speed_packet_prologue)) &&
        matching_code(module_base + packet_grow_rva, packet_grow_prologue, sizeof(packet_grow_prologue)) &&
        matching_code(module_base + swap_player_rva, swap_player_prologue, sizeof(swap_player_prologue)) &&
        matching_code(module_base + flush_updates_rva, flush_updates_prologue, sizeof(flush_updates_prologue)) &&
        matching_code(module_base + route_queues_rva, route_queues_prologue, sizeof(route_queues_prologue)) &&
        matching_code(module_base + set_object_field_rva, set_object_field_prologue, sizeof(set_object_field_prologue)) &&
        matching_code(module_base + 0x5d5f1b7, luck_reader, sizeof(luck_reader)) &&
        matching_code(module_base + 0x5d1978f, area_reader, sizeof(area_reader)) &&
        matching_code(module_base + 0x5cd87f1, pickup_reader, sizeof(pickup_reader));
}

bool game_runtime::writable_data(const void* address, std::size_t size) noexcept {
    auto cursor = reinterpret_cast<std::uintptr_t>(address);
    if (cursor == 0 || size == 0 || size > std::numeric_limits<std::uintptr_t>::max() - cursor) {
        return false;
    }
    const std::uintptr_t end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region) ||
            region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
            return false;
        }
        const DWORD protection = region.Protect & 0xff;
        if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY) {
            return false;
        }
        const auto region_start = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if (region.RegionSize > std::numeric_limits<std::uintptr_t>::max() - region_start) {
            return false;
        }
        const auto region_end = region_start + region.RegionSize;
        if (region_end <= cursor) {
            return false;
        }
        cursor = region_end < end ? region_end : end;
    }
    return true;
}

bool game_runtime::synchronize_speed(void* run_address, speed_sync_diagnostic& diagnostic) noexcept {
    static_assert(sizeof(game_packet) == 32);
    diagnostic = {};
    auto* run_bytes = static_cast<unsigned char*>(run_address);
    diagnostic.reason = "run_memory";
    if (!writable_data(run_bytes + game_layout::run_offset(0x4b0), 0x1d8)) {
        return false;
    }
    __try {
        diagnostic.reason = "speed_value";
        std::memcpy(&diagnostic.speed, run_bytes + game_layout::run_offset(0x4dc), sizeof(diagnostic.speed));
        std::memcpy(&diagnostic.previous_world_speed, run_bytes + game_layout::run_offset(0x5a8),
            sizeof(diagnostic.previous_world_speed));
        const float world_speed = diagnostic.speed * 7.0f;
        if (!std::isfinite(world_speed) || world_speed < 0.0f ||
            !std::isfinite(diagnostic.previous_world_speed)) {
            return false;
        }
        if (std::fabs(world_speed - diagnostic.previous_world_speed) <= 0.01f) {
            return true;
        }
        diagnostic.reason = "native_helpers";
        if (create_speed_packet_ == nullptr || grow_packets_ == nullptr) {
            return false;
        }

        diagnostic.reason = "queue_metadata";
        std::memcpy(&diagnostic.capacity, run_bytes + game_layout::run_offset(0x670), sizeof(diagnostic.capacity));
        std::memcpy(&diagnostic.data_address, run_bytes + game_layout::run_offset(0x678), sizeof(diagnostic.data_address));
        std::memcpy(&diagnostic.count, run_bytes + game_layout::run_offset(0x680), sizeof(diagnostic.count));
        if (diagnostic.count > diagnostic.capacity || diagnostic.capacity > maximum_packet_capacity) {
            return false;
        }
        diagnostic.reason = "queue_capacity_limit";
        if (diagnostic.count == diagnostic.capacity && diagnostic.capacity == maximum_packet_capacity) {
            return false;
        }
        diagnostic.reason = "queue_buffer";
        if (diagnostic.capacity != 0 &&
            ((diagnostic.data_address & (alignof(game_packet) - 1)) != 0 ||
             !writable_data(reinterpret_cast<const void*>(diagnostic.data_address),
                 diagnostic.capacity * sizeof(game_packet)))) {
            return false;
        }
        const std::size_t original_count = diagnostic.count;
        if (diagnostic.count == diagnostic.capacity) {
            diagnostic.reason = "native_grow";
            grow_packets_(run_bytes + game_layout::run_offset(0x670));
            std::memcpy(&diagnostic.capacity, run_bytes + game_layout::run_offset(0x670), sizeof(diagnostic.capacity));
            std::memcpy(&diagnostic.data_address, run_bytes + game_layout::run_offset(0x678), sizeof(diagnostic.data_address));
            std::size_t current_count = 0;
            std::memcpy(&current_count, run_bytes + game_layout::run_offset(0x680), sizeof(current_count));
            diagnostic.reason = "queue_after_grow";
            if (original_count != current_count || original_count >= diagnostic.capacity ||
                diagnostic.capacity > maximum_packet_capacity) {
                diagnostic.count = current_count;
                return false;
            }
            if ((diagnostic.data_address & (alignof(game_packet) - 1)) != 0 ||
                !writable_data(reinterpret_cast<const void*>(diagnostic.data_address),
                    diagnostic.capacity * sizeof(game_packet))) {
                return false;
            }
        }
        diagnostic.reason = "queue_slot";
        const std::size_t slot_offset = original_count * sizeof(game_packet);
        if (slot_offset > std::numeric_limits<std::uintptr_t>::max() - diagnostic.data_address) {
            return false;
        }
        const std::uintptr_t slot_address = diagnostic.data_address + slot_offset;
        auto* packet_slot = reinterpret_cast<void*>(slot_address);
        if (!writable_data(packet_slot, sizeof(game_packet))) {
            return false;
        }

        std::uint64_t player_guid = 0;
        std::uint32_t old_revision = 0;
        std::memcpy(&player_guid, run_bytes + game_layout::run_offset(0x4b0), sizeof(player_guid));
        std::memcpy(&old_revision, run_bytes + game_layout::run_offset(0x5a4), sizeof(old_revision));
        game_packet packet{};
        diagnostic.reason = "native_packet";
        create_speed_packet_(&packet, player_guid, old_revision, world_speed);
        diagnostic.reason = "queue_commit";
        std::memcpy(packet_slot, &packet, sizeof(packet));
        const std::size_t new_count = original_count + 1;
        std::memcpy(run_bytes + game_layout::run_offset(0x680), &new_count, sizeof(new_count));
        std::memcpy(run_bytes + game_layout::run_offset(0x5a8), &world_speed, sizeof(world_speed));
        const std::uint32_t new_revision = old_revision + 1;
        std::memcpy(run_bytes + game_layout::run_offset(0x5a4), &new_revision, sizeof(new_revision));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        diagnostic.captured_seh_code = GetExceptionCode();
        return false;
    }
}

bool game_runtime::process_run(void* run_address, bool publish) noexcept {
    if (!features_enabled_.load(std::memory_order_acquire) || party_runtime::is_member_context(run_address)) {
        return false;
    }
    auto& state = trainer_state::instance();
    speed_update update{};
    if (!state.process_run(run_address, publish, &update)) {
        return false;
    }
    if (!update.written) {
        return true;
    }
    speed_sync_diagnostic diagnostic{};
    if (!synchronize_speed(run_address, diagnostic)) {
        const bool restored = state.reject_speed_update(run_address, update);
        char message[512]{};
        std::snprintf(message, sizeof(message),
            "%s [%s] "
            "speed=%.6g world=%.6g cap=%llu len=%llu ptr=0x%llx SEH=0x%08x rollback=%s",
            localization::translate("Speed change was cancelled; other settings remain active."),
            diagnostic.reason, static_cast<double>(diagnostic.speed),
            static_cast<double>(diagnostic.previous_world_speed),
            static_cast<unsigned long long>(diagnostic.capacity),
            static_cast<unsigned long long>(diagnostic.count),
            static_cast<unsigned long long>(diagnostic.data_address),
            diagnostic.captured_seh_code, restored ? "yes" : "no");
        state.set_message(message);
    }
    return true;
}

bool game_runtime::synchronize_member_speed(void* run_address) noexcept {
    speed_sync_diagnostic diagnostic{};
    return synchronize_speed(run_address, diagnostic);
}

void* __fastcall game_runtime::set_member_field(void* map, std::uint16_t field_id,
    std::uint32_t value_bits) noexcept {
    if (object_field_float_) return object_field_float_(map, field_id, std::bit_cast<float>(value_bits));
    return object_field_bits_ ? object_field_bits_(map, field_id, value_bits) : nullptr;
}

void __fastcall game_runtime::tick_hook(void* run_address, float delta_seconds) noexcept {
    const auto original = original_tick_.load(std::memory_order_acquire);
    if (!features_enabled_.load(std::memory_order_acquire)) {
        original(run_address, delta_seconds);
        return;
    }
    auto& state = trainer_state::instance();
    party_runtime::observe_owner(run_address);
    const auto settings = state.get_automation();
    const auto steps = simulation_clock::split_frame(delta_seconds, settings->time_scale);
    for (std::uint32_t step = 0; step < steps.count; ++step) {
        process_run(run_address, false);
        original(run_address, steps.seconds_per_step);
        const bool host_active = process_run(run_address, true);
        party_runtime::process_owner(run_address);
        if (host_active) {
            const auto current = state.get_automation();
            const auto choice = auto_choice::process_run(run_address, *current, GetTickCount64());
            if (choice.status == auto_choice_status::apply_failed) {
                state.set_message(localization::translate("Auto-choice stopped for this reward: the native handler returned an error."));
            }
        }
    }
}

bool game_runtime::legendary_enabled() noexcept {
    return features_enabled_.load(std::memory_order_acquire) &&
        trainer_state::instance().get_automation()->legendary_final_boss;
}

void game_runtime::report_extension(const char* message) noexcept {
    trainer_state::instance().set_message(message);
}

void __fastcall game_runtime::recalculate_hook(void* run_address) noexcept {
    const auto original = original_recalculate_.load(std::memory_order_acquire);
    original(run_address);
    if (features_enabled_.load(std::memory_order_acquire) && party_runtime::is_member_context(run_address)) {
        party_runtime::process_recalculated(run_address);
        return;
    }
    process_run(run_address, false);
}

void game_runtime::hook_failure(const char* operation, int status) noexcept {
    char message[256]{};
    std::snprintf(message, sizeof(message), localization::translate("Hook initialization failed (%s): %s. Restart the game before retrying."),
        operation, MH_StatusToString(static_cast<MH_STATUS>(status)));
    trainer_state::instance().set_message(message);
    initialization_state_.store(3, std::memory_order_release);
}

bool game_runtime::initialize() noexcept {
    try {
    static_assert(sizeof(void*) == 8, "The supported game requires an x64 trainer.");
    unsigned expected_state = 0;
    if (!initialization_state_.compare_exchange_strong(expected_state, 1, std::memory_order_acq_rel)) {
        return expected_state == 2;
    }
    auto& state = trainer_state::instance();
    state.set_hooks_ready(false);
    std::array<wchar_t, 32768> module_path{};
    const DWORD path_size = GetModuleFileNameW(nullptr, module_path.data(),
        static_cast<DWORD>(module_path.size()));
    if (path_size == 0 || path_size >= module_path.size()) {
        state.set_message(localization::translate("Cannot resolve the main executable path."));
        initialization_state_.store(3, std::memory_order_release);
        return false;
    }
    const wchar_t* file_name = std::wcsrchr(module_path.data(), L'\\');
    file_name = file_name != nullptr ? file_name + 1 : module_path.data();
    if (_wcsicmp(file_name, L"AzerothSurvivors.exe") != 0) {
        state.set_message(localization::translate("Unsupported process: expected AzerothSurvivors.exe."));
        initialization_state_.store(3, std::memory_order_release);
        return false;
    }

    std::array<unsigned char, 32> actual_hash{};
    const char* hash_error = nullptr;
    if (!hash_executable(module_path.data(), actual_hash, hash_error)) {
        state.set_message(hash_error);
        initialization_state_.store(3, std::memory_order_release);
        return false;
    }
    module_base_ = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::string signature_error;
    if (!native_addresses::initialize(module_base_, module_path.data(), actual_hash, signature_error)) {
        const auto message = std::string(localization::translate("Compatibility check failed.")) +
            " [" + signature_error + "]";
        state.set_message(message.c_str());
        initialization_state_.store(3, std::memory_order_release);
        return false;
    }
    if (module_base_ == 0 || !verify_code(module_base_)) {
        state.set_message(localization::translate("Game code bytes differ from the supported build. Disable other code patches and restart the game."));
        initialization_state_.store(3, std::memory_order_release);
        return false;
    }
    create_speed_packet_ = reinterpret_cast<speed_packet_function>(module_base_ + native_addresses::rva(native_symbol::create_speed_packet));
    grow_packets_ = reinterpret_cast<packet_grow_function>(module_base_ + native_addresses::rva(native_symbol::grow_speed_packets));
    party_native_functions party_functions{};
    party_functions.swap_player = reinterpret_cast<decltype(party_functions.swap_player)>(module_base_ + native_addresses::rva(native_symbol::swap_player));
    party_functions.flush_updates = reinterpret_cast<decltype(party_functions.flush_updates)>(module_base_ + native_addresses::rva(native_symbol::flush_updates));
    party_functions.route_queues = reinterpret_cast<decltype(party_functions.route_queues)>(module_base_ + native_addresses::rva(native_symbol::route_queues));
    if (native_addresses::uses_patterns()) {
        object_field_float_ = reinterpret_cast<decltype(object_field_float_)>(module_base_ + native_addresses::rva(native_symbol::set_object_field));
        object_field_bits_ = nullptr;
    } else {
        object_field_bits_ = reinterpret_cast<decltype(object_field_bits_)>(module_base_ + native_addresses::rva(native_symbol::set_object_field));
        object_field_float_ = nullptr;
    }
    party_functions.set_object_field = set_member_field;
    party_functions.synchronize_speed = synchronize_member_speed;
    party_runtime::initialize(party_functions);
    if (!auto_choice::initialize(module_base_)) {
        state.set_message(localization::translate("Reward selection handler could not be verified for this build."));
        initialization_state_.store(3, std::memory_order_release);
        return false;
    }

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        hook_failure("MH_Initialize", status);
        return false;
    }
    void* tick_trampoline = nullptr;
    void* recalculate_trampoline = nullptr;
    auto* tick_address = reinterpret_cast<void*>(module_base_ + native_addresses::rva(native_symbol::owner_frame));
    auto* recalculate_address = reinterpret_cast<void*>(module_base_ + native_addresses::rva(native_symbol::recalculate));
    status = MH_CreateHook(tick_address, reinterpret_cast<void*>(&tick_hook), &tick_trampoline);
    if (status != MH_OK) {
        hook_failure("create tick", status);
        return false;
    }
    original_tick_.store(reinterpret_cast<tick_function>(tick_trampoline), std::memory_order_release);
    status = MH_CreateHook(recalculate_address, reinterpret_cast<void*>(&recalculate_hook), &recalculate_trampoline);
    if (status != MH_OK) {
        hook_failure("create stat rebuild", status);
        return false;
    }
    original_recalculate_.store(reinterpret_cast<recalculate_function>(recalculate_trampoline), std::memory_order_release);
    if (!legendary_loot::initialize(module_base_, legendary_enabled, report_extension)) {
        state.set_message(localization::translate("Legendary set support could not be initialized. Verify the game build and restart."));
        initialization_state_.store(3, std::memory_order_release);
        return false;
    }
    status = MH_QueueEnableHook(tick_address);
    if (status == MH_OK) {
        status = MH_QueueEnableHook(recalculate_address);
    }
    if (status == MH_OK) {
        status = MH_ApplyQueued();
    }
    if (status != MH_OK) {
        hook_failure("enable hooks", status);
        return false;
    }
    state.set_hooks_ready(true);
    state.set_message(localization::translate(
        "Game build verified. Party settings are enabled separately by the host."));
    initialization_state_.store(2, std::memory_order_release);
    features_enabled_.store(true, std::memory_order_release);
    return true;
    } catch (...) {
        trainer_state::instance().set_message(localization::translate("Trainer initialization failed."));
        initialization_state_.store(3, std::memory_order_release);
        return false;
    }
}

void game_runtime::disable_features() noexcept {
    features_enabled_.store(false, std::memory_order_release);
    auto& state = trainer_state::instance();
    state.reset_all();
    state.set_hooks_ready(false);
}

} // namespace azs
