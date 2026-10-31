#include "pattern_scanner_tests.hpp"
#include "pattern_scanner.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace azs {

void pattern_scanner_tests::check(bool condition, const char* description) {
    if (!condition) {
        ++failures_;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

void pattern_scanner_tests::put_bytes(std::vector<std::uint8_t>& image, std::size_t offset,
    std::span<const std::uint8_t> values) {
    if (offset > image.size() || values.size() > image.size() - offset) {
        check(false, "fixture sequence write stays inside owned bytes");
        return;
    }
    std::copy(values.begin(), values.end(), image.begin() + static_cast<std::ptrdiff_t>(offset));
}

std::vector<std::uint8_t> pattern_scanner_tests::make_image() {
    std::vector<std::uint8_t> image(0xb20);
    put_unsigned<std::uint16_t>(image, 0, 0x5a4d);
    put_unsigned<std::uint32_t>(image, 0x3c, 0x80);
    put_unsigned<std::uint32_t>(image, 0x80, 0x00004550);
    put_unsigned<std::uint16_t>(image, 0x84, 0x8664);
    put_unsigned<std::uint16_t>(image, 0x86, 4);
    put_unsigned<std::uint16_t>(image, 0x94, 0xf0);
    put_unsigned<std::uint16_t>(image, 0x98, 0x20b);
    put_unsigned<std::uint64_t>(image, 0xb0, 0x140000000);
    put_unsigned<std::uint32_t>(image, 0xb8, 0x1000);
    put_unsigned<std::uint32_t>(image, 0xbc, 0x200);
    put_unsigned<std::uint32_t>(image, 0xd0, 0x5000);
    put_unsigned<std::uint32_t>(image, 0xd4, 0x400);
    put_unsigned<std::uint32_t>(image, 0x104, 16);
    put_unsigned<std::uint32_t>(image, 0x120, 0x2000);
    put_unsigned<std::uint32_t>(image, 0x124, 36);
    const std::array names{".text", ".pdata", ".xdata", ".data"};
    const std::array<std::uint32_t, 4> rvas{0x1000, 0x2000, 0x3000, 0x4000};
    const std::array<std::uint32_t, 4> raw_offsets{0x400, 0x600, 0x800, 0xa00};
    const std::array<std::uint32_t, 4> raw_sizes{0x200, 0x200, 0x200, 0x100};
    for (std::size_t index = 0; index < names.size(); ++index) {
        const auto section = 0x188 + index * 40;
        std::memcpy(image.data() + section, names[index], std::strlen(names[index]));
        put_unsigned<std::uint32_t>(image, section + 8, index == 3 ? 0x300 : raw_sizes[index]);
        put_unsigned(image, section + 12, rvas[index]);
        put_unsigned(image, section + 16, raw_sizes[index]);
        put_unsigned(image, section + 20, raw_offsets[index]);
        put_unsigned<std::uint32_t>(image, section + 36, index == 0 ? 0x60000020 : 0x40000040);
    }
    std::fill(image.begin() + 0x400, image.begin() + 0x600, std::uint8_t{0x90});
    const std::array<std::uint32_t, 3> begins{0x1000, 0x1080, 0x1100};
    const std::array<std::uint32_t, 3> ends{0x1080, 0x1100, 0x1180};
    const std::array<std::uint32_t, 3> unwind{0x3000, 0x3020, 0x3040};
    for (std::size_t index = 0; index < begins.size(); ++index) {
        put_unsigned(image, 0x600 + index * 12, begins[index]);
        put_unsigned(image, 0x604 + index * 12, ends[index]);
        put_unsigned(image, 0x608 + index * 12, unwind[index]);
        image[0x800 + index * 0x20] = 1;
    }
    return image;
}

void pattern_scanner_tests::test_patterns() {
    auto image = make_image();
    const std::array<std::uint8_t, 6> signature{0x48, 0x8b, 0x12, 0x34, 0x55, 0xaa};
    put_bytes(image, 0x420, signature);
    put_bytes(image, 0x480, signature);
    put_bytes(image, 0xa20, signature);
    const std::array<std::uint8_t, 3> ending{0x11, 0x22, 0x33};
    put_bytes(image, 0x5fd, ending);
    pattern_scanner scanner;
    check(scanner.load_bytes(image), "controlled PE64 pattern fixture loads");
    const auto found = scanner.find("48 8b ?? ? 55 AA");
    check(found.size() == 2 && found[0].rva == 0x1020 && found[0].va == 0x140001020 &&
          found[0].file_offset == 0x420 && found[1].rva == 0x1080 && scanner.error().empty(),
        "mixed-case hex and whole-byte wildcards find all text matches with exact RVA/VA/file offsets");
    check(scanner.find("48 8B ?? ?? 55 AA", "*").size() == 3 &&
          scanner.find("48 8B ?? ?? 55 AA", ".data").size() == 1,
        "section filters distinguish named data and aggregate every matching raw section");
    pattern_match output{123, 456, 789};
    check(!scanner.find_unique("48 8B ?? ?? 55 AA", output) && output.rva == 123 &&
          scanner.error().find("ambiguous") != std::string_view::npos,
        "ambiguous pattern never returns a first match or mutates caller output");
    check(scanner.find_unique("? ? 33", output) && output.rva == 0x11fd,
        "leading wildcards and a final fixed-byte anchor include the last legal section position");
    check(scanner.find_unique("\t11 22\r\n33 ", output) && output.file_offset == 0x5fd,
        "ASCII whitespace separators and an exact end-of-section pattern are accepted");
    check(scanner.find("FE ED FA CE").empty() && scanner.error().empty() &&
          !scanner.find_unique("FE ED FA CE", output) && !scanner.error().empty(),
        "exhaustive not-found is empty while unique not-found explicitly refuses resolution");
    for (const auto* invalid : {"", " ", "?? ? ??", "4?", "?F", "0x48", "GG", "488B", "48,8B", "1"}) {
        check(scanner.find(invalid).empty() && !scanner.error().empty(),
            "empty, all-wildcard, partial-nibble and malformed patterns are rejected");
    }
    check(scanner.find("11", ".missing").empty() && !scanner.error().empty(),
        "unknown section names fail explicitly instead of scanning an unintended section");
    std::string oversized;
    for (unsigned index = 0; index < 4097; ++index) oversized += "90 ";
    check(scanner.find(oversized).empty() && !scanner.error().empty(), "pattern byte count is bounded at 4096");
    pattern_scanner::compiled_pattern compiled;
    check(scanner.compile_pattern("?? 11 22 ?? 33 44 55", compiled) &&
          compiled.anchor_offset == 4 && compiled.anchor_length == 3,
        "compiled search chooses the longest fixed-byte anchor before masked verification");
    std::string syntax_error = "old error";
    check(pattern_scanner::validate_pattern("48 8B ??", syntax_error) && syntax_error.empty() &&
          !pattern_scanner::validate_pattern("?? ??", syntax_error) && !syntax_error.empty(),
        "public pattern validation works independently of a loaded PE and rejects all wildcards");
}

void pattern_scanner_tests::test_mapping() {
    pattern_scanner scanner;
    check(scanner.load_bytes(make_image()) && scanner.valid() && scanner.image_base() == 0x140000000 &&
          scanner.image_size() == 0x5000 && scanner.sections().size() == 4,
        "validated public metadata preserves image base, virtual size and section table");
    std::size_t offset = 99;
    std::uint32_t rva = 99;
    std::uint64_t va = 99;
    check(scanner.rva_to_file_offset(0x100, offset) && offset == 0x100 &&
          scanner.rva_to_file_offset(0x1010, offset, 8) && offset == 0x410 &&
          scanner.file_offset_to_rva(0x410, rva) && rva == 0x1010 &&
          scanner.rva_to_va(rva, va) && va == 0x140001010 && scanner.va_to_rva(va, rva) && rva == 0x1010,
        "header and raw-section mappings round-trip all three address forms");
    offset = 123;
    rva = 123;
    check(!scanner.rva_to_file_offset(0x4200, offset) && offset == 123 &&
          scanner.rva_to_va(0x4200, va) && va == 0x140004200 &&
          !scanner.file_offset_to_rva(0xb10, rva) && rva == 123,
        "virtual-only section tail has an address but no file bytes, and file overlays have no RVA");
    check(!scanner.rva_to_file_offset(0x11ff, offset, 2) &&
          !scanner.rva_to_file_offset(0x1000, offset, std::numeric_limits<std::size_t>::max()) &&
          !scanner.rva_to_file_offset(0xffffffff, offset, 4) &&
          !scanner.va_to_rva(std::numeric_limits<std::uint64_t>::max(), rva) &&
          !scanner.va_to_rva(0x13fffffff, rva) && !scanner.rva_to_va(0x5000, va),
        "cross-section ranges, arithmetic limits and out-of-image virtual addresses refuse mapping");
    check(scanner.view_rva(0x1000, 4).size() == 4 && scanner.view_rva(0x4200, 4).empty() &&
          scanner.view_rva(0x1000, 0).empty(), "immutable RVA views expose exactly backed nonempty byte ranges");
    const auto old_size = scanner.bytes().size();
    check(scanner.load_bytes(scanner.bytes()) && scanner.bytes().size() == old_size,
        "reloading the scanner's own byte view copies before invalidating the previous allocation");
}

void pattern_scanner_tests::test_resolution() {
    auto image = make_image();
    const std::array<std::uint8_t, 7> positive{0x48, 0x8d, 0x05, 0, 0, 0, 0};
    const std::array<std::uint8_t, 7> negative{0x4c, 0x8d, 0x15, 0, 0, 0, 0};
    const std::array<std::uint8_t, 8> field{0xf3, 0x0f, 0x10, 0x81, 0xf4, 0x04, 0, 0};
    put_bytes(image, 0x420, positive);
    put_unsigned<std::uint32_t>(image, 0x423, 0x4200 - 0x1027);
    put_bytes(image, 0x440, negative);
    put_unsigned<std::uint32_t>(image, 0x443, static_cast<std::uint32_t>(-0x47));
    put_bytes(image, 0x460, field);
    const std::array<std::uint8_t, 3> truncated{0x11, 0x22, 0x33};
    put_bytes(image, 0x5fd, truncated);
    pattern_scanner scanner;
    check(scanner.load_bytes(image), "resolution fixture loads without executing its instruction bytes");
    pattern_match match;
    pattern_resolution result{7, 8, 9};
    check(scanner.find_unique("48 8D 05 ?? ?? ?? ??", match) &&
          scanner.resolve(match, pattern_resolve_mode::match, result) &&
          result.rva == 0x1020 && result.va == 0x140001020 && result.field_displacement == 0,
        "match mode preserves the exact found image address");
    check(scanner.resolve(match, pattern_resolve_mode::rip_rel32, result, 3, 7) &&
          result.rva == 0x4200 && result.va == 0x140004200,
        "positive RIP-relative displacement can resolve a virtual-only data target");
    check(scanner.find_unique("4C 8D 15 ?? ?? ?? ??", match) &&
          scanner.resolve(match, pattern_resolve_mode::rip_rel32, result, 3, 7) && result.rva == 0x1000,
        "negative signed RIP displacement uses the instruction end rather than its start");
    check(scanner.find_unique("F3 0F 10 81 ?? ?? ?? ??", match) &&
          scanner.resolve(match, pattern_resolve_mode::field_disp32, result, 4) &&
          result.field_displacement == 0x4f4 && result.rva == match.rva,
        "field disp32 returns a scalar offset separately from the instruction address");
    const auto before = result;
    check(!scanner.resolve(match, pattern_resolve_mode::rip_rel32, result, 4, 7) &&
          result.rva == before.rva && result.field_displacement == before.field_displacement &&
          !scanner.resolve(match, pattern_resolve_mode::field_disp32, result,
              std::numeric_limits<std::size_t>::max()),
        "invalid instruction-end and overflowing displacement requests preserve prior output");
    auto foreign = match;
    ++foreign.file_offset;
    check(!scanner.resolve(foreign, pattern_resolve_mode::match, result),
        "a stale or fabricated match with inconsistent file mapping is rejected");
    check(scanner.find_unique("11 22 33", match) &&
          !scanner.resolve(match, pattern_resolve_mode::field_disp32, result),
        "disp32 extraction cannot cross a raw section boundary even when file bytes follow it");
    put_unsigned<std::uint32_t>(image, 0x464, static_cast<std::uint32_t>(-0x20));
    check(scanner.load_bytes(image) && scanner.find_unique("F3 0F 10 81 ?? ?? ?? ??", match) &&
          scanner.resolve(match, pattern_resolve_mode::field_disp32, result, 4) &&
          result.field_displacement == -0x20, "signed negative field offsets are preserved exactly");
    put_unsigned<std::uint32_t>(image, 0x423, 0x80000000);
    check(scanner.load_bytes(image) && scanner.find_unique("48 8D 05 ?? ?? ?? ??", match) &&
          !scanner.resolve(match, pattern_resolve_mode::rip_rel32, result, 3, 7),
        "RIP target underflow is refused rather than wrapping into a plausible address");
    put_unsigned<std::uint32_t>(image, 0x423, 0x7fffffff);
    check(scanner.load_bytes(image) && scanner.find_unique("48 8D 05 ?? ?? ?? ??", match) &&
          !scanner.resolve(match, pattern_resolve_mode::rip_rel32, result, 3, 7),
        "RIP target beyond SizeOfImage is refused");
}

void pattern_scanner_tests::test_unwind() {
    auto image = make_image();
    image[0x490] = 0xcc;
    image[0x590] = 0xcd;
    pattern_scanner scanner;
    pattern_match match;
    pattern_resolution result;
    check(scanner.load_bytes(image) && scanner.find_unique("CC", match) &&
          scanner.resolve(match, pattern_resolve_mode::function_start, result) && result.rva == 0x1080,
        "interior match resolves through the containing pdata runtime function range");
    check(scanner.find_unique("CD", match) && !scanner.resolve(match, pattern_resolve_mode::function_start, result),
        "function end and following uncovered bytes never inherit the preceding function start");
    image[0x820] = 0x21;
    put_unsigned<std::uint32_t>(image, 0x824, 0x1000);
    put_unsigned<std::uint32_t>(image, 0x828, 0x1080);
    put_unsigned<std::uint32_t>(image, 0x82c, 0x3000);
    check(scanner.load_bytes(image) && scanner.find_unique("CC", match) &&
          scanner.resolve(match, pattern_resolve_mode::function_start, result) && result.rva == 0x1000,
        "UNW_FLAG_CHAININFO fragment resolves to its primary function rather than the fragment start");
    put_unsigned<std::uint32_t>(image, 0x82c, 0x3020);
    check(scanner.load_bytes(image) && scanner.find_unique("CC", match) &&
          !scanner.resolve(match, pattern_resolve_mode::function_start, result) &&
          scanner.error().find("cycle") != std::string_view::npos,
        "cyclic chained unwind metadata fails within a fixed bounded traversal");
    image = make_image();
    image[0x490] = 0xcc;
    put_unsigned<std::uint32_t>(image, 0x614, 0x31fc);
    image[0x9fc] = 1;
    image[0x9fe] = 255;
    check(scanner.load_bytes(image) && scanner.find_unique("CC", match) &&
          !scanner.resolve(match, pattern_resolve_mode::function_start, result),
        "truncated unwind code arrays cannot read through the end of the xdata section");
    image = make_image();
    image[0x490] = 0xcc;
    image[0x820] = 0xe1;
    check(scanner.load_bytes(image) && scanner.find_unique("CC", match) &&
          !scanner.resolve(match, pattern_resolve_mode::function_start, result),
        "unsupported unwind flags fail closed without accepting a guessed function entry");
}

void pattern_scanner_tests::test_bad_images() {
    for (unsigned scenario = 0; scenario < 14; ++scenario) {
        auto image = make_image();
        if (scenario == 0) image.resize(0x20);
        if (scenario == 1) image[0] = 0;
        if (scenario == 2) put_unsigned<std::uint32_t>(image, 0x3c, 0xfffffff0);
        if (scenario == 3) put_unsigned<std::uint16_t>(image, 0x84, 0x14c);
        if (scenario == 4) put_unsigned<std::uint16_t>(image, 0x98, 0x10b);
        if (scenario == 5) put_unsigned<std::uint16_t>(image, 0x86, 97);
        if (scenario == 6) put_unsigned<std::uint32_t>(image, 0x188 + 20, 0xffffff00);
        if (scenario == 7) put_unsigned<std::uint32_t>(image, 0x188 + 12, 0xfffffff0);
        if (scenario == 8) put_unsigned<std::uint32_t>(image, 0x188 + 40 + 20, 0x400);
        if (scenario == 9) put_unsigned<std::uint32_t>(image, 0x188 + 40 + 12, 0x1100);
        if (scenario == 10) put_unsigned<std::uint32_t>(image, 0x124, 35);
        if (scenario == 11) put_unsigned<std::uint32_t>(image, 0x60c, 0x1070);
        if (scenario == 12) put_unsigned<std::uint64_t>(image, 0xb0, std::numeric_limits<std::uint64_t>::max() - 1);
        if (scenario == 13) image.resize(0xa80);
        pattern_scanner scanner;
        check(scanner.load_bytes(make_image()), "bad-PE case begins with a valid image to test stale-state refusal");
        check(!scanner.load_bytes(image) && !scanner.valid() && scanner.bytes().empty() &&
              scanner.sections().empty() && !scanner.error().empty(),
            "bad/truncated/overflowing/overlapping PE clears the old image and reports failure");
        pattern_match result{1, 2, 3};
        check(!scanner.find_unique("90", result) && result.rva == 1,
            "failed image load cannot leave a stale successful pattern address available");
    }
}

void pattern_scanner_tests::test_file_load() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::current_path() / ("pattern_scanner_test_" + std::to_string(stamp) + ".bin");
    const auto image = make_image();
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
        check(static_cast<bool>(output), "uniquely named owned offline fixture file is written completely");
    }
    pattern_scanner scanner;
    check(scanner.load_file(path) && scanner.bytes().size() == image.size() && scanner.valid(),
        "load_file reads an offline PE in file layout without mapping or executing it");
    std::error_code remove_error;
    check(std::filesystem::remove(path, remove_error) && !remove_error,
        "only the exact test-owned fixture file is removed");
    check(!scanner.load_file(path) && !scanner.valid() && !scanner.error().empty(),
        "missing file load reports failure and clears the previous image");
}

int pattern_scanner_tests::run() {
    test_patterns();
    test_mapping();
    test_resolution();
    test_unwind();
    test_bad_images();
    test_file_load();
    std::printf("bounded offline PE pattern scanner: %s (%d failures)\n", failures_ ? "FAILED" : "PASS", failures_);
    return failures_ ? 1 : 0;
}

} // namespace azs

int main() {
    return azs::pattern_scanner_tests::run();
}
