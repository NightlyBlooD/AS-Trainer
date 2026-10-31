#include "auto_choice_tests.hpp"
#include "auto_choice.hpp"
#include "trainer_state.hpp"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <string>

namespace azs {

void auto_choice_tests::check(bool condition, const char* description) {
    if (!condition) {
        ++failures_;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

void auto_choice_tests::prepare(void* run_address, void* choices) {
    auto_choice::reset();
    auto_choice::apply_choice_ = &apply_stub;
    std::memset(run_address, 0, 0x2000);
    std::memset(choices, 0, 0x1000);
    write<std::uint8_t>(run_address, 0x354, 11);
    write<std::uint32_t>(run_address, 0x530, 30);
    write<float>(run_address, 0x4c8, 100);
    write<std::uint8_t>(run_address, 0xda3, 2);
    write<std::uint8_t>(run_address, 0xda1, 0);
    write<std::uint32_t>(run_address, 0xd0c, 7);
    write<std::size_t>(run_address, 0x6a0, 4);
    write<void*>(run_address, 0x6a8, choices);
    write<std::size_t>(run_address, 0x6b0, 2);
    apply_calls_ = 0;
    selected_index_ = static_cast<std::size_t>(-1);
    selected_run_ = nullptr;
    test_reentry_ = false;
    reentry_blocked_ = false;
    raise_exception_ = false;
    reentry_settings_ = nullptr;
}

void auto_choice_tests::new_spell(void* cards, std::size_t index, const char* key, std::uint64_t rarity) {
    auto* card = static_cast<unsigned char*>(cards) + index * 0xa0;
    const char* title = "misleading_title";
    write<const char*>(card, 8, title);
    write<std::size_t>(card, 0x10, std::strlen(title));
    write<std::uint64_t>(card, 0x78, 0x8000000000000000);
    write<const char*>(card, 0x80, key);
    write<std::size_t>(card, 0x88, std::strlen(key));
    write<std::uint64_t>(card, 0x98, rarity);
}

void __fastcall auto_choice_tests::apply_stub(void* run_address, std::size_t index) {
    ++apply_calls_;
    selected_run_ = run_address;
    selected_index_ = index;
    if (test_reentry_) {
        const auto nested = auto_choice::process_run(run_address, *reentry_settings_, 999999);
        reentry_blocked_ = nested.status == auto_choice_status::already_attempted;
    }
    if (raise_exception_) {
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
    }
}

int auto_choice_tests::run() {
    const std::array<choice_candidate, 4> candidates = {{
        {"moonfire", 4, 0}, {"wrath", 0, 1}, {"wrath", 3, 2}, {"hurricane", 4, 3}
    }};
    const std::array<std::string, 2> preferred = {" WRATH\t", "moonfire"};
    check(auto_choice::select(candidates, preferred) == 2,
        "ordered stable key outranks rarity and same-key candidates prefer higher rarity");
    const std::array<std::string, 1> reversed = {"moonfire"};
    check(auto_choice::select(candidates, reversed) == 0, "an earlier preference wins across different keys");
    check(auto_choice::select(candidates, {}) == 0,
        "unmatched/empty priorities choose highest rarity and preserve first-card ties");
    const std::array<choice_candidate, 2> invalid_candidates = {{{"", 4, 0}, {"wrath", 5, 1}}};
    check(auto_choice::select(invalid_candidates, preferred) == static_cast<std::size_t>(-1),
        "empty keys and unsupported rarities cannot become a selection");
    check(!auto_choice::initialize(0), "unverified native application address is refused");

    auto* run_address = VirtualAlloc(nullptr, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    auto* cards = VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    auto* definitions = VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (run_address == nullptr || cards == nullptr || definitions == nullptr) {
        std::fprintf(stderr, "Cannot allocate auto-choice fixtures.\n");
        return 1;
    }
    automation_settings settings{};
    settings.choice_delay_ms = 250;
    check(auto_choice::process_run(reinterpret_cast<void*>(1), settings, 1000).status ==
          auto_choice_status::disabled, "default-off automation never dereferences a run");

    prepare(run_address, cards);
    new_spell(cards, 0, "wrath", 0);
    new_spell(cards, 1, "moonfire", 4);
    settings.auto_choose_spells = true;
    settings.spell_priorities = {"wrath", "moonfire"};
    check(auto_choice::process_run(run_address, settings, 1000).status == auto_choice_status::waiting,
        "a new offer starts its real-time delay without applying immediately");
    check(auto_choice::process_run(run_address, settings, 1249).status == auto_choice_status::waiting &&
          apply_calls_ == 0, "no apply happens before the configured delay");
    write<float>(run_address, 0xce0, 99);
    check(auto_choice::process_run(run_address, settings, 1250).status == auto_choice_status::chosen &&
          apply_calls_ == 1 && selected_index_ == 0 && selected_run_ == run_address,
        "after the delay the native run/index ABI uses payload keys rather than displayed titles");
    for (std::size_t substep = 0; substep < 16; ++substep) {
        check(auto_choice::process_run(run_address, settings, 1250).status ==
              auto_choice_status::already_attempted, "accelerated substeps cannot repeat a pending offer");
    }
    check(apply_calls_ == 1, "one offer receives exactly one native action despite a pending unchanged phase");

    write<std::uint32_t>(run_address, 0xd0c, 8);
    check(auto_choice::process_run(run_address, settings, 1251).status == auto_choice_status::waiting,
        "a new generation reusing the same cards starts a fresh delay");
    check(auto_choice::process_run(run_address, settings, 1501).status == auto_choice_status::chosen &&
          apply_calls_ == 2, "a queued next offer can be applied once after its own delay");

    prepare(run_address, cards);
    new_spell(cards, 0, "wrath", 1);
    new_spell(cards, 1, "moonfire", 4);
    write<std::uint8_t>(run_address, 0xda3, 1);
    check(auto_choice::process_run(run_address, settings, 2000).status == auto_choice_status::not_choosing &&
          apply_calls_ == 0, "native selection is never called during ordinary combat phase");
    write<std::uint8_t>(run_address, 0xda3, 2);
    write<std::uint8_t>(run_address, 0xda1, 1);
    check(auto_choice::process_run(run_address, settings, 2000).status == auto_choice_status::disabled,
        "jewelry offers respect their independent default-off toggle");
    settings.auto_choose_jewelry = true;
    settings.jewelry_priorities = {"moonfire", "wrath"};
    auto_choice::process_run(run_address, settings, 2000);
    check(auto_choice::process_run(run_address, settings, 2250).status == auto_choice_status::chosen &&
          selected_index_ == 1, "jewelry uses its separate ordered priority list");

    prepare(run_address, cards);
    auto* definition_bytes = static_cast<unsigned char*>(definitions);
    const char* spell_key = "hurricane";
    write<const char*>(definitions, 0, spell_key);
    write<std::size_t>(definitions, 8, std::strlen(spell_key));
    write<void*>(definitions, 0x100, definitions);
    write<void*>(run_address, 0x378, definition_bytes + 0x100);
    write<std::size_t>(run_address, 0x380, 1);
    write<std::uint64_t>(cards, 0x78, 1);
    write<void*>(cards, 0x80, definition_bytes + 0x200);
    write<std::size_t>(cards, 0x88, 1);
    write<std::size_t>(cards, 0x90, 0);
    write<std::uint64_t>(cards, 0x98, 3);
    new_spell(cards, 1, "wrath", 1);
    auto_choice::offer_snapshot snapshot{};
    const char* reason = nullptr;
    check(auto_choice::read_offer(run_address, snapshot, reason) &&
          std::string_view(snapshot.choices[0].key.data(), snapshot.choices[0].key_length) == "hurricane",
        "existing-spell upgrade resolves its ability index through the run's 0x58-stride definitions");

    const char* jewel_key = "chosen";
    write<const char*>(definitions, 0x300, jewel_key);
    write<std::size_t>(definitions, 0x308, std::strlen(jewel_key));
    write<std::uint64_t>(cards, 0x78, 0x8000000000000002);
    write<void*>(cards, 0x80, definition_bytes + 0x300);
    write<std::uint64_t>(cards, 0x98, 2);
    const char* amulet_key = "legendary:wrath";
    write<std::size_t>(definitions, 0x400, std::strlen(amulet_key));
    write<const char*>(definitions, 0x408, amulet_key);
    write<std::size_t>(definitions, 0x410, std::strlen(amulet_key));
    auto* second_card = static_cast<unsigned char*>(cards) + 0xa0;
    write<std::uint64_t>(second_card, 0x78, 0x8000000000000003);
    write<void*>(second_card, 0x80, definition_bytes + 0x400);
    write<std::uint64_t>(second_card, 0x98, 4);
    check(auto_choice::read_offer(run_address, snapshot, reason) &&
          std::string_view(snapshot.choices[0].key.data(), snapshot.choices[0].key_length) == "chosen" &&
          std::string_view(snapshot.choices[1].key.data(), snapshot.choices[1].key_length) == "legendary:wrath",
        "normal jewelry and owned-string legendary amulets use distinct verified key layouts");
    write<std::uint64_t>(second_card, 0x78, 0x8000000000000004);
    check(auto_choice::read_offer(run_address, snapshot, reason) &&
          std::string_view(snapshot.choices[1].key.data(), snapshot.choices[1].key_length) == "gold",
        "gold fallback has a deterministic synthetic stable key");
    write<std::uint64_t>(second_card, 0x78, 0x8000000000000008);
    check(!auto_choice::read_offer(run_address, snapshot, reason), "unknown variants fail closed before native actions");

    prepare(run_address, cards);
    new_spell(cards, 0, "wrath", 1);
    new_spell(cards, 1, "moonfire", 4);
    reentry_settings_ = &settings;
    test_reentry_ = true;
    auto_choice::process_run(run_address, settings, 3000);
    auto_choice::process_run(run_address, settings, 3250);
    check(reentry_blocked_ && apply_calls_ == 1,
        "the offer token is marked before invoking native code to block recursive reentry");

    prepare(run_address, cards);
    new_spell(cards, 0, "wrath", 1);
    new_spell(cards, 1, "moonfire", 4);
    raise_exception_ = true;
    auto_choice::process_run(run_address, settings, 4000);
    const auto failed = auto_choice::process_run(run_address, settings, 4250);
    check(failed.status == auto_choice_status::apply_failed && failed.captured_seh_code == 0xc0000005 &&
          auto_choice::process_run(run_address, settings, 5000).status == auto_choice_status::already_attempted &&
          apply_calls_ == 1, "native SEH is contained and its unchanged failed offer is not retried");

    prepare(run_address, cards);
    new_spell(cards, 0, "wrath", 1);
    new_spell(cards, 1, "moonfire", 4);
    settings.choice_delay_ms = 50;
    auto_choice::process_run(run_address, settings, 6000);
    check(auto_choice::process_run(run_address, settings, 6049).status == auto_choice_status::waiting &&
          auto_choice::process_run(run_address, settings, 6050).status == auto_choice_status::chosen,
        "the configured 50-ms lower bound is honored rather than silently replaced by 250 ms");
    prepare(run_address, cards);
    write<void*>(run_address, 0x6a8, reinterpret_cast<void*>(8));
    check(auto_choice::process_run(run_address, settings, 7000).status == auto_choice_status::invalid_offer &&
          apply_calls_ == 0, "invalid offer-vector memory is rejected without native calls");

    auto_choice::reset();
    auto_choice::apply_choice_ = nullptr;
    VirtualFree(run_address, 0, MEM_RELEASE);
    VirtualFree(cards, 0, MEM_RELEASE);
    VirtualFree(definitions, 0, MEM_RELEASE);
    std::printf("automatic offer selection: %s (%d failures)\n", failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

} // namespace azs

int main() {
    return azs::auto_choice_tests::run();
}
