#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <span>
#include <string_view>

namespace azs {

enum class client_language : std::uint32_t {
    english, russian, german, french, spanish, ukrainian, chinese_simplified,
    polish, czech, portuguese_brazil, romanian, turkish, japanese, korean,
    count
};

class localization_tests;

class localization final {
public:
    /** @brief Read the client's persisted language before publishing overlay diagnostics.
     * @return None; missing, unreadable or unsupported settings select English.
     * Reads only the game's language preference, never creates or modifies its files.
     */
    static void initialize() noexcept;

    /** @brief Return the supported client language cached by initialize/refresh.
     * @return One of the client's supported overlay languages; defaults to English.
     * Reading the cache never accesses files and remains English before initialization.
     */
    static client_language current_language() noexcept;

    /** @brief Poll the native language preference when the one-second interval has elapsed.
     * @return None; concurrent callers share an atomic polling deadline.
     */
    static void refresh() noexcept;

    /** @brief Translate one canonical English literal using the current client language.
     * @param english NUL-terminated English key or a previously translated static catalog string.
     * @return Static catalog text when known, otherwise the unchanged input pointer;
     * null input returns an empty static string. Unknown text retains its original lifetime.
     * Static translated statuses are recognized again after a language switch. Format placeholders
     * are preserved exactly; translate before formatting dynamic values.
     */
    static const char* translate(const char* english) noexcept;

    /** @brief Enumerate every static diagnostic translation when building the UI font atlas.
     * @param visitor Nonthrowing callback consuming one UTF-8 catalog string at a time.
     * @return None; null callback is ignored and no allocation or language-file access occurs.
     */
    static void visit_translations(void(*visitor)(const char*)) noexcept;

    /** @brief Format and print a localized loader diagnostic as Unicode console text or UTF-8 bytes.
     * @param output stdout or stderr; @param english Canonical English printf format literal.
     * @param ... Arguments matching the unchanged native printf placeholders.
     * @return None; output is bounded to 4095 UTF-8 bytes and never changes console code pages.
     */
    static void print(std::FILE* output, const char* english, ...) noexcept;

private:
    friend class localization_tests;
    using catalog_row = std::array<const char*, static_cast<std::size_t>(client_language::count)>;

    /** @brief Expose immutable complete rows for every client language and regression checks.
     * @return Static English-keyed catalog; all strings have process lifetime.
     */
    static std::span<const catalog_row> catalog() noexcept;

    /** @brief Decode only language codes accepted by the native preference parser.
     * @param code Bounded UTF-8 file bytes including optional surrounding whitespace.
     * @return Matching overlay language, or English for unknown/invalid input.
     */
    static client_language parse_language(std::string_view code) noexcept;

    /** @brief Resolve the native language file using process environment preferences.
     * @param path Output path beside WOW_ROGUE_SAVE, or APPDATA/wenilla-rogue/language.txt.
     * @return True for a resolved bounded path, false when environment/path access fails.
     */
    static bool language_path(std::filesystem::path& path) noexcept;

    /** @brief Read a bounded preference file without opening it for writing.
     * @param path Native preference path resolved for this process.
     * @return Parsed supported language, otherwise English, including missing/oversized files.
     */
    static client_language read_language(const std::filesystem::path& path) noexcept;

    /** @brief Apply the shared polling deadline with an injected monotonic timestamp.
     * @param now_ms Current milliseconds, normally supplied by GetTickCount64.
     * @return None; only the winning due caller reads the language file.
     */
    static void refresh_at(std::uint64_t now_ms) noexcept;

    inline static std::atomic<client_language> language_{client_language::english};
    inline static std::atomic<std::uint64_t> next_poll_ms_{};
};

} // namespace azs
