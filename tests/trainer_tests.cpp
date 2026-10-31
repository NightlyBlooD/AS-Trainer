#include "trainer_tests.hpp"
#include "trainer_state.hpp"
#include "simulation_clock.hpp"

#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace azs {

void trainer_test_suite::check(bool condition, const char* description) {
    if (!condition) {
        ++failures_;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

std::size_t trainer_test_suite::find_field(std::size_t offset) {
    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
        if (trainer_state::field_at(index).offset == offset) {
            return index;
        }
    }
    return trainer_state::field_count();
}

void trainer_test_suite::initialize_run(void* memory) {
    auto* bytes = static_cast<std::byte*>(memory);
    std::memset(bytes, 0, 0x2000);
    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
        const auto& descriptor = trainer_state::field_at(index);
        if (descriptor.kind == value_kind::real) {
            const auto initial = static_cast<float>(descriptor.minimum);
            std::memcpy(bytes + descriptor.offset, &initial, sizeof(initial));
        }
    }
    *reinterpret_cast<float*>(bytes + 0x4c8) = 100;
    *reinterpret_cast<float*>(bytes + 0x538) = 100;
    *reinterpret_cast<float*>(bytes + 0x4cc) = 1;
    *reinterpret_cast<float*>(bytes + 0x4dc) = 1;
    *reinterpret_cast<float*>(bytes + 0x4f0) = 1;
    *reinterpret_cast<std::uint8_t*>(bytes + 0x354) = 11;
    *reinterpret_cast<std::uint32_t*>(bytes + 0x530) = 30;
    *reinterpret_cast<std::uint8_t*>(bytes + 0xda3) = 1;
}

void trainer_test_suite::test_members(void* host_run, void* first_member, void* second_member) {
    auto& state = trainer_state::instance();
    auto* host = static_cast<std::byte*>(host_run);
    auto* first = static_cast<std::byte*>(first_member);
    auto* second = static_cast<std::byte*>(second_member);
    const auto damage = find_field(0x4cc);
    const auto gold = find_field(0x544);
    const auto speed = find_field(0x4dc);
    check(state.import_preset({}), "party fixture starts without previously configured fields");
    initialize_run(host);
    initialize_run(first);
    initialize_run(second);
    *reinterpret_cast<std::uint64_t*>(first + 0x4b0) = 0x101;
    *reinterpret_cast<std::uint64_t*>(second + 0x4b0) = 0x202;
    check(state.process_run(host, true), "party fixture publishes only its host run");
    check(!state.get_automation()->apply_to_party, "party scope defaults to disabled");
    auto party_options = *state.get_automation();
    party_options.apply_to_party = true;
    check(state.set_automation(party_options), "party scope is retained in immutable automation");
    state.set_party_status(true, 2, 7);
    const auto party_status = state.get_runtime();
    check(party_status.party_host && party_status.party_members == 2 && party_status.party_writes == 7,
          "party diagnostics publish hosting, member count and successful writes");

    member_commands first_commands{};
    member_commands second_commands{};
    check(state.apply(damage, 12.5), "party one-shot is queued once for the shared desired value");
    const auto host_snapshot = state.get_runtime();
    const auto host_damage = state.get_field(damage);
    check(state.process_member(first, first_commands) && first_commands.applied_ids[damage] != 0 &&
          second_commands.applied_ids[damage] == 0 && state.get_field(damage).pending,
          "first member acknowledges only its own copy of a one-shot command");
    check(state.process_member(second, second_commands) &&
          *reinterpret_cast<float*>(first + 0x4cc) == 12.5f &&
          *reinterpret_cast<float*>(second + 0x4cc) == 12.5f &&
          *reinterpret_cast<std::uint64_t*>(first + 0x4b0) != *reinterpret_cast<std::uint64_t*>(second + 0x4b0),
          "one shared one-shot applies independently to two distinct member GUIDs");
    const auto after_members = state.get_runtime();
    check(*reinterpret_cast<float*>(host + 0x4cc) == 1 &&
          state.get_field(damage).current_value == host_damage.current_value &&
          state.get_field(damage).pending && after_members.run_address == host_snapshot.run_address &&
          after_members.level == host_snapshot.level &&
          after_members.last_update_ms == host_snapshot.last_update_ms,
          "members never consume host acknowledgements or replace its memory, UI snapshot and heartbeat");
    *reinterpret_cast<float*>(first + 0x4cc) = 3;
    *reinterpret_cast<float*>(second + 0x4cc) = 4;
    state.process_member(first, first_commands);
    state.process_member(second, second_commands);
    check(*reinterpret_cast<float*>(first + 0x4cc) == 3 && *reinterpret_cast<float*>(second + 0x4cc) == 4,
          "member one-shots are not reapplied after their own acknowledgement");
    state.freeze(damage, true, 20);
    state.process_member(first, first_commands);
    state.process_member(second, second_commands);
    *reinterpret_cast<float*>(first + 0x4cc) = 3;
    *reinterpret_cast<float*>(second + 0x4cc) = 4;
    state.process_member(first, first_commands);
    state.process_member(second, second_commands);
    check(*reinterpret_cast<float*>(first + 0x4cc) == 20 && *reinterpret_cast<float*>(second + 0x4cc) == 20,
          "a member freeze survives native changes independently for both players");

    state.apply(gold, 123);
    for (const auto phase : std::array<std::uint8_t, 3>{0, 4, 5}) {
        *reinterpret_cast<std::uint8_t*>(first + 0xda3) = phase;
        *reinterpret_cast<std::uint8_t*>(second + 0xda3) = phase;
        const auto first_ack = first_commands.applied_ids[gold];
        const auto second_ack = second_commands.applied_ids[gold];
        speed_update rejected_phase{true, 99, 99, 99};
        check(!state.process_member(first, first_commands, &rejected_phase) &&
              !state.process_member(second, second_commands) && !rejected_phase.written &&
              first_commands.applied_ids[gold] == first_ack && second_commands.applied_ids[gold] == second_ack &&
              *reinterpret_cast<std::uint32_t*>(first + 0x544) == 0 &&
              *reinterpret_cast<std::uint32_t*>(second + 0x544) == 0,
              "ready/dead/victory members cannot consume commands or request speed sync");
    }
    *reinterpret_cast<std::uint8_t*>(first + 0xda3) = 2;
    *reinterpret_cast<std::uint8_t*>(second + 0xda3) = 3;
    check(state.process_member(first, first_commands) && state.process_member(second, second_commands) &&
          *reinterpret_cast<std::uint32_t*>(first + 0x544) == 123 &&
          *reinterpret_cast<std::uint32_t*>(second + 0x544) == 123,
          "choosing and paused members accept the pending command after rejected end phases");

    state.reset_all();
    first_commands = {};
    second_commands = {};
    *reinterpret_cast<float*>(first + 0x4cc) = 2;
    *reinterpret_cast<float*>(second + 0x4cc) = 3;
    state.process_member(first, first_commands);
    state.process_member(second, second_commands);
    check(*reinterpret_cast<float*>(first + 0x4cc) == 2 && *reinterpret_cast<float*>(second + 0x4cc) == 3 &&
          !state.get_automation()->apply_to_party,
          "panic reset stops party writes even with fresh acknowledgement storage");

    state.freeze(damage, true, 25);
    state.freeze(speed, true, 2);
    speed_update member_speed{};
    state.process_member(first, first_commands, &member_speed);
    state.process_member(second, second_commands);
    const auto shared_speed = state.get_field(speed);
    check(member_speed.written && member_speed.previous_value == 1 && member_speed.requested_value == 2 &&
          state.reject_member_speed_update(first, member_speed) &&
          *reinterpret_cast<float*>(first + 0x4dc) == 1 && *reinterpret_cast<float*>(second + 0x4dc) == 2 &&
          state.get_field(speed).frozen == shared_speed.frozen &&
          state.get_field(speed).pending == shared_speed.pending && state.get_field(damage).frozen,
          "member speed rollback changes only that member and preserves global commands/freezes");
    first_commands.rejected_speed_command = member_speed.command_id;
    *reinterpret_cast<float*>(first + 0x4cc) = 0;
    state.process_member(first, first_commands, &member_speed);
    check(!member_speed.written && *reinterpret_cast<float*>(first + 0x4dc) == 1 &&
          *reinterpret_cast<float*>(first + 0x4cc) == 25,
          "a rejected member speed generation is skipped while unrelated freezes continue");
    state.apply(speed, 3);
    state.process_member(first, first_commands, &member_speed);
    check(member_speed.written && member_speed.command_id != first_commands.rejected_speed_command &&
          *reinterpret_cast<float*>(first + 0x4dc) == 3 && state.get_field(speed).frozen,
          "a newer speed command is not blocked by an older member rejection");
    *reinterpret_cast<float*>(first + 0x4dc) = 4;
    check(!state.reject_member_speed_update(first, member_speed) &&
          *reinterpret_cast<float*>(first + 0x4dc) == 4,
          "member rollback cannot overwrite a speed value changed after the rejected write");

    state.reset_all();
    state.apply(damage, 8);
    state.set_apply_on_start(damage, false);
    *reinterpret_cast<float*>(host + 0xce0) = 100;
    state.process_run(host, true);
    initialize_run(host);
    check(state.process_run(host, true), "host clock rollback adopts the next party run");
    initialize_run(first);
    initialize_run(second);
    first_commands = {};
    second_commands = {};
    state.process_member(first, first_commands);
    state.process_member(second, second_commands);
    check(*reinterpret_cast<float*>(first + 0x4cc) == 1 && *reinterpret_cast<float*>(second + 0x4cc) == 1,
          "a configured value with Start disabled cannot leak into new-run member acknowledgements");
    trainer_preset member_preset;
    member_preset.fields.push_back({"damage", 11, false, false});
    check(state.import_preset(member_preset) && state.process_member(first, first_commands) &&
          *reinterpret_cast<float*>(first + 0x4cc) == 11,
          "an imported current-run command is active for members even without a next-run flag");
    state.reset_all();
    state.set_party_status(false, 0, 0);
}

int trainer_test_suite::run() {
    auto* first = static_cast<std::byte*>(VirtualAlloc(nullptr, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    auto* second = static_cast<std::byte*>(VirtualAlloc(nullptr, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    auto* executable = VirtualAlloc(nullptr, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    auto* first_member = VirtualAlloc(nullptr, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    auto* second_member = VirtualAlloc(nullptr, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!first || !second || !executable || !first_member || !second_member) {
        std::fprintf(stderr, "Cannot allocate test fixtures.\n");
        return 1;
    }
    initialize_run(first);
    initialize_run(second);
    initialize_run(executable);
    auto& state = trainer_state::instance();
    const auto damage = find_field(0x4cc);
    const auto gold = find_field(0x544);
    const auto skip = find_field(0x2ca);
    const auto cost = find_field(0xd40);
    const auto rerolls = find_field(0x59c);
    const auto cooldown = find_field(0x4d0);
    const auto speed = find_field(0x4dc);

    check(trainer_state::field_count() >= 25, "all requested and basic character fields are exposed");
    check(!state.process_run(first, true), "hooks disabled means no game access");
    state.set_hooks_ready(true);
    check(!state.process_run(nullptr, true), "null pointer rejected");
    check(!state.process_run(reinterpret_cast<void*>(1), true), "invalid pointer rejected without crashing");
    check(!state.process_run(executable, true), "executable memory rejected even if it looks like run data");
    check(state.process_run(first, true), "valid private writable run accepted");
    check(state.get_runtime().run_active && state.get_runtime().level == 30, "fresh runtime snapshot published");
    check(!state.apply(damage, std::numeric_limits<double>::infinity()), "infinity never enters game memory");
    check(!state.apply(damage, std::numeric_limits<double>::quiet_NaN()), "NaN never enters game memory");
    check(!state.apply(gold, -1) && !state.apply(gold, 1.5), "uint32 fields reject negative/fractional values");
    check(!state.apply(trainer_state::field_count(), 1), "invalid descriptor index rejected");
    check(state.apply(damage, 12.5), "one-shot float command accepted");
    check(*reinterpret_cast<float*>(first + 0x4cc) == 1, "UI command does not write from UI thread");
    state.process_run(first, true);
    check(*reinterpret_cast<float*>(first + 0x4cc) == 12.5f, "game update applies exact float field");
    *reinterpret_cast<float*>(first + 0x4cc) = 3;
    state.process_run(first, true);
    check(*reinterpret_cast<float*>(first + 0x4cc) == 3, "one-shot command does not become a freeze");
    check(state.freeze(damage, true, 20), "freeze accepted");
    state.process_run(first, true);
    *reinterpret_cast<float*>(first + 0x4cc) = 4;
    state.process_run(first, true);
    check(*reinterpret_cast<float*>(first + 0x4cc) == 20, "freeze survives simulated native recalculation");
    state.freeze(damage, false, 20);
    *reinterpret_cast<float*>(first + 0x4cc) = 5;
    state.process_run(first, true);
    check(*reinterpret_cast<float*>(first + 0x4cc) == 5, "release stops repeated writes");
    first[0x2c9] = std::byte{0x7a};
    first[0x2cb] = std::byte{0x6b};
    state.apply(skip, 1);
    state.apply(cost, 0);
    state.apply(gold, 1234567);
    state.apply(rerolls, 777);
    state.apply(cooldown, 0.25);
    state.process_run(first, true);
    check(first[0x2ca] == std::byte{1} && first[0x2c9] == std::byte{0x7a} && first[0x2cb] == std::byte{0x6b},
          "boolean skip writes one byte and preserves neighboring flags");
    check(*reinterpret_cast<std::uint32_t*>(first + 0xd40) == 0 &&
          *reinterpret_cast<std::uint32_t*>(first + 0x544) == 1234567 &&
          *reinterpret_cast<std::uint32_t*>(first + 0x59c) == 777, "currency/cost/reroll integer writes are exact");
    check(*reinterpret_cast<float*>(first + 0x4d0) == 0.25f, "cooldown factor is independent from spell area");
    speed_update speed_result{true, 99, 99, 99};
    state.process_run(first, true, &speed_result);
    check(!speed_result.written, "ordinary updates do not request native speed synchronization");
    state.freeze(speed, true, 2);
    state.freeze(gold, true, 50000);
    state.process_run(first, true, &speed_result);
    check(speed_result.written && speed_result.previous_value == 1 && speed_result.requested_value == 2,
          "speed update captures the original value and exact requested generation");
    check(state.reject_speed_update(first, speed_result), "failed speed synchronization restores prior speed");
    check(*reinterpret_cast<float*>(first + 0x4dc) == 1 && !state.get_field(speed).frozen &&
          state.get_field(gold).frozen && state.get_runtime().hooks_ready,
          "speed rejection preserves the rest of the trainer and unrelated freezes");
    *reinterpret_cast<std::uint32_t*>(first + 0x544) = 0;
    state.process_run(first, true, &speed_result);
    check(*reinterpret_cast<std::uint32_t*>(first + 0x544) == 50000 && !speed_result.written,
          "other freezes continue after an isolated speed rejection");
    state.freeze(speed, true, 2);
    state.process_run(first, true, &speed_result);
    state.apply(speed, 3);
    check(state.reject_speed_update(first, speed_result) && state.get_field(speed).pending &&
          state.get_field(speed).frozen, "rejection of an old speed command preserves a newer UI command");
    state.process_run(first, true, &speed_result);
    check(*reinterpret_cast<float*>(first + 0x4dc) == 3,
          "a newer queued speed value is still applied after rollback");
    state.freeze(speed, false, 3);
    state.freeze(damage, true, 30);
    state.process_run(first, true);
    check(!state.process_run(second, false) && state.get_field(damage).frozen &&
          state.get_runtime().run_address == reinterpret_cast<std::uintptr_t>(first),
          "constructor-like stat rebuild cannot replace the authoritative tick object");
    state.process_run(second, true);
    check(state.get_field(damage).frozen && *reinterpret_cast<float*>(second + 0x4cc) == 30,
          "a replacement run reapplies explicitly configured persistent freezes");
    state.freeze(damage, true, 30);
    state.reset_all();
    *reinterpret_cast<float*>(second + 0x4cc) = 1;
    state.process_run(second, true);
    check(!state.get_field(damage).frozen && *reinterpret_cast<float*>(second + 0x4cc) == 1,
          "emergency release cancels pending writes and freezes");
    *reinterpret_cast<std::uint8_t*>(second + 0x354) = 255;
    check(!state.process_run(second, true), "wrong run identity rejected");
    *reinterpret_cast<std::uint8_t*>(second + 0x354) = 8;
    check(state.process_run(second, true), "Mage class ID 8 is recognized");
    state.set_hooks_ready(false);
    check(state.import_preset({}), "empty preset resets configured fields and automation");
    check(state.apply(damage, 6) && state.freeze(cooldown, true, 0.2),
          "preconfiguration is accepted before a live run exists");
    check(!state.get_field(damage).available && state.get_field(damage).configured &&
          state.get_field(damage).apply_on_start && state.get_field(cooldown).frozen,
          "offline snapshots preserve desired values and next-run flags");
    initialize_run(first);
    state.set_hooks_ready(true);
    state.process_run(first, true);
    check(*reinterpret_cast<float*>(first + 0x4cc) == 6 &&
          std::fabs(*reinterpret_cast<float*>(first + 0x4d0) - 0.2f) < 0.00001f,
          "the first authoritative frame applies offline configuration");
    state.set_apply_on_start(damage, false);
    initialize_run(second);
    state.process_run(second, true);
    check(*reinterpret_cast<float*>(second + 0x4cc) == 1 && state.get_field(cooldown).frozen,
          "disabling Start affects the next run without discarding other presets");
    state.apply(damage, 8);
    state.set_apply_on_start(damage, true);
    *reinterpret_cast<float*>(second + 0xce0) = 100;
    state.process_run(second, true);
    *reinterpret_cast<float*>(second + 0xce0) = 0;
    *reinterpret_cast<float*>(second + 0x4cc) = 1;
    check(!state.process_run(second, false), "a reused run address is not adopted through a rebuild callback");
    state.process_run(second, true);
    check(*reinterpret_cast<float*>(second + 0x4cc) == 8 && !state.get_field(damage).frozen,
          "an unfrozen Start value is reapplied when a new run reuses the same address");
    *reinterpret_cast<std::uint8_t*>(second + 0xda3) = 5;
    check(!state.process_run(second, true) && !state.get_runtime().run_active,
          "a victory screen is not treated as an active run");
    state.set_apply_on_start(damage, false);
    check(state.apply(damage, 9) && state.get_field(damage).apply_on_start,
          "editing on an end screen prepares the next run");
    *reinterpret_cast<std::uint8_t*>(second + 0xda3) = 0;
    *reinterpret_cast<float*>(second + 0x4cc) = 1;
    check(!state.process_run(second, true) && *reinterpret_cast<float*>(second + 0x4cc) == 1 &&
          state.get_field(damage).pending, "ready phase cannot consume a next-run value");
    *reinterpret_cast<std::uint8_t*>(second + 0xda3) = 1;
    state.process_run(second, true);
    check(*reinterpret_cast<float*>(second + 0x4cc) == 9,
          "prepared end-screen value applies once the next game begins");
    auto saved_preset = state.export_preset();
    auto broken_preset = saved_preset;
    broken_preset.fields.push_back({"unknown_field", 1, true, true});
    check(!state.import_preset(broken_preset) && state.export_preset().fields.size() == saved_preset.fields.size(),
          "unknown preset fields are rejected without partial state mutation");
    broken_preset = saved_preset;
    broken_preset.fields.push_back(saved_preset.fields.front());
    check(!state.import_preset(broken_preset), "duplicate preset fields are rejected atomically");
    test_members(first, first_member, second_member);
    auto rules = *state.get_automation();
    rules.time_scale = 16;
    rules.auto_choose_spells = true;
    rules.legendary_final_boss = true;
    rules.spell_priorities = {"wrath", "hurricane"};
    check(state.set_automation(rules) && state.get_automation()->time_scale == 16,
          "automation and 16x clock settings are stored without a game pointer");
    rules.time_scale = 17;
    check(!state.set_automation(rules) && state.get_automation()->time_scale == 16,
          "invalid acceleration is rejected without replacing accepted rules");
    rules.time_scale = 16;
    for (const auto* key : {"not a key", "wrath;other", "\xd0\xbc\xd0\xbe\xd0\xbb\xd0\xbd\xd0\xb8\xd1\x8f"}) {
        rules.spell_priorities = {key};
        check(!state.set_automation(rules) && state.get_automation()->spell_priorities.front() == "wrath",
              "UI rules reject keys that cannot round trip through an INI config");
    }
    const auto normal_steps = simulation_clock::split_frame(0.016f, 1);
    const auto accelerated = simulation_clock::split_frame(0.016f, 16);
    const auto fractional = simulation_clock::split_frame(0.016f, 2.5);
    check(normal_steps.count == 1 && normal_steps.seconds_per_step == 0.016f,
          "normal clock preserves the original native interval");
    check(accelerated.count == 16 && accelerated.seconds_per_step == 0.016f &&
          std::fabs(accelerated.count * accelerated.seconds_per_step - 0.256f) < 0.000001f,
          "16x splits time into native-sized steps with exact accelerated total");
    check(fractional.count == 3 && fractional.seconds_per_step <= 0.016f &&
          std::fabs(fractional.count * fractional.seconds_per_step - 0.04f) < 0.000001f,
          "fractional clock also preserves simulation time without large intervals");
    check(simulation_clock::split_frame(std::numeric_limits<float>::quiet_NaN(), 16).seconds_per_step == 0,
          "invalid input time cannot reach the native simulation");
    state.reset_all();
    check(state.get_automation()->time_scale == 1 && !state.get_automation()->auto_choose_spells &&
          !state.get_automation()->legendary_final_boss && !state.get_field(cooldown).frozen,
          "panic reset stops freezes, auto-choice, loot and acceleration together");
    state.set_hooks_ready(false);
    VirtualFree(first, 0, MEM_RELEASE);
    VirtualFree(second, 0, MEM_RELEASE);
    VirtualFree(executable, 0, MEM_RELEASE);
    VirtualFree(first_member, 0, MEM_RELEASE);
    VirtualFree(second_member, 0, MEM_RELEASE);
    std::printf("trainer memory bridge: %s (%d failures)\n", failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

} // namespace azs

int main() {
    return azs::trainer_test_suite::run();
}
