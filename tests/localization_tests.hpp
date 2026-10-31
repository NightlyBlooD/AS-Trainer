#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace azs {

class localization_tests final {
public:
    /** @brief Verify native language codes, complete translations and read-only preference polling.
     * @return Zero after all focused assertions pass, one on any failure.
     * Uses private timestamps and uniquely owned files; real user preferences are never written.
     */
    static int run();

private:
    /** @brief Record an assertion while preserving independent later checks.
     * @param condition Expected true result; @param description Failure context.
     * @return None; increments the failure counter when false.
     */
    static void check(bool condition, const char* description);

    /** @brief Verify the seven supported short codes and reject unproven WoW locale aliases.
     * @return None; does not read environment variables or touch the filesystem.
     */
    static void test_codes();

    /** @brief Verify complete valid UTF-8 rows, unique English keys and printf ABI preservation.
     * @return None; also checks static lifetimes, unknown-key fallback and glyph visitation.
     */
    static void test_catalog();

    /** @brief Exercise APPDATA, save-path override, file changes and one-second polling.
     * @param root Unique owned fixture directory inside the test working directory.
     * @return None; restores the two environment variables before returning.
     */
    static void test_files(const std::filesystem::path& root);

    /** @brief Collect printf conversion tokens while ignoring ordinary translated text.
     * @param text NUL-terminated catalog message or format string.
     * @return Ordered exact placeholders including width and length modifiers.
     */
    static std::string format_signature(const char* text);

    /** @brief Record one immutable glyph-source string supplied by the catalog visitor.
     * @param text Valid process-lifetime UTF-8 catalog text.
     * @return None; counts nonempty strings and remembers whether Chinese was included.
     */
    static void visit_text(const char* text) noexcept;

    /** @brief Write only one named file owned by this test fixture.
     * @param path Fixture filename; @param text Exact bounded bytes to persist.
     * @return True after complete write and close, false on any file error.
     */
    static bool write_file(const std::filesystem::path& path, std::string_view text);

    /** @brief Capture a process environment value before isolated path tests override it.
     * @param name UTF-16 environment variable name.
     * @return Existing value or an empty string when absent.
     */
    static std::wstring environment_value(const wchar_t* name);

    inline static int failures_{};
    inline static std::size_t visited_{};
    inline static bool visited_chinese_{};
};

} // namespace azs
