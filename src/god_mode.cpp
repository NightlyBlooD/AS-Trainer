#include "god_mode.hpp"
#include "god_mode_events.hpp"
#include "god_mode_bindings.hpp"
#include "game_layout.hpp"
#include "pattern_scanner.hpp"
#include "signature_profile.hpp"
#include "native_addresses.hpp"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace azs {

bool god_mode::writable_data(const void* address, std::size_t size) noexcept {
    auto cursor = reinterpret_cast<std::uintptr_t>(address);
    if (cursor == 0 || size == 0 || size > UINTPTR_MAX - cursor) return false;
    const auto end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region) ||
            region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
        const auto protection = region.Protect & 0xff;
        if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY) return false;
        const auto begin = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if (region.RegionSize > UINTPTR_MAX - begin) return false;
        const auto next = begin + region.RegionSize;
        if (next <= cursor) return false;
        cursor = next;
    }
    return true;
}

bool god_mode::matching_bytes(std::uintptr_t module_base, std::uint32_t rva,
    std::span<const std::uint8_t> bytes, bool executable) noexcept {
    if (module_base == 0 || bytes.empty() || rva > UINTPTR_MAX - module_base) return false;
    auto cursor = module_base + rva;
    if (bytes.size() > UINTPTR_MAX - cursor) return false;
    const auto end = cursor + bytes.size();
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region) ||
            region.State != MEM_COMMIT || region.Type != MEM_IMAGE ||
            reinterpret_cast<std::uintptr_t>(region.AllocationBase) != module_base ||
            (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
        const auto protection = region.Protect & 0xff;
        if (executable) {
            if (protection != PAGE_EXECUTE_READ && protection != PAGE_EXECUTE_READWRITE &&
                protection != PAGE_EXECUTE_WRITECOPY) return false;
        } else if (protection != PAGE_READONLY) return false;
        const auto begin = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if (region.RegionSize > UINTPTR_MAX - begin) return false;
        const auto next = begin + region.RegionSize;
        if (next <= cursor) return false;
        cursor = next;
    }
    __try {
        return std::memcmp(reinterpret_cast<const void*>(module_base + rva), bytes.data(), bytes.size()) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool god_mode::initialize(std::uintptr_t module_base, const std::filesystem::path& executable_path,
    const std::array<unsigned char, 32>& digest) noexcept {
    damage_ = nullptr;
    profile_ = {};
    try {
        constexpr char hex_digits[] = "0123456789abcdef";
        std::array<char, 65> hash{};
        for (std::size_t index = 0; index < digest.size(); ++index) {
            hash[index * 2] = hex_digits[digest[index] >> 4];
            hash[index * 2 + 1] = hex_digits[digest[index] & 15];
        }
        pattern_scanner scanner;
        if (!scanner.load_file(executable_path) || signature_profiles::sha256(scanner.bytes()) != hash.data())
            return false;
        god_mode_bindings::candidate binding;
        std::string error;
        if (!god_mode_bindings::resolve_file(scanner, binding, error) ||
            binding.phase_offset != game_layout::run_offset(0xda3) ||
            binding.spells_offset != game_layout::run_offset(0x380) ||
            binding.lifesteal_rva != native_addresses::rva(native_symbol::lifesteal)) return false;
        for (const auto& chunk : binding.verification) {
            if (chunk.relocated_pointer) {
                std::uint64_t target_rva{};
                if (chunk.executable || chunk.bytes.size() != sizeof(target_rva)) return false;
                std::memcpy(&target_rva, chunk.bytes.data(), sizeof(target_rva));
                if (target_rva >= scanner.image_size() || target_rva > UINTPTR_MAX - module_base) return false;
                const std::uint64_t expected_pointer = module_base + target_rva;
                const auto expected_bytes = std::span(
                    reinterpret_cast<const std::uint8_t*>(&expected_pointer), sizeof(expected_pointer));
                if (!matching_bytes(module_base, chunk.rva, expected_bytes, false)) return false;
            } else if (!matching_bytes(module_base, chunk.rva, chunk.bytes, chunk.executable)) return false;
        }
        if (!god_mode_events::initialize(module_base + binding.dispatcher_rva)) return false;
        profile_ = {binding.enemies_offset, binding.enemy_stride, binding.source_offset};
        damage_ = reinterpret_cast<damage_function>(module_base + binding.damage_rva);
        return true;
    } catch (...) {
        return false;
    }
}

bool god_mode::supported() noexcept {
    return damage_ != nullptr;
}

bool god_mode::read_enemies(void* run_address, enemy_vector& result) noexcept {
    __try {
        const auto* bytes = static_cast<const unsigned char*>(run_address) + profile_.enemies_offset;
        std::size_t capacity{}, count{};
        unsigned char* data{};
        std::memcpy(&capacity, bytes, sizeof(capacity));
        std::memcpy(&data, bytes + 8, sizeof(data));
        std::memcpy(&count, bytes + 16, sizeof(count));
        if (capacity > 65536 || count > capacity || profile_.enemy_stride == 0) return false;
        if (count != 0 && !writable_data(data, count * profile_.enemy_stride)) return false;
        result = {data, count};
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

unsigned char* god_mode::find_enemy(void* run_address, std::uint64_t guid) noexcept {
    enemy_vector enemies{};
    if (!read_enemies(run_address, enemies)) return nullptr;
    __try {
        for (std::size_t index = 0; index < enemies.count; ++index) {
            auto* enemy = enemies.data + index * profile_.enemy_stride;
            if (*reinterpret_cast<const std::uint64_t*>(enemy + 0xf0) == guid) return enemy;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

bool god_mode::deal_damage(void* run_address, std::uint64_t guid, float damage,
    float maximum_health, bool raised_cap) {
    auto* bytes = static_cast<unsigned char*>(run_address);
    std::array<unsigned char, 16> source_effects{}, target_effects{};
    std::memcpy(source_effects.data(), bytes + 0x20, source_effects.size());
    std::memcpy(target_effects.data(), bytes + 0x40, target_effects.size());
    const auto source = bytes[profile_.source_offset];
    const auto spells_offset = game_layout::run_offset(0x380);
    std::size_t spell_count{};
    std::memcpy(&spell_count, bytes + spells_offset, sizeof(spell_count));
    const bool previous_events = god_mode_events::set_active(true);
    bool restored = true;
    __try {
        bytes[profile_.source_offset] = 3;
        std::memset(bytes + 0x20, 0, sizeof(std::uint64_t));
        std::memset(bytes + 0x40, 0, sizeof(std::uint64_t));
        std::memset(bytes + spells_offset, 0, sizeof(spell_count));
        damage_(run_address, guid, damage, 0, 0, false, true);
    } __finally {
        god_mode_events::set_active(previous_events);
        std::memcpy(bytes + spells_offset, &spell_count, sizeof(spell_count));
        std::memcpy(bytes + 0x20, source_effects.data(), source_effects.size());
        std::memcpy(bytes + 0x40, target_effects.data(), target_effects.size());
        bytes[profile_.source_offset] = source;
        if (raised_cap) {
            auto* enemy = find_enemy(run_address, guid);
            if (enemy != nullptr) std::memcpy(enemy + 0x17c, &maximum_health, sizeof(maximum_health));
            else restored = false;
        }
    }
    return restored;
}

bool god_mode::solo_playing(const void* run_address) noexcept {
    const auto* run = static_cast<const unsigned char*>(run_address);
    return run[game_layout::run_offset(0xda3)] == 1 && run[game_layout::run_offset(0xd9e)] == 0 &&
        *reinterpret_cast<const std::size_t*>(run + game_layout::run_offset(0xc98)) == 0 &&
        *reinterpret_cast<const std::uint32_t*>(run + game_layout::run_offset(0xda8)) == 2;
}

bool god_mode::process_run(void* run_address) noexcept {
    if (!supported()) return true;
    if (!writable_data(run_address, game_layout::run_size())) return false;
    __try {
        if (!solo_playing(run_address)) return true;
        enemy_vector enemies{};
        if (!read_enemies(run_address, enemies)) return false;
        for (std::size_t index = 0; index < enemies.count; ++index) {
            auto* enemy = enemies.data + index * profile_.enemy_stride;
            if (*reinterpret_cast<const std::uint32_t*>(enemy + 0xb0) == 1) continue;
            const float health = *reinterpret_cast<const float*>(enemy + 0x178);
            if (!std::isfinite(health)) return false;
            if (health <= 0) continue;
            const auto guid = *reinterpret_cast<const std::uint64_t*>(enemy + 0xf0);
            const float maximum_health = *reinterpret_cast<const float*>(enemy + 0x17c);
            const float damage = health * 4.0f;
            const float required_cap = std::nextafter(health * 5.0f, std::numeric_limits<float>::infinity());
            if (guid == 0 || !std::isfinite(maximum_health) || maximum_health <= 0 ||
                !std::isfinite(damage) || !std::isfinite(required_cap)) return false;
            const bool raised_cap = *reinterpret_cast<const std::int32_t*>(enemy + 0xd0) == 3 &&
                *reinterpret_cast<const std::uint64_t*>(enemy + 0xe0) == 0 && maximum_health < required_cap;
            if (raised_cap) std::memcpy(enemy + 0x17c, &required_cap, sizeof(required_cap));
            const bool restored = deal_damage(run_address, guid, damage, maximum_health, raised_cap);
            if (!solo_playing(run_address)) return true;
            if (!restored) return false;
            enemy = find_enemy(run_address, guid);
            if (enemy == nullptr || *reinterpret_cast<const std::uint32_t*>(enemy + 0xb0) != 1) return false;
            if (!read_enemies(run_address, enemies)) return false;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace azs
