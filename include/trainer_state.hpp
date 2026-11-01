#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

namespace azs {

enum class value_kind { real, unsigned_integer, boolean };
enum class field_access { scalar, native_getter };

struct field_descriptor {
    const char* name;
    const char* unit;
    std::size_t offset;
    value_kind kind;
    double minimum;
    double maximum;
    double step;
    const char* key;
    field_access access{field_access::scalar};
};

struct field_snapshot {
    double current_value{};
    double desired_value{};
    bool available{};
    bool frozen{};
    bool pending{};
    bool configured{};
    bool apply_on_start{};
};

struct automation_settings {
    bool auto_choose_spells{};
    bool auto_choose_jewelry{};
    bool legendary_final_boss{};
    bool apply_to_party{};
    std::uint32_t choice_delay_ms{500};
    double time_scale{1.0};
    std::vector<std::string> spell_priorities;
    std::vector<std::string> jewelry_priorities;
};

struct preset_field {
    std::string key;
    double value{};
    bool frozen{};
    bool apply_on_start{};
};

struct trainer_preset {
    std::vector<preset_field> fields;
    automation_settings automation;
};

struct runtime_snapshot {
    bool hooks_ready{};
    bool run_active{};
    std::uintptr_t run_address{};
    std::uint32_t level{};
    std::uint64_t last_update_ms{};
    bool party_host{};
    std::uint32_t party_members{};
    std::uint64_t party_writes{};
    /** @brief Mean milliseconds per intercepted frame inside native owner calls,
     * including any hooks/helpers nested in those calls; zero until a fresh sample exists.
     */
    double owner_chain_ms{};
    /** @brief Mean milliseconds per intercepted frame outside native owner calls,
     * including trainer work and its native helpers; not an exclusive CPU profile.
     */
    double bridge_ms{};
    /** @brief Mean number of actual native owner calls per intercepted frame,
     * measured over the latest one-second sample; zero for unavailable/stale samples.
     */
    double simulation_steps{};
};

struct speed_update {
    bool written{};
    float previous_value{};
    float requested_value{};
    std::uint64_t command_id{};
};

struct member_commands {
    std::array<std::uint64_t, 64> applied_ids{};
    std::uint64_t rejected_speed_command{};
    std::uint64_t lifesteal_generation{};
};

class trainer_state final {
public:
    /** @brief Return the process-wide command/snapshot bridge shared by UI and hooks.
     * @return Reference to a singleton whose lifetime covers the loaded DLL.
     */
    static trainer_state& instance() noexcept;

    /** @brief Return the number of verified editable fields for the supported build.
     * @return Number of descriptors addressable with field_at/get_field.
     */
    static std::size_t field_count() noexcept;

    /** @brief Describe a field without dereferencing any game memory.
     * @param index Field index smaller than field_count().
     * @return Name, units, storage kind, resolved offset and accepted value bounds.
     * The returned reference belongs to this thread and remains valid until that thread
     * observes a new quiescent layout publication. Descriptor tables refresh once per
     * generation without allocating or searching offset maps on the gameplay hot path.
     */
    static const field_descriptor& field_at(std::size_t index) noexcept;

    /** @brief Read a synchronized UI snapshot rather than touching the live run.
     * @param index Field index smaller than field_count().
     * @return Last observed value and current desired/freeze/pending state.
     */
    field_snapshot get_field(std::size_t index) const noexcept;

    /** @brief Queue one bounded field write for the next game-owned update.
     * @param index Field index smaller than field_count().
     * @param value Finite desired value within the descriptor's accepted range.
     * @return True if queued for the current/next run; false for an invalid index/value.
     */
    bool apply(std::size_t index, double value) noexcept;

    /** @brief Enable/disable repeated application of one field in the game thread.
     * @param index Field index smaller than field_count().
     * @param enabled True to hold the supplied value, false to release the override.
     * @param value Finite desired value within the descriptor's accepted range.
     * @return True on acceptance for current/future runs; false for invalid input.
     */
    bool freeze(std::size_t index, bool enabled, double value) noexcept;

    /** @brief Cancel commands/freezes/start flags and automation, returning time to 1x.
     * @return None; already changed game values remain until normal recalculation.
     */
    void reset_all() noexcept;

    /** @brief Change whether a configured field is reapplied at each new run.
     * @param index Descriptor index; @param enabled True to queue the value on run adoption.
     * @return True on acceptance, false if no valid value has been configured.
     */
    bool set_apply_on_start(std::size_t index, bool enabled) noexcept;

    /** @brief Read immutable automation options without copying priority vectors in hooks.
     * @return A shared immutable settings snapshot valid for the caller's entire use.
     */
    std::shared_ptr<const automation_settings> get_automation() const noexcept;

    /** @brief Validate and publish UI/config automation settings.
     * @param settings Desired toggles, bounded choice delay and ordered stable-key lists.
     * @return True when published; false for invalid/oversized lists or allocation failure.
     */
    bool set_automation(const automation_settings& settings) noexcept;

    /** @brief Capture configured field values and automation for a portable INI preset.
     * @return Independent preset snapshot; unconfigured native fields are excluded.
     */
    trainer_preset export_preset() const;

    /** @brief Validate the entire preset before replacing commands and automation atomically.
     * @param preset Stable field keys, desired values and automation settings.
     * @return True on full acceptance; false without changing current settings on any error.
     */
    bool import_preset(const trainer_preset& preset) noexcept;

    /** @brief Read hook readiness and the last fresh run identity for UI display.
     * @return Snapshot with active=false if game updates have not arrived recently.
     */
    runtime_snapshot get_runtime() const noexcept;

    /** @brief Publish bounded party diagnostics without touching any native context.
     * @param hosting True when the runtime verified that this process owns the party.
     * @param member_count Number of party members reported by the runtime.
     * @param writes Number of successful party writes reported by the runtime.
     * @return None; stores diagnostics atomically for get_runtime() and the overlay.
     */
    void set_party_status(bool hosting, std::uint32_t member_count, std::uint64_t writes) noexcept;

    /** @brief Publish one bounded aggregate of game-thread frame timing measurements.
     * @param owner_chain_ms Mean elapsed milliseconds inside native owner trampolines.
     * @param bridge_ms Mean elapsed milliseconds outside those calls, including helpers.
     * @param simulation_steps Mean actual native owner calls per intercepted frame.
     * @return None; copies finite nonnegative samples with atomic stores and no locks.
     * Nested hooks belong to owner_chain_ms. These boundary timings do not measure GPU
     * time or exclusive CPU costs; snapshots expire after three seconds without updates.
     */
    void set_frame_performance(double owner_chain_ms, double bridge_ms,
        double simulation_steps) noexcept;

    /** @brief Store a human-readable diagnostic without exposing game data to the UI.
     * @param message UTF-8 diagnostic describing initialization or validation status.
     * @return None; messages are copied under an internal lock.
     */
    void set_message(const char* message) noexcept;

    /** @brief Copy the latest initialization/status diagnostic for the overlay.
     * @return A UTF-8 string owned by the caller.
     */
    std::string get_message() const;

    /** @brief Publish whether all required game hooks were installed successfully.
     * @param ready True only after executable identity and both hook sites are checked.
     * @return None.
     */
    void set_hooks_ready(bool ready) noexcept;

    /** @brief Apply pending/frozen values while executing on the game's owning thread.
     * @param run_address Candidate run pointer supplied by a verified hook.
     * @param publish True to refresh the UI snapshot and heartbeat after applying.
     * @param speed_result Optional output describing an actual speed write this pass.
     * @param publish_fields False to keep lifecycle/heartbeat publication while deferring
     * field-value reads and UI stores until the final accelerated substep; ignored when
     * publish is false. Memory/phase/clock validation and pending/frozen writes still run.
     * @return True for a validated live run, false without writing invalid memory.
     */
    bool process_run(void* run_address, bool publish, speed_update* speed_result = nullptr,
        bool publish_fields = true) noexcept;

    /** @brief Roll back one rejected speed write and release only its matching command.
     * @param run_address The same game-owned object that received the speed write.
     * @param update Exact prior/requested value and command generation from process_run.
     * @return True if the old speed was restored; unrelated controls are never cancelled.
     */
    bool reject_speed_update(void* run_address, const speed_update& update) noexcept;

    /** @brief Apply active selected overrides to one caller-owned party member context.
     * @param run_address Validated host run with this member temporarily swapped into it.
     * @param commands Per-member, per-run acknowledgements owned and keyed by the runtime.
     * Only successful writes consume that member's command generation; the caller resets
     * these acknowledgements when its member identity or run epoch changes.
     * Computed commands also acknowledge the current party-enable generation so a
     * rapid off/on toggle rearms them even when no native update observed the off state.
     * @param speed_result Optional output describing a speed write requiring native sync.
     * @return True for an enabled bridge and valid member in phase 1, 2 or 3; false
     * without consuming commands when memory, identity or phase validation fails.
     * The caller enforces host ownership/apply_to_party and performs native sync after
     * this method returns. Does not adopt a run, publish UI data, change common command
     * acknowledgements, or hold the controls mutex across any native helper call.
     */
    bool process_member(void* run_address, member_commands& commands,
        speed_update* speed_result = nullptr) noexcept;

    /** @brief Restore only one member's scalar speed after native synchronization fails.
     * @param run_address The member context still swapped into the game-owned host run.
     * @param update Exact previous/requested float values from process_member().
     * @return True if current speed still matches the rejected value and was restored;
     * false when the context/value is invalid or changed. Never cancels global freezes,
     * commands or UI snapshots; the runtime records rejected_speed_command separately.
     */
    bool reject_member_speed_update(void* run_address, const speed_update& update) noexcept;

    /** @brief Resolve a latched lifesteal override for the admitted native owner context.
     * @param run_address Exact current run supplied by the verified native getter.
     * @param value Output fraction of dealt damage to heal, changed only on success.
     * @return True for an active host latch or an enabled party member's matching GUID;
     * false for another thread/run/epoch/player, unsupported layout or inactive latch.
     * Uses bounded atomic reads and guarded identity reads without locks or allocation.
     */
    bool try_lifesteal_override(void* run_address, float& value) noexcept;

    /** @brief Cache one genuine native lifesteal result for later host UI publication.
     * @param run_address Exact admitted owner run still containing its host player.
     * @param native_value Finite fraction returned by the original native getter.
     * @return None; member observations and foreign contexts never replace the host cache.
     * Numeric UI snapshots consume this cache only during ordinary final-step publication.
     */
    void observe_lifesteal(void* run_address, float native_value) noexcept;

    /** @brief Release the matching player's computed latch before native stat rebuilding.
     * @param run_address Admitted run currently containing the host or a known party member.
     * @return None; a subsequent owner update reapplies frozen commands, while acknowledged
     * one-shot values remain released. Does not acquire the controls mutex or call native code.
     */
    void invalidate_lifesteal(void* run_address) noexcept;

private:
    struct field_control {
        std::atomic<double> current_value{};
        std::atomic<double> desired_value{};
        std::atomic<bool> available{};
        std::atomic<bool> frozen{};
        std::atomic<std::uint64_t> command_id{};
        std::atomic<std::uint64_t> applied_id{};
        std::atomic<bool> configured{};
        std::atomic<bool> apply_on_start{};
        std::atomic<bool> active_for_run{};
    };

    struct lifesteal_member {
        std::atomic<std::uint64_t> guid{};
        std::atomic<float> value{};
        std::atomic<bool> active{};
        std::uintptr_t command_owner{};
    };

    /** @brief Check the exact admitted run, thread, phase, GUID and nonreversed run clock.
     * @param run_address Candidate getter/command context.
     * @param guid Output current host-or-member GUID on success.
     * @return True for a current-layout owner context; false without exposing invalid memory.
     * Guarded fixed-field reads replace page queries on this frequently called native path.
     */
    bool lifesteal_context(void* run_address, std::uint64_t& guid) const noexcept;

    /** @brief Activate one computed command without writing a fictitious run-data offset.
     * @param run_address Validated owner context containing the target player.
     * @param value Previously validated lifesteal fraction in the trainer's 0–10 range.
     * @param commands Null for the host, or the owning member's acknowledgement record.
     * @return True when the host latch or a bounded member slot is published; false when
     * ownership/party admission fails or all slots belong to other command records.
     * Called with controls_mutex_ held; no native getter is called under that mutex.
     */
    bool write_lifesteal(void* run_address, double value, const member_commands* commands) noexcept;

    /** @brief Release every bounded party latch while the controls mutex is held.
     * @return None; clears GUIDs and command-record ownership as well as active values.
     */
    void clear_lifesteal_members_locked() noexcept;

    /** @brief Clear host observations and every computed latch under the controls mutex.
     * @return None; used on reset, preset replacement and authoritative run adoption.
     */
    void clear_lifesteal_locked() noexcept;

    /** @brief Validate one requested number before it can reach game memory.
     * @param index Descriptor index; @param value Proposed finite number.
     * @return True when the index, kind and numeric range are acceptable.
     */
    static bool valid_value(std::size_t index, double value) noexcept;

    /** @brief Validate bounded automation lists and delay independently of active game state.
     * @param settings Automation settings proposed by UI or a loaded config.
     * @return True for supported finite bounds and nonempty bounded keys.
     */
    static bool valid_automation(const automation_settings& settings) noexcept;

    /** @brief Check committed nonexecutable read/write memory and run invariants.
     * @param run_address Candidate object base captured by a verified hook.
     * @return True only when the supported fields and identity can be accessed safely.
     */
    static bool valid_run(void* run_address) noexcept;

    /** @brief Read a verified scalar or the host's cached computed getter result.
     * @param run_address Validated run base; @param index Descriptor index.
     * @param value Output decoded value, unchanged on failure.
     * @return True on a finite readable scalar; false on invalid access/value.
     */
    bool read_field(void* run_address, std::size_t index, double& value) noexcept;

    /** @brief Write scalar data bytes or activate a computed host command without a data write.
     * @param run_address Validated run base; @param index Descriptor index.
     * @param value Previously validated desired value.
     * @return True on success; false if memory access fails.
     */
    bool write_field(void* run_address, std::size_t index, double value) noexcept;

    /** @brief Read the level without allowing access exceptions to escape the bridge.
     * @param run_address Validated game-owned run; @param level Output current level.
     * @return True on success, false if access becomes invalid.
     */
    static bool read_level(void* run_address, std::uint32_t& level) noexcept;

    /** @brief Read the finite simulation clock used to detect reused run storage.
     * @param run_address Validated run base; @param elapsed Output elapsed game seconds.
     * @return True on a finite nonnegative clock, false on invalid access/state.
     */
    static bool read_clock(void* run_address, float& elapsed) noexcept;

    /** @brief Read the native phase without accessing UI state.
     * @param run_address Validated run base; @param phase Output ready/playing/choice/pause/end code.
     * @return True on a guarded byte read; false when memory access fails.
     */
    static bool read_phase(void* run_address, std::uint8_t& phase) noexcept;

    /** @brief Read the current player's verified GUID without page queries or allocation.
     * @param run_address Owner-provided run context; @param guid Output player identity.
     * @return True after a guarded read, false on an unexpected access failure.
     */
    static bool read_guid(void* run_address, std::uint64_t& guid) noexcept;

    /** @brief Cancel commands while the caller already owns controls_mutex_.
     * @return None; never acquires the mutex recursively.
     */
    void reset_locked() noexcept;

    mutable std::mutex controls_mutex_;
    std::array<field_control, 64> fields_{};
    std::atomic<bool> hooks_ready_{};
    std::atomic<std::uintptr_t> run_address_{};
    std::atomic<std::uint32_t> level_{};
    std::atomic<std::uint64_t> last_update_ms_{};
    std::atomic<float> last_game_time_{};
    std::atomic<std::uint64_t> host_guid_{};
    std::atomic<std::uint32_t> owner_thread_{};
    std::atomic<float> lifesteal_value_{};
    std::atomic<bool> lifesteal_active_{};
    std::atomic<float> observed_lifesteal_{};
    std::atomic<bool> observed_lifesteal_ready_{};
    std::atomic<bool> lifesteal_party_enabled_{};
    std::atomic<std::uint64_t> lifesteal_party_generation_{};
    std::array<lifesteal_member, 8> lifesteal_members_{};
    std::atomic<bool> party_host_{};
    std::atomic<std::uint32_t> party_members_{};
    std::atomic<std::uint64_t> party_writes_{};
    std::atomic<double> owner_chain_ms_{};
    std::atomic<double> bridge_ms_{};
    std::atomic<double> simulation_steps_{};
    std::atomic<std::uint64_t> performance_update_ms_{};
    std::atomic<std::shared_ptr<const automation_settings>> automation_{
        std::make_shared<const automation_settings>()};
};

} // namespace azs
