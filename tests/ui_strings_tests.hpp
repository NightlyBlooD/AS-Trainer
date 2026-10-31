#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ImFont;
struct ImFontGlyphRangesBuilder;

namespace azs {

class ui_strings_tests final {
public:
    /** @brief Validate all UI translations, field mappings, IDs and headless font glyphs.
     * @return Zero after complete success; one after an assertion or contained exception.
     * Uses static descriptors and an ImGui font atlas without creating a Windows GUI.
     */
    static int run() noexcept;

private:
    /** @brief Record one regression assertion and emit a concise failure description.
     * @param condition Required invariant; @param description Human-readable assertion label.
     * @return None; failed assertions increment the suite's failure count.
     */
    static void check(bool condition, const char* description) noexcept;

    /** @brief Decode one strict Unicode scalar from bounded NUL-terminated UTF-8 text.
     * @param cursor Current byte position, advanced only after successful decoding.
     * @param codepoint Output scalar; overlong sequences and surrogates are rejected.
     * @return True for a valid non-NUL scalar, false at the terminator or invalid input.
     */
    static bool read_codepoint(const char*& cursor, std::uint32_t& codepoint) noexcept;

    /** @brief Check an entire NUL-terminated string for strict UTF-8 validity.
     * @param text UTF-8 catalog string, or null for invalid input.
     * @return True when every byte belongs to a valid Unicode scalar sequence.
     */
    static bool valid_utf8(const char* text) noexcept;

    /** @brief Normalize printf conversion arguments while ignoring literal percent signs.
     * @param text Catalog string containing optional native printf conversions.
     * @return Ordered conversion types, including width/precision star arguments.
     */
    static std::vector<std::string> format_arguments(const char* text);

    /** @brief Check seven complete catalogs, identical format types and stable widget suffixes.
     * @return None; invalid enum/language fallback behavior is checked as well.
     */
    static void check_catalog();

    /** @brief Check names and unit explanations for all 25 immutable field descriptors.
     * @return None; unknown and null key fallbacks must not expose Russian metadata.
     */
    static void check_fields();

    /** @brief Build a headless atlas from the production base/CJK fonts and exact text ranges.
     * @return None; every scalar from seven UI and core diagnostic catalogs is checked.
     * Bundled Chinese font is resolved relative to the CTest source working directory.
     */
    static void check_fonts();

    /** @brief Consume a diagnostic catalog string while collecting its glyph ranges.
     * @param text Process-lifetime UTF-8 translation supplied by localization.
     * @return None; only the suite's currently active range builder is modified.
     */
    static void collect_diagnostic_text(const char* text) noexcept;

    /** @brief Consume a diagnostic string while testing glyph coverage in the built atlas.
     * @param text Process-lifetime UTF-8 translation supplied by localization.
     * @return None; no native window, rendering device or game object is accessed.
     */
    static void check_diagnostic_text(const char* text) noexcept;

    /** @brief Verify each printable scalar exists as a real glyph rather than a fallback box.
     * @param text NUL-terminated UTF-8 text; @param source Label for missing-glyph diagnostics.
     * @return None; newline, carriage return and tab are layout controls and are skipped.
     */
    static void check_glyphs(const char* text, const char* source) noexcept;

    inline static int failures_{};
    inline static unsigned int missing_glyph_reports_{};
    inline static ImFontGlyphRangesBuilder* range_builder_{};
    inline static ImFont* font_{};
};

} // namespace azs
