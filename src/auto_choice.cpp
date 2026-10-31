#include "auto_choice.hpp"
#include "trainer_state.hpp"
#include "game_layout.hpp"
#include "native_addresses.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cmath>
#include <cstring>
#include <limits>

namespace azs {
namespace {

constexpr std::uint64_t variant_mask = 0x8000000000000000;
constexpr std::size_t choice_stride = 0xa0;
constexpr unsigned char expected_apply_prologue[] = {
    0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41,
    0x54, 0x56, 0x57, 0x53, 0x48, 0x81, 0xec, 0xb8,
    0x03, 0x00, 0x00, 0x48, 0x8d, 0xac, 0x24, 0x80,
    0x00, 0x00, 0x00, 0x44, 0x0f, 0x29, 0x95, 0x20
};

} // namespace

bool auto_choice::initialize(std::uintptr_t module_base) noexcept {
    const auto apply_choice_rva = native_addresses::rva(native_symbol::apply_choice);
    apply_choice_ = nullptr;
    reset();
    if (module_base == 0 || module_base >
            std::numeric_limits<std::uintptr_t>::max() - apply_choice_rva) {
        return false;
    }
    const auto address = module_base + apply_choice_rva;
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) != sizeof(region) ||
        region.State != MEM_COMMIT || region.Type != MEM_IMAGE ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD protection = region.Protect & 0xff;
    if (protection != PAGE_EXECUTE_READ && protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY) {
        return false;
    }
    const auto region_start = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    if (address < region_start || region.RegionSize < sizeof(expected_apply_prologue) ||
        address - region_start > region.RegionSize - sizeof(expected_apply_prologue)) {
        return false;
    }
    unsigned char actual[sizeof(expected_apply_prologue)]{};
    if (!copy_bytes(reinterpret_cast<const void*>(address), actual, sizeof(actual)) ||
        std::memcmp(actual, expected_apply_prologue, sizeof(actual)) != 0) {
        return false;
    }
    apply_choice_ = reinterpret_cast<apply_function>(address);
    return true;
}

bool auto_choice::accessible_data(const void* address, std::size_t size, bool writable) noexcept {
    auto cursor = reinterpret_cast<std::uintptr_t>(address);
    if (cursor < 0x10000 || size == 0 || size >
            std::numeric_limits<std::uintptr_t>::max() - cursor) {
        return false;
    }
    const auto end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region) ||
            region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
            return false;
        }
        const DWORD protection = region.Protect & 0xff;
        if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
            (writable || protection != PAGE_READONLY)) {
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

bool auto_choice::copy_bytes(const void* source, void* destination, std::size_t size) noexcept {
    __try {
        std::memcpy(destination, source, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool auto_choice::copy_key(std::uintptr_t address, std::size_t length,
    copied_choice& destination) noexcept {
    if (length == 0 || length > maximum_key_length ||
        !accessible_data(reinterpret_cast<const void*>(address), length, false) ||
        !copy_bytes(reinterpret_cast<const void*>(address), destination.key.data(), length)) {
        return false;
    }
    for (std::size_t index = 0; index < length; ++index) {
        const auto character = static_cast<unsigned char>(destination.key[index]);
        if (character < 0x21 || character > 0x7e) {
            return false;
        }
    }
    destination.key[length] = '\0';
    destination.key_length = length;
    return true;
}

bool auto_choice::read_choice(const unsigned char* run_bytes,
    const unsigned char* card, copied_choice& destination) noexcept {
    std::uint64_t encoded_variant = 0;
    std::uint64_t rarity = 0;
    std::uintptr_t payload = 0;
    if (!read_value(card, 0x78, encoded_variant) || !read_value(card, 0x98, rarity) ||
        !read_value(card, 0x80, payload) || rarity > 4) {
        return false;
    }
    destination.rarity = static_cast<std::uint32_t>(rarity);
    const std::uint64_t variant = (encoded_variant & variant_mask) != 0 ?
        encoded_variant ^ variant_mask : 1;
    std::uintptr_t key_address = 0;
    std::size_t key_length = 0;
    if (variant == 0) {
        key_address = payload;
        if (!read_value(card, 0x88, key_length)) {
            return false;
        }
    } else if (variant == 1) {
        std::size_t effect_count = 0;
        std::size_t ability_index = 0;
        std::size_t ability_count = 0;
        std::uintptr_t abilities_address = 0;
        if (encoded_variant > 128 || !read_value(card, 0x88, effect_count) ||
            effect_count == 0 || effect_count > encoded_variant ||
            !accessible_data(reinterpret_cast<const void*>(payload), effect_count, false) ||
            !read_value(card, 0x90, ability_index) || !read_value(run_bytes, game_layout::run_offset(0x378), abilities_address) ||
            !read_value(run_bytes, game_layout::run_offset(0x380), ability_count) || ability_count > 128 ||
            ability_index >= ability_count || !accessible_data(reinterpret_cast<const void*>(abilities_address),
                ability_count * 0x58, false)) {
            return false;
        }
        std::uintptr_t definition = 0;
        if (!read_value(reinterpret_cast<const void*>(abilities_address), ability_index * 0x58, definition) ||
            !accessible_data(reinterpret_cast<const void*>(definition), 16, false) ||
            !read_value(reinterpret_cast<const void*>(definition), 0, key_address) ||
            !read_value(reinterpret_cast<const void*>(definition), 8, key_length)) {
            return false;
        }
    } else if (variant == 2) {
        if (!accessible_data(reinterpret_cast<const void*>(payload), 0x48, false) ||
            !read_value(reinterpret_cast<const void*>(payload), 0, key_address) ||
            !read_value(reinterpret_cast<const void*>(payload), 8, key_length)) {
            return false;
        }
    } else if (variant == 3) {
        std::size_t key_capacity = 0;
        if (!accessible_data(reinterpret_cast<const void*>(payload), 0x70, false) ||
            !read_value(reinterpret_cast<const void*>(payload), 0, key_capacity) ||
            !read_value(reinterpret_cast<const void*>(payload), 8, key_address) ||
            !read_value(reinterpret_cast<const void*>(payload), 0x10, key_length) ||
            key_length > key_capacity || key_capacity > (1u << 20)) {
            return false;
        }
    } else if (variant == 4) {
        std::memcpy(destination.key.data(), "gold", 5);
        destination.key_length = 4;
        return true;
    } else {
        return false;
    }
    return copy_key(key_address, key_length, destination);
}

bool auto_choice::read_offer(void* run_address, offer_snapshot& output, const char*& reason) noexcept {
    reason = "run_memory";
    if (!accessible_data(run_address, game_layout::scalar_size(), true)) {
        return false;
    }
    const auto* run_bytes = static_cast<const unsigned char*>(run_address);
    std::uint8_t phase = 0;
    if (!read_value(run_bytes, game_layout::run_offset(0xda3), phase)) {
        return false;
    }
    reason = "not_choosing";
    if (phase != 2) {
        return false;
    }
    reason = "run_identity";
    std::uint8_t class_id = 0;
    std::uint32_t level = 0;
    float maximum_hp = 0;
    if (!read_value(run_bytes, game_layout::run_offset(0x354), class_id) || !read_value(run_bytes, game_layout::run_offset(0x530), level) ||
        !read_value(run_bytes, game_layout::run_offset(0x4c8), maximum_hp)) {
        return false;
    }
    const bool valid_class = class_id == 1 || class_id == 2 || class_id == 3 || class_id == 4 ||
        class_id == 5 || class_id == 7 || class_id == 8 || class_id == 9 || class_id == 11;
    if (!valid_class || level == 0 || level > 100000 || !std::isfinite(maximum_hp) ||
        maximum_hp < 1 || maximum_hp > 1000000000) {
        return false;
    }
    reason = "offer_metadata";
    std::size_t capacity = 0;
    if (!read_value(run_bytes, game_layout::run_offset(0xda1), output.kind) || output.kind > 2 ||
        !read_value(run_bytes, game_layout::run_offset(0x6a0), capacity) || !read_value(run_bytes, game_layout::run_offset(0x6a8), output.data_address) ||
        !read_value(run_bytes, game_layout::run_offset(0x6b0), output.count) || output.count == 0 ||
        output.count > maximum_choices || output.count > capacity || capacity > 128 ||
        !read_value(run_bytes, game_layout::run_offset(0xd0c), output.generation) ||
        (output.data_address & 7) != 0 ||
        !accessible_data(reinterpret_cast<const void*>(output.data_address),
            output.count * choice_stride, false)) {
        return false;
    }
    output.run_address = reinterpret_cast<std::uintptr_t>(run_address);
    output.fingerprint = 14695981039346656037ull;
    reason = "choice_payload";
    for (std::size_t index = 0; index < output.count; ++index) {
        auto& candidate = output.choices[index];
        candidate.native_index = index;
        if (!read_choice(run_bytes,
                reinterpret_cast<const unsigned char*>(output.data_address) + index * choice_stride, candidate)) {
            return false;
        }
        for (std::size_t key_index = 0; key_index < candidate.key_length; ++key_index) {
            output.fingerprint ^= static_cast<unsigned char>(candidate.key[key_index]);
            output.fingerprint *= 1099511628211ull;
        }
        output.fingerprint ^= candidate.rarity + 1;
        output.fingerprint *= 1099511628211ull;
    }
    return true;
}

bool auto_choice::same_offer(const offer_snapshot& left, const offer_snapshot& right) noexcept {
    return left.run_address == right.run_address && left.generation == right.generation &&
        left.kind == right.kind &&
        left.data_address == right.data_address && left.count == right.count &&
        left.fingerprint == right.fingerprint;
}

bool auto_choice::equal_key(std::string_view left, std::string_view right) noexcept {
    while (!right.empty() && (right.front() == ' ' || right.front() == '\t' ||
        right.front() == '\r' || right.front() == '\n')) {
        right.remove_prefix(1);
    }
    while (!right.empty() && (right.back() == ' ' || right.back() == '\t' ||
        right.back() == '\r' || right.back() == '\n')) {
        right.remove_suffix(1);
    }
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        const unsigned char left_byte = static_cast<unsigned char>(left[index]);
        const unsigned char right_byte = static_cast<unsigned char>(right[index]);
        const auto normalized_left = left_byte >= 'A' && left_byte <= 'Z' ? left_byte + ('a' - 'A') : left_byte;
        const auto normalized_right = right_byte >= 'A' && right_byte <= 'Z' ? right_byte + ('a' - 'A') : right_byte;
        if (normalized_left != normalized_right) {
            return false;
        }
    }
    return true;
}

std::size_t auto_choice::select(std::span<const choice_candidate> candidates,
    std::span<const std::string> priorities) noexcept {
    std::size_t selected_index = static_cast<std::size_t>(-1);
    std::size_t best_priority = std::numeric_limits<std::size_t>::max();
    std::uint32_t best_rarity = 0;
    for (const auto& candidate : candidates) {
        if (candidate.key.empty() || candidate.rarity > 4) {
            continue;
        }
        std::size_t priority = priorities.size();
        for (std::size_t index = 0; index < priorities.size(); ++index) {
            if (equal_key(candidate.key, priorities[index])) {
                priority = index;
                break;
            }
        }
        if (selected_index == static_cast<std::size_t>(-1) || priority < best_priority ||
            (priority == best_priority && candidate.rarity > best_rarity)) {
            selected_index = candidate.native_index;
            best_priority = priority;
            best_rarity = candidate.rarity;
        }
    }
    return selected_index;
}

bool auto_choice::apply_native(void* run_address, std::size_t index, std::uint32_t& seh_code) noexcept {
    seh_code = 0;
    if (apply_choice_ == nullptr) {
        return false;
    }
    __try {
        apply_choice_(run_address, index);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        seh_code = GetExceptionCode();
        return false;
    }
}

auto_choice_result auto_choice::process_run(void* run_address,
    const automation_settings& settings, std::uint64_t now_ms) noexcept {
    auto_choice_result result{};
    if (!settings.auto_choose_spells && !settings.auto_choose_jewelry) {
        observing_ = false;
        result.status = auto_choice_status::disabled;
        result.reason = "disabled";
        return result;
    }
    offer_snapshot current{};
    if (!read_offer(run_address, current, result.reason)) {
        observing_ = false;
        result.status = std::strcmp(result.reason, "not_choosing") == 0 ?
            auto_choice_status::not_choosing : auto_choice_status::invalid_offer;
        return result;
    }
    if ((current.kind == 0 && !settings.auto_choose_spells) ||
        (current.kind != 0 && !settings.auto_choose_jewelry)) {
        observing_ = false;
        result.status = auto_choice_status::disabled;
        result.reason = "offer_kind_disabled";
        return result;
    }
    if (have_attempted_ && same_offer(current, attempted_)) {
        result.status = auto_choice_status::already_attempted;
        result.reason = "same_offer_already_attempted";
        return result;
    }
    if (!observing_ || !same_offer(current, observed_) || now_ms < observed_since_ms_) {
        observed_ = current;
        observed_since_ms_ = now_ms;
        observing_ = true;
    }
    const auto delay_ms = settings.choice_delay_ms < 50 ? 50u :
        (settings.choice_delay_ms > 5000 ? 5000u : settings.choice_delay_ms);
    if (now_ms - observed_since_ms_ < delay_ms ||
        (have_attempted_ && (now_ms < last_apply_ms_ || now_ms - last_apply_ms_ < delay_ms))) {
        result.status = auto_choice_status::waiting;
        result.reason = "choice_delay";
        return result;
    }
    if (apply_choice_ == nullptr) {
        result.status = auto_choice_status::invalid_offer;
        result.reason = "native_apply_unverified";
        return result;
    }
    std::array<choice_candidate, maximum_choices> candidates{};
    for (std::size_t index = 0; index < current.count; ++index) {
        const auto& copied = current.choices[index];
        candidates[index] = {std::string_view(copied.key.data(), copied.key_length),
            copied.rarity, copied.native_index};
    }
    const auto& priorities = current.kind == 0 ? settings.spell_priorities : settings.jewelry_priorities;
    result.selected_index = select(std::span<const choice_candidate>(candidates.data(), current.count), priorities);
    offer_snapshot confirmed{};
    if (result.selected_index >= current.count || !read_offer(run_address, confirmed, result.reason) ||
        !same_offer(current, confirmed)) {
        observing_ = false;
        result.status = auto_choice_status::invalid_offer;
        result.reason = "offer_changed_before_apply";
        return result;
    }
    attempted_ = confirmed;
    have_attempted_ = true;
    last_apply_ms_ = now_ms;
    result.status = apply_native(run_address, result.selected_index, result.captured_seh_code) ?
        auto_choice_status::chosen : auto_choice_status::apply_failed;
    result.reason = result.status == auto_choice_status::chosen ? "native_choice_applied" : "native_apply_failed";
    return result;
}

void auto_choice::reset() noexcept {
    observing_ = false;
    have_attempted_ = false;
    observed_since_ms_ = 0;
    last_apply_ms_ = 0;
    observed_ = {};
    attempted_ = {};
}

} // namespace azs
