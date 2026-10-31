#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace azs {

struct automation_settings;
class auto_choice_tests;

struct choice_candidate {
    std::string_view key;
    std::uint32_t rarity{};
    std::size_t native_index{};
};

enum class auto_choice_status {
    disabled, not_choosing, waiting, already_attempted, chosen, invalid_offer, apply_failed
};

struct auto_choice_result {
    auto_choice_status status = auto_choice_status::not_choosing;
    std::size_t selected_index = static_cast<std::size_t>(-1);
    const char* reason = "not_choosing";
    std::uint32_t captured_seh_code{};
};

class auto_choice final {
public:
    /** @brief Resolve the native selection routine only after its live code is verified.
     * @param module_base Main-module base already accepted by the runtime's SHA-256 gate.
     * @return True for the supported live apply-function signature; false without actions.
     * Call outside DllMain before the owning-thread frame hook is enabled.
     */
    static bool initialize(std::uintptr_t module_base) noexcept;

    /** @brief Select an offer entry by ordered stable keys, then rarity, then card order.
     * @param candidates Immutable candidates with copied stable keys and native indices.
     * @param priorities Ordered preferred keys; ASCII case and surrounding whitespace ignored.
     * @return Native index of the best valid candidate, or size_t(-1) if none are valid.
     * Unmatched cards share the lowest priority, so their highest rarity wins ties.
     */
    static std::size_t select(std::span<const choice_candidate> candidates,
        std::span<const std::string> priorities) noexcept;

    /** @brief Observe and, after a bounded delay, apply at most one current offer entry.
     * @param run_address Current run already validated by the owning game-frame hook.
     * @param settings Immutable automation snapshot; both toggles default to off.
     * @param now_ms Monotonic milliseconds from GetTickCount64 or an equivalent test clock.
     * @return Status, selected index and a bounded static diagnostic/SEH code.
     * Must run only on the game owner thread, including phase 2; never call from the UI.
     * A failed or pending native action is never automatically retried for the same token.
     */
    static auto_choice_result process_run(void* run_address,
        const automation_settings& settings, std::uint64_t now_ms) noexcept;

    /** @brief Forget observation and duplicate-prevention state during runtime shutdown.
     * @return None; leaves native game state and the resolved apply routine unchanged.
     * Call only on the same owner thread as process_run, or before hooks are active.
     */
    static void reset() noexcept;

private:
    friend class auto_choice_tests;
    using apply_function = void(__fastcall*)(void*, std::size_t);
    static constexpr std::size_t maximum_choices = 16;
    static constexpr std::size_t maximum_key_length = 128;

    struct copied_choice {
        std::array<char, maximum_key_length + 1> key{};
        std::size_t key_length{};
        std::uint32_t rarity{};
        std::size_t native_index{};
    };

    struct offer_snapshot {
        std::array<copied_choice, maximum_choices> choices{};
        std::size_t count{};
        std::uintptr_t data_address{};
        std::uintptr_t run_address{};
        std::uint32_t generation{};
        std::uint8_t kind{};
        std::uint64_t fingerprint{};
    };

    /** @brief Verify a bounded, committed, nonexecutable data range.
     * @param address First candidate byte; @param size Required byte count.
     * @param writable True when native application will require writable run storage.
     * @return True if all covered pages have acceptable access and no pointer overflow.
     */
    static bool accessible_data(const void* address, std::size_t size, bool writable) noexcept;

    /** @brief Contain unexpected memory-access exceptions when copying verified bytes.
     * @param source Readable source; @param destination Writable local output.
     * @param size Exact bounded byte count.
     * @return True on a complete copy; false on an access exception.
     */
    static bool copy_bytes(const void* source, void* destination, std::size_t size) noexcept;

    /** @brief Copy and validate an ASCII stable key using its explicit Rust string length.
     * @param address First key byte; @param length Length excluding a terminating zero.
     * @param destination Local copied card receiving its key bytes and length.
     * @return True for a nonempty printable bounded key, false for invalid memory/text.
     */
    static bool copy_key(std::uintptr_t address, std::size_t length,
        copied_choice& destination) noexcept;

    /** @brief Decode a choice variant's stable spell/jewelry key without using its title.
     * @param run_bytes Validated run base; @param card Validated 0xa0-byte choice.
     * @param destination Copied key, rarity and index output initialized by the caller.
     * @return True for supported new/upgrade spell, jewelry, amulet or gold variants.
     */
    static bool read_choice(const unsigned char* run_bytes,
        const unsigned char* card, copied_choice& destination) noexcept;

    /** @brief Capture a bounded current offer and enough identity to prevent duplicate apply.
     * @param run_address Candidate owner-thread run; @param output Local offer snapshot.
     * @param reason Static failure/not-choosing explanation.
     * @return True only for a verified phase-2 offer and every supported card.
     */
    static bool read_offer(void* run_address, offer_snapshot& output, const char*& reason) noexcept;

    /** @brief Compare stable offer identity across observations and native revalidation.
     * @param left First copied offer; @param right Second copied offer.
     * @return True for matching run, generation, kind, vector and card fingerprint.
     */
    static bool same_offer(const offer_snapshot& left, const offer_snapshot& right) noexcept;

    /** @brief Compare normalized ASCII stable keys without allocating temporary strings.
     * @param left Candidate key; @param right Configured preferred key.
     * @return True after trimming ASCII whitespace and ignoring ASCII letter case.
     */
    static bool equal_key(std::string_view left, std::string_view right) noexcept;

    /** @brief Invoke the verified native selection ABI while containing access exceptions.
     * @param run_address Validated current run; @param index Revalidated native card index.
     * @param seh_code Output exception code, zero on normal return.
     * @return True on a normal native unit return; false on missing routine or SEH failure.
     */
    static bool apply_native(void* run_address, std::size_t index, std::uint32_t& seh_code) noexcept;

    /** @brief Read a trivial scalar from a previously validated range.
     * @tparam value_type Trivially copyable scalar type.
     * @param address Source base; @param offset Source byte offset.
     * @param value Output scalar, meaningful only on success.
     * @return True on a guarded copy; false on an access exception.
     */
    template <typename value_type>
    static bool read_value(const void* address, std::size_t offset, value_type& value) noexcept {
        return copy_bytes(static_cast<const unsigned char*>(address) + offset, &value, sizeof(value));
    }

    inline static apply_function apply_choice_{};
    inline static offer_snapshot observed_{};
    inline static offer_snapshot attempted_{};
    inline static std::uint64_t observed_since_ms_{};
    inline static std::uint64_t last_apply_ms_{};
    inline static bool observing_{};
    inline static bool have_attempted_{};
};

} // namespace azs
