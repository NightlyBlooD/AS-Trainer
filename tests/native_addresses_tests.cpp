#include "native_addresses_tests.hpp"
#include "pattern_scanner.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string_view>
#include <utility>

namespace azs {

void native_addresses_tests::check(bool condition, const char* description) noexcept {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures_;
    }
}

std::string native_addresses_tests::pattern_text(std::span<const std::uint8_t> bytes,
    std::size_t wildcard_offset) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index != 0) result += ' ';
        if (wildcard_offset != static_cast<std::size_t>(-1)
            && index >= wildcard_offset && index - wildcard_offset < 4) {
            result += "??";
        } else {
            result += digits[bytes[index] >> 4];
            result += digits[bytes[index] & 15];
        }
    }
    return result;
}

native_addresses_tests::fixture native_addresses_tests::make_fixture(
    std::uint32_t text_rva, std::uint64_t image_base, bool float_setter) {
    fixture result;
    result.bytes.resize(0x6400);
    result.text_rva = text_rva;
    result.data_rva = text_rva + 0x6000;
    auto& bytes = result.bytes;
    put_unsigned<std::uint16_t>(bytes, 0, 0x5a4d);
    put_unsigned<std::uint32_t>(bytes, 0x3c, 0x80);
    put_unsigned<std::uint32_t>(bytes, 0x80, 0x00004550);
    put_unsigned<std::uint16_t>(bytes, 0x84, 0x8664);
    put_unsigned<std::uint16_t>(bytes, 0x86, 4);
    put_unsigned<std::uint32_t>(bytes, 0x88, 0x12345678);
    put_unsigned<std::uint16_t>(bytes, 0x94, 0xf0);
    put_unsigned<std::uint16_t>(bytes, 0x98, 0x20b);
    put_unsigned<std::uint64_t>(bytes, 0xb0, image_base);
    put_unsigned<std::uint32_t>(bytes, 0xb8, 0x1000);
    put_unsigned<std::uint32_t>(bytes, 0xbc, 0x200);
    put_unsigned<std::uint32_t>(bytes, 0xd0, text_rva + 0x7000);
    put_unsigned<std::uint32_t>(bytes, 0xd4, 0x400);
    put_unsigned<std::uint32_t>(bytes, 0x104, 16);
    put_unsigned<std::uint32_t>(bytes, 0x120, text_rva + 0x4000);
    put_unsigned<std::uint32_t>(bytes, 0x124, 36 * 12);
    constexpr const char* section_names[]{".text", ".pdata", ".xdata", ".rdata"};
    constexpr std::uint32_t section_offsets[]{0, 0x4000, 0x5000, 0x6000};
    constexpr std::uint32_t raw_offsets[]{0x400, 0x4400, 0x4c00, 0x5400};
    constexpr std::uint32_t raw_sizes[]{0x4000, 0x800, 0x800, 0x1000};
    for (std::size_t index = 0; index < std::size(section_names); ++index) {
        const auto section = 0x188 + index * 40;
        std::memcpy(bytes.data() + section, section_names[index], std::strlen(section_names[index]));
        put_unsigned(bytes, section + 8, raw_sizes[index]);
        put_unsigned(bytes, section + 12, text_rva + section_offsets[index]);
        put_unsigned(bytes, section + 16, raw_sizes[index]);
        put_unsigned(bytes, section + 20, raw_offsets[index]);
        put_unsigned<std::uint32_t>(bytes, section + 36, index == 0 ? 0x60000020 : 0x40000040);
    }
    std::fill(bytes.begin() + 0x400, bytes.begin() + 0x4400, std::uint8_t{0x90});
    for (std::size_t index = 0; index < 36; ++index) {
        const auto function = text_rva + static_cast<std::uint32_t>(index * 0x80);
        put_unsigned(bytes, 0x4400 + index * 12, function);
        put_unsigned(bytes, 0x4404 + index * 12, function + 0x80);
        put_unsigned(bytes, 0x4408 + index * 12, text_rva + 0x5000 + static_cast<std::uint32_t>(index * 4));
        bytes[0x4c00 + index * 4] = 1;
    }
    result.profile.name = "Owned complete adaptive fixture";
    result.profile.sha256.assign(64, 'a');
    result.profile.layout = "adaptive";
    constexpr const char* symbols[]{"owner_frame", "recalculate", "create_speed_packet", "grow_speed_packets",
        "swap_player", "flush_updates", "route_queues", "set_object_field", "apply_choice", "boss_loot",
        "grow_loot", "luck_reader", "area_reader", "pickup_reader", "zone_table", "lifesteal"};
    constexpr std::uint8_t bits_prefix[]{0x56, 0x57, 0x53, 0x48, 0x83, 0xec, 0x70, 0x44, 0x89, 0xc7,
        0x89, 0xd3, 0x48, 0x89, 0xce, 0x48, 0x8b, 0x01, 0x48, 0x85, 0xc0};
    constexpr std::uint8_t float_prefix[]{0x56, 0x48, 0x83, 0xec, 0x70, 0x48, 0x89, 0xce, 0x48, 0x8b,
        0x01, 0x48, 0x85, 0xc0};
    for (std::size_t index = 0; index < std::size(symbols); ++index) {
        const auto offset = 0x400 + index * 0x80;
        result.symbols[index] = text_rva + static_cast<std::uint32_t>(index * 0x80);
        if (index == static_cast<std::size_t>(native_symbol::set_object_field)) {
            const auto prefix = float_setter ? std::span<const std::uint8_t>(float_prefix)
                                             : std::span<const std::uint8_t>(bits_prefix);
            std::copy(prefix.begin(), prefix.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
        }
        const auto marker_offset = offset + 0x20;
        bytes[marker_offset] = 0xb8;
        put_unsigned<std::uint32_t>(bytes, marker_offset + 1, 0x31410000 + static_cast<std::uint32_t>(index));
        bytes[marker_offset + 5] = 0xc3;
        signature_rule rule;
        rule.name = symbols[index];
        rule.section = ".text";
        rule.pattern = pattern_text(std::span(bytes).subspan(marker_offset, 6));
        rule.resolve = pattern_resolve_mode::function_start;
        if (index == static_cast<std::size_t>(native_symbol::luck_reader)
            || index == static_cast<std::size_t>(native_symbol::area_reader)
            || index == static_cast<std::size_t>(native_symbol::pickup_reader)) {
            rule.resolve = pattern_resolve_mode::match;
            result.symbols[index] += 0x20;
        }
        if (index == static_cast<std::size_t>(native_symbol::zone_table)) {
            bytes[offset] = 0x48;
            bytes[offset + 1] = 0x8d;
            bytes[offset + 2] = 0x05;
            const auto displacement = static_cast<std::int32_t>(result.data_rva - (text_rva + index * 0x80 + 7));
            put_unsigned<std::uint32_t>(bytes, offset + 3, static_cast<std::uint32_t>(displacement));
            bytes[offset + 7] = 0x41;
            bytes[offset + 8] = 0xb8;
            put_unsigned<std::uint32_t>(bytes, offset + 9, 0x72114561);
            rule.pattern = pattern_text(std::span(bytes).subspan(offset, 13), 3);
            rule.resolve = pattern_resolve_mode::rip_rel32;
            rule.displacement_offset = 3;
            rule.instruction_size = 7;
            result.symbols[index] = result.data_rva;
        }
        result.profile.rules.push_back(std::move(rule));
    }
    constexpr const char* guard_names[]{"guard_owner_frame_abi", "guard_recalculate_abi",
        "guard_speed_packet_abi", "guard_speed_queue_abi", "guard_swap_player_abi",
        "guard_choice_record", "guard_loot_record", "guard_zone_record", "guard_lifesteal_abi"};
    constexpr std::size_t guard_symbols[]{0, 1, 2, 3, 4, 8, 9, 14, 15};
    for (std::size_t index = 0; index < std::size(guard_names); ++index) {
        signature_rule rule = result.profile.rules[guard_symbols[index]];
        rule.name = guard_names[index];
        rule.resolve = pattern_resolve_mode::match;
        rule.pattern = pattern_text(std::span(bytes).subspan(0x400 + guard_symbols[index] * 0x80 + 0x20, 6));
        rule.displacement_offset = rule.instruction_size = 0;
        result.profile.rules.push_back(std::move(rule));
    }
    signature_rule setter_guard;
    setter_guard.name = float_setter ? "guard_set_object_field_float" : "guard_set_object_field_bits";
    setter_guard.section = ".text";
    setter_guard.resolve = pattern_resolve_mode::match;
    setter_guard.pattern = pattern_text(float_setter ? std::span<const std::uint8_t>(float_prefix)
                                                     : std::span<const std::uint8_t>(bits_prefix));
    result.profile.rules.push_back(std::move(setter_guard));
    std::size_t field_index{};
    for (const bool members : {false, true}) {
        const auto keys = members ? game_layout::required_member_offsets() : game_layout::required_run_offsets();
        for (const auto key : keys) {
            if (key == 0) continue;
            const auto offset = 0x400 + 0x1400 + field_index * 16;
            bytes[offset] = 0x8b;
            bytes[offset + 1] = 0x81;
            const auto destination = !members && key == 0x4cc ? 0xd80 : key;
            put_unsigned<std::uint32_t>(bytes, offset + 2, static_cast<std::uint32_t>(destination));
            bytes[offset + 6] = 0x41;
            bytes[offset + 7] = 0xb8;
            put_unsigned<std::uint32_t>(bytes, offset + 8, 0x42110000 + static_cast<std::uint32_t>(field_index));
            bytes[offset + 12] = 0xc3;
            char name[48]{};
            std::snprintf(name, sizeof(name), "%s_%zx", members ? "member" : "run", key);
            signature_rule rule;
            rule.name = name;
            rule.section = ".text";
            rule.resolve = pattern_resolve_mode::field_disp32;
            rule.displacement_offset = 2;
            rule.pattern = pattern_text(std::span(bytes).subspan(offset, 13), 2);
            result.profile.rules.push_back(std::move(rule));
            ++field_index;
        }
    }
    const auto stride_offset = 0x400 + 0x1400 + field_index * 16;
    bytes[stride_offset] = 0x48;
    bytes[stride_offset + 1] = 0x69;
    bytes[stride_offset + 2] = 0xc0;
    put_unsigned<std::uint32_t>(bytes, stride_offset + 3, 0xa38);
    bytes[stride_offset + 7] = 0xc3;
    signature_rule stride;
    stride.name = "participant_stride";
    stride.section = ".text";
    stride.resolve = pattern_resolve_mode::field_disp32;
    stride.displacement_offset = 3;
    stride.pattern = pattern_text(std::span(bytes).subspan(stride_offset, 8), 3);
    result.profile.rules.push_back(std::move(stride));
    return result;
}

bool native_addresses_tests::same_candidate(const native_addresses::candidate& left,
    const native_addresses::candidate& right) noexcept {
    if (left.symbols != right.symbols || left.participant_stride != right.participant_stride
        || left.image_size != right.image_size || left.image_sha256 != right.image_sha256
        || left.layout != right.layout || left.setter_abi != right.setter_abi
        || left.run_offsets.size() != right.run_offsets.size()
        || left.member_offsets.size() != right.member_offsets.size()
        || left.verification.size() != right.verification.size()) return false;
    for (std::size_t index = 0; index < left.run_offsets.size(); ++index)
        if (left.run_offsets[index].legacy_offset != right.run_offsets[index].legacy_offset
            || left.run_offsets[index].resolved_offset != right.run_offsets[index].resolved_offset) return false;
    for (std::size_t index = 0; index < left.member_offsets.size(); ++index)
        if (left.member_offsets[index].legacy_offset != right.member_offsets[index].legacy_offset
            || left.member_offsets[index].resolved_offset != right.member_offsets[index].resolved_offset) return false;
    for (std::size_t index = 0; index < left.verification.size(); ++index)
        if (left.verification[index].rva != right.verification[index].rva
            || left.verification[index].bytes != right.verification[index].bytes) return false;
    return true;
}

void native_addresses_tests::reject(const fixture& input, const native_addresses::candidate& previous,
    const char* description, std::string_view error_fragment) {
    pattern_scanner scanner;
    check(scanner.load_bytes(input.bytes), "rejection fixture remains a valid bounded PE64 image");
    const auto original_bytes = input.bytes;
    const auto selected_layout = game_layout::current_id();
    const auto generation = game_layout::generation();
    const auto owner_rva = native_addresses::rva(native_symbol::owner_frame);
    auto output = previous;
    std::string error;
    check(!native_addresses::resolve_file(scanner, input.profile, output, error)
        && !error.empty() && same_candidate(output, previous), description);
    if (!error_fragment.empty())
        check(error.find(error_fragment) != std::string::npos, "refusal identifies the intended certificate gate");
    check(input.bytes == original_bytes && std::equal(scanner.bytes().begin(), scanner.bytes().end(), input.bytes.begin())
        && game_layout::current_id() == selected_layout && game_layout::generation() == generation
        && native_addresses::rva(native_symbol::owner_frame) == owner_rva,
        "offline refusal leaves fixture bytes, global native bindings and selected layout untouched");
}

void native_addresses_tests::test_complete_relocation() {
    std::string first_hash;
    for (const bool float_setter : {false, true}) {
        for (const auto text_rva : {0x1000u, 0x9000u}) {
            auto input = make_fixture(text_rva, text_rva == 0x1000 ? 0x140000000 : 0x180000000, float_setter);
            const auto before = input.bytes;
            pattern_scanner scanner;
            check(scanner.load_bytes(input.bytes), "complete adaptive fixture loads in file layout without execution");
            const auto generation = game_layout::generation();
            const auto layout = game_layout::current_id();
            native_addresses::candidate output;
            std::string error;
            const bool resolved = native_addresses::resolve_file(scanner, input.profile, output, error);
            if (!resolved) std::fprintf(stderr, "resolve_file diagnostic: %s\n", error.c_str());
            check(resolved && error.empty() && output.symbols == input.symbols
                && output.layout == layout_id::adaptive && output.participant_stride == 0xa38
                && output.setter_abi == (float_setter ? native_addresses::object_field_abi::float_value
                                                     : native_addresses::object_field_abi::bits),
                "unknown-hash trusted catalog resolves moved code/data and independently certified setter ABI");
            check(output.run_offsets.size() == game_layout::required_run_offsets().size()
                && output.member_offsets.size() == game_layout::required_member_offsets().size()
                && std::any_of(output.run_offsets.begin(), output.run_offsets.end(), [](const layout_offset& item) {
                    return item.legacy_offset == 0x4cc && item.resolved_offset == 0xd80;
                }), "complete canonical map preserves a separately discovered nonuniform damage offset");
            check(output.image_sha256 == signature_profiles::sha256(input.bytes)
                && output.image_sha256 != input.profile.sha256 && !output.verification.empty()
                && before == input.bytes && game_layout::generation() == generation && game_layout::current_id() == layout,
                "adaptive proof records actual unknown digest and verification bytes without publishing a live layout");
            if (first_hash.empty()) first_hash = output.image_sha256;
            else if (text_rva == 0x9000)
                check(output.image_sha256 != first_hash, "relocated fixture produces a distinct digest without old-RVA reuse");
        }
    }
}

void native_addresses_tests::test_incomplete_and_ambiguous() {
    const auto good = make_fixture(0x1000, 0x140000000, false);
    pattern_scanner scanner;
    check(scanner.load_bytes(good.bytes), "atomic rejection starts with a complete fixture");
    native_addresses::candidate previous;
    std::string error;
    check(native_addresses::resolve_file(scanner, good.profile, previous, error), "complete candidate exists before rejection checks");
    auto missing_field = good;
    std::erase_if(missing_field.profile.rules, [](const signature_rule& rule) { return rule.name == "run_4cc"; });
    reject(missing_field, previous, "one missing canonical scalar proof refuses the whole candidate atomically");
    auto missing_native = good;
    std::erase_if(missing_native.profile.rules, [](const signature_rule& rule) { return rule.name == "lifesteal"; });
    reject(missing_native, previous, "missing required getter cannot silently inherit its previous fixed RVA");
    auto ambiguous = good;
    std::copy_n(ambiguous.bytes.begin() + 0x420, 6, ambiguous.bytes.begin() + 0x4300);
    reject(ambiguous, previous, "two native marker matches never select a first address or replace a prior certificate");
    auto interior = good;
    interior.profile.rules[0].resolve = pattern_resolve_mode::match;
    reject(interior, previous, "a unique executable interior address is not accepted as a callable native entry");
    auto aliased = good;
    aliased.profile.rules[1].pattern = aliased.profile.rules[0].pattern;
    reject(aliased, previous, "distinct native roles cannot alias one uniquely matched function entry");
    auto duplicate_rule = good;
    duplicate_rule.profile.rules.push_back(duplicate_rule.profile.rules.front());
    reject(duplicate_rule, previous, "a programmatic catalog cannot bypass duplicate-name rejection");
    auto outside_operand = good;
    for (auto& rule : outside_operand.profile.rules)
        if (rule.name == "run_4cc") rule.displacement_offset = 13;
    reject(outside_operand, previous, "a file-backed field operand beyond the verified anchor is refused atomically",
        "decoded operand is outside the verified instruction anchor");
    auto outside_instruction = good;
    outside_instruction.profile.rules[static_cast<std::size_t>(native_symbol::zone_table)].instruction_size = 14;
    reject(outside_instruction, previous, "a RIP instruction end beyond the verified anchor cannot authorize data",
        "decoded operand is outside the verified instruction anchor");
}

void native_addresses_tests::test_section_and_setter_contract() {
    const auto good = make_fixture(0x1000, 0x140000000, false);
    pattern_scanner scanner;
    check(scanner.load_bytes(good.bytes), "section and ABI rejection starts with a complete fixture");
    native_addresses::candidate previous;
    std::string error;
    check(native_addresses::resolve_file(scanner, good.profile, previous, error), "valid certificate precedes typed ABI mutations");
    auto data_target = good;
    std::copy_n(data_target.bytes.begin() + 0x420, 6, data_target.bytes.begin() + 0x6300);
    std::fill_n(data_target.bytes.begin() + 0x420, 6, std::uint8_t{0x90});
    data_target.profile.rules[0].section = ".rdata";
    data_target.profile.rules[0].resolve = pattern_resolve_mode::match;
    reject(data_target, previous, "a pattern in read-only data cannot certify a native code hook");
    auto false_float = good;
    for (auto& rule : false_float.profile.rules)
        if (rule.name == "guard_set_object_field_bits") rule.name = "guard_set_object_field_float";
    reject(false_float, previous, "renaming an exact bits-setter anchor cannot certify a float calling convention");
    auto competing_abis = good;
    for (const auto& rule : good.profile.rules) {
        if (rule.name != "guard_set_object_field_bits") continue;
        auto extra = rule;
        extra.name = "guard_set_object_field_float";
        competing_abis.profile.rules.push_back(std::move(extra));
        break;
    }
    reject(competing_abis, previous, "two claimed setter modes are rejected instead of selecting one by profile order");
}

void native_addresses_tests::test_automatic_refusal() {
    const auto input = make_fixture(0x9000, 0x180000000, false);
    pattern_scanner scanner;
    check(scanner.load_bytes(input.bytes), "automatic refusal input is a valid unknown PE image");
    native_addresses::candidate output;
    output.image_sha256 = "sentinel certificate";
    output.symbols[0] = 0x1234;
    const auto before = output;
    std::string error;
    check(!native_addresses::resolve_auto(scanner, output, error) && !error.empty() && same_candidate(output, before),
        "automatic runtime admission rejects a toy EXE that matches no embedded trusted catalog without stale output");
}

int native_addresses_tests::run() noexcept {
    try {
        test_complete_relocation();
        test_incomplete_and_ambiguous();
        test_section_and_setter_contract();
        test_automatic_refusal();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Native certificate test exception: %s\n", error.what());
        ++failures_;
    } catch (...) {
        check(false, "unexpected native certificate test exception");
    }
    std::printf("Offline native binding certificates: %s (%d failures)\n", failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

} // namespace azs

int main() { return azs::native_addresses_tests::run(); }
