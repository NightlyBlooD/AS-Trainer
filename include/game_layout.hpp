#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <array>
#include <span>

namespace azs {

enum class layout_id : std::uint32_t { legacy, current, adaptive };

struct layout_offset {
    std::size_t legacy_offset;
    std::size_t resolved_offset;
};

class game_layout final {
public:
    static constexpr std::size_t invalid_offset = static_cast<std::size_t>(-1);

    /** @brief Select one immutable, statically verified executable-layout profile.
     * @param id Legacy or current profile identified by the caller's exact executable hash.
     * @return True on selection; false for an invalid enum without changing the active profile.
     * Call before installing hooks; tests may switch only while all consumers are quiescent.
     */
    static bool select(layout_id id) noexcept;

    /** @brief Validate and publish a complete independently discovered layout before hooks start.
     * @param run_offsets Every required canonical run key and its resolved byte offset, in key order.
     * @param member_offsets Every required participant key and its independently resolved offset.
     * @param stride Native participant-record stride extracted from verified traversal instructions.
     * @return True after atomic profile selection; false preserves the selected profile and generation.
     * Maps must be complete, bounded, collision-free, aligned for typed fields and consistent with
     * native Vec metadata. This method validates layout coherence; callers must also prove each
     * mapping's semantic origin and the native ABI. Call only while layout consumers are quiescent.
     */
    static bool select_resolved(std::span<const layout_offset> run_offsets,
        std::span<const layout_offset> member_offsets, std::size_t stride) noexcept;

    /** @brief Check discovered layout coherence without publishing state or touching game memory.
     * @param run_offsets Complete canonical-to-resolved run map; @param member_offsets Participant map.
     * @param stride Proven native participant size.
     * @return True only when select_resolved would accept these maps; false has no side effects.
     */
    static bool validate_resolved(std::span<const layout_offset> run_offsets,
        std::span<const layout_offset> member_offsets, std::size_t stride) noexcept;

    /** @brief Expose the complete canonical run-key set required by all trainer features.
     * @return Sorted immutable keys, including the intrinsic object-tag offset zero.
     */
    static std::span<const std::size_t> required_run_offsets() noexcept;

    /** @brief Expose the complete canonical participant-key set required by party traversal.
     * @return Sorted immutable keys, including the intrinsic object-tag offset zero.
     */
    static std::span<const std::size_t> required_member_offsets() noexcept;

    /** @brief Read the publication generation used to refresh thread-local descriptor tables.
     * @return Monotonically advancing value after each successful quiescent profile selection.
     */
    static std::uint64_t generation() noexcept;

    /** @brief Read the identity of the currently selected immutable profile.
     * @return Legacy before explicit selection, otherwise the last accepted profile ID.
     */
    static layout_id current_id() noexcept;

    /** @brief Translate one verified legacy run-field offset into the active profile.
     * @param legacy_offset Byte offset originally proved for the legacy executable.
     * @return Identity in legacy; an explicit mapped offset in current; invalid_offset
     * for an unknown current offset. There is no guessed range or delta fallback.
     */
    static std::size_t run_offset(std::size_t legacy_offset) noexcept;

    /** @brief Translate an explicitly verified native participant-record field.
     * @param legacy_offset Legacy tag, GUID, chest-cost or individual-phase byte offset.
     * @return Identity in legacy; mapped offset in current; invalid_offset when unknown.
     */
    static std::size_t member_offset(std::size_t legacy_offset) noexcept;

    /** @brief Return the native Vec element stride for a participant record.
     * @return 0xa38 for legacy or 0xad0 for current, as proved by native traversal.
     */
    static std::size_t participant_stride() noexcept;

    /** @brief Return the minimum host-context span including the adjacent app-role alias.
     * @return Required writable span, 0xdac in legacy or 0xecc in current.
     * This bounds trainer host checks and is not a claim about Rust type sizeof.
     */
    static std::size_t run_size() noexcept;

    /** @brief Return the minimum run span through the individual-phase byte.
     * @return 0xda4 for legacy or 0xec4 for current scalar validation.
     */
    static std::size_t scalar_size() noexcept;

private:
    /** @brief Return the minimum byte span actually accessed through one canonical run key.
     * @param legacy_offset A required run key from the verified consumer contract.
     * @return One for byte flags, eight for pointers/counts/GUID, full known container spans,
     * or four for scalar words. Used only during offline layout validation.
     */
    static std::size_t run_field_width(std::size_t legacy_offset) noexcept;

    struct profile {
        layout_id id;
        std::size_t stride;
        std::size_t host_span;
        std::size_t scalar_span;
        const layout_offset* run_offsets{};
        std::size_t run_count{};
        const layout_offset* member_offsets{};
        std::size_t member_count{};
    };

    /** @brief Binary-search a sorted, immutable table of individually proved field pairs.
     * @param legacy_offset Exact old field offset; @param pairs Old/new offset pairs.
     * @param count Number of pairs in the static table.
     * @return Matching new byte offset, otherwise invalid_offset.
     */
    static std::size_t lookup(std::size_t legacy_offset, const std::size_t (*pairs)[2],
                              std::size_t count) noexcept;

    static const profile legacy_profile_;
    static const profile current_profile_;
    static std::atomic<const profile*> active_;
    static profile adaptive_profile_;
    static std::array<layout_offset, 128> adaptive_run_offsets_;
    static std::array<layout_offset, 16> adaptive_member_offsets_;
    static std::atomic<std::uint64_t> generation_;
};

} // namespace azs
