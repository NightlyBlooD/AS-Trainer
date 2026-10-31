#include "legendary_loot_tests.hpp"
#include "legendary_loot.hpp"

#include <Windows.h>

#include <cstdio>

namespace azs {
namespace {

constexpr std::array<const char*, 12> zone_keys = {
    "elwynn", "duskwood", "wsg", "ungoro", "ab", "feralas",
    "arena", "sm", "av", "brm", "aq", "naxx"
};
constexpr std::array<std::uint8_t, 9> supported_classes = {1, 2, 3, 4, 5, 7, 8, 9, 11};

} // namespace

void legendary_loot_tests::check(bool condition, const char* description) {
    if (!condition) {
        ++failures_;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

void* legendary_loot_tests::allocate(std::size_t size) {
    for (auto& allocation : allocations_) {
        if (allocation == nullptr) {
            allocation = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            return allocation;
        }
    }
    return nullptr;
}

void legendary_loot_tests::release(void* address) {
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

void legendary_loot_tests::prepare_tables() {
    zones_.fill(0);
    bosses_.fill(0);
    for (std::size_t zone_index = 0; zone_index < zone_keys.size(); ++zone_index) {
        auto* zone = zones_.data() + zone_index * 0xf0;
        write_value(zone, 8, zone_keys[zone_index]);
        write_value(zone, 0x10, std::strlen(zone_keys[zone_index]));
        write_value(zone, 0x88, boss_at(zone_index, 0));
        write_value<std::size_t>(zone, 0x90, 5);
        auto* final_boss = bosses_.data() + (zone_index * 5 + 4) * 0x68;
        final_boss[0x64] = 1;
    }
    legendary_loot::zone_descriptors_ = zones_.data();
    legendary_loot::grow_loot_ = &grow_queue;
    legendary_loot::original_boss_loot_.store(&original_boss);
    legendary_loot::is_enabled_ = &enabled;
    legendary_loot::report_ = &report;
}

void legendary_loot_tests::prepare_run(void* run_address, std::size_t zone_index, std::uint8_t class_id) {
    release(read_value<void*>(run_address, 0x6d8));
    std::memset(run_address, 0, 0x2000);
    write_value(run_address, 0x354, class_id);
    write_value(run_address, 0x2d0, zone_keys[zone_index]);
    write_value(run_address, 0x2d8, std::strlen(zone_keys[zone_index]));
    write_value<void*>(run_address, 0x6d8, reinterpret_cast<void*>(4));
    write_value<std::uint32_t>(run_address, 0x83c, 54);
    enabled_ = true;
    growth_mode_ = growth_mode::normal;
    original_calls_ = 0;
    grow_calls_ = 0;
    message_calls_ = 0;
    observed_run_ = nullptr;
    observed_boss_ = nullptr;
    legendary_loot::grow_loot_ = &grow_queue;
}

const void* legendary_loot_tests::boss_at(std::size_t zone_index, std::size_t boss_index) {
    return bosses_.data() + (zone_index * 5 + boss_index) * 0x68;
}

bool legendary_loot_tests::enabled() noexcept {
    return enabled_;
}

void legendary_loot_tests::report(const char* message) noexcept {
    if (message != nullptr && *message != '\0') {
        ++message_calls_;
    }
}

void __fastcall legendary_loot_tests::original_boss(void* run_address, const void* boss_address) {
    ++original_calls_;
    observed_run_ = run_address;
    observed_boss_ = boss_address;
    const auto previous = read_value<std::uint32_t>(run_address, 0x83c);
    write_value(run_address, 0x83c, previous + 1);
}

void __fastcall legendary_loot_tests::grow_queue(void* queue_address) {
    ++grow_calls_;
    if (growth_mode_ == growth_mode::unchanged) {
        return;
    }
    if (growth_mode_ == growth_mode::access_exception) {
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
        return;
    }
    const auto capacity = read_value<std::size_t>(queue_address, 0);
    auto* old_data = read_value<void*>(queue_address, 8);
    const auto count = read_value<std::size_t>(queue_address, 16);
    const auto new_capacity = capacity < 4 ? 4 : capacity * 2;
    auto* data = allocate(new_capacity * sizeof(legendary_loot::loot_record));
    check(data != nullptr, "native loot grow stub allocates replacement storage");
    if (data == nullptr) {
        return;
    }
    if (count != 0) {
        std::memcpy(data, old_data, count * sizeof(legendary_loot::loot_record));
    }
    release(old_data);
    write_value(queue_address, 0, new_capacity);
    write_value(queue_address, 8, data);
    if (growth_mode_ == growth_mode::corrupt_count) {
        write_value(queue_address, 16, count + 1);
    }
}

int legendary_loot_tests::run() {
    auto* run_address = allocate(0x2000);
    if (run_address == nullptr) {
        std::fprintf(stderr, "Cannot allocate legendary-loot fixture.\n");
        return 1;
    }
    prepare_tables();
    std::array<std::uint32_t, 10> ids{};
    for (auto class_id : supported_classes) {
        const auto count = legendary_loot::legendary_ids(class_id, ids);
        const std::size_t expected = class_id == 4 || class_id == 7 ? 10 : 9;
        check(count == expected, "each supported class includes eight armor pieces and all weapon variants");
        for (std::size_t index = 0; index < count; ++index) {
            check(ids[index] == 100040u + class_id * 1000u + static_cast<std::uint32_t>(index) * 100u,
                "legendary IDs have current class, slot and rarity 4");
        }
    }
    check(legendary_loot::legendary_ids(0, ids) == 0 && legendary_loot::legendary_ids(6, ids) == 0 &&
          legendary_loot::legendary_ids(10, ids) == 0, "unsupported class IDs cannot create gear");

    for (std::size_t zone_index = 0; zone_index < zone_keys.size(); ++zone_index) {
        prepare_run(run_address, zone_index, 8);
        legendary_loot::boss_loot_hook(run_address, boss_at(zone_index, 4));
        check(original_calls_ == 1 && observed_run_ == run_address && observed_boss_ == boss_at(zone_index, 4),
            "every zone final boss preserves one original call with exact RCX/RDX arguments");
        check(read_value<std::size_t>(run_address, 0x6e0) == 9 && message_calls_ == 0,
            "any current zone final boss queues the complete Mage legendary set");
        const auto* records = read_value<const legendary_loot::loot_record*>(run_address, 0x6d8);
        for (std::size_t index = 0; index < 9; ++index) {
            check(records[index].item_id == 108040u + static_cast<std::uint32_t>(index) * 100u &&
                  records[index].boss_roll_index == 55 && records[index].award_marks == 0,
                "native 12-byte records use post-original roll index and zero added direct mark awards");
        }
    }

    prepare_run(run_address, 0, 4);
    enabled_ = false;
    legendary_loot::boss_loot_hook(run_address, boss_at(0, 4));
    check(original_calls_ == 1 && grow_calls_ == 0 && read_value<std::size_t>(run_address, 0x6e0) == 0,
        "disabled automation leaves native processing unchanged");
    prepare_run(run_address, 0, 4);
    legendary_loot::boss_loot_hook(run_address, boss_at(0, 0));
    legendary_loot::boss_loot_hook(run_address, boss_at(1, 4));
    const std::array<unsigned char, 0x68> foreign_final = [] {
        std::array<unsigned char, 0x68> value{};
        value[0x64] = 1;
        return value;
    }();
    legendary_loot::boss_loot_hook(run_address, foreign_final.data());
    check(original_calls_ == 3 && grow_calls_ == 0 && read_value<std::size_t>(run_address, 0x6e0) == 0,
        "nonfinal, other-zone and forged final descriptors cannot grant a set");

    prepare_run(run_address, 11, 7);
    auto* existing = allocate(4 * sizeof(legendary_loot::loot_record));
    if (existing == nullptr) {
        return 1;
    }
    const legendary_loot::loot_record weapon{107840, 53, 8.0f};
    const legendary_loot::loot_record native_other{101000, 12, 3.0f};
    write_value(existing, 0, weapon);
    write_value(existing, 12, native_other);
    write_value<std::size_t>(run_address, 0x6d0, 4);
    write_value(run_address, 0x6d8, existing);
    write_value<std::size_t>(run_address, 0x6e0, 2);
    legendary_loot::boss_loot_hook(run_address, boss_at(11, 4));
    auto* relocated = read_value<const legendary_loot::loot_record*>(run_address, 0x6d8);
    check(read_value<std::size_t>(run_address, 0x6e0) == 11 && grow_calls_ == 2 &&
          relocated[0].item_id == weapon.item_id && relocated[0].award_marks == 8 &&
          relocated[1].item_id == native_other.item_id && relocated[1].award_marks == 3,
        "growth preserves native prefix and deduplicates an already queued legendary weapon");
    check(legendary_loot::append_legendary_set(run_address, 7) &&
          read_value<std::size_t>(run_address, 0x6e0) == 11,
        "an already complete queued set is idempotent");

    prepare_run(run_address, 10, 11);
    write_value<std::size_t>(run_address, 0x6d0, 1);
    write_value<void*>(run_address, 0x6d8, reinterpret_cast<void*>(1));
    write_value<std::size_t>(run_address, 0x6e0, 2);
    legendary_loot::boss_loot_hook(run_address, boss_at(10, 4));
    check(original_calls_ == 1 && grow_calls_ == 0 && message_calls_ == 1 &&
          read_value<std::size_t>(run_address, 0x6e0) == 2,
        "malformed queue failure is isolated and never calls the allocator or changes its length");

    for (auto mode : {growth_mode::unchanged, growth_mode::corrupt_count, growth_mode::access_exception}) {
        prepare_run(run_address, 0, 1);
        growth_mode_ = mode;
        legendary_loot::boss_loot_hook(run_address, boss_at(0, 4));
        const auto expected_count = mode == growth_mode::corrupt_count ? 1u : 0u;
        check(original_calls_ == 1 && message_calls_ == 1 &&
              read_value<std::size_t>(run_address, 0x6e0) == expected_count,
            "growth invariant/SEH failure never publishes a partial added set or rolls back native metadata");
    }

    prepare_run(run_address, 0, 6);
    legendary_loot::boss_loot_hook(run_address, boss_at(0, 4));
    check(original_calls_ == 1 && grow_calls_ == 0 && message_calls_ == 0,
        "unsupported current class does not enter extension or block original loot");
    check(!legendary_loot::initialize(0, &enabled, &report),
        "invalid module initialization fails before installing any real hook");

    legendary_loot::original_boss_loot_.store(nullptr);
    legendary_loot::grow_loot_ = nullptr;
    legendary_loot::zone_descriptors_ = nullptr;
    legendary_loot::is_enabled_ = nullptr;
    legendary_loot::report_ = nullptr;
    for (auto* allocation : allocations_) {
        release(allocation);
    }
    std::printf("legendary final-boss loot: %s (%d failures)\n", failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

} // namespace azs

int main() {
    return azs::legendary_loot_tests::run();
}
