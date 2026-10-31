#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace azs {

class pattern_scanner_tests final {
public:
    /** @brief Exercise the offline scanner with controlled PE64 bytes and owned temporary files.
     * @return Zero when all assertions pass, one on any regression.
     * No executable, game process, DLL, hook, desktop or runtime profile is used.
     */
    static int run();

private:
    /** @brief Record one regression assertion without suppressing independent later checks.
     * @param condition Expected true result; @param description Failure context.
     * @return None; increments the suite failure count on false.
     */
    static void check(bool condition, const char* description);

    /** @brief Build a minimal bounded AMD64 PE32+ image in file layout.
     * @return Owned bytes with headers, text, pdata, xdata and a virtual-only section tail.
     * ImageBase is 0x140000000; text is RVA1000/file400 and contains three unwind-covered ranges.
     */
    static std::vector<std::uint8_t> make_image();

    /** @brief Copy a byte sequence into one validated region of the test-owned image.
     * @param image Owned fixture bytes; @param offset File-layout destination.
     * @param values Sequence to copy; @return None, with an assertion on invalid fixture bounds.
     */
    static void put_bytes(std::vector<std::uint8_t>& image, std::size_t offset,
        std::span<const std::uint8_t> values);

    /** @brief Encode one unsigned little-endian fixture header field without aliasing casts.
     * @tparam value_type Unsigned integer field type; @param image Owned fixture.
     * @param offset File-layout destination; @param value Field value.
     * @return None; emits exactly sizeof(value_type) bytes into a validated fixture range.
     */
    template <typename value_type>
    static void put_unsigned(std::vector<std::uint8_t>& image,
        std::size_t offset, value_type value) {
        if (offset > image.size() || sizeof(value_type) > image.size() - offset) {
            check(false, "fixture scalar write stays inside owned bytes");
            return;
        }
        const auto bits = static_cast<std::uint64_t>(value);
        for (std::size_t index = 0; index < sizeof(value_type); ++index)
            image[offset + index] = static_cast<std::uint8_t>(bits >> (index * 8));
    }

    /** @brief Validate wildcard parsing, longest anchors, exhaustive matches and uniqueness refusal.
     * @return None; includes ambiguous matches in different named sections and end-boundary matches.
     */
    static void test_patterns();

    /** @brief Verify header, section, overlay and virtual-only RVA/VA/file mappings.
     * @return None; covers unchanged outputs and overflowing range requests.
     */
    static void test_mapping();

    /** @brief Check direct, RIP-relative and signed field displacement interpretation.
     * @return None; includes negative targets, instruction-end bounds and invalid foreign matches.
     */
    static void test_resolution();

    /** @brief Verify exception-table boundaries, chained primary starts and bounded corruption refusal.
     * @return None; includes cyclic chains, truncated codes and unsupported unwind flags.
     */
    static void test_unwind();

    /** @brief Reject malformed PE headers and ranges without retaining a previous valid image.
     * @return None; covers truncation, arithmetic limits, overlapping sections and invalid pdata.
     */
    static void test_bad_images();

    /** @brief Verify a complete owned file load and a failed load without executing the file.
     * @return None; creates and removes only a uniquely named test-owned .bin file.
     */
    static void test_file_load();

    inline static int failures_{};
};

} // namespace azs
