#include "party_runtime.hpp"
#include "game_layout.hpp"
#include "localization.hpp"

#include <Windows.h>

#include <cmath>
#include <cstring>
#include <limits>

namespace azs {

void party_runtime::initialize(const party_native_functions& functions) noexcept {
    native_ = functions;
    owner_run_ = nullptr;
    owner_thread_ = 0;
    owner_guid_ = 0;
    last_elapsed_ = 0;
    enabled_before_ = false;
    failed_ = false;
    host_values_ready_ = false;
    writes_ = 0;
    members_ = {};
}

bool party_runtime::writable_data(const void* address, std::size_t size) noexcept {
    auto cursor = reinterpret_cast<std::uintptr_t>(address);
    if (cursor < 0x10000 || size == 0 || size > UINTPTR_MAX - cursor) return false;
    const auto end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region) ||
            region.State != MEM_COMMIT || (region.Protect & PAGE_GUARD) != 0) return false;
        const auto protection = region.Protect & 0xff;
        if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY) return false;
        const auto next = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
        if (next <= cursor) return false;
        cursor = next;
    }
    return true;
}

bool party_runtime::read_party(void* run_address, unsigned char*& data,
    std::size_t& count, float& elapsed) noexcept {
    data = nullptr;
    count = 0;
    if (!writable_data(run_address, game_layout::run_size())) return false;
    std::uint32_t role{};
    std::uint8_t coop{}, phase{};
    std::size_t index{}, capacity{};
    if (!read_value(run_address, game_layout::run_offset(0xda8), role) || role != 3 ||
        !read_value(run_address, game_layout::run_offset(0xd9e), coop) || coop != 1 ||
        !read_value(run_address, game_layout::run_offset(0xda4), phase) || phase < 1 || phase > 3 ||
        !read_value(run_address, game_layout::run_offset(0xc98), index) || index != 0 ||
        !read_value(run_address, game_layout::run_offset(0xce0), elapsed) || !std::isfinite(elapsed) || elapsed < 0 ||
        !read_value(run_address, game_layout::run_offset(0xa68), capacity) ||
        !read_value(run_address, game_layout::run_offset(0xa70), data) ||
        !read_value(run_address, game_layout::run_offset(0xa78), count) || count > members_.size() || count > capacity ||
        capacity > 64 || (count != 0 && !writable_data(data, count * game_layout::participant_stride()))) return false;
    return true;
}

void party_runtime::observe_owner(void* run_address) noexcept {
    unsigned char* data{};
    std::size_t count{};
    float elapsed{};
    std::uint64_t guid{};
    if (!read_party(run_address, data, count, elapsed) ||
        !read_value(run_address, game_layout::run_offset(0x4b0), guid) || guid == 0) {
        trainer_state::instance().set_party_status(false, 0, writes_);
        return;
    }
    const auto thread = GetCurrentThreadId();
    if (owner_run_ != run_address || owner_guid_ != guid || owner_thread_ != thread ||
        elapsed + 0.0001f < last_elapsed_) {
        owner_run_ = run_address;
        owner_guid_ = guid;
        owner_thread_ = thread;
        members_ = {};
        failed_ = false;
        host_values_ready_ = false;
        writes_ = 0;
    }
    last_elapsed_ = elapsed;
    trainer_state::instance().set_party_status(true, player_count(data, count), writes_);
}

bool party_runtime::is_member_context(void* run_address) noexcept {
    std::size_t index{};
    return run_address != nullptr && read_value(run_address, game_layout::run_offset(0xc98), index) && index != 0;
}

bool party_runtime::read_values(void* run_address, player_values& values) noexcept {
    return read_value(run_address, game_layout::run_offset(0x538), values.health) &&
        read_value(run_address, game_layout::run_offset(0x4c8), values.maximum_health) &&
        read_value(run_address, game_layout::run_offset(0x4d4), values.crit) &&
        std::isfinite(values.health) && values.health >= 0 && values.health <= 1000000000 &&
        std::isfinite(values.maximum_health) && values.maximum_health >= 1 && values.maximum_health <= 1000000000 &&
        std::isfinite(values.crit) && values.crit >= 0 && values.crit <= 100;
}

bool party_runtime::synchronize_values(void* run_address, const player_values& before, bool force) noexcept {
    player_values after{};
    if (!read_values(run_address, after) || !native_.set_object_field) return false;
    __try {
        auto* fields = static_cast<unsigned char*>(run_address) + game_layout::run_offset(0xc78);
        if (force || before.health != after.health)
            native_.set_object_field(fields, 0x16, static_cast<std::uint32_t>(std::ceil(after.health)));
        if (force || before.maximum_health != after.maximum_health)
            native_.set_object_field(fields, 0x1c, static_cast<std::uint32_t>(std::ceil(after.maximum_health)));
        if (force || before.crit != after.crit) {
            const float percent = after.crit * 100;
            std::uint32_t bits{};
            std::memcpy(&bits, &percent, sizeof(bits));
            native_.set_object_field(fields, 0x455, bits);
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool party_runtime::apply_member(void* run_address, member_record& member) noexcept {
    player_values before{};
    if (!read_values(run_address, before)) return false;
    const auto previous_ids = member.commands.applied_ids;
    speed_update speed{};
    auto& state = trainer_state::instance();
    if (!state.process_member(run_address, member.commands, &speed)) return false;
    if (speed.written && (!native_.synchronize_speed || !native_.synchronize_speed(run_address))) {
        state.reject_member_speed_update(run_address, speed);
        member.commands.rejected_speed_command = speed.command_id;
        state.set_message(localization::translate("One player's speed change was cancelled because native queue validation failed. Other settings remain active."));
    }
    if (!synchronize_values(run_address, before, false)) {
        fail("Party changes stopped: could not synchronize a player's health fields.");
        return false;
    }
    if (previous_ids != member.commands.applied_ids) ++writes_;
    return true;
}

bool party_runtime::process_member(void* run_address, void* participant,
    std::size_t index, member_record& member) noexcept {
    bool swapped = false;
    bool restored = false;
    bool route_attempted = false;
    bool applied = false;
    __try {
        __try {
            native_.swap_player(run_address, participant);
            swapped = true;
            std::memcpy(static_cast<unsigned char*>(run_address) + game_layout::run_offset(0xc98), &index, sizeof(index));
            applied = apply_member(run_address, member);
            native_.flush_updates(run_address);
            route_attempted = true;
            native_.route_queues(run_address);
        } __finally {
            if (swapped) {
                __try {
                    if (!route_attempted) native_.route_queues(run_address);
                } __finally {
                    const std::size_t host_index = 0;
                    std::memcpy(static_cast<unsigned char*>(run_address) + game_layout::run_offset(0xc98), &host_index, sizeof(host_index));
                    native_.swap_player(run_address, participant);
                    restored = true;
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fail(restored ? "Party traversal stopped after a native helper error; the host context was restored." :
            "Party traversal stopped: host context restoration could not be confirmed.");
        return false;
    }
    return applied;
}

void party_runtime::process_owner(void* run_address) noexcept {
    observe_owner(run_address);
    auto& state = trainer_state::instance();
    const bool enabled = state.get_automation()->apply_to_party;
    if (!enabled) {
        enabled_before_ = false;
        return;
    }
    if (!enabled_before_) {
        members_ = {};
        failed_ = false;
        host_values_ready_ = false;
        enabled_before_ = true;
    }
    if (failed_ || owner_run_ != run_address || owner_thread_ != GetCurrentThreadId()) return;
    unsigned char* data{};
    std::size_t count{};
    float elapsed{};
    if (!read_party(run_address, data, count, elapsed) || !native_.swap_player ||
        !native_.flush_updates || !native_.route_queues) return;
    if (!synchronize_values(run_address, host_values_, !host_values_ready_) ||
        !read_values(run_address, host_values_)) {
        fail("Party changes stopped: could not synchronize the host's health.");
        return;
    }
    host_values_ready_ = true;
    // These helpers must finish before the first participant swap: the queues are shared.
    if (!flush_context(run_address)) {
        fail("Party traversal stopped: the host's native queue is unavailable.");
        return;
    }
    std::array<std::uint64_t, 4> guids{};
    for (std::size_t slot = 0; slot < count; ++slot) {
        auto* participant = data + slot * game_layout::participant_stride();
        std::uint32_t tag{};
        std::uint64_t guid{};
        if (!read_value(participant, game_layout::member_offset(0), tag) || tag == 2 ||
            !read_value(participant, game_layout::member_offset(0x4b0), guid) || guid == 0) continue;
        guids[slot] = guid;
    }
    for (auto& member : members_) {
        bool present = false;
        for (const auto guid : guids) if (guid != 0 && guid == member.guid) present = true;
        if (!present) member = {};
    }
    for (std::size_t slot = 0; slot < count && !failed_; ++slot) {
        if (guids[slot] == 0) continue;
        auto* participant = data + slot * game_layout::participant_stride();
        auto& member = find_member(guids[slot], slot + 1);
        process_member(run_address, participant, slot + 1, member);
    }
    state.set_party_status(true, player_count(data, count), writes_);
}

bool party_runtime::flush_context(void* run_address) noexcept {
    __try {
        native_.flush_updates(run_address);
        native_.route_queues(run_address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void party_runtime::process_recalculated(void* run_address) noexcept {
    if (failed_ || owner_run_ != run_address || owner_thread_ != GetCurrentThreadId() ||
        !trainer_state::instance().get_automation()->apply_to_party) return;
    std::size_t index{};
    std::uint64_t guid{};
    std::uint32_t role{};
    std::uint8_t coop{}, global_phase{};
    if (!read_value(run_address, game_layout::run_offset(0xda8), role) || role != 3 ||
        !read_value(run_address, game_layout::run_offset(0xd9e), coop) || coop != 1 ||
        !read_value(run_address, game_layout::run_offset(0xda4), global_phase) || global_phase < 1 || global_phase > 3 ||
        !read_value(run_address, game_layout::run_offset(0xc98), index) || index == 0 || index > members_.size() ||
        !read_value(run_address, game_layout::run_offset(0x4b0), guid) || guid == 0) return;
    auto& member = find_member(guid, index);
    apply_member(run_address, member);
}

party_runtime::member_record& party_runtime::find_member(std::uint64_t guid, std::size_t index) noexcept {
    for (auto& member : members_) {
        if (member.guid == guid) {
            member.last_index = index;
            return member;
        }
    }
    for (auto& member : members_) {
        if (member.guid == 0) {
            member = {guid, {}, index};
            return member;
        }
    }
    for (auto& member : members_) {
        if (member.last_index == index) {
            member = {guid, {}, index};
            return member;
        }
    }
    members_.back() = {guid, {}, index};
    return members_.back();
}

std::uint32_t party_runtime::player_count(const unsigned char* data, std::size_t count) noexcept {
    std::uint32_t result = 1;
    for (std::size_t slot = 0; slot < count; ++slot) {
        std::uint32_t tag{};
        std::uint64_t guid{};
        if (read_value(data + slot * game_layout::participant_stride(), game_layout::member_offset(0), tag) && tag != 2 &&
            read_value(data + slot * game_layout::participant_stride(), game_layout::member_offset(0x4b0), guid) && guid != 0) ++result;
    }
    return result;
}

void party_runtime::fail(const char* message) noexcept {
    failed_ = true;
    trainer_state::instance().set_message(localization::translate(message));
}

} // namespace azs
