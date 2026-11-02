#include "god_mode_tests.hpp"
#include "god_mode.hpp"
#include "god_mode_events.hpp"
#include "game_layout.hpp"
#include "pattern_scanner.hpp"
#include "signature_profile.hpp"
#include "native_addresses.hpp"
#include "god_mode_bindings.hpp"

#include <Windows.h>
#include <MinHook.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <limits>
#include <thread>

namespace azs {

void god_mode_tests::check(bool condition, const char* description) {
    if (!condition) {
        ++failures_;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

void god_mode_tests::prepare(bool modern, std::size_t modern_stride) {
    if (old_buffer_protected_) {
        DWORD previous{};
        VirtualProtect(enemies_, 0x4000, PAGE_READWRITE, &previous);
        old_buffer_protected_ = false;
    }
    std::memset(run_, 0, 0x2000);
    std::memset(enemies_, 0, 0x4000);
    std::memset(relocated_, 0, 0x4000);
    game_layout::select(layout_id::current);
    god_mode::profile_ = modern ? god_mode::native_profile{0x640, modern_stride, 0x236}
        : god_mode::native_profile{0x5b8, 0x1f8, 0x1b6};
    god_mode::damage_ = native_damage;
    const auto offset = god_mode::profile_.enemies_offset;
    write_value<std::size_t>(run_, offset, 8);
    write_value(run_, offset + 8, enemies_);
    write_value<std::size_t>(run_, offset + 16, 4);
    write_value<std::uint8_t>(run_, game_layout::run_offset(0xda3), 1);
    write_value<std::uint32_t>(run_, game_layout::run_offset(0xda8), 2);
    for (std::size_t index = 0; index < 4; ++index) {
        auto* enemy = enemies_ + index * god_mode::profile_.enemy_stride;
        write_value<std::uint64_t>(enemy, 0xf0, 100 + index);
        write_value<float>(enemy, 0x178, index == 1 ? 400.0f : 120.0f);
        write_value<float>(enemy, 0x17c, index == 1 ? 400.0f : 120.0f);
        write_value<float>(enemy, 0x1a0, index == 0 ? 1000000.0f : -1000000.0f);
        write_value<std::int32_t>(enemy, 0xd0, index == 1 ? 3 : -1);
    }
    write_value<std::uint32_t>(enemies_ + 2 * god_mode::profile_.enemy_stride, 0xb0, 1);
    write_value<float>(enemies_ + 3 * god_mode::profile_.enemy_stride, 0x178, 0);
    for (std::size_t index = 0; index < 16; ++index) {
        original_source_[index] = static_cast<unsigned char>(index + 1);
        original_target_[index] = static_cast<unsigned char>(index + 33);
    }
    std::memcpy(run_ + 0x20, original_source_.data(), 16);
    std::memcpy(run_ + 0x40, original_target_.data(), 16);
    run_[god_mode::profile_.source_offset] = 7;
    write_value<std::size_t>(run_, game_layout::run_offset(0x380), 7);
    god_mode_events::set_active(false);
    god_mode_events::original_dispatcher_.store(native_dispatch, std::memory_order_release);
    call_count_ = 0;
    credited_damage_ = 0;
    dispatch_count_ = 0;
    relocate_ = false;
    throw_native_ = false;
    finish_run_ = false;
}

void __fastcall god_mode_tests::native_damage(void* run_address, std::uint64_t guid, float damage,
    std::uint32_t spell, std::uint8_t school, bool critical, bool suppress_packet) {
    ++call_count_;
    check(run_address == run_ && spell == 0 && school == 0 && !critical && suppress_packet,
        "native damage has the verified owner ABI and no spell/crit/combat-log request");
    check(run_[god_mode::profile_.source_offset] == 3 && read_value<std::uint64_t>(run_, 0x20) == 0 &&
        read_value<std::uint64_t>(run_, 0x40) == 0 &&
        read_value<std::size_t>(run_, game_layout::run_offset(0x380)) == 0 &&
        std::memcmp(run_ + 0x28, original_source_.data() + 8, 8) == 0 &&
        std::memcmp(run_ + 0x48, original_target_.data() + 8, 8) == 0,
        "source context blocks on-hit and on-kill callbacks without corrupting option payloads");
    const bool previous_events = god_mode_events::set_active(true);
    check(previous_events, "native talent dispatcher suppression is active only inside God damage");
    god_mode_events::set_active(previous_events);
    god_mode_events::dispatcher_hook(run_address, run_address, 0xabc);
    check(dispatch_count_ == 0, "native damage cannot trigger a talent spell dispatcher");
    if (throw_native_) RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
    auto* enemy = god_mode::find_enemy(run_address, guid);
    check(enemy != nullptr, "native damage resolves the current target GUID");
    if (enemy == nullptr) return;
    const float health = read_value<float>(enemy, 0x178);
    float applied = damage * 0.25f;
    if (read_value<std::int32_t>(enemy, 0xd0) == 3 && read_value<std::uint64_t>(enemy, 0xe0) == 0)
        applied = std::min(applied, read_value<float>(enemy, 0x17c) / 5.0f);
    credited_damage_ += std::min(applied, std::max(health, 0.0f));
    write_value<float>(enemy, 0x178, health - applied);
    if (health - applied <= 0) write_value<std::uint32_t>(enemy, 0xb0, 1);
    if (finish_run_) write_value<std::uint8_t>(run_, game_layout::run_offset(0xda3), 5);
    if (relocate_) {
        std::memcpy(relocated_, enemies_, 0x4000);
        write_value(run_, god_mode::profile_.enemies_offset + 8, relocated_);
        DWORD previous{};
        check(VirtualProtect(enemies_, 0x4000, PAGE_NOACCESS, &previous) != 0,
            "fixture protects the old allocation after native vector relocation");
        old_buffer_protected_ = true;
        relocate_ = false;
    }
}

void __fastcall god_mode_tests::native_dispatch(void* run_address, const void* event_address,
    std::uintptr_t source_context) {
    ++dispatch_count_;
    check(run_address == run_ && event_address == run_ && source_context == 0xabc,
        "unsuppressed talent dispatch preserves its exact native arguments");
}

void god_mode_tests::test_gates() {
    prepare();
    for (const auto role : {0u, 1u, 3u, 4u}) {
        write_value(run_, game_layout::run_offset(0xda8), role);
        check(god_mode::process_run(run_) && call_count_ == 0, "only the verified solo app role admits God damage");
    }
    write_value<std::uint32_t>(run_, game_layout::run_offset(0xda8), 2);
    write_value<std::uint8_t>(run_, game_layout::run_offset(0xd9e), 1);
    check(god_mode::process_run(run_) && call_count_ == 0, "coop is blocked even with a solo role alias");
    write_value<std::uint8_t>(run_, game_layout::run_offset(0xd9e), 0);
    write_value<std::size_t>(run_, game_layout::run_offset(0xc98), 1);
    check(god_mode::process_run(run_) && call_count_ == 0, "a participant context cannot receive host God damage");
    write_value<std::size_t>(run_, game_layout::run_offset(0xc98), 0);
    for (const auto phase : std::array<std::uint8_t, 5>{0, 2, 3, 5, 8}) {
        write_value(run_, game_layout::run_offset(0xda3), phase);
        check(god_mode::process_run(run_) && call_count_ == 0, "nonplaying phases never issue damage");
    }
    write_value<std::uint8_t>(run_, game_layout::run_offset(0xda3), 1);
    write_value<std::size_t>(run_, 0x640, 65537);
    check(!god_mode::process_run(run_) && call_count_ == 0, "oversized enemy capacity fails before native calls");
    write_value<std::size_t>(run_, 0x640, 2);
    check(!god_mode::process_run(run_) && call_count_ == 0, "length above capacity fails before native calls");
    check(!god_mode::process_run(nullptr), "an invalid run cannot reach native damage");
    prepare();
    write_value<std::size_t>(run_, 0x640, 0);
    write_value<std::size_t>(run_, 0x650, 0);
    write_value<std::uintptr_t>(run_, 0x648, 8);
    check(god_mode::process_run(run_) && call_count_ == 0, "an empty native dangling-pointer Vec is valid");
    god_mode::damage_ = nullptr;
    check(god_mode::process_run(nullptr), "an unsupported optional binding remains a harmless no-op");
}

void god_mode_tests::test_damage() {
    prepare();
    finish_run_ = true;
    check(god_mode::process_run(run_) && call_count_ == 1 && credited_damage_ == 120,
        "a native victory transition stops further damage without reporting a feature failure");
    for (const std::size_t stride : {0x1f8u, 0x200u, 0x208u}) {
        prepare(stride != 0x1f8, stride);
        check(god_mode::process_run(run_) && call_count_ == 2 && credited_damage_ == 520,
            "all distant living enemies die and exactly their original HP is credited");
        check(read_value<float>(enemies_ + god_mode::profile_.enemy_stride, 0x17c) == 400,
            "special-enemy maximum health is restored after lifting its per-hit cap");
        check(god_mode::process_run(run_) && call_count_ == 2 && credited_damage_ == 520,
            "dead and zero-HP enemies do not generate repeated damage or duplicate credit");
    }
    prepare();
    write_value<float>(enemies_, 0x178, std::numeric_limits<float>::quiet_NaN());
    check(!god_mode::process_run(run_) && call_count_ == 0, "nonfinite enemy health fails closed");
    write_value<float>(enemies_, 0x178, std::numeric_limits<float>::max());
    check(!god_mode::process_run(run_) && call_count_ == 0, "damage input overflow fails before native calls");
    for (const float health : {0.013f, 0.1f, 0.3f, 1.1f, 3.7f, 17.3f, 123.456f, 0.00001f}) {
        prepare();
        write_value<std::size_t>(run_, 0x650, 1);
        write_value<float>(enemies_, 0x178, health);
        write_value<float>(enemies_, 0x17c, health);
        write_value<std::int32_t>(enemies_, 0xd0, 3);
        check(god_mode::process_run(run_) && call_count_ == 1 && credited_damage_ == health &&
            read_value<float>(enemies_, 0x178) <= 0 && read_value<float>(enemies_, 0x17c) == health,
            "fractional HP survives cap rounding with exact credited damage and restored maximum HP");
    }
}

void god_mode_tests::test_restoration() {
    prepare(true, 0x208);
    relocate_ = true;
    check(god_mode::process_run(run_) && call_count_ == 2 && credited_damage_ == 520,
        "native Vec relocation is revalidated before further enemy access");
    check(read_value<float>(relocated_ + god_mode::profile_.enemy_stride, 0x17c) == 400,
        "temporary maximum HP restoration follows GUID into the new allocation");
    check(std::memcmp(run_ + 0x20, original_source_.data(), 16) == 0 &&
        std::memcmp(run_ + 0x40, original_target_.data(), 16) == 0 &&
        run_[god_mode::profile_.source_offset] == 7 &&
        read_value<std::size_t>(run_, game_layout::run_offset(0x380)) == 7 &&
        !god_mode_events::set_active(false),
        "all native source context bytes and dispatcher scope are restored after success");
    prepare(true, 0x208);
    write_value<std::uint32_t>(enemies_, 0xb0, 1);
    relocate_ = true;
    check(god_mode::process_run(run_) && call_count_ == 1 && credited_damage_ == 400 &&
        read_value<float>(relocated_ + god_mode::profile_.enemy_stride, 0x17c) == 400,
        "a capped target relocated during its own damage call restores maximum HP through its GUID");
    prepare(true, 0x208);
    write_value<std::uint32_t>(enemies_, 0xb0, 1);
    throw_native_ = true;
    check(!god_mode::process_run(run_) && call_count_ == 1,
        "a native structured exception reports isolated God damage failure");
    check(read_value<float>(enemies_ + god_mode::profile_.enemy_stride, 0x17c) == 400 &&
        std::memcmp(run_ + 0x20, original_source_.data(), 16) == 0 &&
        std::memcmp(run_ + 0x40, original_target_.data(), 16) == 0 &&
        run_[god_mode::profile_.source_offset] == 7 &&
        read_value<std::size_t>(run_, game_layout::run_offset(0x380)) == 7 &&
        !god_mode_events::set_active(false),
        "native exception restoration preserves both option payloads, source byte and health cap");
}

void god_mode_tests::test_events() {
    prepare();
    god_mode_events::dispatcher_hook(run_, run_, 0xabc);
    check(dispatch_count_ == 1, "ordinary talent events execute exactly once outside God damage");
    dispatch_count_ = 0;
    check(!god_mode_events::set_active(true), "outer God scope starts from an unsuppressed thread");
    god_mode_events::dispatcher_hook(run_, run_, 0xabc);
    const bool nested_previous = god_mode_events::set_active(true);
    check(nested_previous, "a nested God scope preserves prior suppression");
    god_mode_events::set_active(nested_previous);
    god_mode_events::dispatcher_hook(run_, run_, 0xabc);
    check(dispatch_count_ == 0, "restoring a nested scope keeps the outer spell suppression active");
    std::thread other_thread([] {
        god_mode_events::dispatcher_hook(run_, run_, 0xabc);
    });
    other_thread.join();
    check(dispatch_count_ == 1, "another thread retains ordinary event dispatch during a local God scope");
    check(god_mode_events::set_active(false), "outer God scope restores the original suppression flag");
    god_mode_events::dispatcher_hook(run_, run_, 0xabc);
    check(dispatch_count_ == 2, "ordinary event dispatch resumes when the God scope ends");
}

int god_mode_tests::run() {
    run_ = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    enemies_ = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x4000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    relocated_ = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x4000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (run_ == nullptr || enemies_ == nullptr || relocated_ == nullptr) return 1;
    test_gates();
    test_damage();
    test_restoration();
    test_events();
    god_mode::damage_ = nullptr;
    VirtualFree(run_, 0, MEM_RELEASE);
    VirtualFree(enemies_, 0, MEM_RELEASE);
    VirtualFree(relocated_, 0, MEM_RELEASE);
    std::printf("God mode: %s (%d failures)\n", failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

int god_mode_tests::verify_image(const wchar_t* executable_path) {
    pattern_scanner scanner;
    if (!scanner.load_file(executable_path)) return 1;
    const auto hash = signature_profiles::sha256(scanner.bytes());
    god_mode_bindings::candidate pattern_binding;
    std::string pattern_error;
    const bool pattern_verified = god_mode_bindings::resolve_file(scanner, pattern_binding, pattern_error);
    if (!pattern_verified) {
        std::printf("God mode pattern proof: FAILED [%s] error=%s\n", hash.c_str(), pattern_error.c_str());
        return 1;
    }
    std::array<unsigned char, 32> digest{};
    for (std::size_t index = 0; index < digest.size(); ++index) {
        unsigned value{};
        const auto parsed = std::from_chars(hash.data() + index * 2, hash.data() + index * 2 + 2, value, 16);
        if (parsed.ec != std::errc{}) return 1;
        digest[index] = static_cast<unsigned char>(value);
    }
    const HANDLE file = CreateFileW(executable_path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return 1;
    const HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    CloseHandle(file);
    if (mapping == nullptr) return 1;
    void* mapped_image = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(mapping);
    if (mapped_image == nullptr) return 1;
    std::string binding_error;
    const bool core_verified = native_addresses::initialize(
        reinterpret_cast<std::uintptr_t>(mapped_image), executable_path, digest, binding_error) &&
        native_addresses::verify_all();
    const bool minhook_ready = MH_Initialize() == MH_OK;
    const bool verified = core_verified && minhook_ready && god_mode::initialize(
        reinterpret_cast<std::uintptr_t>(mapped_image), executable_path, digest) && god_mode::supported();
    if (minhook_ready) MH_Uninitialize();
    god_mode::damage_ = nullptr;
    god_mode_events::queued_ = false;
    god_mode_events::dispatcher_address_ = 0;
    god_mode_events::original_dispatcher_.store(nullptr);
    UnmapViewOfFile(mapped_image);
    std::printf("God mode native image: %s [%s] core=%s%s%s damage=0x%x dispatcher=0x%x\n", verified ? "PASS" : "FAILED",
        hash.c_str(), core_verified ? "PASS" : "FAILED", binding_error.empty() ? "" : " error=",
        binding_error.c_str(), pattern_binding.damage_rva, pattern_binding.dispatcher_rva);
    return verified ? 0 : 1;
}

} // namespace azs

int wmain(int argc, wchar_t* argv[]) {
    if (argc == 3 && std::wcscmp(argv[1], L"--verify-image") == 0)
        return azs::god_mode_tests::verify_image(argv[2]);
    return azs::god_mode_tests::run();
}
