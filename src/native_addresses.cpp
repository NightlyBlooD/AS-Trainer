#include "native_addresses.hpp"
#include "embedded_signatures.hpp"
#include "game_layout.hpp"
#include "pattern_scanner.hpp"
#include "signature_profile.hpp"

#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <utility>
#include <limits>
#include <unordered_set>

namespace azs {
namespace {
constexpr std::array<std::uint32_t, static_cast<std::size_t>(native_symbol::count)> legacy_rvas = {
    0x5cbb3e0, 0x5caaf00, 0x5b54d60, 0x60c5a60, 0x5ce2c10,
    0x5ca6240, 0x5c9e280, 0x5b4cc40, 0x5d2d750, 0x5cc4370,
    0x60f4e60, 0x5d5f1b7, 0x5d1978f, 0x5cd87f1, 0x70a2e08, 0
};
constexpr std::array<const char*, static_cast<std::size_t>(native_symbol::count)> symbol_names = {
    "owner_frame", "recalculate", "create_speed_packet", "grow_speed_packets", "swap_player",
    "flush_updates", "route_queues", "set_object_field", "apply_choice", "boss_loot",
    "grow_loot", "luck_reader", "area_reader", "pickup_reader", "zone_table", "lifesteal"
};
constexpr const char* legacy_hash = "5dd6819d836748ccf809ab76fe6509c9ee793e087939b5d036c430d835a14e63";
constexpr const char* current_hash = "e60b03aae379c9c3c4c728bba1b3dbaeac161007e34a8c09fc61c061f9cceb9c";
}

std::string native_addresses::hash_text(const std::array<unsigned char, 32>& digest) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(64, '0');
    for (std::size_t index = 0; index < digest.size(); ++index) {
        result[index * 2] = digits[digest[index] >> 4];
        result[index * 2 + 1] = digits[digest[index] & 15];
    }
    return result;
}

bool native_addresses::symbol_from_name(std::string_view name, native_symbol& symbol) noexcept {
    for (std::size_t index = 0; index < symbol_names.size(); ++index) {
        if (name == symbol_names[index]) {
            symbol = static_cast<native_symbol>(index);
            return true;
        }
    }
    return false;
}

std::size_t native_addresses::pattern_size(std::string_view pattern) noexcept {
    std::size_t count = 0;
    bool token = false;
    for (const char value : pattern) {
        const bool space = value == ' ' || value == '\t' || value == '\r' || value == '\n';
        if (!space && !token) ++count;
        token = !space;
    }
    return count;
}

bool native_addresses::matching_image(std::uintptr_t module_base, const pattern_scanner& scanner) noexcept {
    const auto raw = scanner.bytes();
    if (raw.size() < 0x40) return false;
    std::uint32_t pe_offset{};
    std::memcpy(&pe_offset, raw.data() + 0x3c, sizeof(pe_offset));
    if (pe_offset > raw.size() || raw.size() - pe_offset < sizeof(IMAGE_NT_HEADERS64)) return false;
    IMAGE_NT_HEADERS64 offline{};
    std::memcpy(&offline, raw.data() + pe_offset, sizeof(offline));
    __try {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module_base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x100000) return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(module_base + dos->e_lfanew);
        return nt->Signature == IMAGE_NT_SIGNATURE && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
            nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
            nt->FileHeader.TimeDateStamp == offline.FileHeader.TimeDateStamp &&
            nt->OptionalHeader.SizeOfImage == scanner.image_size();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool native_addresses::matching_live(std::uintptr_t base, std::uint32_t rva,
    std::span<const std::uint8_t> bytes, std::uint32_t image_size) noexcept {
    if (bytes.empty() || rva >= image_size || bytes.size() > image_size - rva || base > UINTPTR_MAX - rva) return false;
    auto cursor = base + rva;
    if (bytes.size() > UINTPTR_MAX - cursor) return false;
    const auto end = cursor + bytes.size();
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region) ||
            region.State != MEM_COMMIT || region.Type != MEM_IMAGE || (region.Protect & PAGE_GUARD) != 0) return false;
        const auto protection = region.Protect & 0xff;
        if (protection != PAGE_EXECUTE_READ && protection != PAGE_EXECUTE_READWRITE &&
            protection != PAGE_EXECUTE_WRITECOPY) return false;
        const auto next = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
        if (next <= cursor) return false;
        cursor = next;
    }
    __try {
        return std::memcmp(reinterpret_cast<const void*>(base + rva), bytes.data(), bytes.size()) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}


bool native_addresses::section_range(const pattern_scanner& scanner, std::uint32_t rva,
    std::size_t size, bool executable) noexcept {
    if (size == 0 || scanner.view_rva(rva, size).size() != size) return false;
    for (const auto& section : scanner.sections()) {
        if (rva < section.rva || rva - section.rva >= section.raw_size ||
            size > section.raw_size - (rva - section.rva)) continue;
        const auto flags = section.characteristics;
        if ((flags & IMAGE_SCN_MEM_READ) == 0) return false;
        return executable ? (flags & IMAGE_SCN_MEM_EXECUTE) != 0
            : (flags & (IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE)) == 0;
    }
    return false;
}

bool native_addresses::function_entry(const pattern_scanner& scanner, std::uint32_t rva) noexcept {
    pattern_match entry;
    entry.rva = rva;
    if (!scanner.rva_to_file_offset(rva, entry.file_offset) || !scanner.rva_to_va(rva, entry.va)) return false;
    pattern_resolution function;
    return scanner.resolve(entry, pattern_resolve_mode::function_start, function) && function.rva == rva;
}

bool native_addresses::direct_call_entry(const pattern_scanner& scanner, const signature_rule& rule,
    const pattern_match& match, std::uint32_t target) noexcept {
    if (rule.resolve != pattern_resolve_mode::rip_rel32 || rule.addend != 0 ||
        rule.displacement_offset == 0 ||
        static_cast<std::uint64_t>(rule.displacement_offset) + 4 != rule.instruction_size ||
        rule.displacement_offset - 1 > scanner.image_size() - match.rva) return false;
    const auto call_rva = match.rva + rule.displacement_offset - 1;
    const auto call = scanner.view_rva(call_rva, 5);
    if (call.size() != 5 || call[0] != 0xe8 || !section_range(scanner, call_rva, 5, true)) return false;
    pattern_match call_match;
    call_match.rva = call_rva;
    if (!scanner.rva_to_file_offset(call_rva, call_match.file_offset) ||
        !scanner.rva_to_va(call_rva, call_match.va)) return false;
    pattern_resolution caller, destination;
    return scanner.resolve(call_match, pattern_resolve_mode::function_start, caller) &&
        scanner.resolve(match, rule.resolve, destination, rule.displacement_offset, rule.instruction_size) &&
        destination.rva == target;
}

bool native_addresses::offset_key(std::string_view name, std::string_view prefix,
    std::size_t& offset) noexcept {
    if (!name.starts_with(prefix)) return false;
    name.remove_prefix(prefix.size());
    if (name.starts_with("0x")) name.remove_prefix(2);
    if (name.empty()) return false;
    std::size_t value{};
    const auto result = std::from_chars(name.data(), name.data() + name.size(), value, 16);
    if (result.ec != std::errc{} || result.ptr != name.data() + name.size()) return false;
    offset = value;
    return true;
}

bool native_addresses::matching_live_data(std::uintptr_t base, std::uint32_t rva,
    std::size_t size, std::uint32_t image_size) noexcept {
    if (!size || rva >= image_size || size > image_size - rva || base > UINTPTR_MAX - rva) return false;
    auto cursor = base + rva;
    if (size > UINTPTR_MAX - cursor) return false;
    const auto end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region) ||
            region.State != MEM_COMMIT || region.Type != MEM_IMAGE || (region.Protect & PAGE_GUARD) != 0) return false;
        const auto protection = region.Protect & 0xff;
        if (protection != PAGE_READONLY) return false;
        const auto start = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if (region.RegionSize > UINTPTR_MAX - start) return false;
        const auto next = start + region.RegionSize;
        if (next <= cursor) return false;
        cursor = next;
    }
    return true;
}

bool native_addresses::resolve_file(const pattern_scanner& scanner, const signature_profile& profile,
    candidate& result, std::string& error) noexcept {
    try {
        if (!scanner.valid() || profile.rules.empty() || profile.rules.size() > signature_profiles::maximum_rules) {
            error = "binding catalog or offline PE is invalid";
            return false;
        }
        candidate pending;
        pending.image_sha256 = signature_profiles::sha256(scanner.bytes());
        pending.image_size = scanner.image_size();
        if (pending.image_sha256.empty()) { error = "cannot hash offline binding image"; return false; }
        const bool adaptive = profile.layout == "adaptive";
        if (!adaptive && (profile.layout != "current" || profile.sha256 != current_hash ||
                pending.image_sha256 != current_hash)) {
            error = "exact-build catalog cannot authorize a different executable";
            return false;
        }
        pending.layout = adaptive ? layout_id::adaptive : layout_id::current;
        pending.setter_abi = object_field_abi::float_value;
        pending.participant_stride = adaptive ? 0 : 0xad0;
        std::array<bool, static_cast<std::size_t>(native_symbol::count)> seen{};
        std::array<bool, 3> current_fields{};
        struct rule_proof {
            const signature_rule* rule;
            pattern_match match;
            std::uint32_t target;
        };
        std::array<rule_proof, static_cast<std::size_t>(native_symbol::count)> native_proofs{};
        std::vector<rule_proof> guards;
        struct cached_match { std::string_view pattern, section; pattern_match match; };
        std::vector<cached_match> cache;
        cache.reserve(profile.rules.size());
        guards.reserve(profile.rules.size());
        bool stride_seen = false;
        std::unordered_set<std::string_view> rule_names;
        rule_names.reserve(profile.rules.size());
        const auto add_chunk = [&pending, &scanner](std::uint32_t rva, std::size_t size) {
            const auto found = std::find_if(pending.verification.begin(), pending.verification.end(),
                [rva, size](const verification_chunk& item) { return item.rva == rva && item.bytes.size() == size; });
            if (found == pending.verification.end()) {
                const auto bytes = scanner.view_rva(rva, size);
                pending.verification.push_back({rva, {bytes.begin(), bytes.end()}});
            }
        };
        for (const auto& rule : profile.rules) {
            if (!rule_names.emplace(rule.name).second) { error = "duplicate native binding rule name"; return false; }
            pattern_match match;
            const auto cached = std::find_if(cache.begin(), cache.end(), [&rule](const cached_match& item) {
                return item.pattern == rule.pattern && item.section == rule.section;
            });
            if (cached != cache.end()) match = cached->match;
            else {
                if (!scanner.find_unique(rule.pattern, match, rule.section)) {
                    error = rule.name + ": " + std::string(scanner.error());
                    return false;
                }
                cache.push_back({rule.pattern, rule.section, match});
            }
            const auto size = pattern_size(rule.pattern);
            const bool operand = rule.resolve == pattern_resolve_mode::rip_rel32 ||
                rule.resolve == pattern_resolve_mode::field_disp32;
            if (operand && (static_cast<std::uint64_t>(rule.displacement_offset) + 4 > size ||
                (rule.resolve == pattern_resolve_mode::rip_rel32 && rule.instruction_size > size))) {
                error = rule.name + ": decoded operand is outside the verified instruction anchor";
                return false;
            }
            pattern_resolution target;
            if (!scanner.resolve(match, rule.resolve, target, rule.displacement_offset, rule.instruction_size)) {
                error = rule.name + ": " + std::string(scanner.error());
                return false;
            }
            if (!section_range(scanner, match.rva, size, true)) {
                error = rule.name + ": binding anchor is not complete readable executable image code";
                return false;
            }
            add_chunk(match.rva, size);
            native_symbol symbol{};
            if (symbol_from_name(rule.name, symbol)) {
                if (rule.resolve == pattern_resolve_mode::field_disp32) {
                    error = rule.name + ": native address cannot be a field displacement";
                    return false;
                }
                const auto adjusted = static_cast<std::int64_t>(target.rva) + rule.addend;
                if (adjusted < 0 || adjusted >= scanner.image_size()) {
                    error = rule.name + ": native target is outside the image";
                    return false;
                }
                const auto index = static_cast<std::size_t>(symbol);
                if (seen[index]) { error = "duplicate required native symbol"; return false; }
                seen[index] = true;
                pending.symbols[index] = static_cast<std::uint32_t>(adjusted);
                native_proofs[index] = {&rule, match, pending.symbols[index]};
                const auto bytes = symbol == native_symbol::zone_table ? 12u * 0xf0u : 24u;
                if (!section_range(scanner, pending.symbols[index], bytes, symbol != native_symbol::zone_table)) {
                    error = rule.name + ": target has an incompatible image section or truncated range";
                    return false;
                }
                if (symbol != native_symbol::zone_table) add_chunk(pending.symbols[index], bytes);
                continue;
            }
            if (adaptive && rule.name.starts_with("guard_")) {
                if (rule.resolve == pattern_resolve_mode::field_disp32 || rule.addend != 0) {
                    error = rule.name + ": ABI guards cannot be numeric field aliases";
                    return false;
                }
                guards.push_back({&rule, match, target.rva});
                continue;
            }
            if (!adaptive) {
                constexpr std::array<std::string_view, 3> names{"field_luck", "field_area", "field_pickup"};
                constexpr std::array<std::int32_t, 3> offsets{0x4fc, 0x4f8, 0x4e8};
                bool accepted = false;
                for (std::size_t index = 0; index < names.size(); ++index) {
                    if (rule.name == names[index] && !current_fields[index] &&
                        rule.resolve == pattern_resolve_mode::field_disp32 &&
                        target.field_displacement == offsets[index] && rule.addend == 0) {
                        current_fields[index] = true;
                        accepted = true;
                        break;
                    }
                }
                if (!accepted) { error = rule.name + ": unknown or incompatible exact-build field rule"; return false; }
                continue;
            }
            const auto offset = static_cast<std::int64_t>(target.field_displacement) + rule.addend;
            if (rule.resolve != pattern_resolve_mode::field_disp32 || offset < 0 || offset >= 0x10000) {
                error = rule.name + ": adaptive field requires a bounded typed instruction displacement";
                return false;
            }
            if (rule.name == "participant_stride") {
                if (stride_seen) { error = "duplicate participant stride"; return false; }
                stride_seen = true;
                pending.participant_stride = static_cast<std::size_t>(offset);
                continue;
            }
            std::size_t key{};
            auto* destination = &pending.run_offsets;
            auto required = game_layout::required_run_offsets();
            if (!offset_key(rule.name, "run_", key)) {
                destination = &pending.member_offsets;
                required = game_layout::required_member_offsets();
                if (!offset_key(rule.name, "member_", key)) {
                    error = rule.name + ": unknown adaptive binding rule";
                    return false;
                }
            }
            if (std::find(required.begin(), required.end(), key) == required.end() ||
                std::any_of(destination->begin(), destination->end(), [key](const layout_offset& item) {
                    return item.legacy_offset == key;
                })) {
                error = rule.name + ": unknown or duplicate canonical field key";
                return false;
            }
            destination->push_back({key, static_cast<std::size_t>(offset)});
        }
        for (const auto present : seen) if (!present) { error = "required native symbol is missing"; return false; }
        for (std::size_t index = 0; index < pending.symbols.size(); ++index)
            for (std::size_t previous = 0; previous < index; ++previous)
                if (pending.symbols[index] == pending.symbols[previous]) {
                    error = "distinct native bindings resolve to the same address";
                    return false;
                }
        if (!adaptive) {
            for (const auto present : current_fields)
                if (!present) { error = "required exact-build field fingerprint is missing"; return false; }
        } else {
            for (auto* map : {&pending.run_offsets, &pending.member_offsets}) {
                if (std::none_of(map->begin(), map->end(), [](const layout_offset& item) { return item.legacy_offset == 0; }))
                    map->push_back({0, 0});
                std::sort(map->begin(), map->end(), [](const layout_offset& left, const layout_offset& right) {
                    return left.legacy_offset < right.legacy_offset;
                });
            }
            if (!stride_seen || !game_layout::validate_resolved(pending.run_offsets,
                    pending.member_offsets, pending.participant_stride)) {
                error = "adaptive layout is incomplete, colliding, unaligned or structurally inconsistent";
                return false;
            }
            constexpr std::array<std::string_view, 9> required_guards{
                "guard_owner_frame_abi", "guard_recalculate_abi", "guard_speed_packet_abi",
                "guard_speed_queue_abi", "guard_swap_player_abi", "guard_choice_record",
                "guard_loot_record", "guard_zone_record", "guard_lifesteal_abi"
            };
            std::array<bool, required_guards.size()> guard_seen{};
            constexpr std::array<std::string_view, 4> choice_payload_guards{
                "guard_choice_payload", "guard_choice_payload_rarity",
                "guard_choice_payload_custom", "guard_choice_payload_effects"
            };
            std::array<bool, choice_payload_guards.size()> choice_payload_seen{};
            bool bits_seen = false, float_seen = false;
            for (const auto& guard : guards) {
                const auto name = std::string_view(guard.rule->name);
                const auto payload = std::find(choice_payload_guards.begin(), choice_payload_guards.end(), name);
                if (payload != choice_payload_guards.end())
                    choice_payload_seen[static_cast<std::size_t>(payload - choice_payload_guards.begin())] = true;
                const auto found = std::find(required_guards.begin(), required_guards.end(), name);
                if (found != required_guards.end()) {
                    const auto index = static_cast<std::size_t>(found - required_guards.begin());
                    if (guard_seen[index]) { error = "duplicate ABI contract guard"; return false; }
                    guard_seen[index] = true;
                } else if (name == "guard_set_object_field_bits") {
                    if (bits_seen) { error = "duplicate object-field bits ABI guard"; return false; }
                    bits_seen = true;
                } else if (name == "guard_set_object_field_float") {
                    if (float_seen) { error = "duplicate object-field float ABI guard"; return false; }
                    float_seen = true;
                } else if (name != "guard_set_object_field_abi" && name != "guard_swap_player_entry" &&
                    name != "guard_lifesteal_entry" && payload == choice_payload_guards.end()) {
                    error = guard.rule->name + ": unknown ABI guard";
                    return false;
                }
                native_symbol linked{};
                bool has_link = true;
                if (name == "guard_owner_frame_abi") linked = native_symbol::owner_frame;
                else if (name == "guard_recalculate_abi") linked = native_symbol::recalculate;
                else if (name == "guard_speed_packet_abi") linked = native_symbol::create_speed_packet;
                else if (name == "guard_swap_player_abi" || name == "guard_swap_player_entry") linked = native_symbol::swap_player;
                else if (name == "guard_lifesteal_abi" || name == "guard_lifesteal_entry") linked = native_symbol::lifesteal;
                else if (name.starts_with("guard_set_object_field_")) linked = native_symbol::set_object_field;
                else if (name == "guard_choice_record" || payload != choice_payload_guards.end()) linked = native_symbol::apply_choice;
                else has_link = false;
                if (has_link) {
                    const auto address = pending.symbols[static_cast<std::size_t>(linked)];
                    pattern_resolution owner;
                    const bool at_entry = guard.match.rva == address;
                    const bool in_function = scanner.resolve(guard.match, pattern_resolve_mode::function_start, owner) &&
                        owner.rva == address;
                    const bool call_target = direct_call_entry(scanner, *guard.rule, guard.match, address);
                    if ((!at_entry && !in_function && !call_target) ||
                        ((name == "guard_swap_player_entry" || name == "guard_lifesteal_entry") && !call_target)) {
                        error = guard.rule->name + ": ABI guard is not linked to its native function";
                        return false;
                    }
                }
            }
            const bool split_choice_payload = std::any_of(choice_payload_seen.begin() + 1,
                choice_payload_seen.end(), [](bool present) { return present; });
            if (split_choice_payload && std::any_of(choice_payload_seen.begin(), choice_payload_seen.end(),
                    [](bool present) { return !present; })) {
                error = "split choice payload certificate is incomplete";
                return false;
            }
            for (const auto present : guard_seen) if (!present) { error = "required adaptive ABI guard is missing"; return false; }
            if (bits_seen == float_seen) { error = "adaptive object-field ABI needs exactly one typed mode guard"; return false; }
            pending.setter_abi = bits_seen ? object_field_abi::bits : object_field_abi::float_value;
            constexpr std::array<std::uint8_t, 21> bits_prefix{
                0x56,0x57,0x53,0x48,0x83,0xec,0x70,0x44,0x89,0xc7,0x89,0xd3,
                0x48,0x89,0xce,0x48,0x8b,0x01,0x48,0x85,0xc0};
            constexpr std::array<std::uint8_t, 14> float_prefix{
                0x56,0x48,0x83,0xec,0x70,0x48,0x89,0xce,0x48,0x8b,0x01,0x48,0x85,0xc0};
            const auto expected = bits_seen ? std::span<const std::uint8_t>(bits_prefix)
                                           : std::span<const std::uint8_t>(float_prefix);
            const auto actual = scanner.view_rva(pending.symbols[static_cast<std::size_t>(native_symbol::set_object_field)], expected.size());
            if (actual.size() != expected.size() || !std::equal(actual.begin(), actual.end(), expected.begin())) {
                error = "typed object-field guard contradicts the verified value argument instruction ABI";
                return false;
            }
        }
        for (std::size_t index = 0; index < seen.size(); ++index) {
            const auto symbol = static_cast<native_symbol>(index);
            if (symbol == native_symbol::zone_table || symbol == native_symbol::luck_reader ||
                symbol == native_symbol::area_reader || symbol == native_symbol::pickup_reader) continue;
            const auto address = pending.symbols[index];
            if (function_entry(scanner, address)) continue;
            const bool leaf = symbol == native_symbol::swap_player || symbol == native_symbol::lifesteal;
            if (!leaf) { error = std::string(symbol_names[index]) + ": target is not a verified unwind function entry"; return false; }
            if (!adaptive) continue;
            const auto& native = native_proofs[index];
            bool entry_proved = direct_call_entry(scanner, *native.rule, native.match, address);
            const auto entry_name = symbol == native_symbol::swap_player ? "guard_swap_player_entry" : "guard_lifesteal_entry";
            for (const auto& guard : guards)
                if (guard.rule->name == entry_name)
                    entry_proved = entry_proved || direct_call_entry(scanner, *guard.rule, guard.match, address);
            if (!entry_proved) {
                error = std::string(symbol_names[index]) + ": leaf entry lacks a certified native direct-call target";
                return false;
            }
        }
        result = std::move(pending);
        error.clear();
        return true;
    } catch (...) {
        try { error = "cannot allocate or validate the bounded native binding certificate"; } catch (...) {}
        return false;
    }
}

bool native_addresses::resolve_auto(const pattern_scanner& scanner, candidate& result,
    std::string& error) noexcept {
    try {
        if (!scanner.valid()) { error = "offline PE is not valid"; return false; }
        const auto hash = signature_profiles::sha256(scanner.bytes());
        if (hash.empty()) { error = "cannot calculate offline executable identity"; return false; }
        if (hash == legacy_hash) {
            candidate pending;
            pending.symbols = legacy_rvas;
            pending.image_sha256 = hash;
            pending.image_size = scanner.image_size();
            pending.participant_stride = 0xa38;
            result = std::move(pending);
            error.clear();
            return true;
        }
        const bool exact = hash == current_hash;
        const std::array catalogs{exact ? current_signature_profile : adaptive_signature_profile,
            exact ? current_signature_profile : previous_adaptive_signature_profile};
        std::string first_error;
        for (std::size_t index = 0; index < (exact ? 1u : catalogs.size()); ++index) {
            signature_profile profile;
            if (!signature_profiles::parse(catalogs[index], profile, error)) return false;
            if (profile.layout != (exact ? "current" : "adaptive")) {
                error = "embedded catalog has an incompatible binding policy";
                return false;
            }
            if (resolve_file(scanner, profile, result, error)) return true;
            if (index == 0) first_error = error;
        }
        if (!exact) error = "adaptive: " + first_error + "; previous adaptive: " + error;
        return false;
    } catch (...) {
        try { error = "automatic native binding selection failed"; } catch (...) {}
        return false;
    }
}

bool native_addresses::initialize(std::uintptr_t module_base, const std::filesystem::path& executable_path,
    const std::array<unsigned char, 32>& digest, std::string& error) noexcept {
    try {
        pattern_scanner scanner;
        if (!scanner.load_file(executable_path)) { error = std::string(scanner.error()); return false; }
        candidate pending;
        if (!resolve_auto(scanner, pending, error)) return false;
        if (pending.image_sha256 != hash_text(digest) || !matching_image(module_base, scanner)) {
            error = "on-disk executable and running image identity differ";
            return false;
        }
        for (const auto& chunk : pending.verification) {
            if (!matching_live(module_base, chunk.rva, chunk.bytes, pending.image_size)) {
                error = "live code differs from its complete offline binding certificate";
                return false;
            }
        }
        if (pending.layout != layout_id::legacy &&
            !matching_live_data(module_base, pending.symbols[static_cast<std::size_t>(native_symbol::zone_table)],
                12u * 0xf0u, pending.image_size)) {
            error = "native zone table is not intact read-only image data";
            return false;
        }
        const bool layout_ready = pending.layout == layout_id::adaptive
            ? game_layout::select_resolved(pending.run_offsets, pending.member_offsets, pending.participant_stride)
            : game_layout::select(pending.layout);
        if (!layout_ready) { error = "native typed layout publication was refused"; return false; }
        resolved_ = pending.symbols;
        verification_ = std::move(pending.verification);
        module_base_ = module_base;
        image_size_ = pending.image_size;
        patterns_ = pending.layout != layout_id::legacy;
        current_abi_ = pending.layout != layout_id::legacy;
        setter_abi_ = pending.setter_abi;
        initialized_ = true;
        error.clear();
        return true;
    } catch (...) {
        try { error = "native signature resolution failed"; } catch (...) {}
        return false;
    }
}

std::uint32_t native_addresses::rva(native_symbol symbol) noexcept {
    const auto index = static_cast<std::size_t>(symbol);
    if (index >= legacy_rvas.size()) return 0;
    return initialized_ ? resolved_[index] : legacy_rvas[index];
}

std::string_view native_addresses::symbol_name(native_symbol symbol) noexcept {
    const auto index = static_cast<std::size_t>(symbol);
    return index < symbol_names.size() ? std::string_view(symbol_names[index]) : std::string_view{};
}

bool native_addresses::uses_patterns() noexcept { return patterns_; }
bool native_addresses::uses_current_abi() noexcept { return current_abi_; }
bool native_addresses::object_field_uses_float() noexcept { return setter_abi_ == object_field_abi::float_value; }
bool native_addresses::supports_lifesteal() noexcept { return initialized_ && rva(native_symbol::lifesteal) != 0; }

bool native_addresses::verify_all() noexcept {
    if (!patterns_ || verification_.empty()) return false;
    for (const auto& chunk : verification_)
        if (!matching_live(module_base_, chunk.rva, chunk.bytes, image_size_)) return false;
    return matching_live_data(module_base_, rva(native_symbol::zone_table), 12u * 0xf0u, image_size_);
}

} // namespace azs
