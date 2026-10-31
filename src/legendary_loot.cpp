#include "legendary_loot.hpp"
#include "localization.hpp"
#include "game_layout.hpp"
#include "native_addresses.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <MinHook.h>

#include <cstring>
#include <limits>

namespace azs {
namespace {

constexpr std::size_t zone_count = 12;
constexpr std::size_t zone_stride = 0xf0;
constexpr std::size_t boss_stride = 0x68;
constexpr std::size_t maximum_loot_capacity = 1u << 16;
constexpr unsigned char boss_prologue[] = {
    0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41,
    0x54, 0x56, 0x57, 0x53, 0x48, 0x81, 0xec, 0x98,
    0x00, 0x00, 0x00, 0x48, 0x8d, 0xac, 0x24, 0x80,
    0x00, 0x00, 0x00, 0x44, 0x0f, 0x29, 0x4d, 0x00
};
constexpr unsigned char grow_prologue[] = {
    0x56, 0x57, 0x48, 0x83, 0xec, 0x48, 0x48, 0x89,
    0xce, 0x48, 0x8b, 0x11, 0x48, 0x8d, 0x04, 0x12,
    0x48, 0x83, 0xf8, 0x05, 0xbf, 0x04, 0x00, 0x00,
    0x00, 0x48, 0x0f, 0x43, 0xf8, 0x4c, 0x8b, 0x41
};

} // namespace

bool legendary_loot::data_range(const void* address, std::size_t size, bool writable) noexcept {
    auto cursor = reinterpret_cast<std::uintptr_t>(address);
    if (cursor < 0x10000 || size == 0 || size > UINTPTR_MAX - cursor) {
        return false;
    }
    const auto end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region) ||
            region.State != MEM_COMMIT || (region.Protect & PAGE_GUARD) != 0) {
            return false;
        }
        const auto protection = region.Protect & 0xff;
        if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
            (writable || protection != PAGE_READONLY)) {
            return false;
        }
        const auto region_start = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if (region.RegionSize > UINTPTR_MAX - region_start) {
            return false;
        }
        const auto region_end = region_start + region.RegionSize;
        if (region_end <= cursor) {
            return false;
        }
        cursor = region_end;
    }
    return true;
}

bool legendary_loot::matching_code(std::uintptr_t address, const unsigned char* expected,
    std::size_t size) noexcept {
    MEMORY_BASIC_INFORMATION region{};
    if (size == 0 || VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) != sizeof(region) ||
        region.State != MEM_COMMIT || region.Type != MEM_IMAGE || (region.Protect & PAGE_GUARD) != 0) {
        return false;
    }
    const auto protection = region.Protect & 0xff;
    const auto region_start = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    if ((protection != PAGE_EXECUTE_READ && protection != PAGE_EXECUTE_READWRITE &&
         protection != PAGE_EXECUTE_WRITECOPY) || address < region_start || size > region.RegionSize ||
        address - region_start > region.RegionSize - size) {
        return false;
    }
    __try {
        return std::memcmp(reinterpret_cast<const void*>(address), expected, size) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::size_t legendary_loot::legendary_ids(std::uint8_t class_id,
    std::array<std::uint32_t, 10>& item_ids) noexcept {
    if (class_id != 1 && class_id != 2 && class_id != 3 && class_id != 4 && class_id != 5 &&
        class_id != 7 && class_id != 8 && class_id != 9 && class_id != 11) {
        return 0;
    }
    const std::size_t count = class_id == 4 || class_id == 7 ? 10 : 9;
    const auto first_id = 100000u + static_cast<std::uint32_t>(class_id) * 1000u + 40u;
    for (std::size_t index = 0; index < count; ++index) {
        item_ids[index] = first_id + static_cast<std::uint32_t>(index) * 100u;
    }
    return count;
}

bool legendary_loot::final_boss_event(void* run_address, const void* boss_address,
    std::uint8_t& class_id) noexcept {
    if (!data_range(run_address, game_layout::scalar_size(), true) ||
        !data_range(zone_descriptors_, zone_count * zone_stride, false)) {
        return false;
    }
    __try {
        const auto* run = static_cast<const unsigned char*>(run_address);
        std::memcpy(&class_id, run + game_layout::run_offset(0x354), sizeof(class_id));
        std::array<std::uint32_t, 10> ids{};
        if (legendary_ids(class_id, ids) == 0) {
            return false;
        }
        const char* zone_key = nullptr;
        std::size_t key_size = 0;
        std::memcpy(&zone_key, run + game_layout::run_offset(0x2d0), sizeof(zone_key));
        std::memcpy(&key_size, run + game_layout::run_offset(0x2d8), sizeof(key_size));
        if (key_size == 0 || key_size > 32 || !data_range(zone_key, key_size, false)) {
            return false;
        }
        for (std::size_t zone_index = 0; zone_index < zone_count; ++zone_index) {
            const auto* zone = zone_descriptors_ + zone_index * zone_stride;
            const char* candidate_key = nullptr;
            std::size_t candidate_size = 0;
            std::memcpy(&candidate_key, zone + 8, sizeof(candidate_key));
            std::memcpy(&candidate_size, zone + 0x10, sizeof(candidate_size));
            if (candidate_size != key_size || !data_range(candidate_key, candidate_size, false) ||
                std::memcmp(candidate_key, zone_key, key_size) != 0) {
                continue;
            }
            std::uintptr_t bosses = 0;
            std::size_t bosses_count = 0;
            std::memcpy(&bosses, zone + 0x88, sizeof(bosses));
            std::memcpy(&bosses_count, zone + 0x90, sizeof(bosses_count));
            if (bosses_count != 5 || !data_range(reinterpret_cast<const void*>(bosses),
                    bosses_count * boss_stride, false)) {
                return false;
            }
            const auto boss = reinterpret_cast<std::uintptr_t>(boss_address);
            if (boss < bosses || boss - bosses >= bosses_count * boss_stride ||
                (boss - bosses) % boss_stride != 0) {
                return false;
            }
            return *(static_cast<const unsigned char*>(boss_address) + 0x64) != 0;
        }
        return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool legendary_loot::append_legendary_set(void* run_address, std::uint8_t class_id) noexcept {
    static_assert(sizeof(loot_record) == 12 && alignof(loot_record) == 4);
    if (grow_loot_ == nullptr || !data_range(run_address, game_layout::scalar_size(), true)) {
        return false;
    }
    __try {
        std::array<std::uint32_t, 10> item_ids{};
        const auto item_count = legendary_ids(class_id, item_ids);
        if (item_count == 0) {
            return false;
        }
        auto* run = static_cast<unsigned char*>(run_address);
        std::size_t capacity = 0;
        std::uintptr_t data_address = 0;
        std::size_t count = 0;
        std::memcpy(&capacity, run + game_layout::run_offset(0x6d0), sizeof(capacity));
        std::memcpy(&data_address, run + game_layout::run_offset(0x6d8), sizeof(data_address));
        std::memcpy(&count, run + game_layout::run_offset(0x6e0), sizeof(count));
        if (count > capacity || capacity > maximum_loot_capacity ||
            (capacity != 0 && ((data_address & 3) != 0 ||
             !data_range(reinterpret_cast<const void*>(data_address), capacity * sizeof(loot_record), true)))) {
            return false;
        }
        std::array<loot_record, 10> additions{};
        std::size_t added = 0;
        std::uint32_t boss_roll_index = 0;
        std::memcpy(&boss_roll_index, run + game_layout::run_offset(0x83c), sizeof(boss_roll_index));
        const auto* records = reinterpret_cast<const loot_record*>(data_address);
        for (std::size_t item_index = 0; item_index < item_count; ++item_index) {
            bool already_queued = false;
            for (std::size_t queue_index = 0; queue_index < count; ++queue_index) {
                if (records[queue_index].item_id == item_ids[item_index]) {
                    already_queued = true;
                    break;
                }
            }
            if (!already_queued) {
                additions[added++] = {item_ids[item_index], boss_roll_index, 0.0f};
            }
        }
        if (added == 0) {
            return true;
        }
        if (added > maximum_loot_capacity - count) {
            return false;
        }
        const auto needed = count + added;
        while (capacity < needed) {
            grow_loot_(run + game_layout::run_offset(0x6d0));
            std::size_t grown_capacity = 0;
            std::size_t current_count = 0;
            std::memcpy(&grown_capacity, run + game_layout::run_offset(0x6d0), sizeof(grown_capacity));
            std::memcpy(&data_address, run + game_layout::run_offset(0x6d8), sizeof(data_address));
            std::memcpy(&current_count, run + game_layout::run_offset(0x6e0), sizeof(current_count));
            if (current_count != count || grown_capacity <= capacity || grown_capacity > maximum_loot_capacity ||
                (data_address & 3) != 0 || !data_range(reinterpret_cast<const void*>(data_address),
                    grown_capacity * sizeof(loot_record), true)) {
                return false;
            }
            capacity = grown_capacity;
        }
        const auto offset = count * sizeof(loot_record);
        if (offset > UINTPTR_MAX - data_address) {
            return false;
        }
        auto* destination = reinterpret_cast<void*>(data_address + offset);
        if (!data_range(destination, added * sizeof(loot_record), true)) {
            return false;
        }
        std::memcpy(destination, additions.data(), added * sizeof(loot_record));
        std::memcpy(run + game_layout::run_offset(0x6e0), &needed, sizeof(needed));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void __fastcall legendary_loot::boss_loot_hook(void* run_address, const void* boss_address) noexcept {
    std::uint8_t class_id = 0;
    const bool extend_loot = is_enabled_ != nullptr && is_enabled_() &&
        final_boss_event(run_address, boss_address, class_id);
    const auto original = original_boss_loot_.load(std::memory_order_acquire);
    original(run_address, boss_address);
    if (extend_loot && !append_legendary_set(run_address, class_id) && report_ != nullptr) {
        report_(localization::translate("Legendary set was not added: loot queue validation failed. Other features remain active."));
    }
}

bool legendary_loot::initialize(std::uintptr_t module_base, enabled_getter is_enabled,
    message_sink report) noexcept {
    const auto boss_loot_rva = native_addresses::rva(native_symbol::boss_loot);
    const auto grow_loot_rva = native_addresses::rva(native_symbol::grow_loot);
    const auto zone_descriptors_rva = native_addresses::rva(native_symbol::zone_table);
    unsigned expected = 0;
    if (!initialization_state_.compare_exchange_strong(expected, 1, std::memory_order_acq_rel)) {
        return expected == 2;
    }
    if (is_enabled == nullptr || module_base == 0 ||
        module_base > UINTPTR_MAX - zone_descriptors_rva ||
        !matching_code(module_base + boss_loot_rva, boss_prologue, sizeof(boss_prologue)) ||
        !matching_code(module_base + grow_loot_rva, grow_prologue, sizeof(grow_prologue))) {
        initialization_state_.store(3, std::memory_order_release);
        return false;
    }
    const auto* zones = reinterpret_cast<const unsigned char*>(module_base + zone_descriptors_rva);
    if (!data_range(zones, zone_count * zone_stride, false)) {
        initialization_state_.store(3, std::memory_order_release);
        return false;
    }
    is_enabled_ = is_enabled;
    report_ = report;
    zone_descriptors_ = zones;
    grow_loot_ = reinterpret_cast<grow_loot_function>(module_base + grow_loot_rva);
    auto* target = reinterpret_cast<void*>(module_base + boss_loot_rva);
    void* trampoline = nullptr;
    auto status = MH_CreateHook(target, reinterpret_cast<void*>(&boss_loot_hook), &trampoline);
    if (status == MH_OK) {
        original_boss_loot_.store(reinterpret_cast<boss_loot_function>(trampoline), std::memory_order_release);
        status = MH_EnableHook(target);
    }
    if (status != MH_OK) {
        initialization_state_.store(3, std::memory_order_release);
        return false;
    }
    initialization_state_.store(2, std::memory_order_release);
    return true;
}

} // namespace azs
