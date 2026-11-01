#include "speed_sync_tests.hpp"
#include "game_layout.hpp"
#include "game_runtime.hpp"
#include "trainer_state.hpp"

#include <Windows.h>

#include <algorithm>
#include <bit>
#include <cstdio>
#include <limits>

namespace azs {

void speed_sync_tests::check(bool condition, const char* description) {
    if (!condition) {
        ++failures_;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

std::size_t speed_sync_tests::find_field(std::size_t offset) {
    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
        if (trainer_state::field_at(index).offset == offset) {
            return index;
        }
    }
    return trainer_state::field_count();
}

void* speed_sync_tests::allocate(std::size_t size) {
    for (auto& allocation : allocations_) {
        if (allocation == nullptr) {
            allocation = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            return allocation;
        }
    }
    return nullptr;
}

void speed_sync_tests::release(void* address) {
    if (address == nullptr) {
        return;
    }
    for (auto& allocation : allocations_) {
        if (allocation == address) {
            VirtualFree(allocation, 0, MEM_RELEASE);
            allocation = nullptr;
            return;
        }
    }
}

void speed_sync_tests::prepare_run(void* run_address) {
    auto& state = trainer_state::instance();
    state.set_hooks_ready(false);
    std::memset(run_address, 0, 0x2000);
    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
        const auto& descriptor = trainer_state::field_at(index);
        if (descriptor.kind == value_kind::real && descriptor.access != field_access::native_getter) {
            write_value(run_address, descriptor.offset, static_cast<float>(descriptor.minimum));
        }
    }
    write_value<std::uint8_t>(run_address, 0x354, 11);
    write_value<std::uint32_t>(run_address, 0x530, 30);
    write_value<std::uint8_t>(run_address, 0xda3, 1);
    write_value<float>(run_address, 0x4c8, 100);
    write_value<float>(run_address, 0x538, 100);
    write_value<float>(run_address, 0x4cc, 1);
    write_value<float>(run_address, 0x4dc, 1);
    write_value<float>(run_address, 0x5a8, 7);
    write_value<std::uint64_t>(run_address, 0x4b0, 0x123456789abcdef0);
    write_value<std::uint32_t>(run_address, 0x5a4, 19);
    packet_calls_ = 0;
    grow_calls_ = 0;
    observed_guid_ = 0;
    observed_revision_ = 0;
    observed_world_speed_ = 0;
    growth_mode_ = grow_mode::normal;
    raise_packet_exception_ = false;
    game_runtime::create_speed_packet_ = &create_packet;
    game_runtime::grow_packets_ = &grow_queue;
    game_runtime::features_enabled_.store(true);
    state.set_hooks_ready(true);
    state.set_message("fixture ready");
    game_runtime::process_run(run_address, true);
    check(state.get_runtime().hooks_ready && state.get_runtime().run_active,
        "fixture publishes an active run without installing hooks");
}

void speed_sync_tests::set_queue(void* run_address, std::size_t capacity,
    void* data, std::size_t count) {
    write_value(run_address, 0x670, capacity);
    write_value(run_address, 0x678, data);
    write_value(run_address, 0x680, count);
}

void speed_sync_tests::test_current_layout(void* run_address, void* packet_buffer,
    std::size_t speed_field) {
    auto& state = trainer_state::instance();
    const auto saved_packet = game_runtime::create_speed_packet_;
    const auto saved_grow = game_runtime::grow_packets_;
    const auto saved_float_setter = game_runtime::object_field_float_;
    const auto saved_bits_setter = game_runtime::object_field_bits_;
    const bool saved_features = game_runtime::features_enabled_.load();
    state.set_hooks_ready(false);
    check(game_layout::select(layout_id::current), "current executable layout selected");
    check(speed_field < trainer_state::field_count() &&
          trainer_state::field_at(speed_field).offset == 0x4e4,
        "stable cached speed index resolves to the verified current offset");
    std::memset(run_address, 0, 0x2000);
    std::memset(packet_buffer, 0, 4 * sizeof(game_runtime::game_packet));
    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
        const auto& descriptor = trainer_state::field_at(index);
        if (descriptor.kind == value_kind::real && descriptor.access != field_access::native_getter) {
            write_value(run_address, descriptor.offset, static_cast<float>(descriptor.minimum));
        }
    }
    constexpr std::uint64_t current_guid = 0xfedcba9876543210;
    write_value<std::uint8_t>(run_address, 0x35c, 11);
    write_value<std::uint32_t>(run_address, 0x538, 30);
    write_value<float>(run_address, 0x4d0, 100);
    write_value<float>(run_address, 0x540, 100);
    write_value<std::uint8_t>(run_address, 0xec3, 1);
    write_value<float>(run_address, 0xe00, 0);
    write_value<std::size_t>(run_address, 0xdb8, 0);
    write_value(run_address, 0x4b8, current_guid);
    write_value<float>(run_address, 0x4e4, 1);
    write_value<float>(run_address, 0x5b0, 7);
    write_value<std::uint32_t>(run_address, 0x5ac, 73);
    write_value<std::size_t>(run_address, 0x678, 4);
    write_value(run_address, 0x680, packet_buffer);
    write_value<std::size_t>(run_address, 0x688, 0);
    packet_calls_ = 0;
    grow_calls_ = 0;
    growth_mode_ = grow_mode::normal;
    raise_packet_exception_ = false;
    game_runtime::create_speed_packet_ = &create_packet;
    game_runtime::grow_packets_ = &grow_queue;
    game_runtime::features_enabled_.store(true);
    state.set_hooks_ready(true);
    check(game_runtime::process_run(run_address, true) && state.get_runtime().run_active,
        "current run with new class, level, phase, clock and owner index is adopted");

    check(state.apply(speed_field, 2), "current-layout one-shot speed command accepted");
    check(game_runtime::process_run(run_address, true) && packet_calls_ == 1 &&
          grow_calls_ == 0 && observed_guid_ == current_guid &&
          observed_revision_ == 73 && observed_world_speed_ == 14,
        "current runtime forwards the new GUID, revision and speed fields to the native constructor");
    check(read_value<float>(run_address, 0x4e4) == 2 &&
          read_value<float>(run_address, 0x5b0) == 14 &&
          read_value<std::uint32_t>(run_address, 0x5ac) == 74 &&
          read_value<std::size_t>(run_address, 0x678) == 4 &&
          read_value<void*>(run_address, 0x680) == packet_buffer &&
          read_value<std::size_t>(run_address, 0x688) == 1 &&
          read_value<game_runtime::game_packet>(packet_buffer, 0).protocol == 0xe2,
        "current runtime commits the packet, new queue length, speed mirror and next revision");
    game_runtime::process_run(run_address, true);
    check(packet_calls_ == 1, "current one-shot speed generation is consumed once");

    check(state.freeze(speed_field, true, 3), "current-layout speed freeze accepted");
    game_runtime::process_run(run_address, true);
    check(state.get_field(speed_field).frozen && packet_calls_ == 2 &&
          observed_guid_ == current_guid && observed_revision_ == 74 &&
          observed_world_speed_ == 21 && read_value<float>(run_address, 0x5b0) == 21 &&
          read_value<std::uint32_t>(run_address, 0x5ac) == 75 &&
          read_value<std::size_t>(run_address, 0x688) == 2,
        "current freeze updates the verified mirror and uses the immediately previous revision");
    write_value<float>(run_address, 0x4e4, 1.5f);
    write_value<float>(run_address, 0x5b0, 10.5f);
    game_runtime::process_run(run_address, true);
    check(read_value<float>(run_address, 0x4e4) == 3 &&
          read_value<float>(run_address, 0x5b0) == 21 && packet_calls_ == 3 &&
          observed_guid_ == current_guid && observed_revision_ == 75 &&
          read_value<std::uint32_t>(run_address, 0x5ac) == 76 &&
          read_value<std::size_t>(run_address, 0x688) == 3 && grow_calls_ == 0 &&
          read_value<game_runtime::game_packet>(packet_buffer, 2 * 32).protocol == 0xe2,
        "current freeze reapplies after native changes without touching the legacy queue or allocator");

    test_current_lifesteal(run_address);

    auto* map = static_cast<unsigned char*>(run_address) + 0xd98;
    constexpr auto hp_bits = std::bit_cast<std::uint32_t>(751.0f);
    constexpr std::uint32_t crit_bits = 0x42c80000;
    float_setter_calls_ = 0;
    bits_setter_calls_ = 0;
    game_runtime::object_field_float_ = &set_field_float;
    game_runtime::object_field_bits_ = &set_field_bits;
    check(game_runtime::set_member_field(map, 0x16, hp_bits) == map &&
          float_setter_calls_ == 1 && bits_setter_calls_ == 0 &&
          observed_map_ == map && observed_field_id_ == 0x16 && observed_value_bits_ == hp_bits,
        "current float setter ABI preserves HP 751 as exact XMM2 bits and returns the map");
    check(game_runtime::set_member_field(map, 0x455, crit_bits) == map &&
          float_setter_calls_ == 2 && bits_setter_calls_ == 0 &&
          observed_map_ == map && observed_field_id_ == 0x455 && observed_value_bits_ == crit_bits,
        "current float setter ABI preserves crit 0x42c80000 without integer-to-float conversion");

    game_runtime::object_field_float_ = nullptr;
    check(game_layout::select(layout_id::legacy), "legacy layout restored for integer setter contract");
    check(game_runtime::set_member_field(map, 0x1c, hp_bits) == map &&
          bits_setter_calls_ == 1 && float_setter_calls_ == 2 &&
          observed_map_ == map && observed_field_id_ == 0x1c && observed_value_bits_ == hp_bits,
        "legacy integer setter ABI preserves the complete HP representation in R8D");
    check(game_runtime::set_member_field(map, 0x455, crit_bits) == map &&
          bits_setter_calls_ == 2 && observed_map_ == map &&
          observed_field_id_ == 0x455 && observed_value_bits_ == crit_bits,
        "legacy integer setter ABI forwards the exact crit key and bits");

    state.set_hooks_ready(false);
    game_runtime::create_speed_packet_ = saved_packet;
    game_runtime::grow_packets_ = saved_grow;
    game_runtime::object_field_float_ = saved_float_setter;
    game_runtime::object_field_bits_ = saved_bits_setter;
    game_runtime::features_enabled_.store(saved_features);
}

float __fastcall speed_sync_tests::native_lifesteal(void* run_address) noexcept {
    ++lifesteal_original_calls_;
    lifesteal_observed_run_ = run_address;
    return 0.125f;
}

void __fastcall speed_sync_tests::native_lifesteal_recalculate(void* run_address) noexcept {
    lifesteal_recalculated_value_ = game_runtime::lifesteal_hook(run_address);
}

void speed_sync_tests::test_current_lifesteal(void* run_address) {
    auto& state = trainer_state::instance();
    const auto saved_getter = game_runtime::original_lifesteal_.load();
    const auto saved_recalculate = game_runtime::original_recalculate_.load();
    const bool saved_features = game_runtime::features_enabled_.load();
    game_runtime::original_lifesteal_.store(&native_lifesteal);
    game_runtime::original_recalculate_.store(&native_lifesteal_recalculate);
    lifesteal_original_calls_ = 0;
    lifesteal_observed_run_ = nullptr;
    constexpr std::size_t lifesteal = 25;
    check(trainer_state::field_count() == 26 &&
          std::strcmp(trainer_state::field_at(lifesteal).key, "lifesteal") == 0 &&
          trainer_state::field_at(lifesteal).access == field_access::native_getter,
        "current lifesteal has a dedicated computed descriptor instead of a fake scalar offset");

    auto* foreign_run = reinterpret_cast<void*>(std::uintptr_t{0x12348});
    game_runtime::features_enabled_.store(false);
    check(game_runtime::lifesteal_hook(foreign_run) == 0.125f &&
          lifesteal_original_calls_ == 1 && lifesteal_observed_run_ == foreign_run,
        "disabled lifesteal hook forwards the exact RCX identity and XMM0 float once");
    game_runtime::features_enabled_.store(true);
    check(game_runtime::lifesteal_hook(foreign_run) == 0.125f &&
          lifesteal_original_calls_ == 2 && lifesteal_observed_run_ == foreign_run,
        "unowned getter context delegates once without writing or adopting foreign memory");
    check(game_runtime::lifesteal_hook(run_address) == 0.125f && lifesteal_original_calls_ == 3,
        "an owner without a lifesteal command observes the original getter once");
    check(game_runtime::process_run(run_address, true) && state.get_field(lifesteal).available &&
          state.get_field(lifesteal).current_value == 0.125,
        "owner-thread native lifesteal observations reach the published UI snapshot");

    check(state.apply(lifesteal, 0.25) && game_runtime::process_run(run_address, true),
        "lifesteal one-shot command activates through the validated owning bridge");
    const auto calls_before_override = lifesteal_original_calls_;
    const float effective_rate = game_runtime::lifesteal_hook(run_address);
    const float healed_health = std::min(100.0f, 40.0f + 20.0f * effective_rate);
    check(effective_rate == 0.25f && healed_health == 45.0f &&
          lifesteal_original_calls_ == calls_before_override && !state.get_field(lifesteal).pending,
        "active override bypasses the pure native sum and supplies the fraction consumed by native-style health math");
    check(game_runtime::lifesteal_hook(foreign_run) == 0.125f &&
          lifesteal_original_calls_ == calls_before_override + 1,
        "an active owner override cannot leak into another native context");
    game_runtime::features_enabled_.store(false);
    check(game_runtime::lifesteal_hook(run_address) == 0.125f &&
          lifesteal_original_calls_ == calls_before_override + 2,
        "disabling runtime features bypasses even an already active lifesteal override");
    game_runtime::features_enabled_.store(true);

    game_runtime::recalculate_hook(run_address);
    check(lifesteal_recalculated_value_ == 0.125f &&
          game_runtime::lifesteal_hook(run_address) == 0.125f,
        "native recalculation invalidates a consumed one-shot before the original rebuild runs");
    check(state.apply(lifesteal, 0) && game_runtime::process_run(run_address, true),
        "zero lifesteal is a valid explicit command rather than an absent override");
    const auto calls_before_zero = lifesteal_original_calls_;
    check(game_runtime::lifesteal_hook(run_address) == 0 &&
          lifesteal_original_calls_ == calls_before_zero,
        "an explicit zero suppresses native lifesteal and still bypasses the original getter");
    check(state.freeze(lifesteal, true, 0.5) && game_runtime::process_run(run_address, true),
        "lifesteal freeze is accepted without writing a scalar run field");
    game_runtime::recalculate_hook(run_address);
    check(lifesteal_recalculated_value_ == 0.125f &&
          game_runtime::lifesteal_hook(run_address) == 0.5f,
        "a rebuild sees the original value then frozen lifesteal is reapplied after native recalculation");
    state.reset_all();
    const auto calls_before_reset = lifesteal_original_calls_;
    check(game_runtime::lifesteal_hook(run_address) == 0.125f &&
          lifesteal_original_calls_ == calls_before_reset + 1,
        "panic reset immediately restores the original native lifesteal getter");
    state.set_hooks_ready(false);
    game_runtime::original_lifesteal_.store(saved_getter);
    game_runtime::original_recalculate_.store(saved_recalculate);
    game_runtime::features_enabled_.store(saved_features);
    state.set_hooks_ready(true);
    game_runtime::process_run(run_address, true);
}

void* __fastcall speed_sync_tests::set_field_float(void* map, std::uint16_t field_id,
    float value) noexcept {
    ++float_setter_calls_;
    observed_map_ = map;
    observed_field_id_ = field_id;
    observed_value_bits_ = std::bit_cast<std::uint32_t>(value);
    return map;
}

void* __fastcall speed_sync_tests::set_field_bits(void* map, std::uint16_t field_id,
    std::uint32_t value_bits) noexcept {
    ++bits_setter_calls_;
    observed_map_ = map;
    observed_field_id_ = field_id;
    observed_value_bits_ = value_bits;
    return map;
}

void* __fastcall speed_sync_tests::create_packet(void* output, std::uint64_t guid,
    std::uint32_t revision, float world_speed) {
    ++packet_calls_;
    observed_guid_ = guid;
    observed_revision_ = revision;
    observed_world_speed_ = world_speed;
    if (raise_packet_exception_) {
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
    }
    game_runtime::game_packet packet{};
    packet.protocol = 0xe2;
    std::memcpy(output, &packet, sizeof(packet));
    return output;
}

void __fastcall speed_sync_tests::owner_frame(void* run_address, float delta_seconds) noexcept {
    if (record_owner_values_ && owner_calls_ < owner_values_.size()) {
        owner_values_[owner_calls_] = read_value<float>(run_address, 0x4cc);
        owner_snapshots_[owner_calls_] = trainer_state::instance().get_field(find_field(0x4cc)).current_value;
        write_value(run_address, 0x4cc, owner_values_[owner_calls_] + 1.0f);
    }
    ++owner_calls_;
    owner_total_seconds_ += delta_seconds;
    write_value(run_address, 0xce0, read_value<float>(run_address, 0xce0) + delta_seconds);
}

void speed_sync_tests::test_accelerated_publication(void* run_address) {
    const auto saved_owner = game_runtime::original_tick_.load();
    game_runtime::original_tick_.store(&owner_frame);
    prepare_run(run_address);
    auto& state = trainer_state::instance();
    const auto damage = find_field(0x4cc);
    auto settings = *state.get_automation();
    settings.time_scale = 5;
    check(state.set_automation(settings), "x5 publication fixture settings accepted");
    owner_calls_ = 0;
    owner_values_.fill(0);
    owner_snapshots_.fill(0);
    record_owner_values_ = true;
    constexpr float delta = 1.0f / 64.0f;
    game_runtime::tick_hook(run_address, delta);
    bool final_only = owner_calls_ == 5;
    for (std::size_t index = 0; index < 5; ++index) {
        final_only = final_only && owner_snapshots_[index] == 1 &&
            owner_values_[index] == static_cast<float>(index + 1);
    }
    check(final_only && state.get_field(damage).current_value == 6 && state.get_runtime().run_active,
        "x5 native steps all execute while field snapshots publish only the final value");

    check(state.freeze(damage, true, 20), "per-substep freeze accepted");
    owner_calls_ = 0;
    game_runtime::tick_hook(run_address, delta);
    bool every_step_frozen = owner_calls_ == 5;
    for (std::size_t index = 0; index < 5; ++index) {
        every_step_frozen = every_step_frozen && owner_values_[index] == 20;
    }
    check(every_step_frozen && read_value<float>(run_address, 0x4cc) == 20 &&
          state.get_field(damage).current_value == 20,
        "deferred snapshots preserve freeze writes before and after every accelerated step");

    check(state.freeze(damage, false, 20) && state.apply(damage, 9) &&
          state.set_apply_on_start(damage, true), "new-run start command prepared");
    write_value<float>(run_address, 0x4cc, 1);
    write_value<float>(run_address, 0xce0, 0);
    owner_calls_ = 0;
    game_runtime::tick_hook(run_address, delta);
    check(owner_calls_ == 5 && owner_values_[0] == 1 && owner_values_[1] == 9 &&
          state.get_field(damage).current_value == 13 && !state.get_field(damage).pending,
        "clock rewind adopts a new run and applies start commands at its first substep");
    check(!state.process_run(reinterpret_cast<void*>(1), true, nullptr, false),
        "deferred field publication retains full invalid-pointer validation");

    LARGE_INTEGER frequency{};
    check(QueryPerformanceFrequency(&frequency) != 0 && frequency.QuadPart > 0,
        "performance counter frequency is available for deterministic aggregate checks");
    game_runtime::record_frame_performance(0, 0, 0, 0);
    game_runtime::record_frame_performance(100, 100 + frequency.QuadPart,
        frequency.QuadPart / 2, 5);
    const auto timing = state.get_runtime();
    check(timing.owner_chain_ms > 499 && timing.owner_chain_ms <= 500 &&
          timing.bridge_ms >= 500 && timing.bridge_ms < 501 && timing.simulation_steps == 5,
        "one-second timing aggregate separates owner-chain and outer bridge wall time");
    state.set_frame_performance(std::numeric_limits<double>::quiet_NaN(), 1, 1);
    check(state.get_runtime().simulation_steps == 5,
        "invalid timing sample cannot overwrite the last finite aggregate");
    state.set_hooks_ready(false);
    const auto disabled_timing = state.get_runtime();
    check(disabled_timing.owner_chain_ms == 0 && disabled_timing.bridge_ms == 0 &&
          disabled_timing.simulation_steps == 0,
        "disabled hook bridge immediately invalidates timing snapshots");
    game_runtime::record_frame_performance(0, 0, 0, 0);
    record_owner_values_ = false;
    game_runtime::original_tick_.store(saved_owner);
    prepare_run(run_address);
}

void __fastcall speed_sync_tests::grow_queue(void* queue_address) {
    ++grow_calls_;
    if (growth_mode_ == grow_mode::newer_command) {
        trainer_state::instance().apply(find_field(0x4dc), 3);
        return;
    }
    const auto old_capacity = read_value<std::size_t>(queue_address, 0);
    const auto count = read_value<std::size_t>(queue_address, 16);
    auto* old_data = read_value<void*>(queue_address, 8);
    const auto new_capacity = old_capacity < 4 ? 4 : old_capacity * 2;
    auto* new_data = allocate(new_capacity * sizeof(game_runtime::game_packet));
    check(new_data != nullptr, "native grow stub allocates a replacement queue");
    if (new_data == nullptr) {
        return;
    }
    if (count != 0) {
        std::memcpy(new_data, old_data, count * sizeof(game_runtime::game_packet));
    }
    release(old_data);
    write_value(queue_address, 0, new_capacity);
    write_value(queue_address, 8, new_data);
    if (growth_mode_ == grow_mode::corrupt_count) {
        write_value(queue_address, 16, count + 1);
    }
}

int speed_sync_tests::run() {
    check(game_layout::select(layout_id::legacy), "legacy executable layout selected before existing fixtures");
    auto* run_address = allocate(0x2000);
    if (run_address == nullptr) {
        std::fprintf(stderr, "Cannot allocate the speed-sync run fixture.\n");
        return 1;
    }
    auto& state = trainer_state::instance();
    const auto speed = find_field(0x4dc);
    const auto damage = find_field(0x4cc);

    prepare_run(run_address);
    set_queue(run_address, 1, reinterpret_cast<void*>(1), 2);
    write_value<float>(run_address, 0x5a8, std::numeric_limits<float>::quiet_NaN());
    check(state.freeze(damage, true, 20), "unrelated damage freeze is accepted");
    game_runtime::process_run(run_address, true);
    check(read_value<float>(run_address, 0x4cc) == 20 && state.get_field(damage).frozen &&
          state.get_runtime().hooks_ready && game_runtime::features_enabled_.load(),
        "bad speed cache/queue does not disable unrelated writes when speed was not requested");
    check(packet_calls_ == 0 && grow_calls_ == 0 && state.get_message() == "fixture ready",
        "without a speed command no native helper or speed-error path is entered");

    prepare_run(run_address);
    auto* first_queue = allocate(4 * sizeof(game_runtime::game_packet));
    check(first_queue != nullptr, "spare queue fixture allocated");
    if (first_queue == nullptr) {
        return 1;
    }
    set_queue(run_address, 4, first_queue, 0);
    check(state.apply(speed, 2), "one-shot speed command accepted");
    game_runtime::process_run(run_address, true);
    check(packet_calls_ == 1 && grow_calls_ == 0 && observed_guid_ == 0x123456789abcdef0 &&
          observed_revision_ == 19 && observed_world_speed_ == 14,
        "packet constructor receives exact GUID, old revision and seven-times-speed ABI values");
    check(read_value<std::size_t>(run_address, 0x680) == 1 &&
          read_value<std::uint32_t>(run_address, 0x5a4) == 20 &&
          read_value<float>(run_address, 0x5a8) == 14 &&
          read_value<game_runtime::game_packet>(first_queue, 0).protocol == 0xe2,
        "successful append commits the 32-byte packet, length, mirror and next revision");
    game_runtime::process_run(run_address, true);
    check(packet_calls_ == 1, "consumed one-shot command does not emit a packet every tick");

    prepare_run(run_address);
    set_queue(run_address, 0, reinterpret_cast<void*>(8), 0);
    state.apply(speed, 2);
    game_runtime::process_run(run_address, true);
    check(grow_calls_ == 1 && packet_calls_ == 1 &&
          read_value<std::size_t>(run_address, 0x670) == 4 &&
          read_value<std::size_t>(run_address, 0x680) == 1,
        "empty Rust Vec with aligned dangling pointer grows before appending");

    prepare_run(run_address);
    auto* full_queue = allocate(4 * sizeof(game_runtime::game_packet));
    check(full_queue != nullptr, "full queue fixture allocated");
    if (full_queue == nullptr) {
        return 1;
    }
    for (std::size_t index = 0; index < 4; ++index) {
        game_runtime::game_packet packet{};
        packet.protocol = 0x5100 + index;
        write_value(full_queue, index * sizeof(packet), packet);
    }
    set_queue(run_address, 4, full_queue, 4);
    state.apply(speed, 2);
    game_runtime::process_run(run_address, true);
    auto* relocated_queue = read_value<void*>(run_address, 0x678);
    check(grow_calls_ == 1 && relocated_queue != full_queue && packet_calls_ == 1 &&
          read_value<std::size_t>(run_address, 0x670) == 8 &&
          read_value<std::size_t>(run_address, 0x680) == 5,
        "full queue relocation is reloaded before calculating the new packet slot");
    check(read_value<game_runtime::game_packet>(relocated_queue, 0).protocol == 0x5100 &&
          read_value<game_runtime::game_packet>(relocated_queue, 3 * 32).protocol == 0x5103 &&
          read_value<game_runtime::game_packet>(relocated_queue, 4 * 32).protocol == 0xe2,
        "queue growth preserves existing packets and appends at the replacement buffer");

    prepare_run(run_address);
    set_queue(run_address, 1, reinterpret_cast<void*>(1), 2);
    write_value<float>(run_address, 0x5a8, 14);
    state.freeze(speed, true, 2);
    game_runtime::process_run(run_address, true);
    check(packet_calls_ == 0 && grow_calls_ == 0 && state.get_field(speed).frozen &&
          read_value<float>(run_address, 0x4dc) == 2,
        "matching speed mirror needs no queue access or native call even for frozen speed");

    prepare_run(run_address);
    set_queue(run_address, 1, reinterpret_cast<void*>(1), 2);
    state.freeze(damage, true, 30);
    state.freeze(speed, true, 2);
    game_runtime::process_run(run_address, true);
    check(read_value<float>(run_address, 0x4dc) == 1 && !state.get_field(speed).frozen &&
          state.get_field(damage).frozen && read_value<float>(run_address, 0x4cc) == 30 &&
          state.get_runtime().hooks_ready && game_runtime::features_enabled_.load(),
        "malformed queue rolls back/cancels only speed and preserves all unrelated controls");
    check(packet_calls_ == 0 && grow_calls_ == 0 &&
          state.get_message().find("queue_metadata") != std::string::npos &&
          state.get_message().find("cap=1 len=2 ptr=0x1") != std::string::npos &&
          state.get_message().find("rollback=yes") != std::string::npos,
        "rejection records the exact bounded queue stage, metadata and rollback result");
    write_value<float>(run_address, 0x4cc, 1);
    game_runtime::process_run(run_address, true);
    check(read_value<float>(run_address, 0x4cc) == 30,
        "other freezes keep working on subsequent ticks with the same invalid queue");

    prepare_run(run_address);
    set_queue(run_address, 0, reinterpret_cast<void*>(8), 0);
    growth_mode_ = grow_mode::newer_command;
    state.freeze(speed, true, 2);
    game_runtime::process_run(run_address, true);
    check(read_value<float>(run_address, 0x4dc) == 1 && state.get_field(speed).pending &&
          state.get_field(speed).desired_value == 3,
        "rejecting an old speed generation preserves a newer command queued during propagation");
    growth_mode_ = grow_mode::normal;
    game_runtime::process_run(run_address, true);
    check(read_value<float>(run_address, 0x4dc) == 3 && observed_world_speed_ == 21 &&
          !state.get_field(speed).pending,
        "the newer speed command can propagate successfully after the old failure");

    prepare_run(run_address);
    set_queue(run_address, 0, reinterpret_cast<void*>(8), 0);
    growth_mode_ = grow_mode::corrupt_count;
    state.apply(speed, 2);
    game_runtime::process_run(run_address, true);
    check(grow_calls_ == 1 && packet_calls_ == 0 &&
          read_value<float>(run_address, 0x4dc) == 1 &&
          read_value<std::size_t>(run_address, 0x670) == 4 &&
          read_value<std::size_t>(run_address, 0x680) == 1 &&
          state.get_message().find("queue_after_grow") != std::string::npos,
        "failed post-growth invariant never restores the old freed queue allocation or metadata");

    prepare_run(run_address);
    auto* limit_queue = allocate((1u << 20) * sizeof(game_runtime::game_packet));
    check(limit_queue != nullptr, "capacity-limit fixture allocated");
    if (limit_queue == nullptr) {
        return 1;
    }
    set_queue(run_address, 1u << 20, limit_queue, 0);
    state.apply(speed, 2);
    game_runtime::process_run(run_address, true);
    check(packet_calls_ == 1 && grow_calls_ == 0 &&
          read_value<std::size_t>(run_address, 0x680) == 1,
        "capacity exactly at the limit accepts a valid spare slot");
    prepare_run(run_address);
    set_queue(run_address, 1u << 20, limit_queue, 1u << 20);
    state.apply(speed, 2);
    game_runtime::process_run(run_address, true);
    check(packet_calls_ == 0 && grow_calls_ == 0 &&
          read_value<float>(run_address, 0x4dc) == 1 &&
          state.get_message().find("queue_capacity_limit") != std::string::npos,
        "a full capacity-limit queue refuses growth without calling native allocation");

    prepare_run(run_address);
    set_queue(run_address, 4, first_queue, 0);
    raise_packet_exception_ = true;
    state.apply(speed, 2);
    game_runtime::process_run(run_address, true);
    check(read_value<float>(run_address, 0x4dc) == 1 &&
          read_value<std::size_t>(run_address, 0x680) == 0 &&
          state.get_runtime().hooks_ready &&
          state.get_message().find("native_packet") != std::string::npos &&
          state.get_message().find("SEH=0xc0000005") != std::string::npos,
        "native access exceptions are contained and diagnosed without disabling the trainer");

    prepare_run(run_address);
    const auto saved_owner = game_runtime::original_tick_.load();
    game_runtime::original_tick_.store(&owner_frame);
    constexpr float original_delta = 1.0f / 64.0f;
    owner_calls_ = 0;
    owner_total_seconds_ = 0;
    game_runtime::tick_hook(run_address, original_delta);
    check(owner_calls_ == 1 && owner_total_seconds_ == original_delta &&
          read_value<float>(run_address, 0xce0) == original_delta && state.get_runtime().run_active,
        "real tick hook at 1x calls the native owner once, advances its clock and publishes an active run");

    auto accelerated_options = *state.get_automation();
    accelerated_options.time_scale = 16;
    check(state.set_automation(accelerated_options), "16x immutable automation snapshot accepted");
    const auto accelerated_snapshot = state.get_automation();
    const auto before_acceleration = read_value<float>(run_address, 0xce0);
    owner_calls_ = 0;
    owner_total_seconds_ = 0;
    game_runtime::tick_hook(run_address, original_delta);
    check(owner_calls_ == 16 && owner_total_seconds_ == 16 * original_delta &&
          read_value<float>(run_address, 0xce0) - before_acceleration == 16 * original_delta &&
          state.get_runtime().run_active &&
          state.get_runtime().run_address == reinterpret_cast<std::uintptr_t>(run_address),
        "real tick hook integrates 16 native owner calls and 16x elapsed time into the active state bridge");

    state.reset_all();
    const auto before_reset_frame = read_value<float>(run_address, 0xce0);
    owner_calls_ = 0;
    owner_total_seconds_ = 0;
    game_runtime::tick_hook(run_address, original_delta);
    check(state.get_automation()->time_scale == 1 && accelerated_snapshot->time_scale == 16 &&
          owner_calls_ == 1 && owner_total_seconds_ == original_delta &&
          read_value<float>(run_address, 0xce0) - before_reset_frame == original_delta &&
          state.get_runtime().run_active,
        "panic reset restores real owner ticks to 1x without mutating the retained settings snapshot");
    game_runtime::original_tick_.store(saved_owner);

    test_accelerated_publication(run_address);
    test_current_layout(run_address, first_queue, speed);

    game_runtime::features_enabled_.store(false);
    game_runtime::create_speed_packet_ = nullptr;
    game_runtime::grow_packets_ = nullptr;
    state.set_hooks_ready(false);
    for (auto* allocation : allocations_) {
        release(allocation);
    }
    std::printf("speed synchronization: %s (%d failures)\n", failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

} // namespace azs

int main() {
    return azs::speed_sync_tests::run();
}
