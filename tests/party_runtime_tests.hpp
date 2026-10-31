#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace azs {

class party_runtime_tests final {
public:
    /** @brief Exercise party command traversal against owned Windows virtual memory.
     * @return Zero when all focused assertions pass, one otherwise.
     * No game process, hooks, desktop automation or real native game helpers are used.
     */
    static int run();

private:
    struct native_event {
        char kind{};
        std::size_t index{};
        std::uint64_t guid{};
        std::uint16_t field{};
        std::uint32_t value{};
        std::size_t queued{};
    };

    /** @brief Record an assertion while allowing independent regression cases to continue.
     * @param condition Expected true result; @param description Failure context.
     * @return None; increments the suite failure count on false.
     */
    static void check(bool condition, const char* description);

    /** @brief Locate a public trainer field by its native scalar offset.
     * @param offset Verified run-relative offset.
     * @return Matching descriptor index, or field_count() if absent.
     */
    static std::size_t find_field(std::size_t offset);

    /** @brief Initialize one host or packed participant with finite native-like scalars.
     * @param memory Owned context storage; @param guid Distinct player identity.
     * @param participant True for a 0xa38-byte packed friend, false for a full run.
     * @return None; chest cost and phase use the appropriate context-specific offsets.
     */
    static void initialize_context(void* memory, std::uint64_t guid, bool participant);

    /** @brief Reset bridge, runtime, native traces and both packed participants.
     * @param enabled Desired apply_to_party automation flag.
     * @return None; publishes a clean host snapshot and binds the owning test thread.
     */
    static void prepare(bool enabled = true);

    /** @brief Clear only native trace and helper counters, preserving party ledgers.
     * @return None; useful when proving repeated commands or freezes independently.
     */
    static void clear_trace() noexcept;

    /** @brief Append a bounded native-helper observation without allocating.
     * @param kind Swap/flush/route/map/speed marker; @param run Current full context.
     * @param field Optional object-field identifier; @param value Optional value bits.
     * @return None; records GUID, participant index and pending shared packet count.
     */
    static void record(char kind, void* run, std::uint16_t field = 0,
        std::uint32_t value = 0) noexcept;

    /** @brief Emulate the native context swap for all editable scalar fields.
     * @param run Full run; @param participant One owned packed participant.
     * @return None; exchanges 1b0..5b0 plus native phase/chest mappings.
     */
    static void __fastcall swap_player(void* run, void* participant);

    /** @brief Observe object-update flushing or raise a selected structured exception.
     * @param run Full context with the current player's GUID and participant index.
     * @return None; fault selection is test-owned and never invokes game code.
     */
    static void __fastcall flush_updates(void* run);

    /** @brief Observe queue attribution and consume the fake shared queue length.
     * @param run Full context whose c98 identifies the packet owner.
     * @return None; records routing order before resetting the owned queue count.
     */
    static void __fastcall route_queues(void* run);

    /** @brief Record native map setter ABI inputs or raise a selected access exception.
     * @param map Exact run+c78 pointer; @param field Native object-field identifier.
     * @param value Raw u32 payload, including f32 bits for critical chance.
     * @return The unchanged map pointer, matching the native helper ABI.
     */
    static void* __fastcall set_object_field(void* map, std::uint16_t field, std::uint32_t value);

    /** @brief Accept or reject synchronization for one selected player GUID.
     * @param run Current full player context after a scalar speed write.
     * @return False only for the injected GUID; success updates the fake native speed cache.
     */
    static bool synchronize_speed(void* run) noexcept;

    /** @brief Check ownership, opt-in, phase and malformed Vec refusal without native calls.
     * @return None; covers a dead host while the global party remains active.
     */
    static void test_gates();

    /** @brief Check independent per-GUID commands, freezes, UI isolation and native routing.
     * @return None; also verifies current HP/max HP/crit map payloads and member replacement.
     */
    static void test_commands();

    /** @brief Preserve one-shot ledgers across packed-record reorder, departure and vacant slots.
     * @return None; verifies spent gold follows its GUID and routing follows the current index.
     */
    static void test_reorder_and_departure();

    /** @brief Check deferred ended-player commands and freeze reapplication after recalc.
     * @return None; validates phase rejection does not consume a pending generation.
     */
    static void test_phases_and_recalculation();

    /** @brief Check context restoration after native SEH and isolated member speed rollback.
     * @return None; confirms shared freezes and the normal host bridge remain enabled.
     */
    static void test_failures();

    /** @brief Read a trivial scalar from fixture-owned storage without aliasing casts.
     * @tparam value_type Trivial scalar/result type; @param memory Fixture base.
     * @param offset Byte offset; @return Copied scalar, with bounds guaranteed by the test.
     */
    template <typename value_type>
    static value_type read_value(const void* memory, std::size_t offset) noexcept {
        value_type value{};
        std::memcpy(&value, static_cast<const unsigned char*>(memory) + offset, sizeof(value));
        return value;
    }

    /** @brief Store a trivial scalar into fixture-owned writable storage.
     * @tparam value_type Trivial scalar type; @param memory Fixture base.
     * @param offset Byte offset; @param value Scalar to copy.
     * @return None; the caller guarantees an accessible owned range.
     */
    template <typename value_type>
    static void write_value(void* memory, std::size_t offset, value_type value) noexcept {
        std::memcpy(static_cast<unsigned char*>(memory) + offset, &value, sizeof(value));
    }

    inline static int failures_{};
    inline static unsigned char* run_{};
    inline static unsigned char* participants_{};
    inline static std::array<native_event, 256> events_{};
    inline static std::size_t event_count_{};
    inline static std::size_t speed_calls_{};
    inline static std::uint64_t flush_fault_guid_{};
    inline static std::uint64_t map_fault_guid_{};
    inline static std::uint64_t speed_failure_guid_{};
};

} // namespace azs
