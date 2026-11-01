#include "localization_tests.hpp"
#include "localization.hpp"
#include "config_store.hpp"
#include "trainer_state.hpp"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_set>

namespace azs {

void localization_tests::check(bool condition, const char* description) {
    if (!condition) {
        ++failures_;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

void localization_tests::test_codes() {
    static_assert(native_codes_.size() == static_cast<std::size_t>(client_language::count));
    for (std::size_t index = 0; index < native_codes_.size(); ++index) {
        check(localization::parse_language(native_codes_[index]) == static_cast<client_language>(index),
            "each native short code maps to the intended supported language");
    }
    for (const auto* code : {"", "unknown", "RU", "enUS", "ruRU", "deDE", "frFR", "esES", "ukUA", "zhCN",
                             "PL", "csCZ", "pt-BR", "roRO", "trTR", "jaJP", "koKR", "zh-TW"}) {
        check(localization::parse_language(code) == client_language::english,
            "unsupported native languages and unproven locale aliases fall back to English");
    }
    check(localization::parse_language(" \tru\r\n") == client_language::russian &&
          localization::parse_language(std::string_view("ru\0", 3)) == client_language::english,
        "surrounding whitespace is ignored while embedded NUL never hides an invalid code");
}

std::string localization_tests::format_signature(const char* text) {
    std::string result;
    const std::string_view input(text);
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (input[index] != '%') continue;
        const auto start = index++;
        while (index < input.size() && std::strchr("diuoxXfFeEgGaAcspn%", input[index]) == nullptr) ++index;
        result.append(input.substr(start, index < input.size() ? index - start + 1 : input.size() - start));
        result += '|';
    }
    return result;
}

void localization_tests::visit_text(const char* text) noexcept {
    if (text != nullptr && *text != '\0') {
        ++visited_;
        if (std::strstr(text, "正在等待游戏验证") != nullptr) visited_chinese_ = true;
    }
}

void localization_tests::test_catalog() {
    const auto rows = localization::catalog();
    check(rows.size() >= 70, "catalog includes runtime, party, preset parser and loader diagnostics");
    std::unordered_set<std::string_view> keys;
    for (const auto& row : rows) {
        check(row[0] != nullptr && *row[0] != '\0' && keys.emplace(row[0]).second,
            "canonical English message keys are nonempty and unique");
        const auto signature = format_signature(row[0]);
        for (std::size_t index = 0; index < row.size(); ++index) {
            check(row[index] != nullptr && *row[index] != '\0' &&
                  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, row[index], -1, nullptr, 0) > 0,
                "all native language variants are complete valid UTF-8");
            check(format_signature(row[index]) == signature,
                "all translations preserve ordered printf conversions, widths and integer length modifiers");
            localization::language_.store(static_cast<client_language>(index));
            const auto* translated = localization::translate(row[0]);
            check(std::strcmp(translated, row[index]) == 0 && localization::translate(row[0]) == translated,
                "translation selects process-lifetime text for every language without temporary buffers");
            localization::language_.store(client_language::english);
            check(std::strcmp(localization::translate(row[index]), row[0]) == 0,
                "previously translated static diagnostics return to English after a language switch");
        }
    }
    localization::language_.store(client_language::russian);
    const char unknown[] = "Uncatalogued technical code with %s";
    check(localization::translate(unknown) == unknown && std::strcmp(localization::translate(nullptr), "") == 0,
        "unknown messages keep the exact caller pointer and null input is safe");
    localization::visit_translations(&visit_text);
    localization::visit_translations(nullptr);
    check(visited_ == rows.size() * static_cast<std::size_t>(client_language::count) && visited_chinese_,
        "font glyph visitor receives every language variant including full Chinese diagnostics");
    for (std::size_t index = 0; index < static_cast<std::size_t>(client_language::count); ++index) {
        localization::language_.store(static_cast<client_language>(index));
        trainer_preset output;
        std::string error;
        check(!config_store::parse("[trainer]\nversion=5\n", output, error) &&
              error == std::string(localization::translate("Unknown, duplicate or invalid key/value at line ")) + "2",
            "preset parser translates the diagnostic prefix while preserving its dynamic line number");
    }
    localization::language_.store(client_language::russian);
    trainer_state::instance().set_message(localization::translate("Waiting for game verification..."));
    check(!config_store::load("../escape.ini"), "invalid config filename sets a static diagnostic without touching files");
    for (std::size_t index = 0; index < static_cast<std::size_t>(client_language::count); ++index) {
        localization::language_.store(static_cast<client_language>(index));
        check(trainer_state::instance().get_message() == localization::translate("Waiting for game verification...") &&
              config_store::get_message() == localization::translate("Choose a plain ASCII .ini filename inside configs."),
            "stored static trainer and config statuses follow every subsequent client language switch");
    }
    localization::language_.store(client_language::english);
}

bool localization_tests::write_file(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.close();
    return !output.fail();
}

std::wstring localization_tests::environment_value(const wchar_t* name) {
    std::array<wchar_t, 32768> text{};
    const auto count = GetEnvironmentVariableW(name, text.data(), static_cast<DWORD>(text.size()));
    return count != 0 && count < text.size() ? std::wstring(text.data(), count) : std::wstring{};
}

void localization_tests::test_files(const std::filesystem::path& root) {
    const auto original_appdata = environment_value(L"APPDATA");
    const auto original_save = environment_value(L"WOW_ROGUE_SAVE");
    const auto app_directory = root / L"wenilla-rogue";
    const auto custom_directory = root / L"custom";
    const auto app_language = app_directory / L"language.txt";
    const auto custom_language = custom_directory / L"language.txt";
    check(CreateDirectoryW(root.c_str(), nullptr) != FALSE &&
          CreateDirectoryW(app_directory.c_str(), nullptr) != FALSE &&
          CreateDirectoryW(custom_directory.c_str(), nullptr) != FALSE,
        "uniquely owned language fixture directories are created");
    SetEnvironmentVariableW(L"APPDATA", root.c_str());
    SetEnvironmentVariableW(L"WOW_ROGUE_SAVE", nullptr);
    localization::language_.store(client_language::english);
    localization::next_poll_ms_.store(0);
    check(write_file(app_language, "ru"), "fixture writes only its own Russian language file");
    check(localization::current_language() == client_language::english,
        "cached language remains English until initialize or refresh explicitly polls");
    std::filesystem::path resolved;
    check(localization::language_path(resolved) && resolved == app_language,
        "default preference resolves to APPDATA/wenilla-rogue/language.txt");
    localization::refresh_at(1000);
    check(localization::current_language() == client_language::russian,
        "first due poll reads the client's persisted native short code");
    check(write_file(app_language, "de"), "fixture changes its own persisted language");
    localization::refresh_at(1999);
    check(localization::current_language() == client_language::russian,
        "rapid repeated refresh cannot observe changes before the one-second deadline");
    localization::refresh_at(2000);
    check(localization::current_language() == client_language::german,
        "next due refresh observes a changed preference without restarting");
    check(write_file(app_language, std::string(129, 'x')), "fixture creates an oversized preference");
    localization::refresh_at(3000);
    check(localization::current_language() == client_language::english,
        "oversized language file falls back to English instead of allocating from file size");
    check(write_file(app_language, "ruRU"), "fixture writes an unsupported locale alias");
    localization::refresh_at(4000);
    check(localization::current_language() == client_language::english,
        "unproven WoW-style aliases in files remain unsupported");
    DeleteFileW(app_language.c_str());
    localization::refresh_at(5000);
    check(localization::current_language() == client_language::english,
        "missing preference keeps the safe English fallback");
    check(write_file(app_language, "uk") && write_file(custom_language, "zh"),
        "owned default and overridden preference fixtures contain different languages");
    const auto custom_save = custom_directory / L"custom_save.sav";
    SetEnvironmentVariableW(L"WOW_ROGUE_SAVE", custom_save.c_str());
    check(localization::language_path(resolved) && resolved == custom_language,
        "WOW_ROGUE_SAVE uses its parent directory rather than treating the save file as a folder");
    localization::refresh_at(6000);
    check(localization::current_language() == client_language::chinese_simplified,
        "save-path override wins over an existing APPDATA preference");
    DeleteFileW(custom_language.c_str());
    localization::refresh_at(7000);
    check(localization::current_language() == client_language::english,
        "missing overridden preference does not silently fall back to a different profile's APPDATA language");
    SetEnvironmentVariableW(L"WOW_ROGUE_SAVE", nullptr);
    localization::refresh_at(8000);
    check(localization::current_language() == client_language::ukrainian,
        "removing the controlled override restores APPDATA language resolution");

    std::uint64_t now_ms = 9000;
    for (std::size_t index = 0; index < native_codes_.size(); ++index, now_ms += 1000) {
        const auto previous_language = localization::current_language();
        const auto expected_language = static_cast<client_language>(index);
        check(write_file(app_language, std::string(" \t") + native_codes_[index] + "\r\n"),
            "owned fixture persists each native language code with surrounding whitespace");
        localization::refresh_at(now_ms - 1);
        check(localization::current_language() == previous_language,
            "each persisted language switch respects the shared polling deadline");
        localization::refresh_at(now_ms);
        check(localization::current_language() == expected_language,
            "each native language switches from its persisted preference without restarting");
    }

    SetEnvironmentVariableW(L"APPDATA", original_appdata.empty() ? nullptr : original_appdata.c_str());
    SetEnvironmentVariableW(L"WOW_ROGUE_SAVE", original_save.empty() ? nullptr : original_save.c_str());
    DeleteFileW(app_language.c_str());
    DeleteFileW(custom_language.c_str());
    RemoveDirectoryW(custom_directory.c_str());
    RemoveDirectoryW(app_directory.c_str());
    RemoveDirectoryW(root.c_str());
    localization::language_.store(client_language::english);
    localization::next_poll_ms_.store(0);
}

int localization_tests::run() {
    check(localization::current_language() == client_language::english,
        "fresh localization starts in English without reading real user preferences");
    test_codes();
    test_catalog();
    const auto root = std::filesystem::current_path() / ("localization_test_" +
        std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount64()));
    test_files(root);
    std::printf("native client localization: %s (%d failures)\n", failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

} // namespace azs

int main() {
    return azs::localization_tests::run();
}
