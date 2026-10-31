#include "signature_profile.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <charconv>
#include <fstream>
#include <limits>
#include <unordered_set>
#include <utility>

namespace azs {

std::string_view signature_profiles::trim(std::string_view value) noexcept {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.remove_suffix(1);
    return value;
}

bool signature_profiles::valid_utf8(std::string_view text) noexcept {
    for (std::size_t offset = 0; offset < text.size();) {
        const auto first = static_cast<unsigned char>(text[offset++]);
        if (first == 0) return false;
        if (first < 0x80) continue;
        std::size_t remaining{};
        std::uint32_t value{};
        std::uint32_t minimum{};
        if (first >= 0xc2 && first <= 0xdf) { remaining = 1; value = first & 0x1fu; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { remaining = 2; value = first & 0x0fu; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { remaining = 3; value = first & 7u; minimum = 0x10000; }
        else return false;
        if (remaining > text.size() - offset) return false;
        for (std::size_t index = 0; index < remaining; ++index) {
            const auto next = static_cast<unsigned char>(text[offset++]);
            if ((next & 0xc0u) != 0x80u) return false;
            value = (value << 6) | (next & 0x3fu);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
    }
    return true;
}

bool signature_profiles::valid_symbol(std::string_view name) noexcept {
    if (name.empty() || name.size() > 64) return false;
    for (std::size_t index = 0; index < name.size(); ++index) {
        const auto value = static_cast<unsigned char>(name[index]);
        const bool letter = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || value == '_';
        if (!letter && (index == 0 || value < '0' || value > '9')) return false;
    }
    return true;
}

bool signature_profiles::fail(std::string& error, const char* message, std::size_t line) noexcept {
    try {
        error = message;
        if (line != 0) error += " at line " + std::to_string(line);
    } catch (...) {
        error.clear();
    }
    return false;
}

bool signature_profiles::parse(std::string_view text, signature_profile& result, std::string& error) noexcept {
    try {
        if (text.size() > maximum_document_size || !valid_utf8(text))
            return fail(error, "Profile exceeds one MiB or contains invalid UTF-8/NUL bytes.");
        if (text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
        signature_profile candidate;
        enum class section_kind { none, profile, symbol };
        section_kind section = section_kind::none;
        std::unordered_set<std::string> seen_sections;
        std::vector<unsigned> rule_masks;
        unsigned metadata_mask = 0;
        std::size_t line_number = 0;
        while (!text.empty()) {
            ++line_number;
            const auto newline = text.find('\n');
            const auto line = trim(text.substr(0, newline));
            if (newline == std::string_view::npos) text = {};
            else text.remove_prefix(newline + 1);
            if (line.empty() || line.front() == '#' || line.front() == ';') continue;
            if (line.front() == '[') {
                if (line.size() < 3 || line.back() != ']') return fail(error, "Malformed profile section", line_number);
                const auto name = line.substr(1, line.size() - 2);
                if (!seen_sections.emplace(name).second) return fail(error, "Duplicate profile section", line_number);
                if (name == "profile") {
                    section = section_kind::profile;
                } else if (name.starts_with("symbol.") && valid_symbol(name.substr(7))) {
                    if (candidate.rules.size() == maximum_rules) return fail(error, "Profile exceeds 128 symbol rules", line_number);
                    section = section_kind::symbol;
                    signature_rule rule;
                    rule.name = name.substr(7);
                    candidate.rules.push_back(std::move(rule));
                    rule_masks.push_back(0);
                } else {
                    return fail(error, "Unknown or invalid profile section", line_number);
                }
                continue;
            }
            const auto equals = line.find('=');
            if (equals == std::string_view::npos || section == section_kind::none)
                return fail(error, "Expected a profile section/key assignment", line_number);
            const auto key = trim(line.substr(0, equals));
            const auto value = trim(line.substr(equals + 1));
            if (section == section_kind::profile) {
                const unsigned bit = key == "name" ? 1u : key == "sha256" ? 2u : key == "layout" ? 4u : 0u;
                if (bit == 0 || (metadata_mask & bit) != 0)
                    return fail(error, "Unknown or duplicate profile metadata key", line_number);
                if (bit == 1) {
                    if (value.empty() || value.size() > 128 || value.find_first_of("\r\n\t") != std::string_view::npos)
                        return fail(error, "Profile name is empty, oversized or contains control characters", line_number);
                    for (const auto character : value)
                        if (static_cast<unsigned char>(character) < 0x20 || character == 0x7f)
                            return fail(error, "Profile name contains a control character", line_number);
                    candidate.name = value;
                } else if (bit == 2) {
                    if (value.size() != 64) return fail(error, "Profile sha256 must contain 64 hexadecimal characters", line_number);
                    candidate.sha256.clear();
                    candidate.sha256.reserve(64);
                    for (char digit : value) {
                        if (digit >= 'A' && digit <= 'F') digit = static_cast<char>(digit + ('a' - 'A'));
                        if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f')))
                            return fail(error, "Profile sha256 contains a nonhexadecimal character", line_number);
                        candidate.sha256 += digit;
                    }
                } else {
                    if (!valid_symbol(value)) return fail(error, "Invalid profile layout identifier", line_number);
                    candidate.layout = value;
                }
                metadata_mask |= bit;
                continue;
            }
            auto& rule = candidate.rules.back();
            auto& mask = rule_masks.back();
            unsigned bit = 0;
            if (key == "pattern") bit = 1;
            else if (key == "section") bit = 2;
            else if (key == "resolve") bit = 4;
            else if (key == "displacement_offset") bit = 8;
            else if (key == "instruction_size") bit = 16;
            else if (key == "addend") bit = 32;
            if (bit == 0 || (mask & bit) != 0) return fail(error, "Unknown or duplicate symbol rule key", line_number);
            if (bit == 1) {
                std::string pattern_error;
                if (!pattern_scanner::validate_pattern(value, pattern_error)) {
                    error = "Invalid pattern for symbol " + rule.name + ": " + pattern_error;
                    return false;
                }
                rule.pattern = value;
            } else if (bit == 2) {
                if (value.empty() || value.size() > 8) return fail(error, "Invalid PE section name", line_number);
                for (const auto character : value) {
                    if (static_cast<unsigned char>(character) <= 0x20 || static_cast<unsigned char>(character) >= 0x7f)
                        return fail(error, "PE section name must be printable ASCII without spaces", line_number);
                }
                rule.section = value;
            } else if (bit == 4) {
                if (value == "match") rule.resolve = pattern_resolve_mode::match;
                else if (value == "function_start") rule.resolve = pattern_resolve_mode::function_start;
                else if (value == "rip_rel32") rule.resolve = pattern_resolve_mode::rip_rel32;
                else if (value == "field_disp32") rule.resolve = pattern_resolve_mode::field_disp32;
                else return fail(error, "Unsupported symbol resolution mode", line_number);
            } else if (bit == 32) {
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), rule.addend);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                    return fail(error, "Invalid signed 32-bit addend", line_number);
            } else {
                auto& destination = bit == 8 ? rule.displacement_offset : rule.instruction_size;
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), destination);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                    return fail(error, "Invalid unsigned 32-bit resolution offset", line_number);
            }
            mask |= bit;
        }
        if (metadata_mask != 7) return fail(error, "Profile requires name, sha256 and layout.");
        if (candidate.rules.empty()) return fail(error, "Profile requires at least one symbol rule.");
        for (std::size_t index = 0; index < candidate.rules.size(); ++index) {
            const auto& rule = candidate.rules[index];
            const auto mask = rule_masks[index];
            if ((mask & 7u) != 7u) return fail(error, "Every symbol requires pattern, section and resolve.");
            if (rule.resolve == pattern_resolve_mode::rip_rel32 &&
                ((mask & 24u) != 24u || static_cast<std::uint64_t>(rule.displacement_offset) + 4 > rule.instruction_size))
                return fail(error, "rip_rel32 requires a displacement and an instruction end covering all four bytes.");
            if (rule.resolve == pattern_resolve_mode::field_disp32 && (mask & 8u) == 0)
                return fail(error, "field_disp32 requires displacement_offset.");
        }
        result = std::move(candidate);
        error.clear();
        return true;
    } catch (...) {
        return fail(error, "Cannot allocate or parse the bounded signature profile.");
    }
}

bool signature_profiles::load_file(const std::filesystem::path& path,
    signature_profile& result, std::string& error) noexcept {
    try {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) return fail(error, "Cannot open the signature profile.");
        const auto end = input.tellg();
        if (end < 0 || static_cast<std::uint64_t>(end) > maximum_document_size)
            return fail(error, "Signature profile exceeds one MiB or has an unreadable size.");
        std::string text(static_cast<std::size_t>(end), '\0');
        input.seekg(0);
        if (!text.empty()) input.read(text.data(), static_cast<std::streamsize>(text.size()));
        if (!input) return fail(error, "Cannot read the complete signature profile.");
        return parse(text, result, error);
    } catch (...) {
        return fail(error, "Cannot allocate or read the signature profile.");
    }
}

std::string signature_profiles::sha256(std::span<const std::uint8_t> data) noexcept {
    if (data.size() > pattern_scanner::maximum_file_size) return {};
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    std::array<unsigned char, 4096> object{};
    std::array<unsigned char, 32> digest{};
    bool success = false;
    do {
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) break;
        DWORD object_size{}, returned{};
        if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size),
                sizeof(object_size), &returned, 0) < 0 || object_size == 0 || object_size > object.size()) break;
        if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) break;
        if (!data.empty() && BCryptHashData(hash, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), 0) < 0) break;
        success = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
    } while (false);
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!success) return {};
    try {
        constexpr char hex[] = "0123456789abcdef";
        std::string result(digest.size() * 2, '\0');
        for (std::size_t index = 0; index < digest.size(); ++index) {
            result[index * 2] = hex[digest[index] >> 4];
            result[index * 2 + 1] = hex[digest[index] & 15u];
        }
        return result;
    } catch (...) {
        return {};
    }
}

} // namespace azs
