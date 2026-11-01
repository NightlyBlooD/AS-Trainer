#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace azs {

class speed_sync_tests;

class game_runtime final {
public:
    /** @brief Validate the supported executable and install its required native hooks.
     * @return True when executable identity, complete native/layout proof, live signatures and hook installation
     * succeed; false with a trainer_state diagnostic. Calls are idempotent after success.
     * Initialization must run outside DllMain. Installed hooks and this DLL remain resident.
     */
    static bool initialize() noexcept;

    /** @brief Permanently stop trainer writes while retaining live hook trampolines.
     * @return None; cancels commands/freezes and leaves original game calls functioning.
     * This is a terminal shutdown action, not an unload operation or temporary UI toggle.
     */
    static void disable_features() noexcept;

private:
    friend class speed_sync_tests;

    using tick_function = void(__fastcall*)(void*, float);
    using recalculate_function = void(__fastcall*)(void*);
    using lifesteal_function = float(__fastcall*)(void*);
    using speed_packet_function = void*(__fastcall*)(void*, std::uint64_t, std::uint32_t, float);
    using packet_grow_function = void(__fastcall*)(void*);

    struct game_packet {
        std::uintptr_t buffer_capacity;
        void* buffer;
        std::uintptr_t buffer_size;
        std::uintptr_t protocol;
    };

    struct speed_sync_diagnostic {
        const char* reason = "not_started";
        float speed{};
        float previous_world_speed{};
        std::size_t capacity{};
        std::size_t count{};
        std::uintptr_t data_address{};
        std::uint32_t captured_seh_code{};
    };

    /** @brief Compute the on-disk executable's SHA-256 using bounded read buffers.
     * @param file_path Absolute wide-character path returned for the main module.
     * @param digest Output 32-byte digest, meaningful only on success.
     * @param error Output static diagnostic explaining any Windows/CNG failure.
     * @return True after a complete read/hash; false without modifying the file.
     */
    static bool hash_executable(const wchar_t* file_path,
        std::array<unsigned char, 32>& digest, const char*& error) noexcept;

    /** @brief Match a critical live code sequence before any hook is installed.
     * @param address Candidate instruction address inside the main executable image.
     * @param expected Expected bytes from the supported executable.
     * @param size Number of expected bytes to compare.
     * @return True only for committed executable image memory with identical bytes.
     */
    static bool matching_code(std::uintptr_t address,
        const unsigned char* expected, std::size_t size) noexcept;

    /** @brief Check every critical hook/native-helper/known reader code signature.
     * @param module_base ASLR-adjusted base of the main executable.
     * @return True if supported instruction bytes are intact; false after code tampering.
     */
    static bool verify_code(std::uintptr_t module_base) noexcept;

    /** @brief Validate an entire scalar/packet memory range before accessing it.
     * @param address First candidate data byte; must not be null.
     * @param size Required contiguous byte count; executable and guarded pages are refused.
     * @return True when every covered region is committed readable/writable data memory.
     */
    static bool writable_data(const void* address, std::size_t size) noexcept;

    /** @brief Propagate an overridden speed through the game's own packet queue.
     * @param run_address Run already validated by trainer_state on its owning game thread.
     * @param diagnostic Output bounded stage/value/SEH evidence if propagation is refused.
     * @return True for unchanged speed or successful mirror/revision/packet update;
     * false without writing an invalid packet range. Uses the exact native 32-byte
     * packet constructor and specialized Vec grow helper; UI threads never call it.
     */
    static bool synchronize_speed(void* run_address, speed_sync_diagnostic& diagnostic) noexcept;

    /** @brief Apply synchronized trainer commands and maintain speed propagation.
     * @param run_address Candidate current run provided by a verified native hook.
     * @param publish True to publish the final game-thread snapshot and heartbeat.
     * @param publish_fields False to defer numeric UI snapshots while retaining run
     * adoption, clock tracking, heartbeat and every pending/frozen scalar write.
     * @return True for a current validated run, including an isolated speed rollback;
     * false when disabled or validation fails and native/data writes are skipped.
     */
    static bool process_run(void* run_address, bool publish, bool publish_fields = true) noexcept;

    /** @brief Accumulate one bounded frame sample and publish mean timings once a second.
     * @param started_ticks QueryPerformanceCounter timestamp before bridge work.
     * @param finished_ticks Timestamp after all substeps and bridge work.
     * @param owner_ticks Sum of intervals spent inside native owner trampolines.
     * @param steps Actual native owner calls during this frame; zero discards the
     * partial sampling window after disabled features or an invalid timer reading.
     * @return None; uses thread-local counters, cached timer frequency and atomic output.
     * Native intervals include nested hooks. Remaining time includes trainer-side native
     * helpers; the two wall-time boundaries are not exclusive CPU/GPU profiling results.
     */
    static void record_frame_performance(std::int64_t started_ticks, std::int64_t finished_ticks,
        std::int64_t owner_ticks, std::uint32_t steps) noexcept;

    /** @brief Synchronize a friend's speed through the same guarded native packet path.
     * @param run_address Full run temporarily containing the selected participant.
     * @return True after accepted native synchronization, false for isolated member rollback.
     */
    static bool synchronize_member_speed(void* run_address) noexcept;

    /** @brief Write native object-field bits through the selected build's exact setter ABI.
     * @param map Native dirty-field map; @param field_id Verified uint16 object-field key.
     * @param value_bits Exact 32-bit integer/float representation to preserve.
     * @return Native map pointer; current builds pass a bit-cast float in XMM2,
     * while the legacy build passes the unchanged integer in R8D.
     */
    static void* __fastcall set_member_field(void* map, std::uint16_t field_id,
        std::uint32_t value_bits) noexcept;

    /** @brief Read the final-boss extension switch only after complete runtime initialization.
     * @return True when trainer features and the current immutable loot option are enabled.
     */
    static bool legendary_enabled() noexcept;

    /** @brief Forward a bounded extension diagnostic to the UI command bridge.
     * @param message UTF-8 diagnostic produced on the owning game thread.
     * @return None; no game memory is accessed by the UI.
     */
    static void report_extension(const char* message) noexcept;

    /** @brief Advance the native owning update through bounded simulation substeps.
     * @param run_address RCX: current run owned by the calling game thread.
     * @param delta_seconds XMM1: original floating-point update interval.
     * @return None, matching the original Rust unit return. Calls the trampoline once
     * at normal speed or up to sixteen times with normal-sized intervals when accelerated.
     */
    static void __fastcall tick_hook(void* run_address, float delta_seconds) noexcept;

    /** @brief Preserve native stat rebuilding and reapply active trainer overrides.
     * @param run_address RCX: current run supplied by the original game caller.
     * @return None, matching the original Rust unit return. Calls its trampoline once.
     */
    static void __fastcall recalculate_hook(void* run_address) noexcept;

    /** @brief Replace the verified current-build lifesteal getter only for an active override.
     * @param run_address RCX: the exact native run/member context supplied by the game.
     * @return XMM0 float fraction of dealt damage restored as health; zero disables healing.
     * Active admitted overrides bypass the original pure summation getter. Otherwise the
     * original executes exactly once, and only its observed value is offered to the state
     * bridge. Disabled features always delegate unchanged. No scalar game field is written;
     * the game's own damage handler retains health clamping and object-field updates.
     */
    static float __fastcall lifesteal_hook(void* run_address) noexcept;

    /** @brief Publish a precise MinHook installation failure without throwing.
     * @param operation Static name of the failed initialization/install operation.
     * @param status Numeric MH_STATUS value returned by the failed operation.
     * @return None; features remain disabled and any live trampolines remain allocated.
     */
    static void hook_failure(const char* operation, int status) noexcept;

    inline static std::atomic<unsigned> initialization_state_{};
    inline static std::atomic<bool> features_enabled_{};
    inline static std::atomic<tick_function> original_tick_{};
    inline static std::atomic<recalculate_function> original_recalculate_{};
    inline static std::atomic<lifesteal_function> original_lifesteal_{};
    inline static speed_packet_function create_speed_packet_{};
    inline static packet_grow_function grow_packets_{};
    inline static std::uintptr_t module_base_{};
    inline static void*(__fastcall* object_field_bits_)(void*, std::uint16_t, std::uint32_t){};
    inline static void*(__fastcall* object_field_float_)(void*, std::uint16_t, float){};
};

} // namespace azs
