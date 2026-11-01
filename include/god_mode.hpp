#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace azs {

class god_mode_tests;

class god_mode final {
public:
    /** @brief Discover and bind spell-free native enemy damage from certified code signatures.
     * @param module_base Base of the already admitted main executable image.
     * @param executable_path Exact executable whose digest was verified by game_runtime.
     * @param digest Previously verified SHA-256 of that executable.
     * @return True when unique signatures, complete normalized functions, linked helpers and
     * core layout agree, and all actual live bytes match the independently hashed file.
     * Unknown executable hashes are allowed after this proof; incompatible code leaves only
     * this optional feature unavailable without affecting other trainer features.
     * Call only during quiescent initialization, before enabling gameplay hooks.
     */
    static bool initialize(std::uintptr_t module_base, const std::filesystem::path& executable_path,
        const std::array<unsigned char, 32>& digest) noexcept;

    /** @brief Report availability of the independently verified native damage binding.
     * @return True after successful initialization, false when the signature proof is incomplete.
     */
    static bool supported() noexcept;

    /** @brief Kill every living enemy in the complete local solo-run enemy container.
     * @param run_address Current authoritative run on its owning game thread, after a native
     * update. The caller must already have checked the immutable god_mode option and runtime
     * feature admission. This method additionally refuses multiplayer, participant contexts,
     * nonplaying phases and malformed containers; no visibility or distance test is applied.
     * @return True after completion or a safely refused inactive/online/unsupported context;
     * false for invalid memory, nonfinite health, native exceptions or failed lethal damage.
     * Native damage credits the character and preserves normal death rewards. A scoped native
     * source context and empty borrowed spell view suppress spell callbacks and packets;
     * the original spell count, all context bytes and temporary
     * special-enemy health caps are restored, including after a structured exception.
     */
    static bool process_run(void* run_address) noexcept;

private:
    friend class god_mode_tests;

    using damage_function = void(__fastcall*)(void*, std::uint64_t, float,
        std::uint32_t, std::uint8_t, bool, bool);

    struct native_profile {
        std::size_t enemies_offset{};
        std::size_t enemy_stride{};
        std::size_t source_offset{};
    };

    struct enemy_vector {
        unsigned char* data{};
        std::size_t count{};
    };

    /** @brief Validate committed writable nonexecutable memory before game-data access.
     * @param address First candidate byte; @param size Required nonzero range size.
     * @return True only when every byte belongs to committed writable nonguarded data pages.
     */
    static bool writable_data(const void* address, std::size_t size) noexcept;

    /** @brief Compare certified code or constant data against its exact offline image bytes.
     * @param module_base Actual admitted main module; @param rva Function entry RVA.
     * @param bytes File-backed function, callee anchor or required constant bytes.
     * @param executable True requires readable executable code; false requires read-only data.
     * @return True only for matching committed MEM_IMAGE bytes allocated to that module.
     */
    static bool matching_bytes(std::uintptr_t module_base, std::uint32_t rva,
        std::span<const std::uint8_t> bytes, bool executable) noexcept;

    /** @brief Read and validate the complete active enemy vector using the exact build profile.
     * @param run_address Validated full run; @param result Output buffer and active length.
     * @return True for a bounded coherent vector, including an empty dangling-pointer Vec;
     * false for capacity above 65536, length/capacity mismatch or invalid backing memory.
     */
    static bool read_enemies(void* run_address, enemy_vector& result) noexcept;

    /** @brief Check the exact solo-owner and playing-phase gates without changing game data.
     * @param run_address Writable run already validated on its owning game thread.
     * @return True only for phase one, coop disabled, participant index zero and offline role two.
     * The caller guards reads with SEH and repeats this check after native damage, because
     * defeating a final boss may finish the run before the remaining enemies are processed.
     */
    static bool solo_playing(const void* run_address) noexcept;

    /** @brief Find an enemy after native damage may have relocated its vector allocation.
     * @param run_address Current full run; @param guid Stable native enemy identity.
     * @return Current writable enemy record, or null if absent or metadata is invalid.
     */
    static unsigned char* find_enemy(void* run_address, std::uint64_t guid) noexcept;

    /** @brief Invoke spell-free native damage with exception-safe temporary context changes.
     * @param run_address Validated solo owner; @param guid Current enemy identity.
     * @param damage Positive native input sufficient after the minimum damage multiplier.
     * @param maximum_health Original enemy maximum HP to restore after temporary cap lifting.
     * @param raised_cap True when the special enemy's maximum HP was temporarily increased.
     * @return True after native completion and successful context/cap restoration; a native
     * structured exception propagates to process_run only after the finally restoration.
     */
    static bool deal_damage(void* run_address, std::uint64_t guid, float damage,
        float maximum_health, bool raised_cap);

    static inline damage_function damage_{};
    static inline native_profile profile_{};
};

} // namespace azs
