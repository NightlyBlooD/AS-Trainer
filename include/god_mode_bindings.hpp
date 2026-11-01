#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace azs {

class pattern_scanner;
class god_mode_bindings_tests;

class god_mode_bindings final {
public:
    struct verification_chunk {
        std::uint32_t rva{};
        std::vector<std::uint8_t> bytes;
        bool executable{};
        bool relocated_pointer{};
    };

    struct candidate {
        std::uint32_t damage_rva{};
        std::uint32_t damage_size{};
        std::uint32_t dispatcher_rva{};
        std::uint32_t dispatcher_size{};
        std::uint32_t lifesteal_rva{};
        std::size_t enemies_offset{};
        std::size_t enemy_stride{};
        std::size_t source_offset{};
        std::size_t phase_offset{};
        std::size_t spells_offset{};
        std::vector<verification_chunk> verification;
    };

    /** @brief Discover both native God Mode functions using embedded relocation-compatible certificates.
     * @param scanner Validated immutable file-layout PE; no loaded process memory is accessed.
     * @param result Complete output, replaced only after exactly one embedded family passes.
     * @param error Output diagnostic, cleared on success.
     * @return True when unique entries, unwind bounds, complete normalized instruction bodies,
     * linked dispatcher/getter calls, scalar constants and external callee anchors all match.
     * Executable hashes are not an admission list: address-only updates may pass. Register,
     * field-layout, internal branch and ABI changes remain unsupported until independently proved.
     * Callers must additionally bind the file identity and every returned byte range to the live image.
     */
    static bool resolve_file(const pattern_scanner& scanner, candidate& result,
        std::string& error) noexcept;

private:
    friend class god_mode_bindings_tests;

    enum class reference_kind {
        relative_call, rip_data, rip_address, rip_jump_table, rip_inactive, rip_string_table, rip_import
    };

    struct function_certificate;

    struct relocation_rule {
        std::uint32_t operand_offset{};
        std::uint32_t instruction_end{};
        reference_kind kind{};
        std::uint16_t width{};
        std::array<std::uint8_t, 16> expected_value{};
        std::string_view target_pattern;
        bool executable_target{};
        bool require_function_entry{};
        std::string_view target_import;
        std::string_view target_digest;
        const function_certificate* target_certificate{};
    };

    struct function_certificate {
        std::string_view entry_pattern;
        std::uint32_t size{};
        std::string_view normalized_sha256;
        std::span<const relocation_rule> relocations;
    };

    struct family_certificate {
        function_certificate damage;
        function_certificate dispatcher;
        std::size_t enemies_offset{};
        std::size_t enemy_stride{};
        std::size_t source_offset{};
        std::size_t phase_offset{};
        std::size_t spells_offset{};
        std::array<std::uint32_t, 2> dispatcher_call_operands{};
        std::uint32_t lifesteal_call_operand{0xffffffffu};
    };

    struct cached_anchor {
        std::string_view pattern;
        std::uint32_t rva{};
        bool unique{};
    };
    using resolution_cache = std::vector<cached_anchor>;

    /** @brief Resolve an explicit trusted in-memory certificate set without publishing partial output.
     * @param scanner Immutable PE; @param certificates Embedded families or owned test fixtures.
     * @param result Complete prior candidate preserved on refusal; @param error Failure diagnostic.
     * @return True for exactly one fully certified family, false for missing, altered or ambiguous code.
     * This private seam exists for synthetic PE regression tests; external profiles cannot authorize calls.
     */
    static bool resolve_certificates(const pattern_scanner& scanner,
        std::span<const family_certificate> certificates, candidate& result, std::string& error) noexcept;

    /** @brief Verify one entire function while retaining its actual file bytes for later live comparison.
     * @param scanner Validated PE; @param certificate Trusted entry/body/reference contract.
     * @param rva Output function entry; @param verification Output actual code and scalar ranges.
     * @param error Output diagnostic; @return True after every instruction/reference guard succeeds.
     */
    static bool resolve_function(const pattern_scanner& scanner, const function_certificate& certificate,
        std::uint32_t& rva, std::vector<verification_chunk>& verification, std::string& error,
        resolution_cache& cache, std::uint32_t known_rva = 0, std::size_t depth = 0);

    /** @brief Require a file-backed range in one readable code or immutable-data section.
     * @param scanner Immutable PE; @param rva First byte; @param size Required nonzero width.
     * @param executable True requires code, false requires read-only nonexecutable data.
     * @return True only for a bounded, nonwritable range with the requested section attributes.
     */
    static bool section_range(const pattern_scanner& scanner, std::uint32_t rva,
        std::size_t size, bool executable) noexcept;

    /** @brief Match trusted hexadecimal byte/wildcard text at one certified target without rescanning.
     * @param scanner Immutable PE; @param rva Direct target; @param pattern Expected bytepairs/??.
     * @param size Output matched byte width, unchanged on failure.
     * @return True for a bounded exact local match; no arbitrary first global match is selected.
     */
    static bool matching_pattern(const pattern_scanner& scanner, std::uint32_t rva,
        std::string_view pattern, std::size_t& size) noexcept;

    /** @brief Resolve a direct callee through bounded argument-preserving E9 import/allocator thunks.
     * @param scanner Immutable PE; @param target Certified direct E8 target; @param rule Trusted anchor/import.
     * @param verification Actual thunk/native code ranges; @return True for the specific native helper
     * or imported DLL/symbol identity, rather than an interchangeable generic push/jump prologue.
     */
    static bool matching_callee(const pattern_scanner& scanner, std::uint32_t target,
        const relocation_rule& rule, std::vector<verification_chunk>& verification,
        resolution_cache& cache, std::size_t depth);

    /** @brief Cache one whole-image uniqueness result for a trusted native entry anchor.
     * @param scanner Immutable PE used for this resolution; @param pattern Trusted pattern text.
     * @param result Output unique match; @param cache Per-resolution results, never shared across images.
     * @return True only for exactly one global match; duplicate callee lookalikes remain refused.
     */
    static bool unique_anchor(const pattern_scanner& scanner, std::string_view pattern,
        std::uint32_t& result, resolution_cache& cache);

    /** @brief Obtain the bounded original DLL/symbol identity of an offline AMD64 IAT slot.
     * @param scanner Validated immutable PE; @param iat_rva Slot addressed by a certified FF25 thunk.
     * @param identity Output module!name or module!#ordinal; @return True for one import descriptor/table entry.
     */
    static bool imported_symbol(const pattern_scanner& scanner, std::uint32_t iat_rva,
        std::string& identity);

    /** @brief Read one bounded NUL-terminated ASCII import identifier from immutable image bytes.
     * @param scanner Validated PE; @param rva String RVA; @param maximum Maximum accepted bytes.
     * @param value Output string; @return True for a nonempty complete printable ASCII identifier.
     */
    static bool read_ascii(const pattern_scanner& scanner, std::uint32_t rva,
        std::size_t maximum, std::string& value);

    /** @brief Copy one little-endian DWORD from a bounded offline RVA.
     * @param scanner Immutable PE; @param rva First byte; @param value Output scalar.
     * @return True only when all four file-backed bytes exist.
     */
    static bool read_u32(const pattern_scanner& scanner, std::uint32_t rva, std::uint32_t& value) noexcept;

    /** @brief Copy one little-endian AMD64 thunk word from a bounded offline RVA.
     * @param scanner Immutable PE; @param rva First byte; @param value Output scalar.
     * @return True only when all eight file-backed bytes exist.
     */
    static bool read_u64(const pattern_scanner& scanner, std::uint32_t rva, std::uint64_t& value) noexcept;

    /** @brief Resolve a signed rel32 operand only when it is inside its verified function body.
     * @param bytes Complete function bytes; @param function_rva Entry; @param rule Operand metadata.
     * @param target Output nonnegative image-relative target; @return False after overflow/bounds errors.
     */
    static bool reference_target(std::span<const std::uint8_t> bytes, std::uint32_t function_rva,
        const relocation_rule& rule, std::uint32_t& target) noexcept;

    /** @brief Resolve a required linked direct E8 call from certified relocation metadata.
     * @param scanner Immutable PE; @param function_rva Verified function entry.
     * @param certificate Verified body metadata; @param operand Exact required operand position.
     * @param target Output target; @return True only for an explicitly certified direct-call descriptor.
     */
    static bool linked_call(const pattern_scanner& scanner, std::uint32_t function_rva,
        const function_certificate& certificate, std::uint32_t operand, std::uint32_t& target) noexcept;

    /** @brief Append one unique actual byte range without exposing a partial result to callers.
     * @param scanner Immutable PE; @param rva First byte; @param size Nonzero byte count.
     * @param executable True for code and false for read-only scalar data.
     * @param verification Candidate-owned ranges; @return None; allocations may throw to the guarded caller.
     */
    static void add_chunk(const pattern_scanner& scanner, std::uint32_t rva, std::size_t size,
        bool executable, std::vector<verification_chunk>& verification);
};

} // namespace azs
