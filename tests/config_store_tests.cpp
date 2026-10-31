#include "config_store_tests.hpp"
#include "config_store.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>

namespace azs {

void config_store_tests::check(bool condition, const char* description) {
    if (!condition) {
        ++failures_;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

trainer_preset config_store_tests::sample_preset() {
    trainer_preset preset;
    preset.fields = {{"gold", 123456, false, true}, {"damage", 3.125, true, true},
                     {"skip", 1, true, false}};
    preset.automation.auto_choose_spells = true;
    preset.automation.auto_choose_jewelry = true;
    preset.automation.legendary_final_boss = true;
    preset.automation.apply_to_party = true;
    preset.automation.choice_delay_ms = 750;
    preset.automation.time_scale = 2.5;
    preset.automation.spell_priorities = {"wrath", "hurricane"};
    preset.automation.jewelry_priorities = {"chosen", "legendary:wrath"};
    return preset;
}

void config_store_tests::reject(const std::string& text, const char* description) {
    auto output = sample_preset();
    const auto before = config_store::serialize(output);
    const auto global_before = config_store::serialize(trainer_state::instance().export_preset());
    std::string error;
    const bool accepted = config_store::parse(text, output, error);
    check(!accepted && !error.empty() && config_store::serialize(output) == before &&
          config_store::serialize(trainer_state::instance().export_preset()) == global_before, description);
}

int config_store_tests::run() {
    const auto sample = sample_preset();
    const auto serialized = config_store::serialize(sample);
    trainer_preset parsed;
    std::string error;
    check(config_store::parse(serialized, parsed, error) && error.empty(),
        "serialized schema 3 preset parses completely");
    check(config_store::serialize(parsed) == serialized && parsed.fields.size() == 3 &&
          parsed.fields.front().key == "damage" && parsed.fields.front().value == 3.125 &&
          parsed.automation.legendary_final_boss && parsed.automation.apply_to_party &&
          parsed.automation.time_scale == 2.5 &&
          parsed.automation.spell_priorities == sample.automation.spell_priorities &&
          parsed.automation.jewelry_priorities == sample.automation.jewelry_priorities,
        "round trip preserves values, flags, stable descriptor order and priority order");
    check(serialized.find("frozen=true") != std::string::npos &&
          serialized.find("legendary_final_boss=true") != std::string::npos &&
          serialized.find("apply_to_party=true") != std::string::npos &&
          serialized.find("spell_priorities=wrath,hurricane") != std::string::npos,
        "INI booleans and list delimiters use canonical locale-independent spellings");

    auto legacy_text = serialized;
    const auto party_line = legacy_text.find("apply_to_party=true\n");
    check(party_line != std::string::npos, "schema 3 fixture contains the optional party flag");
    if (party_line != std::string::npos) {
        legacy_text.erase(party_line, sizeof("apply_to_party=true\n") - 1);
        auto legacy = sample;
        check(config_store::parse(legacy_text, legacy, error) && !legacy.automation.apply_to_party &&
              legacy.automation.legendary_final_boss && legacy.automation.time_scale == 2.5 &&
              legacy.fields.size() == sample.fields.size(),
            "existing schema 3 automation defaults omitted apply_to_party to false");
    }
    trainer_preset party_disabled;
    check(config_store::parse("[trainer]\nversion=3\n[automation]\napply_to_party=false\ntime_scale=1\n",
              party_disabled, error) && !party_disabled.automation.apply_to_party &&
          party_disabled.automation.time_scale == 1,
        "explicit false party flag preserves normal multiplayer time scale");

    const std::string field_prefix = "[trainer]\nversion=3\n[field.damage]\nvalue=";
    const std::string field_suffix = "\nfrozen=false\napply_on_start=true\n";
    for (const auto* value : {"nan", "inf", "-inf", "1e309", "0", "1001", "2oops", "0x1", "1,5"}) {
        reject(field_prefix + value + field_suffix, "invalid/nonfinite/out-of-range floating field is rejected atomically");
    }
    for (const auto* text : {
        "[trainer]\nversion=4\n",
        "[trainer]\nversion=2\n",
        "[trainer]\nversion=3\nversion=3\n",
        "[trainer]\nversion=3\n[trainer]\n",
        "[trainer]\nversion=3\n[unknown]\n",
        "[trainer]\nversion=3\n[field.not_a_field]\nvalue=1\nfrozen=false\napply_on_start=true\n",
        "[trainer]\nversion=3\n[automation]\nunknown=true\n",
        "[trainer]\nversion=3\n[automation]\ntime_scale=2\ntime_scale=3\n",
        "[trainer]\nversion=3\n[automation]\ntime_scale=nan\n",
        "[trainer]\nversion=3\n[automation]\ntime_scale=17\n",
        "[trainer]\nversion=3\n[automation]\nchoice_delay_ms=49\n",
        "[trainer]\nversion=3\n[automation]\nchoice_delay_ms=50.5\n",
        "[trainer]\nversion=3\n[automation]\nauto_choose_spells=True\n",
        "[trainer]\nversion=3\n[automation]\napply_to_party=True\n",
        "[trainer]\nversion=3\n[automation]\napply_to_party=1\n",
        "[trainer]\nversion=3\n[automation]\napply_to_party=true\napply_to_party=false\n",
        "[trainer]\nversion=3\n[automation]\nspell_priorities=wrath,,hurricane\n",
        "[trainer]\nversion=3\n[automation]\nspell_priorities=wrath,\n",
        "[trainer]\nversion=3\n[field.damage]\nvalue=2\nfrozen=false\n",
        "[trainer]\nversion=3\n[field.gold]\nvalue=1.5\nfrozen=false\napply_on_start=true\n",
        "[trainer]\nversion=3\n[field.skip]\nvalue=2\nfrozen=false\napply_on_start=true\n",
        "[trainer]\nversion=3\n[field.damage]\nvalue=2\nfrozen=false\napply_on_start=true\n[field.damage]\n",
        "[trainer]\nversion=3\n[field.damage]\nvalue=2\nvalue=3\nfrozen=false\napply_on_start=true\n",
        "[trainer]\nversion=3\n[field.damage]\nvalue=2\nfrozen=false\napply_on_start=true\nextra=1\n",
        "version=3\n",
        "[trainer]\n"
    }) {
        reject(text, "unknown/duplicate/missing/invalid schema content never partially replaces settings");
    }
    reject(std::string(1024 * 1024 + 1, ' '), "oversized document is rejected before parsing");
    reject(std::string("[trainer]\nversion=3\n") + std::string(1, '\0') + "hidden",
        "embedded NUL cannot hide trailing preset content");
    reject("[trainer]\nversion=3\n[automation]\nspell_priorities=" + std::string(129, 'a') + '\n',
        "priority tokens are bounded at 128 ASCII characters");
    std::string long_list = "[trainer]\nversion=3\n[automation]\nspell_priorities=";
    for (unsigned index = 0; index < 65; ++index) {
        long_list += index == 0 ? "wrath" : ",wrath";
    }
    reject(long_list, "priority lists are bounded at 64 entries");
    trainer_preset safe_defaults;
    check(config_store::parse("\xef\xbb\xbf; UTF-8 preset\r\n [trainer] \r\n version = 3 \r\n",
              safe_defaults, error) && safe_defaults.fields.empty() &&
          !safe_defaults.automation.auto_choose_spells && !safe_defaults.automation.auto_choose_jewelry &&
          !safe_defaults.automation.legendary_final_boss && !safe_defaults.automation.apply_to_party &&
          safe_defaults.automation.time_scale == 1,
        "BOM/comments/CRLF are accepted and omitted automation remains disabled");

    for (const auto* filename : {"../escape.ini", "sub/file.ini", "sub\\file.ini", "x..ini", "C:x.ini",
                                 ".ini", "preset.txt", "x.ini:stream", "CON.ini", "com1.ini", "LPT9.ini"}) {
        check(!config_store::valid_filename(filename), "path escapes, device paths and non-INI names are rejected");
    }
    check(config_store::valid_filename("default.ini") && config_store::valid_filename("test-42_A.ini") &&
          !config_store::valid_filename(std::string(125, 'a') + ".ini"),
        "plain bounded ASCII preset names are accepted");

    check(config_store::initialize(GetModuleHandleW(nullptr)), "config directory is located beside this test executable");
    if (config_store::directory_.empty()) {
        return 1;
    }
    const auto filename = "config_store_test_" + std::to_string(GetCurrentProcessId()) + "_" +
        std::to_string(GetTickCount64()) + ".ini";
    const auto path = config_store::directory_ / filename;
    auto& state = trainer_state::instance();
    check(state.import_preset(sample), "fixture settings import succeeds before file tests");
    check(config_store::save(filename), "atomic save writes a complete uniquely owned preset");
    std::string original_text;
    check(config_store::read_text(path, original_text, error) && original_text == serialized,
        "saved bytes equal complete canonical serialization");
    auto alternative = sample;
    alternative.fields[1].value = 7;
    check(state.import_preset(alternative), "alternate in-memory settings are prepared");
    const auto held = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(held != INVALID_HANDLE_VALUE, "fixture holds target open without delete sharing");
    if (held != INVALID_HANDLE_VALUE) {
        check(!config_store::save(filename), "failed atomic replacement reports failure instead of truncating target");
        CloseHandle(held);
        std::string preserved;
        check(config_store::read_text(path, preserved, error) && preserved == original_text,
            "replacement failure leaves all previous target bytes intact");
    }
    auto names = config_store::list_configs();
    check(std::is_sorted(names.begin(), names.end()) &&
          std::find(names.begin(), names.end(), filename) != names.end(),
        "list_configs returns sorted plain local INI names");
    check(config_store::load(filename) && config_store::serialize(state.export_preset()) == serialized,
        "valid preset load imports complete settings atomically");
    const auto before_invalid_load = config_store::serialize(state.export_preset());
    {
        std::ofstream invalid(path, std::ios::binary | std::ios::trunc);
        invalid << "[trainer]\nversion=3\n[field.unknown]\nvalue=1\n";
    }
    check(!config_store::load(filename) && !config_store::get_message().empty() &&
          config_store::serialize(state.export_preset()) == before_invalid_load,
        "malformed file load preserves every prior field and automation setting");
    check(!config_store::load("../escape.ini") && !config_store::save("../escape.ini") &&
          config_store::serialize(state.export_preset()) == before_invalid_load,
        "filename rejection cannot mutate trainer settings or write outside configs");
    DeleteFileW(path.c_str());
    std::printf("strict local preset store: %s (%d failures)\n", failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

} // namespace azs

int main() {
    return azs::config_store_tests::run();
}
