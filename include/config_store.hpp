#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "trainer_state.hpp"
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace azs {

class config_store_tests;

class config_store final {
public:
    /** @brief Set the preset directory beside the actual loaded DLL and create it if needed.
     * @param module Loaded DLL handle; preview may pass its executable module handle.
     * @return True when the configs directory is available, false with a diagnostic.
     */
    static bool initialize(HMODULE module) noexcept;

    /** @brief Enumerate existing INI presets under the configured local directory.
     * @return Sorted UTF-8 filenames without paths; empty on a missing directory/error.
     */
    static std::vector<std::string> list_configs();

    /** @brief Save current configured fields and automation using an atomic file replacement.
     * @param filename Plain filename ending in .ini, never an arbitrary external path.
     * @return True on complete save; false without overwriting with a partial file.
     */
    static bool save(const std::string& filename) noexcept;

    /** @brief Parse and validate an INI preset, then replace trainer settings as one operation.
     * @param filename Existing plain filename ending in .ini inside configs.
     * @return True on acceptance; false without partially applying malformed settings.
     */
    static bool load(const std::string& filename) noexcept;

    /** @brief Return the latest config action diagnostic for a UI status label.
     * @return UTF-8 diagnostic copied for the caller.
     */
    static std::string get_message();

    /** @brief Serialize a validated preset using stable ASCII field/automation keys.
     * @param preset Desired snapshot; no live game pointer is accessed.
     * @return Complete UTF-8 INI text.
     */
    static std::string serialize(const trainer_preset& preset);

    /** @brief Parse an INI document without changing global trainer state.
     * @param text Complete document up to one MiB; @param preset Parsed output on success.
     * @param error Human-readable reason on failure.
     * @return True for valid supported schema, false without replacing preset on error.
     */
    static bool parse(const std::string& text, trainer_preset& preset, std::string& error) noexcept;

private:
    friend class config_store_tests;

    /** @brief Restrict preset access to a single bounded .ini filename.
     * @param filename UTF-8 plain filename; path separators/traversal are rejected.
     * @return True for an accepted local preset filename.
     */
    static bool valid_filename(const std::string& filename) noexcept;

    /** @brief Remove permitted surrounding INI whitespace without allocating.
     * @param value One key, value or line from the source document.
     * @return View excluding leading/trailing space, tab and carriage return.
     */
    static std::string_view trim(std::string_view value) noexcept;

    /** @brief Validate an ASCII stable field/effect identifier used by this schema.
     * @param value Nonempty identifier of at most 128 ASCII letters/digits or _ . : -.
     * @return True for a single bounded identifier, false for delimiters/control bytes.
     */
    static bool valid_key(std::string_view value) noexcept;

    /** @brief Parse a complete locale-independent finite floating-point token.
     * @param value Trimmed number; @param result Parsed output on success.
     * @return True only when from_chars consumes every byte and the result is finite.
     */
    static bool parse_number(std::string_view value, double& result) noexcept;

    /** @brief Parse the exact supported boolean spellings.
     * @param value Trimmed true/false token; @param result Parsed output on success.
     * @return True for lowercase true or false, false otherwise.
     */
    static bool parse_boolean(std::string_view value, bool& result) noexcept;

    /** @brief Parse a comma-separated bounded priority list into local storage.
     * @param value Trimmed source list, possibly empty; @param result Parsed tokens.
     * @return True for at most 64 valid stable keys, false for empty/invalid tokens.
     */
    static bool parse_priorities(std::string_view value, std::vector<std::string>& result);

    /** @brief Validate a complete preset without importing or touching live game state.
     * @param preset Proposed portable settings; @param error Failure diagnostic.
     * @return True for known unique field keys, finite in-range values and bounded automation.
     */
    static bool validate_preset(const trainer_preset& preset, std::string& error);

    /** @brief Format a finite number with locale-independent round-trip precision.
     * @param value Validated numeric field or time multiplier.
     * @return ASCII representation suitable for the strict parser.
     */
    static std::string format_number(double value);

    /** @brief Read one complete bounded preset while excluding concurrent file writers.
     * @param path Local preset path; @param text Complete bytes on success.
     * @param error Human-readable Windows/file-size failure reason.
     * @return True after a complete read of at most one MiB, false otherwise.
     */
    static bool read_text(const std::filesystem::path& path, std::string& text, std::string& error);

    /** @brief Write and flush a same-directory temporary file, then atomically replace the target.
     * @param path Validated local target; @param text Complete serialized preset.
     * @param error Human-readable failure reason; caller owns mutex_.
     * @return True after MoveFileEx replacement, false with the prior target preserved.
     */
    static bool write_atomic(const std::filesystem::path& path, const std::string& text, std::string& error);

    /** @brief Publish a bounded fallback diagnostic after a public action unwinds its lock.
     * @param message Static action-failure context.
     * @return None; contains any secondary allocation or locking exception.
     */
    static void report_failure(const char* message) noexcept;

    inline static std::mutex mutex_;
    inline static std::filesystem::path directory_;
    inline static std::string message_;
    inline static std::uint64_t temporary_sequence_{};
};

} // namespace azs
