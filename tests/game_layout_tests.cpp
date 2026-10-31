#include "game_layout_tests.hpp"
#include "game_layout.hpp"
#include "trainer_state.hpp"
#include "party_runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <string_view>

namespace azs {

int game_layout_tests::run() noexcept {
    try {
        check_profiles();
        check_current_scalars();
        check_current_party();
    } catch (const std::exception& exception) {
        std::fprintf(stderr, "Layout test exception: %s\n", exception.what());
        ++failures_;
    } catch (...) {
        check(false, "unexpected layout test exception");
    }
    trainer_state::instance().set_hooks_ready(false);
    game_layout::select(layout_id::legacy);
    std::printf("Verified executable layouts: %s (%d failures)\n", failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

void game_layout_tests::check(bool condition, const char* description) noexcept {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures_;
    }
}

void game_layout_tests::check_profiles() {
    check(game_layout::current_id() == layout_id::legacy, "legacy remains the default for existing suites");
    check(game_layout::run_offset(0x4cc) == 0x4cc && game_layout::run_offset(0x123) == 0x123,
          "legacy retains identity offsets");
    check(game_layout::participant_stride() == 0xa38 && game_layout::run_size() == 0xdac
              && game_layout::scalar_size() == 0xda4, "legacy validation spans and stride are unchanged");
    check(game_layout::select(layout_id::current), "verified current layout can be selected before hooks");
    check(game_layout::run_offset(0x83c) == 0x8bc && game_layout::run_offset(0xd0c) == 0xe2c,
          "boss-roll index and offer generation use individually proved nonuniform mappings");
    check(game_layout::run_offset(0xa68) == 0xb00 && game_layout::run_offset(0xa70) == 0xb08
              && game_layout::run_offset(0xa78) == 0xb10, "new participant Vec metadata is selected");
    check(game_layout::run_offset(0x670) == 0x678 && game_layout::run_offset(0x678) == 0x680
              && game_layout::run_offset(0x680) == 0x688, "native 32-byte packet queue uses its new fields");
    check(game_layout::run_offset(0xc78) == 0xd98 && game_layout::run_offset(0xc98) == 0xdb8
              && game_layout::run_offset(0xda8) + 0x10 == 0xed8, "dirty map, routing index and app role are mapped");
    check(game_layout::member_offset(0x4b0) == 0x4b8 && game_layout::member_offset(0xa24) == 0xabc
              && game_layout::member_offset(0xa34) == 0xacc, "new participant GUID, chest cost and phase are mapped");
    check(game_layout::participant_stride() == 0xad0 && game_layout::run_size() == 0xecc
              && game_layout::scalar_size() == 0xec4, "new validation spans include all guarded reads");
    check(game_layout::run_offset(0x123) == game_layout::invalid_offset
              && game_layout::member_offset(0x123) == game_layout::invalid_offset,
          "unknown current offsets fail closed instead of guessing a delta");
    check(!game_layout::select(static_cast<layout_id>(99))
              && game_layout::current_id() == layout_id::current, "invalid profile selection preserves the prior profile");
    constexpr std::size_t expected[] = {0x4d4, 0x4e4, 0x4d8, 0x4e8, 0x4f8, 0x4fc,
        0x54c, 0xe60, 0x5a4, 0x2d2, 0x540, 0x4d0, 0x4dc, 0x4e0, 0x4ec, 0x4f0,
        0x4f4, 0x500, 0x504, 0x508, 0x50c, 0x510, 0x514, 0x518, 0x5a0};
    check(trainer_state::field_count() == std::size(expected), "all 25 fields have explicit current descriptors");
    for (std::size_t index = 0; index < std::size(expected); ++index) {
        check(trainer_state::field_at(index).offset == expected[index], "immutable field descriptor uses its new offset");
    }
}

void game_layout_tests::seed_run(unsigned char* bytes, std::uint64_t guid, std::uint8_t class_id) noexcept {
    store(bytes, 0x35c, class_id);
    store(bytes, 0x538, std::uint32_t{10});
    store(bytes, 0x4b8, guid);
    store(bytes, 0x4d0, 1000.0f);
    store(bytes, 0x540, 800.0f);
    store(bytes, 0x4dc, 0.25f);
    store(bytes, 0xec3, std::uint8_t{1});
    store(bytes, 0xe00, 10.0f);
}

void game_layout_tests::check_current_scalars() {
    auto& state = trainer_state::instance();
    check(state.import_preset({}), "fresh current scalar settings accepted");
    state.set_hooks_ready(true);
    alignas(8) std::array<unsigned char, 0x1000> bytes{};
    seed_run(bytes.data(), 101, 11);
    store(bytes.data(), 0x4cc, 123.0f);
    std::array<double, 25> desired{};
    for (std::size_t index = 0; index < desired.size(); ++index) {
        const auto& field = trainer_state::field_at(index);
        desired[index] = field.kind == value_kind::boolean ? 1.0
            : std::clamp(field.kind == value_kind::real ? 2.5 : 2.0, field.minimum, field.maximum);
        check(state.apply(index, desired[index]), "each current field command passes its unchanged numeric bounds");
    }
    check(state.process_run(bytes.data(), true), "current scalar fixture is adopted and commands applied");
    for (std::size_t index = 0; index < desired.size(); ++index) {
        const auto& field = trainer_state::field_at(index);
        const double actual = field.kind == value_kind::real ? read<float>(bytes.data(), field.offset)
            : (field.kind == value_kind::boolean ? read<std::uint8_t>(bytes.data(), field.offset)
                                                 : read<std::uint32_t>(bytes.data(), field.offset));
        check(std::fabs(actual - desired[index]) < 0.00001, "each command writes its verified current data bytes");
    }
    check(read<float>(bytes.data(), 0x4cc) == 123.0f, "old damage location is not used by the current profile");
    check(state.get_runtime().level == 10, "current level identity is published from new metadata");

    alignas(8) std::array<unsigned char, 0x1000> legacy_only{};
    store(legacy_only.data(), 0x354, std::uint8_t{11});
    store(legacy_only.data(), 0x530, std::uint32_t{10});
    store(legacy_only.data(), 0x4c8, 1000.0f);
    store(legacy_only.data(), 0xda3, std::uint8_t{1});
    check(!state.process_run(legacy_only.data(), true), "a legacy-shaped object is rejected under the current profile");
    state.reset_all();
}

void game_layout_tests::check_current_party() {
    auto& state = trainer_state::instance();
    check(state.import_preset({}), "fresh current party settings accepted");
    alignas(8) std::array<unsigned char, 0x1000> bytes{};
    alignas(8) std::array<unsigned char, 0xad0> participant{};
    party_run_ = bytes.data();
    seed_run(bytes.data(), 101, 11);
    store(participant.data(), 0x35c, std::uint8_t{8});
    store(participant.data(), 0x538, std::uint32_t{10});
    store(participant.data(), 0x4b8, std::uint64_t{202});
    store(participant.data(), 0x4d0, 1000.0f);
    store(participant.data(), 0x540, 800.0f);
    store(participant.data(), 0x4dc, 0.25f);
    store(participant.data(), 0xacc, std::uint8_t{1});
    store(bytes.data(), 0xec8, std::uint32_t{3});
    store(bytes.data(), 0xebe, std::uint8_t{1});
    store(bytes.data(), 0xec4, std::uint8_t{1});
    store(bytes.data(), 0xdb8, std::size_t{0});
    store(bytes.data(), 0xb00, std::size_t{1});
    store(bytes.data(), 0xb08, participant.data());
    store(bytes.data(), 0xb10, std::size_t{1});
    store(participant.data(), 0x4d4, 5.0f);
    check(state.freeze(0, true, 42), "current party damage command configured");
    check(state.process_run(bytes.data(), true), "current host fixture adopted");
    automation_settings settings{};
    settings.apply_to_party = true;
    check(state.set_automation(settings), "current party scope enabled");
    party_runtime::initialize({swap_player, flush_updates, route_queues, set_object_field, nullptr});
    party_runtime::observe_owner(bytes.data());
    party_runtime::process_owner(bytes.data());
    check(read<float>(participant.data(), 0x4d4) == 42, "friend receives the current-layout scalar command");
    check(read<std::uint8_t>(bytes.data(), 0x35c) == 11
              && read<std::uint8_t>(participant.data(), 0x35c) == 8,
          "native swaps restore both independent hero classes");
    check(read<std::uint64_t>(bytes.data(), 0x4b8) == 101
              && read<std::uint64_t>(participant.data(), 0x4b8) == 202
              && read<std::size_t>(bytes.data(), 0xdb8) == 0, "GUIDs and host routing index are restored");
    check(swap_calls_ == 2 && route_calls_ >= 2 && field_calls_ >= 3,
          "current traversal performs a paired swap and native field/queue synchronization");
    check(state.get_runtime().party_host && state.get_runtime().party_members == 2,
          "current participant stride and GUID fields produce the correct host snapshot");
    settings.apply_to_party = false;
    state.set_automation(settings);
    store(participant.data(), 0x4d4, 7.0f);
    party_runtime::process_owner(bytes.data());
    check(read<float>(participant.data(), 0x4d4) == 7, "disabled party scope leaves the friend's current data unchanged");
    state.reset_all();
    party_run_ = nullptr;
}

void game_layout_tests::swap_bytes(unsigned char* run, unsigned char* member, std::size_t run_offset,
                                   std::size_t member_offset, std::size_t size) noexcept {
    std::array<unsigned char, 8> temporary{};
    std::memcpy(temporary.data(), run + run_offset, size);
    std::memcpy(run + run_offset, member + member_offset, size);
    std::memcpy(member + member_offset, temporary.data(), size);
}

void __fastcall game_layout_tests::swap_player(void* run_address, void* participant) noexcept {
    auto* run = static_cast<unsigned char*>(run_address);
    auto* member = static_cast<unsigned char*>(participant);
    ++swap_calls_;
    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
        const auto& field = trainer_state::field_at(index);
        const auto member_offset = std::string_view(field.key) == "chest_cost" ? std::size_t{0xabc} : field.offset;
        swap_bytes(run, member, field.offset, member_offset, field.kind == value_kind::boolean ? 1 : 4);
    }
    swap_bytes(run, member, 0x35c, 0x35c, 1);
    swap_bytes(run, member, 0x538, 0x538, 4);
    swap_bytes(run, member, 0x4b8, 0x4b8, 8);
    swap_bytes(run, member, 0xec3, 0xacc, 1);
}

void __fastcall game_layout_tests::flush_updates(void* run_address) noexcept {
    check(run_address == party_run_, "native flush receives the owned full run");
}

void __fastcall game_layout_tests::route_queues(void* run_address) noexcept {
    ++route_calls_;
    check(run_address == party_run_ && read<std::size_t>(party_run_, 0xdb8) <= 1,
          "native routing receives a current-layout host/member index");
}

void* __fastcall game_layout_tests::set_object_field(void* fields, std::uint16_t key,
                                                     std::uint32_t value) noexcept {
    (void)value;
    ++field_calls_;
    check(fields == party_run_ + 0xd98 && (key == 0x16 || key == 0x1c || key == 0x455),
          "native field synchronization targets the current dirty map and unchanged protocol field IDs");
    return fields;
}

} // namespace azs

int main() {
    return azs::game_layout_tests::run();
}
