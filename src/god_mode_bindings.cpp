#include "god_mode_bindings.hpp"
#include "pattern_scanner.hpp"
#include "signature_profile.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace azs {

bool god_mode_bindings::section_range(const pattern_scanner& scanner, std::uint32_t rva,
    std::size_t size, bool executable) noexcept {
    if (size == 0 || scanner.view_rva(rva, size).size() != size) return false;
    for (const auto& section : scanner.sections()) {
        const auto span = static_cast<std::uint64_t>(section.raw_size);
        if (rva < section.rva || static_cast<std::uint64_t>(rva) - section.rva >= span ||
            size > span - (static_cast<std::uint64_t>(rva) - section.rva)) continue;
        constexpr std::uint32_t readable = 0x40000000, writable = 0x80000000, code = 0x20000000;
        return (section.characteristics & readable) != 0 &&
            (section.characteristics & writable) == 0 &&
            ((section.characteristics & code) != 0) == executable;
    }
    return false;
}

bool god_mode_bindings::matching_pattern(const pattern_scanner& scanner, std::uint32_t rva,
    std::string_view pattern, std::size_t& size) noexcept {
    std::size_t cursor = 0, count = 0;
    for (;;) {
        while (cursor < pattern.size() && (pattern[cursor] == ' ' || pattern[cursor] == '\t')) ++cursor;
        if (cursor == pattern.size()) break;
        const auto start = cursor;
        while (cursor < pattern.size() && pattern[cursor] != ' ' && pattern[cursor] != '\t') ++cursor;
        const auto token = pattern.substr(start, cursor - start);
        const bool wildcard = token == "?" || token == "??";
        unsigned expected = 0;
        if (!wildcard) {
            if (token.size() != 2) return false;
            for (const char character : token) {
                unsigned nibble{};
                if (character >= '0' && character <= '9') nibble = character - '0';
                else if (character >= 'a' && character <= 'f') nibble = character - 'a' + 10;
                else if (character >= 'A' && character <= 'F') nibble = character - 'A' + 10;
                else return false;
                expected = expected * 16 + nibble;
            }
        }
        if (++count > 4096 || count - 1 > std::numeric_limits<std::uint32_t>::max() - rva) return false;
        const auto byte = scanner.view_rva(rva + static_cast<std::uint32_t>(count - 1), 1);
        if (byte.empty() || (!wildcard && byte[0] != expected)) return false;
    }
    if (count == 0) return false;
    size = count;
    return true;
}

bool god_mode_bindings::reference_target(std::span<const std::uint8_t> bytes,
    std::uint32_t function_rva, const relocation_rule& rule, std::uint32_t& target) noexcept {
    if (rule.operand_offset > bytes.size() || bytes.size() - rule.operand_offset < 4 ||
        rule.instruction_end < static_cast<std::uint64_t>(rule.operand_offset) + 4 ||
        rule.instruction_end > bytes.size()) return false;
    std::int32_t displacement{};
    std::memcpy(&displacement, bytes.data() + rule.operand_offset, sizeof(displacement));
    const auto resolved = static_cast<std::int64_t>(function_rva) + rule.instruction_end + displacement;
    if (resolved < 0 || resolved > std::numeric_limits<std::uint32_t>::max()) return false;
    target = static_cast<std::uint32_t>(resolved);
    return true;
}

bool god_mode_bindings::read_u32(const pattern_scanner& scanner, std::uint32_t rva,
    std::uint32_t& value) noexcept {
    const auto bytes = scanner.view_rva(rva, sizeof(value));
    if (bytes.size() != sizeof(value)) return false;
    std::memcpy(&value, bytes.data(), sizeof(value));
    return true;
}

bool god_mode_bindings::read_u64(const pattern_scanner& scanner, std::uint32_t rva,
    std::uint64_t& value) noexcept {
    const auto bytes = scanner.view_rva(rva, sizeof(value));
    if (bytes.size() != sizeof(value)) return false;
    std::memcpy(&value, bytes.data(), sizeof(value));
    return true;
}

bool god_mode_bindings::read_ascii(const pattern_scanner& scanner, std::uint32_t rva,
    std::size_t maximum, std::string& value) {
    std::string pending;
    for (std::size_t index = 0; index < maximum; ++index) {
        if (index > std::numeric_limits<std::uint32_t>::max() - rva) return false;
        const auto byte = scanner.view_rva(rva + static_cast<std::uint32_t>(index), 1);
        if (byte.empty()) return false;
        if (byte[0] == 0) {
            if (pending.empty()) return false;
            value = std::move(pending);
            return true;
        }
        if (byte[0] < 0x20 || byte[0] > 0x7e) return false;
        pending += static_cast<char>(byte[0]);
    }
    return false;
}

bool god_mode_bindings::imported_symbol(const pattern_scanner& scanner, std::uint32_t iat_rva,
    std::string& identity) {
    const auto file = scanner.bytes();
    if (file.size() < 0x40) return false;
    std::uint32_t nt_offset{};
    std::memcpy(&nt_offset, file.data() + 0x3c, sizeof(nt_offset));
    constexpr std::size_t import_directory_offset = 0x90;
    if (nt_offset > file.size() || file.size() - nt_offset < import_directory_offset + 8) return false;
    std::uint32_t directory{}, directory_size{};
    std::memcpy(&directory, file.data() + nt_offset + import_directory_offset, 4);
    std::memcpy(&directory_size, file.data() + nt_offset + import_directory_offset + 4, 4);
    if (directory == 0 || directory_size < 20 || directory_size > 1024 * 1024 ||
        directory >= scanner.image_size() || directory_size > scanner.image_size() - directory) return false;
    const auto count = std::min<std::uint32_t>(directory_size / 20, 1024);
    for (std::uint32_t index = 0; index < count; ++index) {
        if (index * 20 > std::numeric_limits<std::uint32_t>::max() - directory) return false;
        const auto descriptor = directory + index * 20;
        std::uint32_t original_thunk{}, name_rva{}, first_thunk{};
        if (!read_u32(scanner, descriptor, original_thunk) || !read_u32(scanner, descriptor + 12, name_rva) ||
            !read_u32(scanner, descriptor + 16, first_thunk)) return false;
        if (name_rva == 0 || first_thunk == 0) break;
        if (iat_rva < first_thunk || (iat_rva - first_thunk) % 8 != 0) continue;
        const auto slot = (iat_rva - first_thunk) / 8;
        if (slot >= 4096) continue;
        const auto lookup = original_thunk != 0 ? original_thunk : first_thunk;
        if (static_cast<std::uint64_t>(lookup) + slot * 8 > std::numeric_limits<std::uint32_t>::max()) return false;
        std::uint64_t thunk{};
        bool belongs = true;
        for (std::uint32_t entry = 0; entry <= slot; ++entry) {
            if (!read_u64(scanner, lookup + entry * 8, thunk)) return false;
            if (thunk == 0) { belongs = false; break; }
        }
        if (!belongs) continue;
        std::string module, symbol;
        if (!read_ascii(scanner, name_rva, 256, module)) return false;
        if ((thunk & (std::uint64_t{1} << 63)) != 0) symbol = "#" + std::to_string(thunk & 0xffff);
        else {
            if (thunk > std::numeric_limits<std::uint32_t>::max() - 2 ||
                !read_ascii(scanner, static_cast<std::uint32_t>(thunk) + 2, 512, symbol)) return false;
        }
        identity = module + "!" + symbol;
        return true;
    }
    return false;
}

bool god_mode_bindings::unique_anchor(const pattern_scanner& scanner, std::string_view pattern,
    std::uint32_t& result, resolution_cache& cache) {
    const auto found = std::find_if(cache.begin(), cache.end(),
        [pattern](const cached_anchor& entry) { return entry.pattern == pattern; });
    if (found != cache.end()) {
        if (found->unique) result = found->rva;
        return found->unique;
    }
    pattern_match match;
    const bool unique = scanner.find_unique(pattern, match);
    cache.push_back({pattern, match.rva, unique});
    if (unique) result = match.rva;
    return unique;
}

bool god_mode_bindings::matching_callee(const pattern_scanner& scanner, std::uint32_t target,
    const relocation_rule& rule, std::vector<verification_chunk>& verification,
    resolution_cache& cache, std::size_t depth) {
    std::array<std::uint32_t, 8> visited{};
    for (std::size_t hop = 0; hop < visited.size(); ++hop) {
        if (std::find(visited.begin(), visited.begin() + hop, target) != visited.begin() + hop) return false;
        visited[hop] = target;
        const auto bytes = scanner.view_rva(target, 6);
        if (bytes.empty() || !section_range(scanner, target, bytes.size(), true)) return false;
        if (bytes[0] == 0xe9) {
            std::int32_t displacement{};
            std::memcpy(&displacement, bytes.data() + 1, sizeof(displacement));
            const auto next = static_cast<std::int64_t>(target) + 5 + displacement;
            if (next < 0 || next > std::numeric_limits<std::uint32_t>::max()) return false;
            add_chunk(scanner, target, 5, true, verification);
            target = static_cast<std::uint32_t>(next);
            continue;
        }
        if (bytes[0] == 0xff && bytes[1] == 0x25) {
            if (rule.target_import.empty()) return false;
            std::int32_t displacement{};
            std::memcpy(&displacement, bytes.data() + 2, sizeof(displacement));
            const auto slot = static_cast<std::int64_t>(target) + 6 + displacement;
            std::string identity;
            if (slot < 0 || slot > std::numeric_limits<std::uint32_t>::max() ||
                !imported_symbol(scanner, static_cast<std::uint32_t>(slot), identity) ||
                identity != rule.target_import) return false;
            add_chunk(scanner, target, 6, true, verification);
            return true;
        }
        if (!rule.target_import.empty()) return false;
        if (rule.target_certificate != nullptr) {
            std::uint32_t entry{};
            std::string error;
            return resolve_function(scanner, *rule.target_certificate, entry, verification, error,
                cache, target, depth + 1) && entry == target;
        }
        std::size_t anchor_size{};
        if (!matching_pattern(scanner, target, rule.target_pattern, anchor_size) ||
            !section_range(scanner, target, anchor_size, true)) return false;
        if (rule.target_pattern != "C3") {
            std::uint32_t unique_rva{};
            if (!unique_anchor(scanner, rule.target_pattern, unique_rva, cache) || unique_rva != target)
                return false;
        }
        if (rule.require_function_entry) {
            std::uint32_t begin{}, end{};
            if (!scanner.function_range(target, begin, end) || begin != target || anchor_size > end - begin)
                return false;
        }
        add_chunk(scanner, target, anchor_size, true, verification);
        return true;
    }
    return false;
}

void god_mode_bindings::add_chunk(const pattern_scanner& scanner, std::uint32_t rva,
    std::size_t size, bool executable, std::vector<verification_chunk>& verification) {
    const auto found = std::find_if(verification.begin(), verification.end(),
        [rva, size, executable](const verification_chunk& entry) {
            return entry.rva == rva && entry.bytes.size() == size && entry.executable == executable &&
                !entry.relocated_pointer;
        });
    if (found != verification.end()) return;
    const auto bytes = scanner.view_rva(rva, size);
    verification.push_back({rva, {bytes.begin(), bytes.end()}, executable});
}

bool god_mode_bindings::resolve_function(const pattern_scanner& scanner,
    const function_certificate& certificate, std::uint32_t& rva,
    std::vector<verification_chunk>& verification, std::string& error,
    resolution_cache& cache, std::uint32_t known_rva, std::size_t depth) {
    if (certificate.size == 0 || certificate.size > 128 * 1024 ||
        certificate.normalized_sha256.size() != 64 || certificate.relocations.size() > 1024 || depth > 4 ||
        (depth != 0 && certificate.size > 4096)) {
        error = "God mode function certificate is malformed";
        return false;
    }
    std::uint32_t entry = known_rva;
    std::size_t entry_width{};
    if (known_rva != 0 ? !matching_pattern(scanner, entry, certificate.entry_pattern, entry_width) :
        !unique_anchor(scanner, certificate.entry_pattern, entry, cache)) {
        error = "God mode entry signature is missing or ambiguous";
        return false;
    }
    std::uint32_t begin{}, end{};
    if (!scanner.function_range(entry, begin, end) || begin != entry ||
        end - begin != certificate.size || !section_range(scanner, begin, certificate.size, true)) {
        error = "God mode function entry, unwind bounds or code section changed";
        return false;
    }
    const auto bytes = scanner.view_rva(begin, certificate.size);
    std::vector<std::uint8_t> normalized(bytes.begin(), bytes.end());
    std::vector<bool> masked(bytes.size());
    for (const auto& rule : certificate.relocations) {
        std::uint32_t target{};
        if (!reference_target(bytes, begin, rule, target)) {
            error = "God mode address operand is outside its certified instruction";
            return false;
        }
        if (target >= begin && target < end) {
            error = "God mode internal code references cannot be masked as relocations";
            return false;
        }
        for (std::size_t offset = rule.operand_offset; offset < rule.operand_offset + 4; ++offset) {
            if (masked[offset]) {
                error = "God mode address operands overlap";
                return false;
            }
            masked[offset] = true;
            normalized[offset] = 0;
        }
        if (rule.kind == reference_kind::relative_call) {
            if (rule.operand_offset == 0 || bytes[rule.operand_offset - 1] != 0xe8 ||
                rule.instruction_end != rule.operand_offset + 4 || rule.width != 0 ||
                !rule.executable_target || (target >= begin && target < end)) {
                error = "God mode external call is not a certified direct E8 instruction";
                return false;
            }
            if (!matching_callee(scanner, target, rule, verification, cache, depth)) {
                error = "God mode native callee anchor or code section changed";
                return false;
            }
        } else if (rule.kind == reference_kind::rip_data) {
            if (rule.width == 0 || rule.width > rule.expected_value.size() || rule.executable_target ||
                !section_range(scanner, target, rule.width, false)) {
                error = "God mode scalar constant is not immutable file-backed data";
                return false;
            }
            const auto value = scanner.view_rva(target, rule.width);
            if (std::memcmp(value.data(), rule.expected_value.data(), rule.width) != 0) {
                error = "God mode damage or spell-suppression constant changed";
                return false;
            }
            add_chunk(scanner, target, rule.width, false, verification);
        } else if (rule.kind == reference_kind::rip_address) {
            std::size_t target_size{};
            if (rule.executable_target ? !matching_callee(scanner, target, rule, verification, cache, depth) :
                (!matching_pattern(scanner, target, rule.target_pattern, target_size) ||
                 !section_range(scanner, target, target_size, false))) {
                error = "God mode referenced address has an incompatible section";
                return false;
            }
            if (!rule.executable_target) add_chunk(scanner, target, target_size, false, verification);
        } else if (rule.kind == reference_kind::rip_jump_table) {
            if (rule.width == 0 || rule.width > 1024 || rule.width % 4 != 0 ||
                rule.target_digest.size() != 64 || !section_range(scanner, target, rule.width, false)) {
                error = "God mode jump table metadata or immutable data section changed";
                return false;
            }
            const auto table = scanner.view_rva(target, rule.width);
            std::vector<std::uint8_t> normalized_table(table.size());
            for (std::size_t offset = 0; offset < table.size(); offset += 4) {
                std::int32_t displacement{};
                std::memcpy(&displacement, table.data() + offset, sizeof(displacement));
                const auto branch = static_cast<std::int64_t>(target) + displacement;
                if (branch < begin || branch >= end) {
                    error = "God mode jump table branches outside its certified function";
                    return false;
                }
                const auto relative = static_cast<std::uint32_t>(branch - begin);
                std::memcpy(normalized_table.data() + offset, &relative, sizeof(relative));
            }
            if (signature_profiles::sha256(normalized_table) != rule.target_digest) {
                error = "God mode jump-table control flow changed";
                return false;
            }
            add_chunk(scanner, target, rule.width, false, verification);
        } else if (rule.kind == reference_kind::rip_inactive) {
            if (rule.width == 0 || !section_range(scanner, target, rule.width, rule.executable_target)) {
                error = "God mode gated diagnostic or inactive spell data has an incompatible section";
                return false;
            }
        } else if (rule.kind == reference_kind::rip_import) {
            std::string identity;
            if (rule.target_import.empty() || !imported_symbol(scanner, target, identity) ||
                identity != rule.target_import) {
                error = "God mode imported helper identity changed";
                return false;
            }
        } else if (rule.kind == reference_kind::rip_string_table) {
            if (rule.width < 24 || rule.width > 264 || (rule.width - 8) % 16 != 0 ||
                rule.target_digest.size() != 64 || !section_range(scanner, target, rule.width, false)) {
                error = "God mode stat descriptor has an incompatible immutable layout";
                return false;
            }
            const auto descriptor = scanner.view_rva(target, rule.width);
            std::vector<std::uint8_t> normalized_descriptor(descriptor.begin(), descriptor.end());
            const auto views = (rule.width - 8) / 16;
            for (std::size_t index = 0; index < views; ++index) {
                std::uint64_t pointer{}, length{};
                std::memcpy(&pointer, descriptor.data() + index * 16, 8);
                std::memcpy(&length, descriptor.data() + index * 16 + 8, 8);
                if (pointer < scanner.image_base() || pointer - scanner.image_base() >
                    std::numeric_limits<std::uint32_t>::max() || length == 0 || length > 512) {
                    error = "God mode stat string view is malformed";
                    return false;
                }
                const auto text_rva = static_cast<std::uint32_t>(pointer - scanner.image_base());
                if (!section_range(scanner, text_rva, static_cast<std::size_t>(length), false)) {
                    error = "God mode stat string is not bounded immutable data";
                    return false;
                }
                std::fill_n(normalized_descriptor.begin() + index * 16, 8, std::uint8_t{});
                if (index != 0) std::fill_n(normalized_descriptor.begin() + index * 16 + 8, 8, std::uint8_t{});
                else {
                    const auto text = scanner.view_rva(text_rva, static_cast<std::size_t>(length));
                    normalized_descriptor.insert(normalized_descriptor.end(), text.begin(), text.end());
                }
                std::vector<std::uint8_t> pointer_rva(8);
                const std::uint64_t wide_rva = text_rva;
                std::memcpy(pointer_rva.data(), &wide_rva, sizeof(wide_rva));
                verification.push_back({target + static_cast<std::uint32_t>(index * 16),
                    std::move(pointer_rva), false, true});
                add_chunk(scanner, target + static_cast<std::uint32_t>(index * 16 + 8), 8, false, verification);
                add_chunk(scanner, text_rva, static_cast<std::size_t>(length), false, verification);
            }
            if (signature_profiles::sha256(normalized_descriptor) != rule.target_digest) {
                error = "God mode stat lookup key or descriptor tag changed";
                return false;
            }
            add_chunk(scanner, target + rule.width - 8, 8, false, verification);
        } else {
            error = "God mode certificate contains an unknown reference kind";
            return false;
        }
    }
    if (signature_profiles::sha256(normalized) != certificate.normalized_sha256) {
        error = "God mode native instructions, field layout, internal branches or ABI changed";
        return false;
    }
    add_chunk(scanner, begin, certificate.size, true, verification);
    rva = begin;
    return true;
}

bool god_mode_bindings::linked_call(const pattern_scanner& scanner, std::uint32_t function_rva,
    const function_certificate& certificate, std::uint32_t operand, std::uint32_t& target) noexcept {
    const auto found = std::find_if(certificate.relocations.begin(), certificate.relocations.end(),
        [operand](const relocation_rule& rule) {
            return rule.operand_offset == operand && rule.kind == reference_kind::relative_call;
        });
    return found != certificate.relocations.end() &&
        reference_target(scanner.view_rva(function_rva, certificate.size), function_rva, *found, target);
}

bool god_mode_bindings::resolve_certificates(const pattern_scanner& scanner,
    std::span<const family_certificate> certificates, candidate& result, std::string& error) noexcept {
    try {
        if (!scanner.valid() || certificates.empty() || certificates.size() > 8) {
            error = "God mode PE or embedded certificate set is invalid";
            return false;
        }
        candidate accepted;
        resolution_cache cache;
        cache.reserve(256);
        std::size_t matched = 0;
        std::string last_error;
        for (const auto& certificate : certificates) {
            candidate pending;
            if (!resolve_function(scanner, certificate.damage, pending.damage_rva,
                    pending.verification, last_error, cache) ||
                !resolve_function(scanner, certificate.dispatcher, pending.dispatcher_rva,
                    pending.verification, last_error, cache)) continue;
            if (pending.damage_rva == pending.dispatcher_rva ||
                certificate.dispatcher_call_operands[0] == certificate.dispatcher_call_operands[1]) {
                last_error = "God mode native functions or required call sites alias";
                continue;
            }
            bool linked = true;
            for (const auto operand : certificate.dispatcher_call_operands) {
                std::uint32_t target{};
                linked = linked && linked_call(scanner, pending.damage_rva, certificate.damage, operand, target) &&
                    target == pending.dispatcher_rva;
            }
            if (!linked) {
                last_error = "God mode damage does not call the certified spell dispatcher";
                continue;
            }
            if (certificate.lifesteal_call_operand != 0xffffffffu &&
                !linked_call(scanner, pending.damage_rva, certificate.damage,
                    certificate.lifesteal_call_operand, pending.lifesteal_rva)) {
                last_error = "God mode damage getter call is missing";
                continue;
            }
            pending.damage_size = certificate.damage.size;
            pending.dispatcher_size = certificate.dispatcher.size;
            pending.enemies_offset = certificate.enemies_offset;
            pending.enemy_stride = certificate.enemy_stride;
            pending.source_offset = certificate.source_offset;
            pending.phase_offset = certificate.phase_offset;
            pending.spells_offset = certificate.spells_offset;
            accepted = std::move(pending);
            if (++matched > 1) {
                error = "God mode embedded certificates are ambiguous";
                return false;
            }
        }
        if (matched == 0) {
            error = last_error.empty() ? "God mode has no compatible instruction certificate" : last_error;
            return false;
        }
        result = std::move(accepted);
        error.clear();
        return true;
    } catch (...) {
        try { error = "God mode certificate resolution failed"; } catch (...) {}
        return false;
    }
}

bool god_mode_bindings::resolve_file(const pattern_scanner& scanner, candidate& result,
    std::string& error) noexcept {
#include "god_mode_certificates.hpp"
    return resolve_certificates(scanner, families, result, error);
}

} // namespace azs
