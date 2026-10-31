#include "ui_strings_tests.hpp"
#include "ui_strings.hpp"
#include "trainer_state.hpp"
#include "localization.hpp"
#include "imgui.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <exception>
#include <string_view>

namespace azs {

int ui_strings_tests::run() noexcept {
    failures_ = 0;
    missing_glyph_reports_ = 0;
    try {
        check_catalog();
        check_fields();
        check_fonts();
    } catch (const std::exception& exception) {
        std::fprintf(stderr, "UI suite exception: %s\n", exception.what());
        ++failures_;
    } catch (...) {
        check(false, "unexpected UI suite exception");
    }
    range_builder_ = nullptr;
    font_ = nullptr;
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }
    std::printf("UI catalog and headless glyphs: %s (%d failures)\n",
                failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

void ui_strings_tests::check(bool condition, const char* description) noexcept {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures_;
    }
}

bool ui_strings_tests::read_codepoint(const char*& cursor, std::uint32_t& codepoint) noexcept {
    const auto lead = static_cast<unsigned char>(*cursor);
    if (!lead) {
        return false;
    }
    if (lead < 0x80) {
        codepoint = lead;
        ++cursor;
        return true;
    }
    unsigned int length = 0;
    std::uint32_t value = 0;
    std::uint32_t minimum = 0;
    if (lead >= 0xc2 && lead <= 0xdf) {
        length = 2;
        value = lead & 0x1f;
        minimum = 0x80;
    } else if (lead >= 0xe0 && lead <= 0xef) {
        length = 3;
        value = lead & 0x0f;
        minimum = 0x800;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
        length = 4;
        value = lead & 0x07;
        minimum = 0x10000;
    } else {
        return false;
    }
    for (unsigned int index = 1; index < length; ++index) {
        const auto continuation = static_cast<unsigned char>(cursor[index]);
        if ((continuation & 0xc0) != 0x80) {
            return false;
        }
        value = (value << 6) | (continuation & 0x3f);
    }
    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
        return false;
    }
    cursor += length;
    codepoint = value;
    return true;
}

bool ui_strings_tests::valid_utf8(const char* text) noexcept {
    if (!text) {
        return false;
    }
    while (*text) {
        std::uint32_t codepoint = 0;
        if (!read_codepoint(text, codepoint)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> ui_strings_tests::format_arguments(const char* text) {
    std::vector<std::string> arguments;
    const std::string_view input(text ? text : "");
    for (std::size_t position = 0; position < input.size(); ++position) {
        if (input[position] != '%' || ++position == input.size()) {
            continue;
        }
        if (input[position] == '%') {
            continue;
        }
        while (position < input.size() && std::strchr("-+ #0'", input[position])) {
            ++position;
        }
        if (position < input.size() && input[position] == '*') {
            arguments.emplace_back("int");
            ++position;
        } else {
            while (position < input.size() && input[position] >= '0' && input[position] <= '9') {
                ++position;
            }
        }
        if (position < input.size() && input[position] == '.') {
            ++position;
            if (position < input.size() && input[position] == '*') {
                arguments.emplace_back("int");
                ++position;
            } else {
                while (position < input.size() && input[position] >= '0' && input[position] <= '9') {
                    ++position;
                }
            }
        }
        std::string length;
        if (position < input.size() && std::strchr("hljztL", input[position])) {
            length += input[position++];
            if (position < input.size() && (length == "h" || length == "l")
                && input[position] == length[0]) {
                length += input[position++];
            }
        }
        if (position == input.size()) {
            break;
        }
        const char conversion = input[position];
        if (std::strchr("di", conversion)) {
            arguments.emplace_back("signed:" + length);
        } else if (std::strchr("uoxX", conversion)) {
            arguments.emplace_back("unsigned:" + length);
        } else if (std::strchr("fFeEgGaA", conversion)) {
            arguments.emplace_back(length == "L" ? "long_double" : "double");
        } else if (conversion == 's') {
            arguments.emplace_back("string:" + length);
        } else if (conversion == 'c') {
            arguments.emplace_back("character:" + length);
        } else if (conversion == 'p') {
            arguments.emplace_back("pointer");
        } else if (conversion == 'n') {
            arguments.emplace_back("count_pointer:" + length);
        }
    }
    return arguments;
}

void ui_strings_tests::check_catalog() {
    const auto count = static_cast<std::size_t>(ui_text_id::count);
    check(count == 102, "all 102 UI entries are included");
    for (std::size_t index = 0; index < count; ++index) {
        const auto id = static_cast<ui_text_id>(index);
        const char* english = ui_strings::get(id, client_language::english);
        const auto english_arguments = format_arguments(english);
        const char* english_suffix = std::strstr(english, "###");
        for (std::uint32_t language_index = 0; language_index < 7; ++language_index) {
            const char* translated = ui_strings::get(id, static_cast<client_language>(language_index));
            check(translated && translated[0], "every UI translation is nonempty");
            check(valid_utf8(translated), "every UI translation is strict UTF-8");
            check(format_arguments(translated) == english_arguments, "printf argument order/types are preserved");
            const char* suffix = std::strstr(translated, "###");
            check((!english_suffix && !suffix)
                      || (english_suffix && suffix && std::strcmp(english_suffix, suffix) == 0),
                  "ImGui IDs are identical across languages");
        }
    }
    check(!ui_strings::get(ui_text_id::count, client_language::english)[0],
          "invalid UI IDs return empty text");
    check(std::strcmp(ui_strings::get(ui_text_id::yes, static_cast<client_language>(999)),
                      ui_strings::get(ui_text_id::yes, client_language::english)) == 0,
          "invalid languages fall back to English");
    check(!valid_utf8("\xc0\xaf") && !valid_utf8("\xed\xa0\x80")
              && !valid_utf8("\xf4\x90\x80\x80") && !valid_utf8("\xe2\x82"),
          "UTF-8 verifier rejects overlong, surrogate, oversized and truncated text");
}

void ui_strings_tests::check_fields() {
    check(trainer_state::field_count() == 25, "all 25 native fields are covered");
    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
        const char* key = trainer_state::field_at(index).key;
        for (std::uint32_t language_index = 0; language_index < 7; ++language_index) {
            const auto language = static_cast<client_language>(language_index);
            const char* name = ui_strings::field_name(key, language);
            const char* unit = ui_strings::field_unit(key, language);
            check(name && name[0] && std::strcmp(name, key) != 0 && valid_utf8(name),
                  "every field name resolves to a valid translation");
            check(unit && unit[0] && valid_utf8(unit), "every field unit resolves to a valid translation");
        }
    }
    const char* unknown = "unknown_field_key";
    for (std::uint32_t language_index = 0; language_index < 7; ++language_index) {
        const auto language = static_cast<client_language>(language_index);
        check(ui_strings::field_name(unknown, language) == unknown,
              "unknown names retain the stable key without metadata fallback");
        check(!ui_strings::field_unit(unknown, language)[0], "unknown units are empty");
        check(!ui_strings::field_name(nullptr, language)[0]
                  && !ui_strings::field_unit(nullptr, language)[0], "null key fallbacks are empty");
    }
}

void ui_strings_tests::check_fonts() {
    IMGUI_CHECKVERSION();
    check(ImGui::CreateContext() != nullptr, "headless ImGui context created");
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    static constexpr ImWchar base_ranges[] = {
        0x0020, 0x024f, 0x0400, 0x052f, 0x2000, 0x206f, 0
    };
    ImFontConfig base_configuration;
    base_configuration.PixelSnapH = true;
    base_configuration.OversampleH = 1;
    base_configuration.OversampleV = 1;
    std::array<wchar_t, MAX_PATH> windows_directory{};
    std::array<wchar_t, 1024> font_path{};
    std::array<char, 4096> utf8_path{};
    const UINT directory_length = GetWindowsDirectoryW(windows_directory.data(),
                                                        static_cast<UINT>(windows_directory.size()));
    if (directory_length && directory_length < windows_directory.size()) {
        for (const wchar_t* filename : {L"segoeui.ttf", L"arial.ttf"}) {
            if (std::swprintf(font_path.data(), font_path.size(), L"%ls\\Fonts\\%ls",
                              windows_directory.data(), filename) <= 0
                || GetFileAttributesW(font_path.data()) == INVALID_FILE_ATTRIBUTES
                || !WideCharToMultiByte(CP_UTF8, 0, font_path.data(), -1, utf8_path.data(),
                                         static_cast<int>(utf8_path.size()), nullptr, nullptr)) {
                continue;
            }
            font_ = io.Fonts->AddFontFromFileTTF(utf8_path.data(), 17.0f, &base_configuration, base_ranges);
            if (font_) {
                break;
            }
        }
    }
    if (!font_) {
        font_ = io.Fonts->AddFontDefault();
    }
    check(font_ != nullptr, "base Latin/Cyrillic font loaded");

    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    for (std::size_t index = 0; index < static_cast<std::size_t>(ui_text_id::count); ++index) {
        for (std::uint32_t language_index = 0; language_index < 7; ++language_index) {
            builder.AddText(ui_strings::get(static_cast<ui_text_id>(index),
                                            static_cast<client_language>(language_index)));
        }
    }
    range_builder_ = &builder;
    localization::visit_translations(collect_diagnostic_text);
    range_builder_ = nullptr;
    ImVector<ImWchar> glyph_ranges;
    builder.BuildRanges(&glyph_ranges);

    const DWORD bundled_attributes = GetFileAttributesW(L"fonts\\NotoSansCJKsc-Regular.otf");
    const bool bundled_exists = bundled_attributes != INVALID_FILE_ATTRIBUTES
        && !(bundled_attributes & FILE_ATTRIBUTE_DIRECTORY);
    check(bundled_exists, "bundled CJK font exists relative to the source working directory");
    if (!bundled_exists) {
        return;
    }
    ImFontConfig merge_configuration = base_configuration;
    merge_configuration.MergeMode = true;
    check(io.Fonts->AddFontFromFileTTF("fonts/NotoSansCJKsc-Regular.otf", 17.0f,
                                      &merge_configuration, glyph_ranges.Data) != nullptr,
          "production bundled Noto OpenType font accepted for merging");
    if (!io.Fonts->Build()) {
        check(false, "headless atlas builds successfully with production glyph ranges");
        return;
    }
    for (std::size_t index = 0; index < static_cast<std::size_t>(ui_text_id::count); ++index) {
        for (std::uint32_t language_index = 0; language_index < 7; ++language_index) {
            check_glyphs(ui_strings::get(static_cast<ui_text_id>(index),
                                         static_cast<client_language>(language_index)), "UI");
        }
    }
    localization::visit_translations(check_diagnostic_text);
}

void ui_strings_tests::collect_diagnostic_text(const char* text) noexcept {
    check(valid_utf8(text), "every diagnostic font source is valid UTF-8");
    if (range_builder_ && text) {
        range_builder_->AddText(text);
    }
}

void ui_strings_tests::check_diagnostic_text(const char* text) noexcept {
    check_glyphs(text, "core diagnostic");
}

void ui_strings_tests::check_glyphs(const char* text, const char* source) noexcept {
    if (!text || !font_) {
        check(false, "glyph coverage has valid text and a built base font");
        return;
    }
    while (*text) {
        std::uint32_t codepoint = 0;
        if (!read_codepoint(text, codepoint)) {
            check(false, "glyph source decodes as strict UTF-8");
            return;
        }
        if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t') {
            continue;
        }
        if (codepoint > static_cast<std::uint32_t>(IM_UNICODE_CODEPOINT_MAX)
            || !font_->FindGlyphNoFallback(static_cast<ImWchar>(codepoint))) {
            ++failures_;
            if (missing_glyph_reports_++ < 16) {
                std::fprintf(stderr, "FAIL: missing %s glyph U+%04X\n", source, codepoint);
            }
        }
    }
}

} // namespace azs

int main() {
    return azs::ui_strings_tests::run();
}
