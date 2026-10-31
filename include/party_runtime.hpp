#pragma once

#include "trainer_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace azs {

struct party_native_functions {
    void(__fastcall* swap_player)(void*, void*){};
    void(__fastcall* flush_updates)(void*){};
    void(__fastcall* route_queues)(void*){};
    void*(__fastcall* set_object_field)(void*, std::uint16_t, std::uint32_t){};
    bool(*synchronize_speed)(void*) noexcept{};
};

class party_runtime_tests;

class party_runtime final {
public:
    /** @brief Bind helpers whose code signatures were verified by game_runtime.
     * @param functions Native context swap, object-field/queue helpers and guarded speed callback.
     * @return None; initialization occurs before trainer hooks are enabled.
     */
    static void initialize(const party_native_functions& functions) noexcept;

    /** @brief Observe the verified owning update and bind its thread and run epoch.
     * @param run_address Full run supplied by the original owner-frame caller.
     * @return None; pointer/GUID/clock changes invalidate old participant acknowledgements.
     * This never calls native helpers or writes game memory.
     */
    static void observe_owner(void* run_address) noexcept;

    /** @brief Apply configured commands to every live friend of the locally hosted party.
     * @param run_address Full run at an owning-frame boundary, with participant index zero.
     * @return None; every swapped context is restored before returning, including native errors.
     * Host queues are routed before the first swap; friend queues are routed before restoration.
     */
    static void process_owner(void* run_address) noexcept;

    /** @brief Reapply party freezes after native rebuilding of a temporary friend context.
     * @param run_address Full run containing the participant selected by the active profile's index.
     * @return None; host-only controls and UI snapshots are never consumed for a friend.
     */
    static void process_recalculated(void* run_address) noexcept;

    /** @brief Detect a temporary participant context before using the host command bridge.
     * @param run_address Candidate full run; only a guarded scalar read is performed.
     * @return True for a nonzero participant index, false for host or unreadable memory.
     */
    static bool is_member_context(void* run_address) noexcept;

private:
    friend class party_runtime_tests;

    struct member_record {
        std::uint64_t guid{};
        member_commands commands;
        std::size_t last_index{};
    };

    struct player_values {
        float health{};
        float maximum_health{};
        float crit{};
    };

    /** @brief Check a bounded writable nonexecutable range before native field access.
     * @param address Start of the candidate range; @param size Required byte count.
     * @return True only for committed writable data pages covering the whole range.
     */
    static bool writable_data(const void* address, std::size_t size) noexcept;

    /** @brief Read the root context, host role and bounded participant Vec metadata.
     * @param run_address Verified owning run; @param data Output participant buffer.
     * @param count Output allocated records in the active Vec; @param elapsed Output game clock.
     * @return True only for local hosting, coop flag one, global phase 1/2/3 and index zero.
     * A dead host may still own a live party; individual phases are checked on member writes.
     */
    static bool read_party(void* run_address, unsigned char*& data,
        std::size_t& count, float& elapsed) noexcept;

    /** @brief Read scalar values mirrored to the game object's update map.
     * @param run_address Current full run; @param values Output HP/max HP/crit snapshot.
     * @return True for readable finite values in the supported bounds.
     */
    static bool read_values(void* run_address, player_values& values) noexcept;

    /** @brief Enqueue changed HP/max HP/crit using the native dirty-field map setter.
     * @param run_address Full run containing the affected player context.
     * @param before Values observed before overrides; @param force True for initial host sync.
     * @return True after native synchronization, false for invalid values or access exceptions.
     */
    static bool synchronize_values(void* run_address, const player_values& before, bool force) noexcept;

    /** @brief Apply one participant's commands and preserve independent speed rollback.
     * @param run_address Full run with this participant active; @param member Its GUID/ack ledger.
     * @return True for a valid member pass, false without consuming invalid-phase commands.
     */
    static bool apply_member(void* run_address, member_record& member) noexcept;

    /** @brief Swap one native participant in, apply fields, route queues and restore host.
     * @param run_address Full owning run; @param participant Validated native record.
     * @param index One-based participant index; @param member Independent acknowledgement ledger.
     * @return True after a completed pass; false after a contained error with restoration attempted.
     */
    static bool process_member(void* run_address, void* participant,
        std::size_t index, member_record& member) noexcept;

    /** @brief Flush and route the current context without allowing a native exception to escape.
     * @param run_address Full run with the correct current participant index.
     * @return True after both verified helpers return, false on an access exception.
     */
    static bool flush_context(void* run_address) noexcept;

    /** @brief Preserve one-shot acknowledgements when the native Vec reorders participants.
     * @param guid Stable current player GUID; @param index Current one-based routing index.
     * @return Existing GUID ledger or a fresh ledger for a newly joined participant.
     * Position is used only to reclaim a replaced entry when native temporarily owns the Vec.
     */
    static member_record& find_member(std::uint64_t guid, std::size_t index) noexcept;

    /** @brief Count valid occupied participant records without counting vacant native slots.
     * @param data Validated Vec storage; @param count Bounded native record count.
     * @return Host plus occupied participant records with nonzero GUIDs.
     */
    static std::uint32_t player_count(const unsigned char* data, std::size_t count) noexcept;

    /** @brief Read a guarded trivial scalar from a current native context.
     * @tparam value_type Trivially copyable scalar type.
     * @param address Native base; @param offset Byte offset; @param value Output scalar.
     * @return True after a successful read, false for null/overflow/sentinel offsets or an access exception.
     */
    template <typename value_type>
    static bool read_value(const void* address, std::size_t offset, value_type& value) noexcept {
        const auto base = reinterpret_cast<std::uintptr_t>(address);
        if (!base || offset > UINTPTR_MAX - base || sizeof(value) > UINTPTR_MAX - base - offset) {
            return false;
        }
        __try {
            std::memcpy(&value, static_cast<const unsigned char*>(address) + offset, sizeof(value));
            return true;
        } __except (1) {
            return false;
        }
    }

    /** @brief Publish a bounded failure once and stop this epoch's manual party traversal.
     * @param message Static UTF-8 diagnostic; @return None, original game updates continue.
     */
    static void fail(const char* message) noexcept;

    inline static party_native_functions native_{};
    inline static std::atomic<void*> owner_run_{};
    inline static std::atomic<std::uint32_t> owner_thread_{};
    inline static std::uint64_t owner_guid_{};
    inline static float last_elapsed_{};
    inline static bool enabled_before_{};
    inline static bool failed_{};
    inline static bool host_values_ready_{};
    inline static player_values host_values_{};
    inline static std::uint64_t writes_{};
    inline static std::array<member_record, 4> members_{};
};

} // namespace azs
