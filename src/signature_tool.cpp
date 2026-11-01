#include "signature_tool.hpp"
#include "localization.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <cwchar>
#include <fstream>
#include <limits>
#include <system_error>

namespace azs {

std::string signature_tool::json_string(std::string_view text) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(text.size() + 2);
    result += '"';
    for (const unsigned char byte : text) {
        if (byte == '"' || byte == '\\') {
            result += '\\';
            result += static_cast<char>(byte);
        } else if (byte < 0x20) {
            result += "\\u00";
            result += digits[byte >> 4];
            result += digits[byte & 15u];
        } else {
            result += static_cast<char>(byte);
        }
    }
    result += '"';
    return result;
}

std::string signature_tool::utf8_path(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

std::string signature_tool::hex(std::uint64_t value) {
    std::array<char, 18> buffer{};
    buffer[0] = '0';
    buffer[1] = 'x';
    const auto formatted = std::to_chars(buffer.data() + 2, buffer.data() + buffer.size(), value, 16);
    return {buffer.data(), formatted.ptr};
}

std::vector<signature_tool::rule_result> signature_tool::scan(const pattern_scanner& scanner,
    const signature_profile& profile) {
    std::vector<rule_result> results;
    results.reserve(profile.rules.size());
    for (const auto& rule : profile.rules) {
        rule_result current;
        current.name = rule.name;
        current.field_mode = rule.resolve == pattern_resolve_mode::field_disp32;
        if (!scanner.find_unique(rule.pattern, current.match, rule.section)) {
            current.error = scanner.error();
            if (scanner.error() == "Pattern was not found.") current.status = "not_found";
            else if (scanner.error().find("ambiguous") != std::string_view::npos) {
                current.status = "multiple_matches";
                current.match_count = 2;
                current.match_count_is_lower_bound = true;
            } else current.status = "search_error";
            results.push_back(std::move(current));
            continue;
        }
        current.found = true;
        current.match_count = 1;
        if (!scanner.resolve(current.match, rule.resolve, current.resolution,
                rule.displacement_offset, rule.instruction_size)) {
            current.status = "resolution_error";
            current.error = scanner.error();
            results.push_back(std::move(current));
            continue;
        }
        const auto value = static_cast<std::int64_t>(current.field_mode ?
            current.resolution.field_displacement : static_cast<std::int64_t>(current.resolution.rva)) + rule.addend;
        if (current.field_mode) {
            if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max()) {
                current.status = "addend_overflow";
                current.error = "Field displacement plus addend exceeds signed 32-bit bounds.";
                results.push_back(std::move(current));
                continue;
            }
            current.resolution.field_displacement = static_cast<std::int32_t>(value);
        } else {
            if (value < 0 || static_cast<std::uint64_t>(value) >= scanner.image_size() ||
                !scanner.rva_to_va(static_cast<std::uint32_t>(value), current.resolution.va)) {
                current.status = "addend_overflow";
                current.error = "Resolved RVA plus addend is outside the PE image.";
                results.push_back(std::move(current));
                continue;
            }
            current.resolution.rva = static_cast<std::uint32_t>(value);
        }
        current.status = "resolved";
        current.resolved = true;
        results.push_back(std::move(current));
    }
    return results;
}

std::string signature_tool::report(const std::filesystem::path& executable,
    const std::filesystem::path& profile_path, const signature_profile& profile,
    std::string_view actual_hash, std::span<const rule_result> results, bool all_resolved) {
    std::string output = "{\n  \"exe\": " + json_string(utf8_path(executable)) +
        ",\n  \"profile_path\": " + json_string(utf8_path(profile_path)) +
        ",\n  \"profile\": " + json_string(profile.name) +
        ",\n  \"layout\": " + json_string(profile.layout) +
        ",\n  \"actual_sha256\": " + json_string(actual_hash) +
        ",\n  \"expected_sha256\": " + json_string(profile.sha256) +
        ",\n  \"hash_matches\": " + (actual_hash == profile.sha256 ? "true" : "false") +
        ",\n  \"all_resolved\": " + (all_resolved ? "true" : "false") + ",\n  \"symbols\": [\n";
    bool first = true;
    for (const auto& current : results) {
        if (!first) output += ",\n";
        first = false;
        output += "    {\"name\": " + json_string(current.name) + ", \"status\": " + json_string(current.status) +
            ", \"match_count\": " + std::to_string(current.match_count) +
            ", \"match_count_is_lower_bound\": " + (current.match_count_is_lower_bound ? "true" : "false");
        if (current.found) {
            output += ", \"match_rva\": " + json_string(hex(current.match.rva)) +
                ", \"match_va\": " + json_string(hex(current.match.va)) +
                ", \"file_offset\": " + json_string(hex(current.match.file_offset));
        } else output += ", \"match_rva\": null, \"match_va\": null, \"file_offset\": null";
        if (current.resolved) {
            output += ", \"rva\": " + json_string(hex(current.resolution.rva)) +
                ", \"va\": " + json_string(hex(current.resolution.va)) + ", \"field_displacement\": ";
            output += current.field_mode ? std::to_string(current.resolution.field_displacement) : "null";
        } else output += ", \"rva\": null, \"va\": null, \"field_displacement\": null";
        output += ", \"error\": " + json_string(current.error) + '}';
    }
    output += "\n  ]\n}\n";
    return output;
}

bool signature_tool::output_is_input(const std::filesystem::path& output,
    const std::filesystem::path& executable, const std::filesystem::path& profile) {
    if (output.empty()) return false;
    const auto normalized = std::filesystem::absolute(output).lexically_normal();
    if (normalized == std::filesystem::absolute(executable).lexically_normal() ||
        (!profile.empty() && normalized == std::filesystem::absolute(profile).lexically_normal())) return true;
    std::error_code error;
    if (std::filesystem::equivalent(output, executable, error) && !error) return true;
    error.clear();
    return !profile.empty() && std::filesystem::equivalent(output, profile, error) && !error;
}

std::string signature_tool::report_auto(const std::filesystem::path& executable,
    std::string_view actual_hash, const native_addresses::candidate& candidate,
    bool compatible, std::string_view error) {
    std::string output = "{\n  \"mode\": \"auto\",\n  \"executable\": " + json_string(utf8_path(executable))
        + ",\n  \"actual_sha256\": " + json_string(actual_hash)
        + ",\n  \"offline_compatible\": " + (compatible ? "true" : "false")
        + ",\n  \"live_validation_required\": true,\n  \"error\": " + json_string(error);
    if (compatible) {
        const auto layout = candidate.layout == layout_id::legacy ? "legacy"
            : candidate.layout == layout_id::current ? "current" : "adaptive";
        output += ",\n  \"layout\": " + json_string(layout)
            + ",\n  \"object_field_abi\": " + json_string(candidate.setter_abi ==
                native_addresses::object_field_abi::float_value ? "float" : "bits")
            + ",\n  \"participant_stride\": " + json_string(hex(candidate.participant_stride))
            + ",\n  \"verification_chunks\": " + std::to_string(candidate.verification.size())
            + ",\n  \"symbols\": [";
        for (std::size_t index = 0; index < candidate.symbols.size(); ++index) {
            if (index != 0) output += ',';
            output += "\n    {\"name\": " + json_string(native_addresses::symbol_name(static_cast<native_symbol>(index)))
                + ", \"rva\": " + json_string(hex(candidate.symbols[index])) + '}';
        }
        output += "\n  ],\n  \"run_offsets\": [";
        bool first = true;
        for (const auto& item : candidate.run_offsets) {
            if (!first) output += ',';
            first = false;
            output += "\n    {\"canonical\": " + json_string(hex(item.legacy_offset))
                + ", \"resolved\": " + json_string(hex(item.resolved_offset)) + '}';
        }
        output += "\n  ],\n  \"member_offsets\": [";
        first = true;
        for (const auto& item : candidate.member_offsets) {
            if (!first) output += ',';
            first = false;
            output += "\n    {\"canonical\": " + json_string(hex(item.legacy_offset))
                + ", \"resolved\": " + json_string(hex(item.resolved_offset)) + '}';
        }
        output += "\n  ]";
    }
    output += "\n}\n";
    return output;
}

bool signature_tool::write_report(const std::filesystem::path& output, std::string_view text) {
    if (output.empty()) {
        return std::fwrite(text.data(), 1, text.size(), stdout) == text.size() && std::fflush(stdout) == 0;
    }
    std::ofstream file(output, std::ios::binary | std::ios::trunc);
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.close();
    return !file.fail();
}

int signature_tool::fail(const char* message) noexcept {
    localization::print(stderr, "%s\n", message);
    return 1;
}

int signature_tool::run(int argument_count, wchar_t** arguments) noexcept {
    try {
        if (argument_count < 1 || arguments == nullptr)
            return fail("Usage: signature_scan.exe --exe PATH --profile INI [--output JSON]");
        std::filesystem::path executable;
        std::filesystem::path profile_path;
        std::filesystem::path output_path;
        bool output_seen = false;
        bool auto_mode = false;
        for (int index = 1; index < argument_count; ++index) {
            if (arguments[index] != nullptr && std::wstring_view(arguments[index]) == L"--auto") {
                if (auto_mode || !profile_path.empty()) return fail("Use either --auto or --profile INI once.");
                auto_mode = true;
                continue;
            }
            if (index + 1 >= argument_count || arguments[index] == nullptr || arguments[index + 1] == nullptr)
                return fail("Usage: signature_scan.exe --exe PATH --profile INI [--output JSON]");
            const std::wstring_view option(arguments[index]);
            if (std::wstring_view(arguments[index + 1]).starts_with(L"--"))
                return fail("Missing option value. Usage: signature_scan.exe --exe PATH --profile INI [--output JSON]");
            const std::filesystem::path value(arguments[++index]);
            if (value.empty()) return fail("Signature scan paths must not be empty.");
            if (option == L"--exe" && executable.empty()) executable = value;
            else if (option == L"--profile" && profile_path.empty() && !auto_mode) profile_path = value;
            else if (option == L"--output" && !output_seen) { output_path = value; output_seen = true; }
            else return fail("Unknown or duplicate option. Usage: signature_scan.exe --exe PATH --profile INI [--output JSON]");
        }
        if (executable.empty() || (!auto_mode && profile_path.empty()))
            return fail("Usage: signature_scan.exe --exe PATH (--profile INI | --auto) [--output JSON]");
        if (output_is_input(output_path, executable, profile_path))
            return fail("Output cannot overwrite the executable or signature profile input.");
        signature_profile profile;
        std::string error;
        if (!auto_mode && !signature_profiles::load_file(profile_path, profile, error)) return fail(error.c_str());
        pattern_scanner scanner;
        if (!scanner.load_file(executable)) return fail(scanner.error().data());
        const auto hash = signature_profiles::sha256(scanner.bytes());
        if (hash.empty()) return fail("Cannot calculate the offline image SHA-256 digest.");
        if (auto_mode) {
            native_addresses::candidate candidate;
            const bool compatible = native_addresses::resolve_auto(scanner, candidate, error);
            const auto json = report_auto(std::filesystem::absolute(executable).lexically_normal(),
                hash, candidate, compatible, error);
            if (!write_report(output_path, json)) return fail("Cannot write the complete UTF-8 signature report.");
            return compatible ? 0 : 2;
        }
        const auto results = scan(scanner, profile);
        const bool all_resolved = std::all_of(results.begin(), results.end(),
            [](const rule_result& result) { return result.resolved; });
        const auto json = report(std::filesystem::absolute(executable).lexically_normal(),
            std::filesystem::absolute(profile_path).lexically_normal(), profile, hash, results, all_resolved);
        if (!write_report(output_path, json)) return fail("Cannot write the complete UTF-8 signature report.");
        return all_resolved ? 0 : 2;
    } catch (...) {
        return fail("Unexpected offline signature scan input, allocation or filesystem failure.");
    }
}

} // namespace azs

int wmain(int argument_count, wchar_t** arguments) {
    return azs::signature_tool::run(argument_count, arguments);
}
