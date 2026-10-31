#include "signature_profile_tests.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>

namespace azs {

void signature_profile_tests::check(bool condition, const char* description) {
    if (!condition) {
        ++failures_;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

std::string signature_profile_tests::profile_text(std::string_view rules) {
    std::string text = "[profile]\nname=Fixture\nsha256=";
    for (unsigned index = 0; index < 4; ++index) text += "0123456789ABCDEF";
    text += "\nlayout=analysis\n\n";
    text += rules.empty() ? "[symbol.tick]\npattern=48 8B ?? ??\nsection=.text\nresolve=match\n" : rules;
    return text;
}

bool signature_profile_tests::same_profile(const signature_profile& left, const signature_profile& right) {
    if (left.name != right.name || left.sha256 != right.sha256 || left.layout != right.layout ||
        left.rules.size() != right.rules.size()) return false;
    for (std::size_t index = 0; index < left.rules.size(); ++index) {
        const auto& a = left.rules[index];
        const auto& b = right.rules[index];
        if (a.name != b.name || a.pattern != b.pattern || a.section != b.section || a.resolve != b.resolve ||
            a.displacement_offset != b.displacement_offset || a.instruction_size != b.instruction_size ||
            a.addend != b.addend) return false;
    }
    return true;
}

void signature_profile_tests::reject(std::string_view text, const char* description) {
    signature_profile output;
    std::string error;
    check(signature_profiles::parse(profile_text(), output, error), "atomic rejection fixture starts with a valid profile");
    const auto before = output;
    check(!signature_profiles::parse(text, output, error) && !error.empty() && same_profile(output, before), description);
}

void signature_profile_tests::test_valid_profiles() {
    signature_profile result;
    std::string error;
    check(signature_profiles::parse(profile_text(), result, error) && error.empty() && result.name == "Fixture" &&
          result.layout == "analysis" && result.sha256 ==
              "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" &&
          result.rules.size() == 1 && result.rules[0].name == "tick" && result.rules[0].addend == 0 &&
          result.rules[0].displacement_offset == 0 && result.rules[0].instruction_size == 0,
        "complete profile normalizes SHA case, permits analysis layout and defaults optional numbers to zero");
    const auto all_modes = profile_text(
        "[symbol.direct]\npattern=55 48 89\nsection=.text\nresolve=match\naddend=2147483647\n"
        "[symbol.unwind]\npattern=48 83 EC ??\nsection=.text\nresolve=function_start\naddend=-2147483648\n"
        "[symbol.call]\npattern=E8 ?? ?? ?? ??\nsection=.text\nresolve=rip_rel32\ndisplacement_offset=1\ninstruction_size=5\naddend=-8\n"
        "[symbol.field_luck]\npattern=F3 44 0F 10 B6 ?? ?? ?? ??\nsection=*\nresolve=field_disp32\ndisplacement_offset=5\n");
    check(signature_profiles::parse(all_modes, result, error) && result.rules.size() == 4 &&
          result.rules[0].resolve == pattern_resolve_mode::match && result.rules[0].addend == 2147483647 &&
          result.rules[1].resolve == pattern_resolve_mode::function_start &&
          result.rules[1].addend == std::numeric_limits<std::int32_t>::min() &&
          result.rules[2].resolve == pattern_resolve_mode::rip_rel32 && result.rules[2].displacement_offset == 1 &&
          result.rules[2].instruction_size == 5 && result.rules[2].addend == -8 &&
          result.rules[3].resolve == pattern_resolve_mode::field_disp32 && result.rules[3].section == "*",
        "every resolution mode preserves its exact signed addend and displacement parameters");
    check(signature_profiles::parse(profile_text(
        "[symbol.boundary]\npattern=48 ??\nsection=.text\nresolve=rip_rel32\n"
        "displacement_offset=4294967291\ninstruction_size=4294967295\n"), result, error) &&
          result.rules[0].instruction_size == std::numeric_limits<std::uint32_t>::max(),
        "unsigned offset boundary is parsed without overflow before image-specific validation");
    auto unicode = profile_text();
    unicode.replace(unicode.find("name=Fixture"), sizeof("name=Fixture") - 1, "name=Новый клиент 中文");
    check(signature_profiles::parse("\xef\xbb\xbf; comment\r\n" + unicode, result, error) &&
          result.name == "Новый клиент 中文", "BOM, comments and valid Unicode metadata are preserved losslessly");
    auto maximum = profile_text();
    maximum.append(signature_profiles::maximum_document_size - maximum.size(), ' ');
    check(signature_profiles::parse(maximum, result, error), "exactly one MiB of bounded profile text remains accepted");
}

void signature_profile_tests::test_rejections() {
    const auto good = profile_text();
    for (const auto* rules : {
        "[symbol.tick]\npattern=48 8B\nsection=.text\nresolve=match\nunknown=1\n",
        "[symbol.tick]\npattern=48 8B\npattern=55\nsection=.text\nresolve=match\n",
        "[symbol.tick]\npattern=48 8B\nsection=.text\nresolve=match\n[symbol.tick]\n",
        "[symbol.1bad]\npattern=48\nsection=.text\nresolve=match\n",
        "[symbol.bad-name]\npattern=48\nsection=.text\nresolve=match\n",
        "[unknown]\npattern=48\n",
        "[symbol.tick]\npattern=? ??\nsection=.text\nresolve=match\n",
        "[symbol.tick]\npattern=4?\nsection=.text\nresolve=match\n",
        "[symbol.tick]\npattern=48\nsection=.toolong9\nresolve=match\n",
        "[symbol.tick]\npattern=48\nsection=\nresolve=match\n",
        "[symbol.tick]\npattern=48\nsection=.text\nresolve=first\n",
        "[symbol.tick]\npattern=48\nsection=.text\n",
        "[symbol.tick]\npattern=48\nsection=.text\nresolve=field_disp32\n",
        "[symbol.tick]\npattern=48\nsection=.text\nresolve=rip_rel32\ndisplacement_offset=1\n",
        "[symbol.tick]\npattern=48\nsection=.text\nresolve=rip_rel32\ndisplacement_offset=4\ninstruction_size=7\n",
        "[symbol.tick]\npattern=48\nsection=.text\nresolve=match\ndisplacement_offset=-1\n",
        "[symbol.tick]\npattern=48\nsection=.text\nresolve=match\ndisplacement_offset=4294967296\n",
        "[symbol.tick]\npattern=48\nsection=.text\nresolve=match\ninstruction_size=0x10\n",
        "[symbol.tick]\npattern=48\nsection=.text\nresolve=match\naddend=2147483648\n",
        "[symbol.tick]\npattern=48\nsection=.text\nresolve=match\naddend=-2147483649\n",
        "[symbol.tick]\npattern=48\nsection=.text\nresolve=match\naddend=1.5\n",
        "[symbol.tick]\npattern=48\nsection=.text\nresolve=match\naddend=1\naddend=2\n"
    }) reject(profile_text(rules), "unknown, duplicate, malformed or overflowing rule content is rejected atomically");
    for (const auto* metadata : {
        "[profile]\nname=A\nsha256=123\nlayout=current\n",
        "[profile]\nname=A\nname=B\n",
        "[profile]\nname=A\nunknown=1\n",
        "[profile]\nname=\n",
        "[profile]\nname=A\nlayout=bad layout\n",
        "[profile]\nname=A\n[profile]\n",
        "name=A\n",
        "[profile\nname=A\n"
    }) reject(metadata, "invalid metadata, duplicate section and missing section are rejected atomically");
    auto invalid_hash = good;
    invalid_hash[invalid_hash.find("sha256=") + 7] = 'g';
    reject(invalid_hash, "SHA-256 accepts exactly hexadecimal digits");
    auto no_rules = good.substr(0, good.find("[symbol."));
    reject(no_rules, "profile without symbol rules cannot produce a vacuous successful scan");
    reject(good + std::string(1, '\0'), "embedded NUL cannot conceal trailing profile content");
    for (const auto& invalid_utf8 : {std::string("\xc0\xaf"), std::string("\xed\xa0\x80"),
                                    std::string("\xf4\x90\x80\x80"), std::string("\xe2\x82")})
        reject(good + invalid_utf8, "overlong, surrogate, out-of-range and truncated UTF-8 are rejected");
    reject(std::string(signature_profiles::maximum_document_size + 1, ' '),
        "profile larger than one MiB is refused before parsing");
    std::string rules;
    for (unsigned index = 0; index < 128; ++index)
        rules += "[symbol.rule_" + std::to_string(index) + "]\npattern=90\nsection=.text\nresolve=match\n";
    signature_profile result;
    std::string error;
    check(signature_profiles::parse(profile_text(rules), result, error) && result.rules.size() == 128,
        "exactly 128 unique bounded rules are accepted");
    rules += "[symbol.rule_128]\npattern=90\nsection=.text\nresolve=match\n";
    reject(profile_text(rules), "a 129th rule is rejected without partial output replacement");
}

void signature_profile_tests::test_file_load() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::current_path() / ("signature_profile_test_" + std::to_string(stamp) + ".ini");
    const auto valid = profile_text();
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(valid.data(), static_cast<std::streamsize>(valid.size()));
        check(static_cast<bool>(file), "only a uniquely owned temporary INI is written for file tests");
    }
    signature_profile result;
    std::string error;
    check(signature_profiles::load_file(path, result, error) && result.rules.size() == 1,
        "complete bounded file load commits its validated profile");
    const auto before = result;
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << "[profile]\nunknown=1\n";
    }
    check(!signature_profiles::load_file(path, result, error) && same_profile(result, before),
        "malformed file content does not mutate a previously loaded profile");
    std::error_code remove_error;
    check(std::filesystem::remove(path, remove_error) && !remove_error,
        "file test removes only its exact owned INI path");
    check(!signature_profiles::load_file(path, result, error) && same_profile(result, before) && !error.empty(),
        "missing file load reports an error while preserving all previous metadata and rules");
}

void signature_profile_tests::test_hashes() {
    const std::array<std::uint8_t, 3> abc{'a', 'b', 'c'};
    check(signature_profiles::sha256(abc) ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "Windows SHA-256 matches the standard abc vector with exact lowercase hexadecimal output");
    check(signature_profiles::sha256({}) ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "empty immutable byte view hashes to the standard empty-input SHA-256 digest");
}

int signature_profile_tests::run() {
    test_valid_profiles();
    test_rejections();
    test_file_load();
    test_hashes();
    std::printf("strict offline signature profiles: %s (%d failures)\n", failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

} // namespace azs

int main() {
    return azs::signature_profile_tests::run();
}
