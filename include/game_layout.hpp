#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace azs {

enum class layout_id : std::uint32_t { legacy, current };

class game_layout final {
public:
    static constexpr std::size_t invalid_offset = static_cast<std::size_t>(-1);

    /** @brief Select one immutable, statically verified executable-layout profile.
     * @param id Legacy or current profile identified by the caller's exact executable hash.
     * @return True on selection; false for an invalid enum without changing the active profile.
     * Call before installing hooks; tests may switch only while all consumers are quiescent.
     */
    static bool select(layout_id id) noexcept;

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
    struct profile {
        layout_id id;
        std::size_t stride;
        std::size_t host_span;
        std::size_t scalar_span;
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
};

} // namespace azs
