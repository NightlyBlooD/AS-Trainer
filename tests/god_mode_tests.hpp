#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace azs {

class god_mode_tests final {
public:
    /** @brief Exercise solo admission, exact lethal credit, spell suppression and native relocation.
     * @return Zero after all owned-memory fixtures pass, otherwise one with failure diagnostics.
     * No game process or real native game damage is used.
     */
    static int run();

    /** @brief Verify the complete core bindings, God damage and its MinHook trampoline against a PE image.
     * @param executable_path Existing supported game executable, opened read-only and mapped
     * with SEC_IMAGE. No entry point, game update or native damage function is executed.
     * @return Zero when exact image verification and optional hook creation succeed, one otherwise.
     * This optional local validation is separate from the portable owned-memory fixture suite.
     */
    static int verify_image(const wchar_t* executable_path);

private:
    /** @brief Record a meaningful assertion without stopping independent regression cases.
     * @param condition Required true result; @param description Expected observed behavior.
     * @return None; increments the suite failure count on false.
     */
    static void check(bool condition, const char* description);

    /** @brief Store one trivially copyable fixture scalar without alignment assumptions.
     * @tparam value_type Scalar representation; @param memory Owned writable buffer.
     * @param offset Byte offset; @param value Exact scalar bits to copy.
     * @return None; caller supplies sufficient backing memory.
     */
    template <typename value_type>
    static void write_value(void* memory, std::size_t offset, value_type value) noexcept {
        std::memcpy(static_cast<unsigned char*>(memory) + offset, &value, sizeof(value));
    }

    /** @brief Read one trivially copyable fixture scalar without alignment assumptions.
     * @tparam value_type Scalar representation; @param memory Owned readable buffer.
     * @param offset Byte offset within the allocation.
     * @return Exact copied scalar; caller supplies sufficient backing memory.
     */
    template <typename value_type>
    static value_type read_value(const void* memory, std::size_t offset) noexcept {
        value_type result{};
        std::memcpy(&result, static_cast<const unsigned char*>(memory) + offset, sizeof(result));
        return result;
    }

    /** @brief Prepare a supported native profile and four enemies in an owned solo run.
     * @param modern True for modern enemy records, false for v0.132.0 records.
     * @param modern_stride Audited modern NPC width: 0x200 before v0.139.0, 0x208 in v0.139.0.
     * @return None; resets counters, source context, native fault/relocation injection and gates.
     */
    static void prepare(bool modern = true, std::size_t modern_stride = 0x200);

    /** @brief Emulate native mitigation, special-enemy cap, character credit and death state.
     * @param run_address Owned admitted run; @param guid Target enemy identity.
     * @param damage Native damage input; @param spell Zero for direct spell-free damage.
     * @param school Zero for the verified direct damage school; @param critical False.
     * @param suppress_packet True to avoid a spell combat-log packet.
     * @return None; optionally relocates the Vec or raises an SEH fault after context assertions.
     */
    static void __fastcall native_damage(void* run_address, std::uint64_t guid, float damage,
        std::uint32_t spell, std::uint8_t school, bool critical, bool suppress_packet);

    /** @brief Count an unsuppressed native talent dispatch and validate unchanged arguments.
     * @param run_address Owned run; @param event_address Borrowed test event marker.
     * @param source_context Arbitrary scalar context forwarded unchanged by the hook.
     * @return None; represents a spell callback that God damage must suppress.
     */
    static void __fastcall native_dispatch(void* run_address, const void* event_address,
        std::uintptr_t source_context);

    /** @brief Verify no native calls occur for network roles, coop, participants or inactive phases.
     * @return None; also proves malformed capacity/length and inaccessible memory fail closed.
     */
    static void test_gates();

    /** @brief Verify complete-container lethal damage, exact credit and one-shot dead skipping.
     * @return None; repeats the contract against all three independently supported NPC widths.
     */
    static void test_damage();

    /** @brief Verify context and health-cap restoration after vector relocation and native faults.
     * @return None; checks old backing memory is never dereferenced after native reallocation.
     */
    static void test_restoration();

    /** @brief Verify native dispatcher delegation, nested scopes and thread-local isolation.
     * @return None; proves ordinary gameplay events remain delegated outside God damage.
     */
    static void test_events();

    static inline int failures_{};
    static inline unsigned char* run_{};
    static inline unsigned char* enemies_{};
    static inline unsigned char* relocated_{};
    static inline std::size_t call_count_{};
    static inline std::size_t dispatch_count_{};
    static inline double credited_damage_{};
    static inline bool relocate_{};
    static inline bool throw_native_{};
    static inline bool finish_run_{};
    static inline bool old_buffer_protected_{};
    static inline std::array<unsigned char, 16> original_source_{};
    static inline std::array<unsigned char, 16> original_target_{};
};

} // namespace azs
