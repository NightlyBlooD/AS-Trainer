#include "party_runtime_tests.hpp"
#include "party_runtime.hpp"

#include <Windows.h>

#include <cstdio>
#include <limits>
#include <utility>

namespace azs {

void party_runtime_tests::check(bool condition, const char* description) {
    if (!condition) {
        ++failures_;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

std::size_t party_runtime_tests::find_field(std::size_t offset) {
    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
        if (trainer_state::field_at(index).offset == offset) return index;
    }
    return trainer_state::field_count();
}

void party_runtime_tests::initialize_context(void* memory, std::uint64_t guid, bool participant) {
    std::memset(memory, 0, participant ? 0xa38 : 0x2000);
    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
        const auto& field = trainer_state::field_at(index);
        const auto offset = participant && field.offset == 0xd40 ? 0xa24 : field.offset;
        if (field.kind == value_kind::real) write_value(memory, offset, static_cast<float>(field.minimum));
    }
    write_value<std::uint8_t>(memory, 0x354, 7);
    write_value<std::uint32_t>(memory, 0x530, 30);
    write_value<std::uint64_t>(memory, 0x4b0, guid);
    write_value<float>(memory, 0x4c8, 400);
    write_value<float>(memory, 0x538, 400);
    write_value<float>(memory, 0x4cc, 1);
    write_value<float>(memory, 0x4dc, 1);
    write_value<float>(memory, 0x4d4, 0.1f);
    write_value<float>(memory, 0x5a8, 7);
    write_value<std::uint32_t>(memory, 0x544, 10);
    write_value<std::uint32_t>(memory, 0x598, 3);
    write_value<std::uint32_t>(memory, 0x59c, 2);
    write_value<std::uint32_t>(memory, participant ? 0xa24 : 0xd40, 25);
    write_value<std::uint8_t>(memory, participant ? 0xa34 : 0xda3, 1);
}

void party_runtime_tests::clear_trace() noexcept {
    events_ = {};
    event_count_ = 0;
    speed_calls_ = 0;
}

void party_runtime_tests::prepare(bool enabled) {
    auto& state = trainer_state::instance();
    state.set_hooks_ready(false);
    check(state.import_preset({}), "fixture removes all previously configured controls");
    initialize_context(run_, 100, false);
    initialize_context(participants_, 200, true);
    initialize_context(participants_ + 0xa38, 300, true);
    write_value<std::uint32_t>(run_, 0xda8, 3);
    write_value<std::uint8_t>(run_, 0xd9e, 1);
    write_value<std::uint8_t>(run_, 0xda4, 1);
    write_value<std::size_t>(run_, 0xc98, 0);
    write_value<std::size_t>(run_, 0xa68, 2);
    write_value(run_, 0xa70, participants_);
    write_value<std::size_t>(run_, 0xa78, 2);
    state.set_hooks_ready(true);
    check(state.process_run(run_, true), "fixture publishes the finite class-7 host run");
    automation_settings settings;
    settings.apply_to_party = enabled;
    check(state.set_automation(settings), "fixture installs the requested party scope");
    state.set_message("party fixture ready");
    flush_fault_guid_ = map_fault_guid_ = speed_failure_guid_ = 0;
    party_runtime::initialize({&swap_player, &flush_updates, &route_queues,
        &set_object_field, &synchronize_speed});
    party_runtime::observe_owner(run_);
    clear_trace();
}

void party_runtime_tests::record(char kind, void* run, std::uint16_t field, std::uint32_t value) noexcept {
    if (event_count_ == events_.size()) return;
    events_[event_count_++] = {kind, read_value<std::size_t>(run, 0xc98),
        read_value<std::uint64_t>(run, 0x4b0), field, value,
        read_value<std::size_t>(run, 0x680)};
}

void __fastcall party_runtime_tests::swap_player(void* run, void* participant) {
    record('S', run);
    std::array<unsigned char, 0x400> temporary{};
    auto* host = static_cast<unsigned char*>(run);
    auto* member = static_cast<unsigned char*>(participant);
    std::memcpy(temporary.data(), host + 0x1b0, temporary.size());
    std::memcpy(host + 0x1b0, member + 0x1b0, temporary.size());
    std::memcpy(member + 0x1b0, temporary.data(), temporary.size());
    const auto phase = read_value<std::uint8_t>(host, 0xda3);
    write_value(host, 0xda3, read_value<std::uint8_t>(member, 0xa34));
    write_value(member, 0xa34, phase);
    const auto cost = read_value<std::uint32_t>(host, 0xd40);
    write_value(host, 0xd40, read_value<std::uint32_t>(member, 0xa24));
    write_value(member, 0xa24, cost);
}

void __fastcall party_runtime_tests::flush_updates(void* run) {
    record('F', run);
    if (read_value<std::uint64_t>(run, 0x4b0) == flush_fault_guid_)
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
}

void __fastcall party_runtime_tests::route_queues(void* run) {
    record('R', run);
    write_value<std::size_t>(run, 0x680, 0);
}

void* __fastcall party_runtime_tests::set_object_field(void* map, std::uint16_t field, std::uint32_t value) {
    auto* run = static_cast<unsigned char*>(map) - 0xc78;
    record('M', run, field, value);
    if (read_value<std::uint64_t>(run, 0x4b0) == map_fault_guid_)
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
    return map;
}

bool party_runtime_tests::synchronize_speed(void* run) noexcept {
    ++speed_calls_;
    record('V', run);
    if (read_value<std::uint64_t>(run, 0x4b0) == speed_failure_guid_) return false;
    write_value<float>(run, 0x5a8, read_value<float>(run, 0x4dc) * 7);
    write_value<std::uint32_t>(run, 0x5a4, read_value<std::uint32_t>(run, 0x5a4) + 1);
    write_value<std::size_t>(run, 0x680, read_value<std::size_t>(run, 0x680) + 1);
    return true;
}

void party_runtime_tests::test_gates() {
    auto& state = trainer_state::instance();
    const auto damage = find_field(0x4cc);
    prepare(false);
    state.apply(damage, 12);
    party_runtime::process_owner(run_);
    check(event_count_ == 0 && read_value<float>(participants_, 0x4cc) == 1,
        "party scope disabled never calls native helpers or writes friends");

    prepare();
    write_value<std::uint32_t>(run_, 0xda8, 2);
    state.apply(damage, 12);
    party_runtime::observe_owner(run_);
    party_runtime::process_owner(run_);
    check(event_count_ == 0 && !state.get_runtime().party_host &&
          read_value<float>(participants_, 0x4cc) == 1,
        "guest network role cannot enter the host traversal");

    prepare();
    write_value<std::uint8_t>(run_, 0xda3, 4);
    state.apply(damage, 13);
    party_runtime::process_owner(run_);
    check(read_value<float>(participants_, 0x4cc) == 13 &&
          read_value<float>(participants_ + 0xa38, 0x4cc) == 13 &&
          read_value<std::uint8_t>(run_, 0xda3) == 4,
        "dead host does not block live friends while the global party phase is active");

    for (unsigned scenario = 0; scenario < 6; ++scenario) {
        prepare();
        if (scenario == 0) write_value<std::size_t>(run_, 0xa68, 65);
        if (scenario == 1) write_value<std::size_t>(run_, 0xa78, 5);
        if (scenario == 2) write_value<std::size_t>(run_, 0xa68, 1);
        if (scenario == 3) write_value(run_, 0xa70, reinterpret_cast<unsigned char*>(1));
        if (scenario == 4) write_value<std::uint8_t>(run_, 0xda4, 4);
        if (scenario == 5) write_value<std::size_t>(run_, 0xc98, 1);
        state.apply(damage, 12);
        party_runtime::process_owner(run_);
        check(event_count_ == 0 && read_value<float>(participants_, 0x4cc) == 1,
            "malformed bounded Vec, global phase or foreign context refuses all native calls");
    }
}

void party_runtime_tests::test_commands() {
    prepare();
    auto& state = trainer_state::instance();
    const auto damage = find_field(0x4cc);
    const auto gold = find_field(0x544);
    state.apply(damage, 7.5);
    state.apply(find_field(0xd40), 0);
    state.apply(find_field(0x2ca), 1);
    state.apply(find_field(0x4fc), 8);
    state.freeze(gold, true, 100);
    state.freeze(find_field(0x4c8), true, 1000.1);
    state.apply(find_field(0x538), 750.1);
    state.apply(find_field(0x4d4), 0.5);
    state.process_run(run_, true);
    const auto host_before = state.get_runtime();
    const auto ui_before = state.get_field(damage);
    write_value<std::size_t>(run_, 0x680, 7);
    party_runtime::process_owner(run_);
    for (std::size_t slot = 0; slot < 2; ++slot) {
        auto* member = participants_ + slot * 0xa38;
        check(read_value<float>(member, 0x4cc) == 7.5f &&
              read_value<std::uint32_t>(member, 0x544) == 100 &&
              read_value<std::uint32_t>(member, 0xa24) == 0 &&
              read_value<std::uint8_t>(member, 0x2ca) == 1 &&
              read_value<std::uint32_t>(member, 0x4fc) == 8 &&
              read_value<float>(member, 0x538) == 750.1f && read_value<float>(member, 0x4c8) == 1000.1f,
            "both friends receive selected real, integer, boolean and remapped chest commands");
        std::uint32_t observed_mask = 0;
        for (std::size_t index = 0; index < event_count_; ++index) {
            const auto& event = events_[index];
            if (event.kind != 'M' || event.guid != 200 + slot * 100 || event.index != slot + 1) continue;
            if (event.field == 0x16 && event.value == 751) observed_mask |= 1;
            if (event.field == 0x1c && event.value == 1001) observed_mask |= 2;
            if (event.field == 0x455 && event.value == 0x42480000) observed_mask |= 4;
        }
        check(observed_mask == 7, "HP/max HP use native ceilf while crit carries its exact native field ID and float bits");
    }
    std::size_t first_swap = event_count_;
    bool routed_host_queue = false;
    bool routed_first = false;
    bool routed_second = false;
    for (std::size_t index = 0; index < event_count_; ++index) {
        const auto& event = events_[index];
        if (event.kind == 'S' && first_swap == event_count_) first_swap = index;
        if (event.kind == 'R' && event.guid == 100 && event.index == 0 && event.queued == 7 && index < first_swap)
            routed_host_queue = true;
        if (event.kind == 'R' && event.guid == 200 && event.index == 1) routed_first = true;
        if (event.kind == 'R' && event.guid == 300 && event.index == 2) routed_second = true;
    }
    const auto host_after = state.get_runtime();
    check(routed_host_queue && routed_first && routed_second,
        "pending host packets route before the first swap and friend queues keep their own indices");
    check(read_value<std::uint64_t>(run_, 0x4b0) == 100 && read_value<std::size_t>(run_, 0xc98) == 0 &&
          read_value<float>(run_, 0x4cc) == 7.5f && host_after.run_address == host_before.run_address &&
          host_after.last_update_ms == host_before.last_update_ms &&
          state.get_field(damage).current_value == ui_before.current_value &&
          state.get_field(damage).pending == ui_before.pending && host_after.party_writes == 2,
        "friend passes restore host identity and preserve its UI heartbeat, values and acknowledgements");

    write_value<float>(participants_, 0x4cc, 3);
    write_value<float>(participants_ + 0xa38, 0x4cc, 4);
    write_value<std::uint32_t>(participants_, 0x544, 7);
    write_value<std::uint32_t>(participants_ + 0xa38, 0x544, 8);
    party_runtime::process_owner(run_);
    check(read_value<float>(participants_, 0x4cc) == 3 &&
          read_value<float>(participants_ + 0xa38, 0x4cc) == 4 &&
          read_value<std::uint32_t>(participants_, 0x544) == 100 &&
          read_value<std::uint32_t>(participants_ + 0xa38, 0x544) == 100 &&
          state.get_runtime().party_writes == 2,
        "acknowledged one-shots stay consumed while freezes reapply without inflating acknowledgement count");
    state.apply(damage, 9);
    party_runtime::process_owner(run_);
    check(read_value<float>(participants_, 0x4cc) == 9 &&
          read_value<float>(participants_ + 0xa38, 0x4cc) == 9 &&
          state.get_field(damage).pending && state.get_field(damage).current_value == 7.5 &&
          state.get_runtime().party_writes == 4,
        "a new command reaches both friends without consuming the host pending generation");
    write_value<std::uint64_t>(participants_, 0x4b0, 400);
    write_value<float>(participants_, 0x4cc, 1);
    party_runtime::process_owner(run_);
    check(read_value<float>(participants_, 0x4cc) == 9 && party_runtime::members_[0].guid == 400 &&
          state.get_runtime().party_writes == 5,
        "replacing a participant GUID creates a new ledger and applies the current command once");
}

void party_runtime_tests::test_reorder_and_departure() {
    prepare();
    auto& state = trainer_state::instance();
    const auto gold = find_field(0x544);
    state.apply(gold, 100);
    party_runtime::process_owner(run_);
    const auto first_ack = party_runtime::members_[0].commands.applied_ids;
    const auto second_ack = party_runtime::members_[1].commands.applied_ids;
    check(first_ack[gold] != 0 && second_ack[gold] == first_ack[gold] &&
          read_value<std::uint32_t>(participants_, 0x544) == 100 &&
          read_value<std::uint32_t>(participants_ + 0xa38, 0x544) == 100,
        "both GUIDs acknowledge the same one-shot gold generation before spending");
    write_value<std::uint32_t>(participants_, 0x544, 30);
    write_value<std::uint32_t>(participants_ + 0xa38, 0x544, 40);
    std::array<unsigned char, 0xa38> temporary{};
    std::memcpy(temporary.data(), participants_, temporary.size());
    std::memcpy(participants_, participants_ + 0xa38, temporary.size());
    std::memcpy(participants_ + 0xa38, temporary.data(), temporary.size());
    clear_trace();
    party_runtime::process_owner(run_);
    bool first_routed_at_two = false;
    bool second_routed_at_one = false;
    for (std::size_t index = 0; index < event_count_; ++index) {
        const auto& event = events_[index];
        if (event.kind == 'R' && event.guid == 200 && event.index == 2) first_routed_at_two = true;
        if (event.kind == 'R' && event.guid == 300 && event.index == 1) second_routed_at_one = true;
    }
    check(read_value<std::uint64_t>(participants_, 0x4b0) == 300 &&
          read_value<std::uint32_t>(participants_, 0x544) == 40 &&
          read_value<std::uint64_t>(participants_ + 0xa38, 0x4b0) == 200 &&
          read_value<std::uint32_t>(participants_ + 0xa38, 0x544) == 30 &&
          party_runtime::members_[0].guid == 200 && party_runtime::members_[0].commands.applied_ids == first_ack &&
          party_runtime::members_[1].guid == 300 && party_runtime::members_[1].commands.applied_ids == second_ack &&
          state.get_runtime().party_writes == 2 && first_routed_at_two && second_routed_at_one,
        "full record reorder preserves each GUID's spent gold and ledger while routing follows its new index");

    std::memcpy(participants_, participants_ + 0xa38, 0xa38);
    write_value<std::size_t>(run_, 0xa78, 1);
    party_runtime::process_owner(run_);
    bool departed_ledger_present = false;
    bool survivor_ack_preserved = false;
    for (const auto& member : party_runtime::members_) {
        if (member.guid == 300) departed_ledger_present = true;
        if (member.guid == 200 && member.commands.applied_ids == first_ack && member.last_index == 1)
            survivor_ack_preserved = true;
    }
    check(read_value<std::uint64_t>(participants_, 0x4b0) == 200 &&
          read_value<std::uint32_t>(participants_, 0x544) == 30 && survivor_ack_preserved &&
          !departed_ledger_present && state.get_runtime().party_members == 2 &&
          state.get_runtime().party_writes == 2,
        "departing first record and compacting the survivor retains its spent gold and acknowledgement");

    initialize_context(participants_ + 0xa38, 300, true);
    write_value<std::uint32_t>(participants_ + 0xa38, 0, 2);
    write_value<std::size_t>(run_, 0xa78, 2);
    clear_trace();
    party_runtime::process_owner(run_);
    bool vacant_was_processed = false;
    for (std::size_t index = 0; index < event_count_; ++index) {
        if (events_[index].guid == 300) vacant_was_processed = true;
    }
    check(state.get_runtime().party_host && state.get_runtime().party_members == 2 &&
          state.get_runtime().party_writes == 2 && !vacant_was_processed &&
          read_value<std::uint32_t>(participants_ + 0xa38, 0x544) == 10 &&
          read_value<std::uint64_t>(run_, 0x4b0) == 100 && read_value<std::size_t>(run_, 0xc98) == 0,
        "tag-two vacant slot is neither processed nor counted as a player despite its stale nonzero GUID");
}

void party_runtime_tests::test_phases_and_recalculation() {
    prepare();
    auto& state = trainer_state::instance();
    const auto damage = find_field(0x4cc);
    state.apply(damage, 12);
    write_value<std::uint8_t>(participants_, 0xa34, 4);
    party_runtime::process_owner(run_);
    check(read_value<float>(participants_, 0x4cc) == 1 &&
          party_runtime::members_[0].commands.applied_ids[damage] == 0 &&
          read_value<float>(participants_ + 0xa38, 0x4cc) == 12 &&
          state.get_runtime().party_writes == 1,
        "ended friend phase neither applies nor acknowledges the pending command");
    write_value<std::uint8_t>(participants_, 0xa34, 1);
    party_runtime::process_owner(run_);
    check(read_value<float>(participants_, 0x4cc) == 12 && state.get_runtime().party_writes == 2,
        "returning to phase one applies the generation deferred while the friend was ended");

    state.freeze(damage, true, 20);
    party_runtime::process_owner(run_);
    const auto acknowledgements = party_runtime::writes_;
    swap_player(run_, participants_);
    write_value<std::size_t>(run_, 0xc98, 1);
    write_value<float>(run_, 0x4cc, 1);
    check(party_runtime::is_member_context(run_), "temporary participant index is detected before host processing");
    party_runtime::process_recalculated(run_);
    check(read_value<float>(run_, 0x4cc) == 20 && party_runtime::writes_ == acknowledgements,
        "recalculation reapplies an existing friend freeze without acknowledging it twice");
    write_value<std::uint8_t>(run_, 0xda3, 4);
    write_value<float>(run_, 0x4cc, 2);
    party_runtime::process_recalculated(run_);
    check(read_value<float>(run_, 0x4cc) == 2,
        "recalculation hook preserves an invalid-phase friend without consuming commands");
    write_value<std::size_t>(run_, 0xc98, 0);
    swap_player(run_, participants_);
    check(!party_runtime::is_member_context(run_) && read_value<std::uint64_t>(run_, 0x4b0) == 100,
        "recalculation fixture restores the host identity and participant index");
}

void party_runtime_tests::test_failures() {
    auto& state = trainer_state::instance();
    const auto damage = find_field(0x4cc);
    const auto speed = find_field(0x4dc);
    for (const bool map_fault : {false, true}) {
        prepare();
        state.freeze(damage, true, 20);
        state.apply(find_field(0x538), 700);
        if (map_fault) map_fault_guid_ = 200;
        else flush_fault_guid_ = 200;
        party_runtime::process_owner(run_);
        check(read_value<std::uint64_t>(run_, 0x4b0) == 100 &&
              read_value<std::size_t>(run_, 0xc98) == 0 &&
              read_value<float>(run_, 0x538) == 400 && read_value<float>(run_, 0x4cc) == 1 &&
              party_runtime::failed_ && state.get_runtime().hooks_ready && state.get_field(damage).frozen,
            "native flush/map access exception restores host and leaves global hooks/freezes enabled");
        check(read_value<float>(participants_ + 0xa38, 0x4cc) == 1,
            "contained native failure stops later manual traversal before touching another friend");
        check(state.process_run(run_, true) && read_value<float>(run_, 0x4cc) == 20 &&
              state.get_runtime().run_active,
            "normal host memory bridge still applies controls after a party-native failure");
    }

    prepare();
    state.freeze(speed, true, 2);
    state.freeze(damage, true, 25);
    speed_failure_guid_ = 200;
    party_runtime::process_owner(run_);
    const auto rejected = party_runtime::members_[0].commands.rejected_speed_command;
    check(read_value<float>(participants_, 0x4dc) == 1 &&
          read_value<float>(participants_ + 0xa38, 0x4dc) == 2 &&
          read_value<float>(participants_, 0x4cc) == 25 &&
          read_value<float>(participants_ + 0xa38, 0x4cc) == 25 && rejected != 0 &&
          party_runtime::members_[1].commands.rejected_speed_command == 0 &&
          !party_runtime::failed_ && state.get_field(speed).frozen && state.get_field(damage).frozen &&
          state.get_runtime().hooks_ready && speed_calls_ == 2,
        "one rejected speed rolls back only its friend while other fields, peers and global freezes continue");
    write_value<float>(participants_, 0x4dc, 1.25f);
    clear_trace();
    party_runtime::process_owner(run_);
    check(read_value<float>(participants_, 0x4dc) == 1.25f && speed_calls_ == 1 &&
          state.get_field(speed).frozen,
        "rejected friend speed generation is suppressed without releasing the shared speed freeze");
    speed_failure_guid_ = 0;
    state.freeze(speed, true, 3);
    party_runtime::process_owner(run_);
    check(read_value<float>(participants_, 0x4dc) == 3 &&
          read_value<float>(participants_ + 0xa38, 0x4dc) == 3 &&
          party_runtime::members_[0].commands.applied_ids[speed] != rejected &&
          read_value<std::uint64_t>(run_, 0x4b0) == 100 && read_value<std::size_t>(run_, 0xc98) == 0,
        "a newer speed generation is retried independently and both participant contexts restore cleanly");
}

int party_runtime_tests::run() {
    run_ = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x2000,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    participants_ = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x2000,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (run_ == nullptr || participants_ == nullptr) {
        if (run_) VirtualFree(run_, 0, MEM_RELEASE);
        if (participants_) VirtualFree(participants_, 0, MEM_RELEASE);
        std::fprintf(stderr, "Cannot allocate the party runtime fixture.\n");
        return 1;
    }
    test_gates();
    test_commands();
    test_reorder_and_departure();
    test_phases_and_recalculation();
    test_failures();
    trainer_state::instance().set_hooks_ready(false);
    party_runtime::initialize({});
    VirtualFree(participants_, 0, MEM_RELEASE);
    VirtualFree(run_, 0, MEM_RELEASE);
    participants_ = run_ = nullptr;
    std::printf("isolated party runtime: %s (%d failures)\n", failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

} // namespace azs

int main() {
    return azs::party_runtime_tests::run();
}
