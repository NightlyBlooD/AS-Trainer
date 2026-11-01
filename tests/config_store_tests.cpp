#include "config_store_tests.hpp"
#include "config_store.hpp"
#include "game_layout.hpp"

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

void config_store_tests::check_schema_migration() {
    const std::string legacy_fields =
        "[field.thorns]\nvalue=7.25\nfrozen=true\napply_on_start=false\n"
        "[field.buff_power]\nvalue=0.75\nfrozen=false\napply_on_start=true\n";
    const std::string legacy_source = "[trainer]\nversion=3\n" + legacy_fields;
    const auto original_source = legacy_source;
    trainer_preset migrated;
    std::string error;
    check(config_store::parse(legacy_source, migrated, error) && migrated.fields.size() == 2,
        "schema 3 accepts both old field names before canonical migration");
    if (migrated.fields.size() == 2) {
        check(migrated.fields[0].key == "buff_power" && migrated.fields[0].value == 7.25
            && migrated.fields[0].frozen && !migrated.fields[0].apply_on_start
            && migrated.fields[1].key == "dot_haste" && migrated.fields[1].value == 0.75
            && !migrated.fields[1].frozen && migrated.fields[1].apply_on_start,
            "schema 3 correction preserves the actual field effects, numeric values and flags");
    }
    const std::string canonical = config_store::serialize(migrated);
    check(canonical.starts_with("[trainer]\nversion=4\n")
        && canonical.find("[field.thorns]") == std::string::npos
        && canonical.find("[field.buff_power]\nvalue=7.25") != std::string::npos
        && canonical.find("[field.dot_haste]\nvalue=0.75") != std::string::npos,
        "migrated settings serialize only canonical schema 4 field names");
    trainer_preset round_trip;
    check(config_store::parse(canonical, round_trip, error)
        && config_store::serialize(round_trip) == canonical && legacy_source == original_source,
        "schema 4 round trip is stable and parsing leaves original schema 3 text unchanged");

    trainer_preset late_header;
    check(config_store::parse(legacy_fields + "[trainer]\nversion=3\n", late_header, error)
        && config_store::serialize(late_header) == canonical,
        "schema 3 migration does not depend on the trainer header preceding fields");
    const std::string canonical_fields =
        "[field.buff_power]\nvalue=12.5\nfrozen=false\napply_on_start=true\n"
        "[field.dot_haste]\nvalue=0.5\nfrozen=true\napply_on_start=false\n";
    trainer_preset canonical_preset;
    check(config_store::parse(canonical_fields + "[trainer]\nversion=4\n", canonical_preset, error)
        && canonical_preset.fields.size() == 2
        && canonical_preset.fields[0].key == "buff_power" && canonical_preset.fields[0].value == 12.5
        && canonical_preset.fields[1].key == "dot_haste" && canonical_preset.fields[1].value == 0.5,
        "schema 4 preserves canonical buff values above one instead of applying legacy renaming");
    reject("[trainer]\nversion=4\n" + legacy_fields,
        "schema 4 rejects the obsolete thorns field rather than treating it as an alias");
    reject("[trainer]\nversion=3\n" + canonical_fields,
        "schema 3 rejects new dot_haste keys instead of accepting mixed ambiguous names");
    reject("[trainer]\nversion=3\n[field.thorns]\nvalue=8\nfrozen=true\napply_on_start=true\n"
        "[field.buff_power]\nvalue=1.01\nfrozen=false\napply_on_start=true\n",
        "invalid migrated DoT value cannot partially replace output or global settings");
    reject("[trainer]\nversion=4\n" + canonical_fields + canonical_fields,
        "duplicate canonical field sections are rejected atomically");
    for (const auto* value : {"-0.01", "1.01", "100", "nan"}) {
        reject(std::string("[trainer]\nversion=4\n[field.dot_haste]\nvalue=") + value
            + "\nfrozen=false\napply_on_start=true\n",
            "canonical DoT field rejects nonfinite or out-of-cap values without clamping");
    }
    const auto prior_layout = game_layout::current_id();
    game_layout::select(layout_id::current);
    const std::string lifesteal_fields =
        "[field.lifesteal]\nvalue=0.25\nfrozen=true\napply_on_start=false\n";
    trainer_preset computed;
    check(config_store::parse("[trainer]\nversion=4\n" + lifesteal_fields, computed, error)
        && computed.fields.size() == 1 && computed.fields[0].key == "lifesteal"
        && computed.fields[0].value == 0.25 && computed.fields[0].frozen
        && !computed.fields[0].apply_on_start
        && config_store::serialize(computed).find(lifesteal_fields) != std::string::npos,
        "current profile round-trips computed lifesteal values and flags through schema 4");
    reject("[trainer]\nversion=3\n" + lifesteal_fields,
        "schema 3 rejects the newly introduced computed field rather than accepting an unknown old key");
    reject("[trainer]\nversion=4\n[field.lifesteal]\nvalue=10.01\nfrozen=true\napply_on_start=true\n",
        "computed editor range also protects preset import without a silent clamp");
    game_layout::select(layout_id::legacy);
    reject("[trainer]\nversion=4\n" + lifesteal_fields,
        "legacy preset loading rejects the unsupported computed native getter field atomically");
    game_layout::select(prior_layout);
}

int config_store_tests::run() {
    const auto sample = sample_preset();
    const auto serialized = config_store::serialize(sample);
    trainer_preset parsed;
    std::string error;
    check(config_store::parse(serialized, parsed, error) && error.empty(),
        "serialized schema 4 preset parses completely");
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
    check(party_line != std::string::npos, "schema 4 fixture contains the optional party flag");
    if (party_line != std::string::npos) {
        legacy_text.erase(party_line, sizeof("apply_to_party=true\n") - 1);
        auto legacy = sample;
        check(config_store::parse(legacy_text, legacy, error) && !legacy.automation.apply_to_party &&
              legacy.automation.legendary_final_boss && legacy.automation.time_scale == 2.5 &&
              legacy.fields.size() == sample.fields.size(),
            "schema 4 automation defaults omitted apply_to_party to false");
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
        "[trainer]\nversion=5\n",
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
    check_schema_migration();

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
    const std::string legacy_file =
        "[field.thorns]\nvalue=3.5\nfrozen=true\napply_on_start=false\n"
        "[field.buff_power]\nvalue=0.5\nfrozen=false\napply_on_start=true\n"
        "[trainer]\nversion=3\n";
    {
        std::ofstream legacy(path, std::ios::binary | std::ios::trunc);
        legacy << legacy_file;
    }
    std::string preserved_legacy;
    check(config_store::load(filename)
        && config_store::read_text(path, preserved_legacy, error) && preserved_legacy == legacy_file
        && config_store::serialize(state.export_preset()).find("[field.buff_power]\nvalue=3.5") != std::string::npos
        && config_store::serialize(state.export_preset()).find("[field.dot_haste]\nvalue=0.5") != std::string::npos,
        "loading a schema 3 file migrates settings without rewriting any source bytes");
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
