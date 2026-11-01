#include "god_mode_bindings_tests.hpp"
#include "pattern_scanner.hpp"
#include "signature_profile.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <exception>
#include <utility>

namespace azs {

void god_mode_bindings_tests::check(bool condition, const char* description) {
    if (!condition) {
        ++failures_;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

god_mode_bindings_tests::fixture god_mode_bindings_tests::make_fixture(std::uint32_t text_rva,
    std::uint64_t image_base, std::uint32_t dispatcher_offset, std::uint32_t helper_offset,
    std::uint32_t constant_offset, bool imported_helper) {
    fixture result{{}, text_rva, dispatcher_offset, helper_offset, constant_offset, imported_helper};
    auto& bytes = result.bytes;
    bytes.resize(0xe00);
    put_unsigned<std::uint16_t>(bytes, 0, 0x5a4d);
    put_unsigned<std::uint32_t>(bytes, 0x3c, 0x80);
    put_unsigned<std::uint32_t>(bytes, 0x80, 0x00004550);
    put_unsigned<std::uint16_t>(bytes, 0x84, 0x8664);
    put_unsigned<std::uint16_t>(bytes, 0x86, 4);
    put_unsigned<std::uint32_t>(bytes, 0x88, 0x11552233);
    put_unsigned<std::uint16_t>(bytes, 0x94, 0xf0);
    put_unsigned<std::uint16_t>(bytes, 0x98, 0x20b);
    put_unsigned<std::uint64_t>(bytes, 0xb0, image_base);
    put_unsigned<std::uint32_t>(bytes, 0xb8, 0x1000);
    put_unsigned<std::uint32_t>(bytes, 0xbc, 0x200);
    put_unsigned<std::uint32_t>(bytes, 0xd0, text_rva + 0x4000);
    put_unsigned<std::uint32_t>(bytes, 0xd4, 0x400);
    put_unsigned<std::uint32_t>(bytes, 0x104, 16);
    put_unsigned<std::uint32_t>(bytes, 0x120, text_rva + 0x1000);
    put_unsigned<std::uint32_t>(bytes, 0x124, 4 * 12);
    constexpr const char* section_names[]{".text", ".pdata", ".xdata", ".rdata"};
    constexpr std::uint32_t section_offsets[]{0, 0x1000, 0x2000, 0x3000};
    constexpr std::uint32_t raw_offsets[]{0x400, 0x800, 0xa00, 0xc00};
    constexpr std::uint32_t raw_sizes[]{0x400, 0x200, 0x200, 0x200};
    for (std::size_t index = 0; index < std::size(section_names); ++index) {
        const auto section = 0x188 + index * 40;
        std::memcpy(bytes.data() + section, section_names[index], std::strlen(section_names[index]));
        put_unsigned(bytes, section + 8, raw_sizes[index]);
        put_unsigned(bytes, section + 12, text_rva + section_offsets[index]);
        put_unsigned(bytes, section + 16, raw_sizes[index]);
        put_unsigned(bytes, section + 20, raw_offsets[index]);
        put_unsigned<std::uint32_t>(bytes, section + 36, index == 0 ? 0x60000020 : 0x40000040);
    }
    std::fill(bytes.begin() + 0x400, bytes.begin() + 0x800, std::uint8_t{0x90});
    const std::array<std::uint32_t, 4> function_offsets{0, dispatcher_offset, helper_offset, 0x200};
    for (std::size_t index = 0; index < function_offsets.size(); ++index) {
        put_unsigned(bytes, 0x800 + index * 12, text_rva + function_offsets[index]);
        put_unsigned(bytes, 0x804 + index * 12, text_rva + function_offsets[index] + (index == 0 ? 0x80u : 0x40u));
        put_unsigned(bytes, 0x808 + index * 12, text_rva + 0x2000 + static_cast<std::uint32_t>(index * 4));
        bytes[0xa00 + index * 4] = 1;
    }
    constexpr std::array<std::uint8_t, 81> damage{
        0x55,0x48,0x89,0xe5,0x48,0x83,0xec,0x20,
        0xf3,0x0f,0x10,0x05,0,0,0,0,
        0xf3,0x0f,0x10,0x89,0x78,0x01,0,0,
        0xf3,0x0f,0x5c,0xc1,0x75,0x05,
        0xe8,0,0,0,0,0xe8,0,0,0,0,
        0x80,0xb9,0x36,0x02,0,0,0x03,
        0xe8,0,0,0,0,
        0x48,0x8d,0x05,0,0,0,0,
        0x48,0x63,0x0c,0x88,0x48,0x01,0xc1,0xff,0xe1,
        0x48,0x83,0xc4,0x20,0x5d,0xc3,
        0x48,0x8d,0x15,0,0,0,0};
    constexpr std::array<std::uint8_t, 36> dispatcher{
        0x55,0x41,0x57,0x48,0x83,0xec,0x20,0x90,
        0x48,0x8b,0x89,0x20,0x0a,0,0,
        0xf3,0x0f,0x10,0x05,0,0,0,0,
        0xe8,0,0,0,0,
        0x48,0x83,0xc4,0x20,0x41,0x5f,0x5d,0xc3};
    std::copy(damage.begin(), damage.end(), bytes.begin() + 0x400);
    std::copy(dispatcher.begin(), dispatcher.end(), bytes.begin() + 0x400 + dispatcher_offset);
    constexpr std::array<std::uint8_t, 9> helper{0xb8,0x45,0x23,0x11,0x77,0x90,0x90,0x90,0xc3};
    std::copy(helper.begin(), helper.end(), bytes.begin() + 0x400 + helper_offset);
    put_unsigned<std::uint32_t>(bytes, 0x40c, 0x3000 + constant_offset - 16);
    put_unsigned<std::uint32_t>(bytes, 0x41f, helper_offset - 35);
    put_unsigned<std::uint32_t>(bytes, 0x424, dispatcher_offset - 40);
    put_unsigned<std::uint32_t>(bytes, 0x430, dispatcher_offset - 52);
    put_unsigned<std::uint32_t>(bytes, 0x437, 0x3020 + constant_offset - 59);
    put_unsigned<std::uint32_t>(bytes, 0x44d, 0x3080 + constant_offset - 81);
    put_unsigned<std::uint32_t>(bytes, 0x400 + dispatcher_offset + 19,
        0x3000 + constant_offset + 8 - (dispatcher_offset + 23));
    put_unsigned<std::uint32_t>(bytes, 0x400 + dispatcher_offset + 24,
        helper_offset - (dispatcher_offset + 28));
    put_unsigned<std::uint32_t>(bytes, 0xc00 + constant_offset, 0x3e800000);
    put_unsigned<std::uint32_t>(bytes, 0xc08 + constant_offset, 0x40a00000);
    for (const auto target_offset : {88u, 96u, 104u, 112u}) {
        const auto index = (target_offset - 88) / 8;
        put_unsigned<std::uint32_t>(bytes, 0xc20 + constant_offset + index * 4,
            target_offset - (0x3020 + constant_offset));
        bytes[0x400 + target_offset] = 0xeb;
        bytes[0x401 + target_offset] = static_cast<std::uint8_t>(68 - (target_offset + 2));
    }
    constexpr std::array<std::string_view, 5> strings{"toy_stat", "generic", "known", "Toy Label", "Toy_Icon"};
    std::uint32_t string_offset = 0x100;
    for (std::size_t index = 0; index < strings.size(); ++index) {
        const auto slot = 0xc80 + constant_offset + index * 16;
        put_unsigned<std::uint64_t>(bytes, slot, image_base + text_rva + 0x3000 + constant_offset + string_offset);
        put_unsigned<std::uint64_t>(bytes, slot + 8, strings[index].size());
        std::copy(strings[index].begin(), strings[index].end(), bytes.begin() + 0xc00 + constant_offset + string_offset);
        string_offset += static_cast<std::uint32_t>(strings[index].size());
    }
    put_unsigned<std::uint64_t>(bytes, 0xcd0 + constant_offset, 1);
    if (imported_helper) {
        put_unsigned<std::uint32_t>(bytes, 0x110, text_rva + 0x3180);
        put_unsigned<std::uint32_t>(bytes, 0x114, 40);
        put_unsigned<std::uint32_t>(bytes, 0xd80, text_rva + 0x31c0);
        put_unsigned<std::uint32_t>(bytes, 0xd8c, text_rva + 0x31b0);
        put_unsigned<std::uint32_t>(bytes, 0xd90, text_rva + 0x31d0);
        constexpr char module[] = "toycrt.dll";
        constexpr char symbol[] = "toy_helper";
        std::copy_n(module, sizeof(module), bytes.begin() + 0xdb0);
        std::copy_n(symbol, sizeof(symbol), bytes.begin() + 0xde2);
        put_unsigned<std::uint64_t>(bytes, 0xdc0, text_rva + 0x31e0);
        put_unsigned<std::uint64_t>(bytes, 0xdd0, text_rva + 0x31e0);
        bytes[0x400 + helper_offset] = 0xff;
        bytes[0x401 + helper_offset] = 0x25;
        put_unsigned<std::uint32_t>(bytes, 0x402 + helper_offset, 0x31d0 - helper_offset - 6);
    }
    return result;
}

bool god_mode_bindings_tests::resolve_fixture(const fixture& input, const fixture& trusted,
    god_mode_bindings::candidate& output, std::string& error, certificate_fault fault) {
    constexpr std::string_view damage_entry = "55 48 89 E5 48 83 EC 20";
    constexpr std::string_view dispatcher_entry = "55 41 57 48 83 EC 20 90";
    constexpr std::string_view helper_entry = "B8 45 23 11 77 90 90 90";
    using rule = god_mode_bindings::relocation_rule;
    using kind = god_mode_bindings::reference_kind;
    std::array<rule, 6> damage_references{{
        {12,16,kind::rip_data,4,{0x00,0x00,0x80,0x3e},{},false,false},
        {31,35,kind::relative_call,0,{},helper_entry,true,true},
        {36,40,kind::relative_call,0,{},dispatcher_entry,true,true},
        {48,52,kind::relative_call,0,{},dispatcher_entry,true,true},
        {55,59,kind::rip_jump_table,16,{},{},false,false},
        {77,81,kind::rip_string_table,88,{},{},false,false}}};
    std::array<rule, 2> dispatcher_references{{
        {19,23,kind::rip_data,4,{0x00,0x00,0xa0,0x40},{},false,false},
        {24,28,kind::relative_call,0,{},helper_entry,true,true}}};
    if (trusted.imported_helper) {
        for (auto* reference : {&damage_references[1], &dispatcher_references[1]}) {
            reference->target_pattern = {};
            reference->require_function_entry = false;
            reference->target_import = "toycrt.dll!toy_helper";
        }
    }
    std::vector<std::uint8_t> jump_targets(16);
    for (std::uint32_t index = 0; index < 4; ++index)
        put_unsigned<std::uint32_t>(jump_targets, index * 4, 88 + index * 8);
    const auto jump_hash = signature_profiles::sha256(jump_targets);
    damage_references[4].target_digest = jump_hash;
    auto string_descriptor = std::vector<std::uint8_t>(trusted.bytes.begin() + 0xc80 + trusted.constant_offset,
        trusted.bytes.begin() + 0xcd8 + trusted.constant_offset);
    for (std::size_t index = 0; index < 5; ++index) {
        std::fill_n(string_descriptor.begin() + index * 16, 8, std::uint8_t{});
        if (index != 0) std::fill_n(string_descriptor.begin() + index * 16 + 8, 8, std::uint8_t{});
    }
    constexpr std::string_view key = "toy_stat";
    string_descriptor.insert(string_descriptor.end(), key.begin(), key.end());
    const auto string_hash = signature_profiles::sha256(string_descriptor);
    damage_references[5].target_digest = string_hash;
    const auto reference_count = fault == certificate_fault::missing_dispatcher_call ? 3u : 6u;
    auto damage_normalized = std::vector<std::uint8_t>(trusted.bytes.begin() + 0x400,
        trusted.bytes.begin() + 0x480);
    auto dispatcher_normalized = std::vector<std::uint8_t>(
        trusted.bytes.begin() + 0x400 + trusted.dispatcher_offset,
        trusted.bytes.begin() + 0x440 + trusted.dispatcher_offset);
    for (std::size_t index = 0; index < reference_count; ++index) {
        const auto offset = damage_references[index].operand_offset;
        std::fill_n(damage_normalized.begin() + offset, 4, std::uint8_t{});
    }
    for (const auto& reference : dispatcher_references)
        std::fill_n(dispatcher_normalized.begin() + reference.operand_offset, 4, std::uint8_t{});
    const auto damage_hash = signature_profiles::sha256(damage_normalized);
    const auto dispatcher_hash = signature_profiles::sha256(dispatcher_normalized);
    if (fault == certificate_fault::invalid_operand)
        damage_references[0] = {127,131,kind::rip_data,4,{0x00,0x00,0x80,0x3e},{},false,false};
    god_mode_bindings::family_certificate family{
        {damage_entry,0x80,damage_hash,std::span(damage_references).first(reference_count)},
        {dispatcher_entry,0x40,dispatcher_hash,dispatcher_references},
        0x640,0x200,0x236,0x100,0x108,{36,48},31};
    std::array families{family, family};
    pattern_scanner scanner;
    if (!scanner.load_bytes(input.bytes)) {
        error = std::string(scanner.error());
        return false;
    }
    return god_mode_bindings::resolve_certificates(scanner,
        std::span(families).first(fault == certificate_fault::duplicate_family ? 2u : 1u), output, error);
}

bool god_mode_bindings_tests::same_candidate(const god_mode_bindings::candidate& left,
    const god_mode_bindings::candidate& right) {
    if (left.damage_rva != right.damage_rva || left.damage_size != right.damage_size ||
        left.dispatcher_rva != right.dispatcher_rva || left.dispatcher_size != right.dispatcher_size ||
        left.lifesteal_rva != right.lifesteal_rva || left.enemies_offset != right.enemies_offset ||
        left.enemy_stride != right.enemy_stride || left.source_offset != right.source_offset ||
        left.phase_offset != right.phase_offset || left.spells_offset != right.spells_offset ||
        left.verification.size() != right.verification.size()) return false;
    for (std::size_t index = 0; index < left.verification.size(); ++index) {
        const auto& first = left.verification[index];
        const auto& second = right.verification[index];
        if (first.rva != second.rva || first.executable != second.executable ||
            first.relocated_pointer != second.relocated_pointer || first.bytes != second.bytes)
            return false;
    }
    return true;
}

void god_mode_bindings_tests::reject(const fixture& input, const fixture& trusted,
    const god_mode_bindings::candidate& previous, const char* description) {
    auto result = previous;
    std::string error;
    check(!resolve_fixture(input, trusted, result, error) && !error.empty() &&
        same_candidate(result, previous), description);
}

void god_mode_bindings_tests::test_relocation() {
    const auto trusted = make_fixture();
    const std::array inputs{trusted, make_fixture(0x9000, 0x180000000, 0x100, 0x1a0, 0x40)};
    std::string initial_hash;
    for (const auto& input : inputs) {
        god_mode_bindings::candidate result;
        std::string error;
        check(resolve_fixture(input, trusted, result, error) && error.empty(),
            "unknown image identity and independent function, helper and RIP data relocations are accepted");
        check(result.damage_rva == input.text_rva && result.damage_size == 0x80 &&
            result.dispatcher_rva == input.text_rva + input.dispatcher_offset && result.dispatcher_size == 0x40 &&
            result.lifesteal_rva == input.text_rva + input.helper_offset,
            "certified direct calls publish the relocated damage, dispatcher and getter entries");
        check(result.enemies_offset == 0x640 && result.enemy_stride == 0x200 && result.source_offset == 0x236 &&
            result.phase_offset == 0x100 && result.spells_offset == 0x108 && result.verification.size() >= 5,
            "complete certificate publishes unchanged fixed fields and actual code/data proof ranges");
        const auto pointers = std::count_if(result.verification.begin(), result.verification.end(),
            [](const auto& chunk) { return chunk.relocated_pointer; });
        check(pointers == 5, "all borrowed string pointers publish relocation-aware rather than absolute-VA live proofs");
        for (const auto& chunk : result.verification) {
            if (!chunk.relocated_pointer) continue;
            std::uint64_t target{};
            if (chunk.bytes.size() == sizeof(target)) std::memcpy(&target, chunk.bytes.data(), sizeof(target));
            check(chunk.bytes.size() == sizeof(target) && target >= input.text_rva + 0x3100 &&
                target < input.text_rva + 0x3200,
                "relocated pointer evidence contains an image-relative target independent of preferred ImageBase");
        }
        const auto hash = signature_profiles::sha256(input.bytes);
        if (initial_hash.empty()) initial_hash = hash;
        else check(hash != initial_hash, "relocated admission uses a genuinely different full executable hash");
    }
    pattern_scanner scanner;
    check(scanner.load_bytes(trusted.bytes), "production refusal fixture remains a valid PE64 image");
    god_mode_bindings::candidate prior;
    prior.damage_rva = 0x1234;
    const auto before = prior;
    std::string error;
    check(!god_mode_bindings::resolve_file(scanner, prior, error) && !error.empty() && same_candidate(prior, before),
        "invented toy certificates cannot authorize code through the production embedded catalog");
    auto cosmetic = trusted;
    put_unsigned<std::uint64_t>(cosmetic.bytes, 0xc80 + 3 * 16 + 8, 3);
    god_mode_bindings::candidate cosmetic_result;
    check(resolve_fixture(cosmetic, trusted, cosmetic_result, error),
        "cosmetic borrowed-label length changes retain the same logical stat key and descriptor kind");
}

void god_mode_bindings_tests::test_refusal() {
    const auto trusted = make_fixture();
    god_mode_bindings::candidate previous;
    std::string error;
    check(resolve_fixture(trusted, trusted, previous, error), "refusal cases start with a complete accepted candidate");
    auto input = trusted;
    input.bytes[0x400] ^= 1;
    reject(input, trusted, previous, "a missing unique damage entry refuses without publishing partial state");
    input = trusted;
    std::copy_n(input.bytes.begin() + 0x400, 0x40, input.bytes.begin() + 0x600);
    reject(input, trusted, previous, "two matching damage entries refuse rather than selecting the first");
    input = trusted;
    input.bytes[0x400 + trusted.dispatcher_offset] ^= 1;
    reject(input, trusted, previous, "a missing linked dispatcher cannot leave a damage-only candidate");
    input = trusted;
    std::copy_n(input.bytes.begin() + 0x400 + trusted.dispatcher_offset, 0x40, input.bytes.begin() + 0x600);
    reject(input, trusted, previous, "two matching dispatcher entries refuse atomically");
    input = trusted;
    input.bytes[0x418] = 0xf2;
    reject(input, trusted, previous, "changing the fixed scalar instruction ABI refuses admission");
    input = trusted;
    input.bytes[0x414] = 0x7c;
    reject(input, trusted, previous, "an enemy HP field shift refuses rather than reusing a stale fixed layout");
    input = trusted;
    input.bytes[0x42e] = 2;
    reject(input, trusted, previous, "changing the spell-suppression source constant refuses admission");
    input = trusted;
    input.bytes[0x41d] = 4;
    reject(input, trusted, previous, "an altered internal branch destination is never normalized away");
    input = trusted;
    put_unsigned<std::uint32_t>(input.bytes, 0xc00, 0x3f000000);
    reject(input, trusted, previous, "an altered shield multiplier fails its immutable scalar guard");
    input = trusted;
    put_unsigned<std::uint32_t>(input.bytes, 0xc20, 89u - 0x3020u);
    reject(input, trusted, previous, "an altered jump-table target is rejected even inside the certified function");
    input = trusted;
    put_unsigned<std::uint32_t>(input.bytes, 0xc20, 0x3000u - 0x3020u);
    reject(input, trusted, previous, "a jump-table target outside the certified function cannot redirect native control flow");
    input = trusted;
    put_unsigned<std::uint32_t>(input.bytes, 0x437, 0x3000u - 59u);
    reject(input, trusted, previous, "a jump-table LEA redirected to arbitrary read-only data refuses admission");
    input = trusted;
    input.bytes[0xd00] = 'x';
    reject(input, trusted, previous, "changing the logical borrowed stat key refuses admission");
    input = trusted;
    put_unsigned<std::uint64_t>(input.bytes, 0xcd0, 2);
    reject(input, trusted, previous, "changing the borrowed descriptor kind refuses admission");
    input = trusted;
    put_unsigned<std::uint64_t>(input.bytes, 0xc80 + 4 * 16 + 8, 600);
    reject(input, trusted, previous, "an oversized cosmetic string view cannot escape the bounded data contract");
    input = trusted;
    put_unsigned<std::uint64_t>(input.bytes, 0xc80 + 4 * 16, 0x140001010);
    reject(input, trusted, previous, "a borrowed string view redirected to executable bytes refuses admission");
    input = trusted;
    put_unsigned<std::uint32_t>(input.bytes, 0x430, trusted.helper_offset - 52);
    reject(input, trusted, previous, "a redirected mandatory dispatcher call cannot pair unrelated functions");
    input = trusted;
    input.bytes[0x400 + trusted.helper_offset] = 0xc2;
    reject(input, trusted, previous, "an altered external callee fails its local target anchor");
    input = trusted;
    std::copy_n(input.bytes.begin() + 0x400 + trusted.helper_offset, 8, input.bytes.begin() + 0x600);
    input.bytes[0x608] = 0x31;
    input.bytes[0x609] = 0xc0;
    input.bytes[0x60a] = 0xc3;
    put_unsigned<std::uint32_t>(input.bytes, 0x41f, 0x200u - 35u);
    reject(input, trusted, previous, "a new callee sharing the old prefix cannot receive a redirected certified call");
    input = trusted;
    put_unsigned<std::uint32_t>(input.bytes, 0x804, trusted.text_rva + 0x3f);
    reject(input, trusted, previous, "truncating the unwind-covered function rejects an otherwise matching body");
    input = trusted;
    put_unsigned<std::uint32_t>(input.bytes, 0x800, trusted.text_rva + 1);
    reject(input, trusted, previous, "a signature outside its exact unwind entry cannot authorize a callable target");
    input = trusted;
    put_unsigned<std::uint32_t>(input.bytes, 0x188 + 3 * 40 + 36, 0xc0000040);
    reject(input, trusted, previous, "writable scalar targets fail the immutable data-section contract");
    input = trusted;
    put_unsigned<std::uint32_t>(input.bytes, 0x40c, 0x210 - 16);
    put_unsigned<std::uint32_t>(input.bytes, 0x610, 0x3e800000);
    reject(input, trusted, previous, "matching scalar bytes in executable memory cannot substitute for read-only data");
    input = trusted;
    input.bytes.resize(0xd00);
    reject(input, trusted, previous, "a truncated file fails before any prior binding output changes");
}

void god_mode_bindings_tests::test_metadata() {
    const auto trusted = make_fixture();
    god_mode_bindings::candidate previous;
    std::string error;
    check(resolve_fixture(trusted, trusted, previous, error), "metadata refusal starts with a complete candidate");
    for (const auto fault : {certificate_fault::missing_dispatcher_call, certificate_fault::invalid_operand,
                             certificate_fault::duplicate_family}) {
        auto result = previous;
        error.clear();
        check(!resolve_fixture(trusted, trusted, result, error, fault) && !error.empty() && same_candidate(result, previous),
            "incomplete relocation metadata, out-of-body operands and competing families refuse atomically");
    }
}

void god_mode_bindings_tests::test_imports() {
    const auto trusted = make_fixture(0x1000, 0x140000000, 0x80, 0x180, 0, true);
    god_mode_bindings::candidate previous;
    std::string error;
    check(resolve_fixture(trusted, trusted, previous, error),
        "a named imported helper is certified through its exact DLL and symbol identity");
    const auto moved = make_fixture(0x9000, 0x180000000, 0x100, 0x1a0, 0x40, true);
    god_mode_bindings::candidate moved_result;
    check(resolve_fixture(moved, trusted, moved_result, error) &&
        moved_result.lifesteal_rva == moved.text_rva + moved.helper_offset,
        "moving code, image base and IAT references retains the named import contract");
    auto input = trusted;
    input.bytes[0xde2] = 'x';
    reject(input, trusted, previous, "a helper thunk redirected to a different imported symbol refuses admission");
    input = trusted;
    input.bytes[0xdb0] = 'x';
    reject(input, trusted, previous, "the same symbol name from a different DLL cannot satisfy the import certificate");
    input = trusted;
    put_unsigned<std::uint64_t>(input.bytes, 0xdc0, 0);
    reject(input, trusted, previous, "an import slot without a complete original lookup entry refuses admission");
    input = trusted;
    put_unsigned<std::uint32_t>(input.bytes, 0x402 + trusted.helper_offset, 0x31d8 - trusted.helper_offset - 6);
    reject(input, trusted, previous, "a thunk pointing beyond the populated import entries cannot borrow an unrelated identity");
    input = trusted;
    input.bytes[0x400 + trusted.helper_offset] = 0xe9;
    put_unsigned<std::uint32_t>(input.bytes, 0x401 + trusted.helper_offset, 0xfffffffbu);
    reject(input, trusted, previous, "a cyclic native forwarder cannot loop during import verification");
    input = trusted;
    input.bytes[0x600] = 0xff;
    input.bytes[0x601] = 0x25;
    put_unsigned<std::uint32_t>(input.bytes, 0x602, 0x31d0 - 0x200 - 6);
    input.bytes[0x400 + trusted.helper_offset] = 0xe9;
    put_unsigned<std::uint32_t>(input.bytes, 0x401 + trusted.helper_offset, 0x200 - trusted.helper_offset - 5);
    god_mode_bindings::candidate forwarded;
    check(resolve_fixture(input, trusted, forwarded, error),
        "a bounded direct forwarder preserves the exact imported-helper identity");
}

int god_mode_bindings_tests::run() {
    try {
        test_relocation();
        test_refusal();
        test_metadata();
        test_imports();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "God binding certificate test exception: %s\n", error.what());
        ++failures_;
    } catch (...) {
        check(false, "unexpected exception in owned God binding certificate fixtures");
    }
    std::printf("Adaptive God mode binding certificates: %s (%d failures)\n",
        failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

} // namespace azs

int main() { return azs::god_mode_bindings_tests::run(); }
