#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace azs {

class legendary_loot_tests;

class legendary_loot final {
public:
    using enabled_getter = bool (*)() noexcept;
    using message_sink = void (*)(const char*) noexcept;

    /** @brief Install the verified final-boss loot extension after MinHook initialization.
     * @param module_base ASLR-adjusted base of the SHA-256-verified supported executable.
     * @param is_enabled Owning-thread-safe callback returning the current automation switch.
     * @param report Optional diagnostic callback; a loot validation failure never disables other features.
     * @return True when the exact boss/grow signatures and zone data match and the hook is active.
     * The hook calls the native boss handler once, then extends its pending-loot queue on that
     * same game thread. Native award processing owns collection updates, duplicate handling and
     * persistence. This class neither equips items nor edits saved progress collections directly.
     * Installed hooks and trampolines remain resident for the process lifetime.
     */
    static bool initialize(std::uintptr_t module_base, enabled_getter is_enabled,
        message_sink report = nullptr) noexcept;

private:
    friend class legendary_loot_tests;

    using boss_loot_function = void (__fastcall*)(void*, const void*);
    using grow_loot_function = void (__fastcall*)(void*);

    struct loot_record {
        std::uint32_t item_id;
        std::uint32_t boss_roll_index;
        float award_marks;
    };

    /** @brief Validate a bounded readable or writable nonexecutable data range.
     * @param address First byte of the requested range.
     * @param size Number of contiguous bytes; address addition must not overflow.
     * @param writable True to require read/write data, false to permit read-only tables.
     * @return True only when every page is committed, accessible and not executable/guarded.
     */
    static bool data_range(const void* address, std::size_t size, bool writable) noexcept;

    /** @brief Match a supported native helper's bytes inside committed image code.
     * @param address First instruction byte in the verified executable.
     * @param expected Immutable expected prologue bytes.
     * @param size Number of bytes to compare.
     * @return True for matching readable executable image bytes, false otherwise.
     */
    static bool matching_code(std::uintptr_t address, const unsigned char* expected,
        std::size_t size) noexcept;

    /** @brief Build every permanent legendary armor/weapon ID for one supported class.
     * @param class_id Native class ID, including Mage 8.
     * @param item_ids Output storage for eight armor pieces and up to two weapon variants.
     * @return Nine IDs for ordinary classes, ten for Rogue/Shaman, or zero for an invalid class.
     */
    static std::size_t legendary_ids(std::uint8_t class_id,
        std::array<std::uint32_t, 10>& item_ids) noexcept;

    /** @brief Confirm the event descriptor is the current run zone's actual final boss.
     * @param run_address Native run passed to the hooked boss handler.
     * @param boss_address Native boss descriptor passed to that same handler.
     * @param class_id Output supported current class ID, meaningful only on success.
     * @return True only for a matching zone key, descriptor membership and final-boss flag.
     * A final flag on a foreign descriptor or another zone's boss is insufficient.
     */
    static bool final_boss_event(void* run_address, const void* boss_address,
        std::uint8_t& class_id) noexcept;

    /** @brief Append missing legendary records through the verified native loot Vec grower.
     * @param run_address Current run on its owning thread, after native boss processing.
     * @param class_id Supported class captured from the validated event.
     * @return True for a complete pending set/already queued set; false without publishing a partial suffix.
     * Records preserve the native 12-byte layout and current boss-roll index. Added records carry
     * zero direct mark awards; the native award path retains its normal owned-item policy.
     * Existing queue records and any allocation relocated by native growth remain owned by the game.
     */
    static bool append_legendary_set(void* run_address, std::uint8_t class_id) noexcept;

    /** @brief Preserve native boss loot before optionally adding the complete legendary set.
     * @param run_address RCX: current owning-thread run.
     * @param boss_address RDX: current zone's defeated boss descriptor.
     * @return None, matching the original Rust unit-return function.
     */
    static void __fastcall boss_loot_hook(void* run_address, const void* boss_address) noexcept;

    inline static std::atomic<unsigned> initialization_state_{};
    inline static std::atomic<boss_loot_function> original_boss_loot_{};
    inline static grow_loot_function grow_loot_{};
    inline static const unsigned char* zone_descriptors_{};
    inline static enabled_getter is_enabled_{};
    inline static message_sink report_{};
};

} // namespace azs
