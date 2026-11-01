#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace azs {

class game_layout_tests final {
public:
    /** @brief Verify profile selection, new scalar writes and a synthetic current-layout party.
     * @return Zero after every layout regression passes; one on failure or contained exception.
     * Uses owned byte arrays and fake native callbacks, never a game process.
     */
    static int run() noexcept;

private:
    /** @brief Record one regression invariant with a concise diagnostic on failure.
     * @param condition Required invariant; @param description Assertion explanation.
     * @return None; a failed invariant increments the suite failure count.
     */
    static void check(bool condition, const char* description) noexcept;

    /** @brief Check default legacy identity, explicit current translations and unknown rejection.
     * @return None; all field descriptors must agree with the active immutable profile.
     */
    static void check_profiles();

    /** @brief Apply all 25 commands to a synthetic new run while preserving unrelated bytes.
     * @return None; an object containing only legacy identity fields must be rejected.
     */
    static void check_current_scalars();

    /** @brief Traverse one current-layout friend through fake native helpers and restore the host.
     * @return None; verifies new stride, role/index, GUIDs, dirty map and opt-out behavior.
     */
    static void check_current_party();

    /** @brief Exercise a complete discovered map with a nonuniform scalar relocation.
     * @return None; verifies actual synthetic writes, descriptor refresh and atomic rejection
     * of incomplete, colliding and inconsistent maps without touching a running game.
     */
    static void check_adaptive_layout();

    /** @brief Seed the verified current-layout scalar identity of one owned test object.
     * @param bytes Writable fixture base; @param guid Hero GUID; @param class_id Native class ID.
     * @return None; initializes health, level, phase and elapsed clock only.
     */
    static void seed_run(unsigned char* bytes, std::uint64_t guid, std::uint8_t class_id) noexcept;

    /** @brief Store a trivial scalar into an owned synthetic memory block.
     * @tparam value_type Scalar type; @param base Fixture base; @param offset Exact byte position.
     * @param value Scalar value to copy; @return None.
     */
    template <typename value_type>
    static void store(unsigned char* base, std::size_t offset, value_type value) noexcept {
        std::memcpy(base + offset, &value, sizeof(value));
    }

    /** @brief Decode a trivial scalar from an owned synthetic memory block.
     * @tparam value_type Scalar type; @param base Fixture base; @param offset Exact byte position.
     * @return A copied scalar; all caller-provided fixture ranges are statically bounded.
     */
    template <typename value_type>
    static value_type read(const unsigned char* base, std::size_t offset) noexcept {
        value_type value{};
        std::memcpy(&value, base + offset, sizeof(value));
        return value;
    }

    /** @brief Exchange one bounded fixture scalar while retaining each hero's own identity.
     * @param run Host context; @param member Participant record; @param run_offset Run byte offset.
     * @param member_offset Record byte offset; @param size Scalar size no larger than eight bytes.
     * @return None; this is a fake native operation on test-owned memory only.
     */
    static void swap_bytes(unsigned char* run, unsigned char* member, std::size_t run_offset,
                            std::size_t member_offset, std::size_t size) noexcept;

    /** @brief Fake the verified native player swap for the scalar fields used by the bridge.
     * @param run_address Owned run fixture; @param participant Owned native-record fixture.
     * @return None; paired calls restore all exchanged fixture fields.
     */
    static void __fastcall swap_player(void* run_address, void* participant) noexcept;

    /** @brief Fake the native dirty-map flush without touching any external memory.
     * @param run_address Owned current-context fixture; @return None.
     */
    static void __fastcall flush_updates(void* run_address) noexcept;

    /** @brief Check the mapped participant-routing index during a fake queue route.
     * @param run_address Owned current-context fixture; @return None.
     */
    static void __fastcall route_queues(void* run_address) noexcept;

    /** @brief Check health-field synchronization targets the new run's exact dirty map.
     * @param fields Expected fixture dirty map; @param key Native object-field ID.
     * @param value Encoded field value; @return The same fixture map pointer.
     */
    static void* __fastcall set_object_field(void* fields, std::uint16_t key,
                                              std::uint32_t value) noexcept;

    inline static int failures_{};
    inline static unsigned char* party_run_{};
    inline static std::size_t swap_calls_{};
    inline static std::size_t route_calls_{};
    inline static std::size_t field_calls_{};
};

} // namespace azs
