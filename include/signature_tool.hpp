#pragma once

#include "signature_profile.hpp"
#include "native_addresses.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace azs {

class signature_tool final {
public:
    /** @brief Run the offline signature diagnostic command line without touching a game process.
     * @param argument_count Number of Windows wide command-line arguments including program name.
     * @param arguments signature_scan.exe --exe PATH (--profile INI | --auto) [--output JSON].
     * @return Zero when every rule resolves, two when any rule fails, one for input/hash/output errors.
     * The actual and expected hashes are reported even when different; mismatch alone is not failure.
     * Output is UTF-8 JSON on stdout or at the explicit output path. Executable/profile input paths
     * cannot be reused as output, including existing filesystem aliases. No DLL or image is executed.
     * --auto uses only embedded trusted catalogs and reports full offline layout/ABI compatibility;
     * a successful report still requires live-image verification before hooks can be installed.
     */
    static int run(int argument_count, wchar_t** arguments) noexcept;

private:
    struct rule_result {
        std::string name;
        std::string status;
        std::string error;
        pattern_match match;
        pattern_resolution resolution;
        std::size_t match_count{};
        bool match_count_is_lower_bound{};
        bool found{};
        bool resolved{};
        bool field_mode{};
    };

    /** @brief Escape one valid UTF-8 string as a complete JSON string literal.
     * @param text UTF-8 data; @return Quoted JSON with controls, quotes and backslashes escaped.
     * Non-ASCII UTF-8 scalar bytes remain unchanged, preserving Unicode paths and profile names.
     */
    static std::string json_string(std::string_view text);

    /** @brief Convert one filesystem-native path to lossless UTF-8 bytes for JSON output.
     * @param path Native path; @return UTF-8 representation, or throws for invalid native encoding.
     */
    static std::string utf8_path(const std::filesystem::path& path);

    /** @brief Format an address as an exact hexadecimal string without locale-dependent streams.
     * @param value Unsigned address/offset; @return Lowercase 0x-prefixed representation.
     */
    static std::string hex(std::uint64_t value);

    /** @brief Resolve every symbol independently and apply its checked signed addend.
     * @param scanner Valid loaded offline image; @param profile Validated input rules.
     * @return Ordered complete results, including no-match/multiple-match failures without first-match use.
     */
    static std::vector<rule_result> scan(const pattern_scanner& scanner, const signature_profile& profile);

    /** @brief Serialize complete offline diagnostics into one UTF-8 JSON report.
     * @param executable Actual executable path; @param profile_path Input profile path.
     * @param profile Validated profile; @param actual_hash Hash of the exact scanned file bytes.
     * @param results Complete ordered per-symbol outcomes; @param all_resolved Final success flag.
     * @return Human-readable JSON containing hash comparison and exact matched/resolved addresses.
     */
    static std::string report(const std::filesystem::path& executable,
        const std::filesystem::path& profile_path, const signature_profile& profile,
        std::string_view actual_hash, std::span<const rule_result> results, bool all_resolved);

    /** @brief Serialize the embedded automatic resolver's full offline compatibility result.
     * @param executable Canonical PE path; @param actual_hash Digest of the scanned bytes.
     * @param candidate Complete independent binding, read only when compatible is true.
     * @param compatible Whether all required layout, native-target and ABI evidence passed.
     * @param error Exact failed invariant, empty after success.
     * @return UTF-8 JSON with resolved symbols/maps and explicit pending live verification.
     */
    static std::string report_auto(const std::filesystem::path& executable,
        std::string_view actual_hash, const native_addresses::candidate& candidate,
        bool compatible, std::string_view error);

    /** @brief Check whether an explicit output would overwrite either read-only input.
     * @param output Report destination; @param executable PE input; @param profile INI input.
     * @return True for identical absolute paths or existing equivalent filesystem objects.
     */
    static bool output_is_input(const std::filesystem::path& output,
        const std::filesystem::path& executable, const std::filesystem::path& profile);

    /** @brief Write complete UTF-8 JSON to the explicit output or stdout.
     * @param output Empty for stdout, otherwise the requested report filename.
     * @param text Complete owned JSON bytes; @return True after complete output, false on I/O failure.
     */
    static bool write_report(const std::filesystem::path& output, std::string_view text);

    /** @brief Print a bounded English technical diagnostic to stderr using the Unicode console helper.
     * @param message NUL-terminated UTF-8 text; @return One, matching the CLI bad-input/error status.
     */
    static int fail(const char* message) noexcept;
};

} // namespace azs
