#include "config_store.hpp"
#include "localization.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace azs {
namespace {

constexpr std::size_t maximum_document_size = 1024u * 1024u;

} // namespace

std::string_view config_store::trim(std::string_view value) noexcept {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
        value.remove_suffix(1);
    }
    return value;
}

bool config_store::valid_key(std::string_view value) noexcept {
    if (value.empty() || value.size() > 128) {
        return false;
    }
    for (const unsigned char character : value) {
        if ((character < 'a' || character > 'z') && (character < 'A' || character > 'Z') &&
            (character < '0' || character > '9') && character != '_' && character != '.' &&
            character != ':' && character != '-') {
            return false;
        }
    }
    return true;
}

bool config_store::valid_filename(const std::string& filename) noexcept {
    if (filename.size() < 5 || filename.size() > 128 || filename.compare(filename.size() - 4, 4, ".ini") != 0) {
        return false;
    }
    const std::string_view stem(filename.data(), filename.size() - 4);
    for (const unsigned char character : stem) {
        if ((character < 'a' || character > 'z') && (character < 'A' || character > 'Z') &&
            (character < '0' || character > '9') && character != '_' && character != '-') {
            return false;
        }
    }
    std::array<char, 5> device{};
    if (stem.size() <= 4) {
        for (std::size_t index = 0; index < stem.size(); ++index) {
            const auto character = stem[index];
            device[index] = character >= 'a' && character <= 'z' ? character - ('a' - 'A') : character;
        }
        const std::string_view name(device.data(), stem.size());
        if (name == "CON" || name == "PRN" || name == "AUX" || name == "NUL" ||
            (name.size() == 4 && (name.substr(0, 3) == "COM" || name.substr(0, 3) == "LPT") &&
             name[3] >= '1' && name[3] <= '9')) {
            return false;
        }
    }
    return true;
}

bool config_store::parse_number(std::string_view value, double& result) noexcept {
    double parsed{};
    const auto conversion = std::from_chars(value.data(), value.data() + value.size(), parsed,
        std::chars_format::general);
    if (conversion.ec != std::errc{} || conversion.ptr != value.data() + value.size() || !std::isfinite(parsed)) {
        return false;
    }
    result = parsed;
    return true;
}

bool config_store::parse_boolean(std::string_view value, bool& result) noexcept {
    if (value == "true") {
        result = true;
        return true;
    }
    if (value == "false") {
        result = false;
        return true;
    }
    return false;
}

bool config_store::parse_priorities(std::string_view value, std::vector<std::string>& result) {
    result.clear();
    if (value.empty()) {
        return true;
    }
    while (true) {
        const auto delimiter = value.find(',');
        const auto token = trim(value.substr(0, delimiter));
        if (!valid_key(token) || result.size() >= 64) {
            return false;
        }
        result.emplace_back(token);
        if (delimiter == std::string_view::npos) {
            return true;
        }
        value.remove_prefix(delimiter + 1);
    }
}

bool config_store::validate_preset(const trainer_preset& preset, std::string& error) {
    if (preset.fields.size() > trainer_state::field_count()) {
        error = localization::translate("Too many configured fields.");
        return false;
    }
    std::array<bool, 64> seen{};
    for (const auto& field : preset.fields) {
        std::size_t found = trainer_state::field_count();
        for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
            const auto& descriptor = trainer_state::field_at(index);
            if (descriptor.key != nullptr && field.key == descriptor.key) {
                found = index;
                break;
            }
        }
        if (found >= trainer_state::field_count() || !valid_key(field.key) || seen[found]) {
            error = localization::translate("Unknown or duplicate field key: ") + field.key;
            return false;
        }
        seen[found] = true;
        const auto& descriptor = trainer_state::field_at(found);
        if (!std::isfinite(field.value) || field.value < descriptor.minimum || field.value > descriptor.maximum ||
            (descriptor.kind != value_kind::real && std::floor(field.value) != field.value)) {
            error = localization::translate("Invalid value for field: ") + field.key;
            return false;
        }
    }
    const auto& automation = preset.automation;
    if (automation.choice_delay_ms < 50 || automation.choice_delay_ms > 5000 ||
        !std::isfinite(automation.time_scale) || automation.time_scale < 1 || automation.time_scale > 16) {
        error = localization::translate("Automation delay/time multiplier is outside its supported range.");
        return false;
    }
    for (const auto* priorities : {&automation.spell_priorities, &automation.jewelry_priorities}) {
        if (priorities->size() > 64) {
            error = localization::translate("A priority list exceeds 64 entries.");
            return false;
        }
        for (const auto& key : *priorities) {
            if (!valid_key(key)) {
                error = localization::translate("Invalid stable key in a priority list.");
                return false;
            }
        }
    }
    return true;
}

std::string config_store::format_number(double value) {
    std::array<char, 64> buffer{};
    const auto conversion = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
        std::chars_format::general, std::numeric_limits<double>::max_digits10);
    if (conversion.ec != std::errc{}) {
        throw std::runtime_error(localization::translate("Cannot format preset number."));
    }
    return {buffer.data(), conversion.ptr};
}

std::string config_store::serialize(const trainer_preset& preset) {
    std::string error;
    if (!validate_preset(preset, error)) {
        throw std::invalid_argument(error);
    }
    std::string text = "[trainer]\nversion=4\n\n";
    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
        const auto& descriptor = trainer_state::field_at(index);
        if (descriptor.key == nullptr) {
            continue;
        }
        for (const auto& field : preset.fields) {
            if (field.key != descriptor.key) {
                continue;
            }
            text += "[field." + field.key + "]\nvalue=" + format_number(field.value);
            text += field.frozen ? "\nfrozen=true" : "\nfrozen=false";
            text += field.apply_on_start ? "\napply_on_start=true\n\n" : "\napply_on_start=false\n\n";
            break;
        }
    }
    const auto& automation = preset.automation;
    text += "[automation]\nauto_choose_spells=";
    text += automation.auto_choose_spells ? "true" : "false";
    text += "\nauto_choose_jewelry=";
    text += automation.auto_choose_jewelry ? "true" : "false";
    text += "\nlegendary_final_boss=";
    text += automation.legendary_final_boss ? "true" : "false";
    text += "\napply_to_party=";
    text += automation.apply_to_party ? "true" : "false";
    text += "\nchoice_delay_ms=" + std::to_string(automation.choice_delay_ms);
    text += "\ntime_scale=" + format_number(automation.time_scale);
    for (const auto& pair : {std::pair{"spell_priorities", &automation.spell_priorities},
                             std::pair{"jewelry_priorities", &automation.jewelry_priorities}}) {
        text += '\n';
        text += pair.first;
        text += '=';
        bool first = true;
        for (const auto& key : *pair.second) {
            if (!first) {
                text += ',';
            }
            text += key;
            first = false;
        }
    }
    text += '\n';
    return text;
}

bool config_store::parse(const std::string& text, trainer_preset& preset, std::string& error) noexcept {
    try {
        if (text.size() > maximum_document_size || text.find('\0') != std::string::npos) {
            error = localization::translate("Preset is oversized or contains a NUL byte.");
            return false;
        }
        trainer_preset candidate;
        enum class section_kind { none, trainer, automation, field };
        section_kind section = section_kind::none;
        std::unordered_set<std::string> sections;
        std::vector<unsigned char> field_masks;
        std::size_t current_field = 0;
        unsigned automation_mask = 0;
        unsigned schema_version = 0;
        std::string_view input(text);
        if (input.starts_with("\xef\xbb\xbf")) {
            input.remove_prefix(3);
        }
        std::size_t line_number = 0;
        while (!input.empty()) {
            ++line_number;
            const auto newline = input.find('\n');
            const auto line = trim(input.substr(0, newline));
            if (newline == std::string_view::npos) {
                input = {};
            } else {
                input.remove_prefix(newline + 1);
            }
            if (line.empty() || line.front() == '#' || line.front() == ';') {
                continue;
            }
            if (line.front() == '[') {
                if (line.size() < 3 || line.back() != ']') {
                    error = localization::translate("Malformed section at line ") + std::to_string(line_number);
                    return false;
                }
                const auto name = line.substr(1, line.size() - 2);
                if (!sections.emplace(name).second) {
                    error = localization::translate("Duplicate section at line ") + std::to_string(line_number);
                    return false;
                }
                if (name == "trainer") {
                    section = section_kind::trainer;
                } else if (name == "automation") {
                    section = section_kind::automation;
                } else if (name.starts_with("field.")) {
                    const auto key = name.substr(6);
                    bool known = key == "thorns";
                    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
                        const auto* descriptor_key = trainer_state::field_at(index).key;
                        if (descriptor_key != nullptr && key == descriptor_key) {
                            known = true;
                            break;
                        }
                    }
                    if (!known) {
                        error = localization::translate("Unknown field section at line ") + std::to_string(line_number);
                        return false;
                    }
                    section = section_kind::field;
                    current_field = candidate.fields.size();
                    candidate.fields.push_back({std::string(key), 0, false, false});
                    field_masks.push_back(0);
                } else {
                    error = localization::translate("Unknown section at line ") + std::to_string(line_number);
                    return false;
                }
                continue;
            }
            const auto equals = line.find('=');
            if (equals == std::string_view::npos || section == section_kind::none) {
                error = localization::translate("Expected a section/key assignment at line ") + std::to_string(line_number);
                return false;
            }
            const auto key = trim(line.substr(0, equals));
            const auto value = trim(line.substr(equals + 1));
            bool accepted = false;
            if (section == section_kind::trainer) {
                if (key == "version" && schema_version == 0 && (value == "3" || value == "4")) {
                    schema_version = value == "3" ? 3 : 4;
                    accepted = true;
                }
            } else if (section == section_kind::field) {
                auto& field = candidate.fields[current_field];
                auto& mask = field_masks[current_field];
                const unsigned char bit = key == "value" ? 1 : key == "frozen" ? 2 : key == "apply_on_start" ? 4 : 0;
                if (bit != 0 && (mask & bit) == 0) {
                    accepted = bit == 1 ? parse_number(value, field.value) :
                        parse_boolean(value, bit == 2 ? field.frozen : field.apply_on_start);
                    if (accepted) {
                        mask |= bit;
                    }
                }
            } else if (section == section_kind::automation) {
                unsigned bit = 0;
                if (key == "auto_choose_spells") bit = 1;
                else if (key == "auto_choose_jewelry") bit = 2;
                else if (key == "legendary_final_boss") bit = 4;
                else if (key == "choice_delay_ms") bit = 8;
                else if (key == "time_scale") bit = 16;
                else if (key == "spell_priorities") bit = 32;
                else if (key == "jewelry_priorities") bit = 64;
                else if (key == "apply_to_party") bit = 128;
                if (bit != 0 && (automation_mask & bit) == 0) {
                    auto& automation = candidate.automation;
                    if (bit == 1) accepted = parse_boolean(value, automation.auto_choose_spells);
                    else if (bit == 2) accepted = parse_boolean(value, automation.auto_choose_jewelry);
                    else if (bit == 4) accepted = parse_boolean(value, automation.legendary_final_boss);
                    else if (bit == 8) {
                        const auto conversion = std::from_chars(value.data(), value.data() + value.size(),
                            automation.choice_delay_ms);
                        accepted = conversion.ec == std::errc{} && conversion.ptr == value.data() + value.size();
                    } else if (bit == 16) accepted = parse_number(value, automation.time_scale);
                    else if (bit == 32) accepted = parse_priorities(value, automation.spell_priorities);
                    else if (bit == 64) accepted = parse_priorities(value, automation.jewelry_priorities);
                    else accepted = parse_boolean(value, automation.apply_to_party);
                    if (accepted) {
                        automation_mask |= bit;
                    }
                }
            }
            if (!accepted) {
                error = localization::translate("Unknown, duplicate or invalid key/value at line ") + std::to_string(line_number);
                return false;
            }
        }
        if (schema_version == 0) {
            error = localization::translate("Missing supported [trainer] version.");
            return false;
        }
        for (auto mask : field_masks) {
            if (mask != 7) {
                error = localization::translate("Every field needs value, frozen and apply_on_start.");
                return false;
            }
        }
        if (schema_version == 3) {
            for (auto& field : candidate.fields) {
                if (field.key == "dot_haste" || field.key == "lifesteal") {
                    error = localization::translate("Unknown or duplicate field key: ") + field.key;
                    return false;
                }
                if (field.key == "thorns") {
                    field.key = "buff_power";
                } else if (field.key == "buff_power") {
                    field.key = "dot_haste";
                }
            }
        }
        if (!validate_preset(candidate, error)) {
            return false;
        }
        preset = std::move(candidate);
        error.clear();
        return true;
    } catch (...) {
        try {
            error = localization::translate("Cannot allocate or parse preset storage.");
        } catch (...) {
        }
        return false;
    }
}

bool config_store::initialize(HMODULE module) noexcept {
    try {
        std::lock_guard lock(mutex_);
        directory_.clear();
        std::array<wchar_t, 32768> module_path{};
        const auto length = GetModuleFileNameW(module, module_path.data(), static_cast<DWORD>(module_path.size()));
        if (length == 0 || length >= module_path.size()) {
            message_ = localization::translate("Cannot resolve the loaded module's config directory.");
            return false;
        }
        const auto directory = std::filesystem::path(module_path.data()).parent_path() / L"configs";
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error || !std::filesystem::is_directory(directory, error) || error) {
            message_ = localization::translate("Cannot create or access the local configs directory.");
            return false;
        }
        directory_ = directory;
        message_ = localization::translate("Config directory ready; no preset was loaded automatically.");
        return true;
    } catch (...) {
        report_failure("Cannot initialize preset storage.");
        return false;
    }
}

std::vector<std::string> config_store::list_configs() {
    std::lock_guard lock(mutex_);
    std::vector<std::string> names;
    std::error_code error;
    if (directory_.empty()) {
        return names;
    }
    for (std::filesystem::directory_iterator iterator(directory_, error), end; !error && iterator != end;
         iterator.increment(error)) {
        if (!iterator->is_regular_file(error) || error) {
            continue;
        }
        const auto utf8 = iterator->path().filename().u8string();
        std::string name(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        if (valid_filename(name)) {
            names.push_back(std::move(name));
        }
    }
    if (error) {
        message_ = localization::translate("Cannot enumerate all local config presets.");
        return {};
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool config_store::read_text(const std::filesystem::path& path, std::string& text, std::string& error) {
    const auto raw = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (raw == INVALID_HANDLE_VALUE) {
        error = localization::translate("Cannot open preset (Win32 ") + std::to_string(GetLastError()) + ").";
        return false;
    }
    const std::unique_ptr<void, decltype(&CloseHandle)> handle(raw, &CloseHandle);
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(raw, &size) || size.QuadPart < 0 || size.QuadPart > maximum_document_size) {
        error = localization::translate("Preset file exceeds one MiB or has an unreadable size.");
        return false;
    }
    text.resize(static_cast<std::size_t>(size.QuadPart));
    DWORD read = 0;
    if (!ReadFile(raw, text.data(), static_cast<DWORD>(text.size()), &read, nullptr) || read != text.size()) {
        error = localization::translate("Cannot read the complete preset file.");
        return false;
    }
    return true;
}

bool config_store::write_atomic(const std::filesystem::path& path, const std::string& text, std::string& error) {
    std::filesystem::path temporary;
    HANDLE raw = INVALID_HANDLE_VALUE;
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        temporary = path.parent_path() / (path.filename().wstring() + L"." + std::to_wstring(GetCurrentProcessId()) +
            L"." + std::to_wstring(++temporary_sequence_) + L".tmp");
        raw = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (raw != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_EXISTS) {
            break;
        }
    }
    if (raw == INVALID_HANDLE_VALUE) {
        error = localization::translate("Cannot create a local temporary preset file.");
        return false;
    }
    std::unique_ptr<void, decltype(&CloseHandle)> handle(raw, &CloseHandle);
    DWORD written = 0;
    const auto complete = text.size() <= maximum_document_size &&
        WriteFile(raw, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
        written == text.size() && FlushFileBuffers(raw);
    handle.reset();
    if (!complete) {
        DeleteFileW(temporary.c_str());
        error = localization::translate("Cannot write and flush the complete preset; previous file preserved.");
        return false;
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto code = GetLastError();
        DeleteFileW(temporary.c_str());
        error = localization::translate("Cannot replace preset atomically (Win32 ") + std::to_string(code) + ").";
        return false;
    }
    return true;
}

bool config_store::save(const std::string& filename) noexcept {
    try {
        std::lock_guard lock(mutex_);
        if (directory_.empty() || !valid_filename(filename)) {
            message_ = localization::translate("Choose a plain ASCII .ini filename inside configs.");
            return false;
        }
        const auto text = serialize(trainer_state::instance().export_preset());
        std::string error;
        if (!write_atomic(directory_ / std::filesystem::path(filename), text, error)) {
            message_ = std::move(error);
            return false;
        }
        message_ = localization::translate("Saved ") + filename;
        return true;
    } catch (...) {
        report_failure("Cannot serialize or save preset; previous file preserved.");
        return false;
    }
}

bool config_store::load(const std::string& filename) noexcept {
    try {
        std::lock_guard lock(mutex_);
        if (directory_.empty() || !valid_filename(filename)) {
            message_ = localization::translate("Choose a plain ASCII .ini filename inside configs.");
            return false;
        }
        std::string text;
        std::string error;
        trainer_preset preset;
        if (!read_text(directory_ / std::filesystem::path(filename), text, error) || !parse(text, preset, error)) {
            message_ = std::move(error);
            return false;
        }
        if (!trainer_state::instance().import_preset(preset)) {
            message_ = localization::translate("Preset rejected; current trainer settings preserved.");
            return false;
        }
        message_ = localization::translate("Loaded ") + filename;
        return true;
    } catch (...) {
        report_failure("Cannot load preset; current trainer settings preserved.");
        return false;
    }
}

std::string config_store::get_message() {
    std::lock_guard lock(mutex_);
    return localization::translate(message_.c_str());
}

void config_store::report_failure(const char* message) noexcept {
    try {
        std::lock_guard lock(mutex_);
        message_ = localization::translate(message);
    } catch (...) {
    }
}

} // namespace azs
