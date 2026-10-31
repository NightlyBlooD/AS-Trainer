#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace azs {

struct automation_settings;

class auto_choice_tests final {
public:
    /** @brief Verify selection order, stable-key decoding and native-action deduplication.
     * @return Zero on success, one when any focused check fails.
     * Uses only allocated fixtures and a stub native routine; no game process is touched.
     */
    static int run();

private:
    /** @brief Record a regression assertion without aborting remaining checks.
     * @param condition Expected condition; @param description Failure explanation.
     * @return None; increments the failure counter only when false.
     */
    static void check(bool condition, const char* description);

    /** @brief Reset an owned fake run, offer vector and native-call observation.
     * @param run_address Writable 0x2000-byte fixture; @param choices Card-buffer base.
     * @return None; installs only the test stub and a valid phase-2 class-11 baseline.
     */
    static void prepare(void* run_address, void* choices);

    /** @brief Populate a new-spell card with separate misleading title and stable payload.
     * @param cards Array base; @param index Card index; @param key Borrowed ASCII stable key.
     * @param rarity Native rarity in 0..4.
     * @return None; writes one 0xa0-byte owned fixture entry.
     */
    static void new_spell(void* cards, std::size_t index, const char* key, std::uint64_t rarity);

    /** @brief Stub the exact native apply ABI, optionally reentering or raising an exception.
     * @param run_address Fixture run; @param index Selected zero-based native card index.
     * @return None, matching the game's unit-return function.
     */
    static void __fastcall apply_stub(void* run_address, std::size_t index);

    /** @brief Write a trivial test value into its owned fixture range.
     * @tparam value_type Trivially copyable scalar type.
     * @param address Base; @param offset Byte offset; @param value Scalar to copy.
     * @return None; caller supplies valid owned memory.
     */
    template <typename value_type>
    static void write(void* address, std::size_t offset, value_type value) noexcept {
        std::memcpy(static_cast<unsigned char*>(address) + offset, &value, sizeof(value));
    }

    inline static int failures_{};
    inline static std::size_t apply_calls_{};
    inline static std::size_t selected_index_{};
    inline static void* selected_run_{};
    inline static const automation_settings* reentry_settings_{};
    inline static bool test_reentry_{};
    inline static bool reentry_blocked_{};
    inline static bool raise_exception_{};
};

} // namespace azs
