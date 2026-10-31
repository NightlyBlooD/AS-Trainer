#pragma once

#include "pattern_scanner.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace azs {

struct signature_rule {
    std::string name;
    std::string pattern;
    std::string section;
    pattern_resolve_mode resolve{pattern_resolve_mode::match};
    std::uint32_t displacement_offset{};
    std::uint32_t instruction_size{};
    std::int32_t addend{};
};

struct signature_profile {
    std::string name;
    std::string sha256;
    std::string layout;
    std::vector<signature_rule> rules;
};

class signature_profile_tests;

class signature_profiles final {
public:
    static constexpr std::size_t maximum_document_size = 1024u * 1024u;
    static constexpr std::size_t maximum_rules = 128;

    /** @brief Parse a strict bounded UTF-8 signature INI into an independent candidate.
     * @param text Profile text, optionally beginning with a UTF-8 BOM.
     * @param result Output profile, replaced only after every key and rule is valid.
     * @param error Output diagnostic, cleared after successful parsing.
     * @return True for one complete [profile] section and 1..128 unique [symbol.NAME] rules.
     * Metadata name/sha256/layout and each rule's pattern/section/resolve are required.
     * rip_rel32 additionally requires displacement_offset/instruction_size; field_disp32
     * requires displacement_offset. Other numeric fields default to zero. Unknown or
     * duplicate content, invalid layout identifiers, malformed patterns and partial inputs fail atomically.
     * Layout is a stable ASCII identifier; policy about which layouts/hashes may execute belongs to callers.
     */
    static bool parse(std::string_view text, signature_profile& result, std::string& error) noexcept;

    /** @brief Read one existing bounded profile without modifying it or the prior output.
     * @param path UTF-16/native filesystem path to the INI.
     * @param result Output, committed only after complete read and successful parse.
     * @param error Output file/parse diagnostic.
     * @return False for missing, unreadable or greater-than-one-MiB inputs; no global state changes.
     */
    static bool load_file(const std::filesystem::path& path,
        signature_profile& result, std::string& error) noexcept;

    /** @brief Hash bounded offline bytes with the Windows SHA-256 provider.
     * @param data Immutable input up to pattern_scanner::maximum_file_size bytes.
     * @return Exactly 64 lowercase hexadecimal characters, or an empty string on failure.
     * An empty byte view produces the standard SHA-256 digest of empty input.
     */
    static std::string sha256(std::span<const std::uint8_t> data) noexcept;

private:
    friend class signature_profile_tests;

    /** @brief Remove permitted ASCII whitespace at both ends without allocating.
     * @param value Bounded source view; @return Trimmed view into the original storage.
     */
    static std::string_view trim(std::string_view value) noexcept;

    /** @brief Check UTF-8 scalar encoding without accepting overlong sequences or surrogates.
     * @param text Complete bounded document or metadata value.
     * @return True only for well-formed Unicode scalar sequences without embedded NUL bytes.
     */
    static bool valid_utf8(std::string_view text) noexcept;

    /** @brief Validate a stable bounded ASCII symbol identifier.
     * @param name Symbol after the symbol. section prefix.
     * @return True for a letter/underscore followed by letters, digits or underscores, up to 64 bytes.
     */
    static bool valid_symbol(std::string_view name) noexcept;

    /** @brief Store a fixed diagnostic and optional source line while containing allocation failure.
     * @param error Output string; @param message Diagnostic text; @param line Optional 1-based line.
     * @return False for concise guarded parse returns; never mutates a profile output.
     */
    static bool fail(std::string& error, const char* message, std::size_t line = 0) noexcept;
};

} // namespace azs
