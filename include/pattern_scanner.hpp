#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace azs {

struct pattern_match {
    std::uint32_t rva{};
    std::uint64_t va{};
    std::size_t file_offset{};
};

enum class pattern_resolve_mode { match, function_start, rip_rel32, field_disp32 };

struct pattern_resolution {
    std::uint32_t rva{};
    std::uint64_t va{};
    std::int32_t field_displacement{};
};

struct pe_section {
    std::string name;
    std::uint32_t rva{};
    std::uint32_t virtual_size{};
    std::uint32_t raw_size{};
    std::size_t file_offset{};
    std::uint32_t characteristics{};
};

class pattern_scanner_tests;

class pattern_scanner final {
public:
    static constexpr std::size_t maximum_file_size = 512u * 1024u * 1024u;
    static constexpr std::size_t maximum_matches = 1024u * 1024u;

    /** @brief Load and validate one offline AMD64 PE32+ file without executing or mapping it.
     * @param path Existing executable/image file; file bytes are read once into owned storage.
     * @return True for a bounded valid image, false with error() on read/format/allocation failure.
     * Maximum file size is 512 MiB. A failed load clears all previously loaded image metadata.
     */
    bool load_file(const std::filesystem::path& path) noexcept;

    /** @brief Copy and validate an offline PE image supplied by a caller or controlled test fixture.
     * @param data Bounded file-layout bytes; the caller retains ownership of the original view.
     * @return True after a complete owned copy and validation, otherwise false with error().
     * A view returned by bytes() may be supplied back safely; failed loads clear the old image.
     */
    bool load_bytes(std::span<const std::uint8_t> data) noexcept;

    /** @brief Validate IDA pattern syntax without requiring a loaded image.
     * @param pattern Hex byte pairs and ?/?? whole-byte wildcards.
     * @param error Output diagnostic, cleared on success.
     * @return True for 1..4096 bytes containing at least one concrete byte.
     */
    static bool validate_pattern(std::string_view pattern, std::string& error) noexcept;

    /** @brief Search selected file-backed sections using an IDA-style byte pattern.
     * @param pattern Space-separated hexadecimal byte pairs and ?/?? whole-byte wildcards.
     * @param section Exact PE section name; empty or * scans all file-backed sections.
     * @return All matches sorted by RVA, or an empty vector with error() on invalid input/failure.
     * At most 4096 pattern bytes and one million matches are accepted; all-wildcard patterns
     * are rejected. No matches is a successful empty result. No loaded code is called.
     */
    std::vector<pattern_match> find(std::string_view pattern,
        std::string_view section = ".text") const noexcept;

    /** @brief Require exactly one match instead of silently selecting the first occurrence.
     * @param pattern IDA-style byte pattern; @param result Output match, unchanged on failure.
     * @param section Exact section name, or empty or * for all file-backed sections.
     * @return True only for one match; zero or two-or-more results return false with error().
     * Scanning stops at the second result, so broad ambiguous patterns stay bounded.
     */
    bool find_unique(std::string_view pattern, pattern_match& result,
        std::string_view section = ".text") const noexcept;

    /** @brief Resolve a verified match to itself, its unwind function, a RIP target or a field offset.
     * @param match Match returned for this image; its RVA/VA/file mapping is validated again.
     * @param mode Requested interpretation; @param result Output, unchanged on failure.
     * @param displacement_offset Offset from match start to a little-endian signed disp32.
     * @param instruction_size End offset of the RIP-relative instruction from match start;
     * may include preceding pattern bytes and must cover displacement_offset+4.
     * @return True after bounded resolution; false with error() for invalid/unmapped/ambiguous data.
     * function_start uses the exception directory and follows bounded UNW_FLAG_CHAININFO parents.
     * rip_rel32 permits a target in a virtual-only section, but rejects targets outside the image.
     * field_disp32 retains the match RVA/VA and returns the signed field displacement separately.
     */
    bool resolve(const pattern_match& match, pattern_resolve_mode mode,
        pattern_resolution& result, std::size_t displacement_offset = 0,
        std::size_t instruction_size = 0) const noexcept;

    /** @brief Resolve one RVA to the primary unwind-described function range.
     * @param rva File-backed instruction RVA covered by the validated exception directory.
     * @param begin Output primary function entry RVA; unchanged on failure.
     * @param end Output exclusive end of that primary range; unchanged on failure.
     * @return True after bounded unwind-chain validation; false for uncovered, cyclic or
     * malformed metadata. Chained fragments return their primary range rather than an
     * invented enclosing span. No executable bytes are invoked or process memory accessed.
     */
    bool function_range(std::uint32_t rva, std::uint32_t& begin, std::uint32_t& end) const noexcept;

    /** @brief Convert an RVA range to its unique contiguous file-layout offset.
     * @param rva Image-relative address; @param result Output file offset, unchanged on failure.
     * @param size Required nonzero file-backed byte count.
     * @return False for virtual-only data, gaps, truncation or an overflowing range.
     */
    bool rva_to_file_offset(std::uint32_t rva, std::size_t& result,
        std::size_t size = 1) const noexcept;

    /** @brief Convert one file-backed offset to a unique mapped RVA.
     * @param offset Offset in the original file; @param result Output RVA, unchanged on failure.
     * @return True for headers or section raw bytes; false for overlays, gaps and out-of-bounds bytes.
     */
    bool file_offset_to_rva(std::size_t offset, std::uint32_t& result) const noexcept;

    /** @brief Convert an RVA inside SizeOfImage to its preferred virtual address.
     * @param rva Image-relative byte address; @param result Output VA, unchanged on failure.
     * @return False without a valid image or when the RVA is outside its declared range.
     */
    bool rva_to_va(std::uint32_t rva, std::uint64_t& result) const noexcept;

    /** @brief Convert a preferred virtual address back to an image-relative address.
     * @param va Address based on this PE's ImageBase; @param result Output RVA.
     * @return True only for addresses inside the declared image; no runtime process is inspected.
     */
    bool va_to_rva(std::uint64_t va, std::uint32_t& result) const noexcept;

    /** @brief Expose a bounded immutable view at an RVA for signature verification or inspection.
     * @param rva File-backed image-relative address; @param size Nonzero requested byte count.
     * @return Exact owned byte view, or an empty view when mapping fails.
     */
    std::span<const std::uint8_t> view_rva(std::uint32_t rva, std::size_t size) const noexcept;

    /** @brief Return all owned offline file bytes without copying.
     * @return Immutable file-layout view valid until the next load or scanner destruction.
     */
    std::span<const std::uint8_t> bytes() const noexcept;

    /** @brief Return validated section metadata without copying.
     * @return Immutable section table view valid until the next load or scanner destruction.
     */
    std::span<const pe_section> sections() const noexcept;

    /** @brief Report whether a complete valid offline image is currently loaded.
     * @return True only after a successful load and all header/range checks.
     */
    bool valid() const noexcept;

    /** @brief Read the PE's preferred ImageBase, independent of any live module address.
     * @return Preferred base for a valid image, zero otherwise.
     */
    std::uint64_t image_base() const noexcept;

    /** @brief Read SizeOfImage, including virtual-only section tails.
     * @return Declared valid image size, zero when no image is loaded.
     */
    std::uint32_t image_size() const noexcept;

    /** @brief Inspect the latest load/search/resolve diagnostic without allocating.
     * @return Owned diagnostic view valid until the next mutating operation on this scanner.
     * Scanner instances are intended for a single caller; metadata and views never access a process.
     */
    std::string_view error() const noexcept;

private:
    friend class pattern_scanner_tests;

    struct compiled_pattern {
        std::vector<std::uint8_t> values;
        std::vector<std::uint8_t> fixed;
        std::size_t anchor_offset{};
        std::size_t anchor_length{};
    };

    struct runtime_function {
        std::uint32_t begin{};
        std::uint32_t end{};
        std::uint32_t unwind{};
    };

    /** @brief Validate a subrange using subtraction rather than overflowing end arithmetic.
     * @param total Available size; @param offset Subrange start; @param size Required count.
     * @return True when the entire subrange is contained, including a zero-size endpoint.
     */
    static bool contains(std::size_t total, std::size_t offset, std::size_t size) noexcept;

    /** @brief Decode one unsigned little-endian field from bounded file-layout bytes.
     * @tparam value_type Unsigned integer up to 64 bits; @param data Immutable byte view.
     * @param offset Byte offset; @param result Output scalar, unchanged on failed bounds.
     * @return True after the exact scalar has been decoded without unaligned pointer casts.
     */
    template <typename value_type>
    static bool read_unsigned(std::span<const std::uint8_t> data,
        std::size_t offset, value_type& result) noexcept {
        static_assert(std::is_unsigned_v<value_type> && sizeof(value_type) <= 8);
        if (!contains(data.size(), offset, sizeof(value_type))) return false;
        std::uint64_t value{};
        for (std::size_t index = 0; index < sizeof(value_type); ++index)
            value |= static_cast<std::uint64_t>(data[offset + index]) << (index * 8);
        result = static_cast<value_type>(value);
        return true;
    }

    /** @brief Discard image data and metadata without discarding a just-recorded error.
     * @return None; all formerly returned byte and section views become invalid.
     */
    void clear_image() noexcept;

    /** @brief Save a static failure reason without allocating storage.
     * @param message Diagnostic literal; @return False for convenient guarded-return use.
     */
    bool fail(const char* message) const noexcept;

    /** @brief Validate DOS/PE64 headers, unique section mappings and exception-directory entries.
     * @return True only after complete validation; the caller clears partial state on false.
     */
    bool parse_image();

    /** @brief Decode an IDA pattern and choose its longest fixed-byte anchor.
     * @param text Pattern text; @param result Output compiled bytes and wildcard mask.
     * @return True for a bounded pattern containing at least one concrete byte.
     */
    bool compile_pattern(std::string_view text, compiled_pattern& result) const;

    /** @brief Scan raw selected sections using memchr/memcmp anchors before masked verification.
     * @param pattern Compiled nonempty pattern; @param section Section filter.
     * @param result Collected matches; @param limit Stop once this many matches are collected.
     * @return True after valid traversal; false if the requested section does not exist.
     */
    bool scan_sections(const compiled_pattern& pattern, std::string_view section,
        std::vector<pattern_match>& result, std::size_t limit) const;

    /** @brief Resolve one containing exception-table entry and its chained unwind parent.
     * @param rva File-backed matched instruction address; @param result Output function RVA.
     * @return True for a valid bounded noncyclic unwind chain, otherwise false with error().
     */
    bool function_start(std::uint32_t rva, std::uint32_t& result) const noexcept;

    std::vector<std::uint8_t> file_data_;
    std::vector<pe_section> sections_;
    std::vector<runtime_function> functions_;
    std::uint64_t image_base_{};
    std::uint32_t image_size_{};
    std::uint32_t header_size_{};
    bool valid_{};
    mutable const char* error_{""};
};

} // namespace azs
