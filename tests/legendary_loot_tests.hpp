#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace azs {

class legendary_loot_tests final {
public:
    /** @brief Test final-boss gating, complete class IDs and native pending-loot queue extension.
     * @return Zero when every focused assertion passes, one otherwise.
     * Uses only owned fixtures and an injected native-growth stub; installs no hooks,
     * opens no game process and never touches real progress or game memory.
     */
    static int run();

private:
    enum class growth_mode { normal, unchanged, corrupt_count, access_exception };

    /** @brief Record a regression assertion while continuing the independent checks.
     * @param condition Required true condition; @param description Failure context.
     * @return None; increments failures_ when false.
     */
    static void check(bool condition, const char* description);

    /** @brief Allocate tracked writable virtual memory for one fixture buffer.
     * @param size Required bytes, allocated independently from the real game allocator.
     * @return Owned memory or null if allocation/tracking fails.
     */
    static void* allocate(std::size_t size);

    /** @brief Free one tracked allocation, including an old buffer relocated by growth.
     * @param address Exact allocation base; null/untracked addresses are ignored.
     * @return None.
     */
    static void release(void* address);

    /** @brief Construct synthetic zone descriptors and install injected callbacks.
     * @return None; the fixture preserves the verified table and boss record offsets.
     */
    static void prepare_tables();

    /** @brief Reset the owned run to one supported zone/class and an empty native Vec.
     * @param run_address Owned storage at least 0x2000 bytes long.
     * @param zone_index Zone 0..11; @param class_id Native supported/negative-test class.
     * @return None; previous tracked queue allocation is released first.
     */
    static void prepare_run(void* run_address, std::size_t zone_index, std::uint8_t class_id);

    /** @brief Return a synthetic current or foreign-zone boss descriptor.
     * @param zone_index Fixture zone 0..11; @param boss_index Boss 0..4.
     * @return Descriptor pointer whose final flag is true only at index four.
     */
    static const void* boss_at(std::size_t zone_index, std::size_t boss_index);

    /** @brief Supply the automation switch through the production callback ABI.
     * @return enabled_, without accessing trainer state or a game process.
     */
    static bool enabled() noexcept;

    /** @brief Record an isolated feature failure through the production diagnostic ABI.
     * @param message Static diagnostic from the loot extension.
     * @return None; increments message_calls_ only for a nonempty message.
     */
    static void report(const char* message) noexcept;

    /** @brief Simulate original boss processing once without allocating or awarding real items.
     * @param run_address Fixture run in RCX; @param boss_address Descriptor in RDX.
     * @return None; records the arguments and increments the native boss-roll counter.
     */
    static void __fastcall original_boss(void* run_address, const void* boss_address);

    /** @brief Model the exact 12-byte native RawVec growth ABI and selected failures.
     * @param queue_address Fixture Vec header containing capacity, pointer and count.
     * @return None; ordinary mode relocates the allocation and preserves its initialized prefix.
     */
    static void __fastcall grow_queue(void* queue_address);

    /** @brief Copy one trivially copyable value from owned fixture bytes.
     * @tparam value_type Scalar or record type.
     * @param address Fixture base; @param offset Byte offset.
     * @return Copied value; callers guarantee the fixture storage is accessible.
     */
    template <typename value_type>
    static value_type read_value(const void* address, std::size_t offset) noexcept {
        value_type value{};
        std::memcpy(&value, static_cast<const unsigned char*>(address) + offset, sizeof(value));
        return value;
    }

    /** @brief Copy one trivially copyable value into owned fixture bytes.
     * @tparam value_type Scalar or record type.
     * @param address Fixture base; @param offset Byte offset; @param value Value to store.
     * @return None; callers guarantee the fixture storage is writable.
     */
    template <typename value_type>
    static void write_value(void* address, std::size_t offset, value_type value) noexcept {
        std::memcpy(static_cast<unsigned char*>(address) + offset, &value, sizeof(value));
    }

    inline static int failures_{};
    inline static std::array<void*, 128> allocations_{};
    inline static std::array<unsigned char, 12 * 0xf0> zones_{};
    inline static std::array<unsigned char, 12 * 5 * 0x68> bosses_{};
    inline static bool enabled_{};
    inline static growth_mode growth_mode_{};
    inline static std::size_t original_calls_{};
    inline static std::size_t grow_calls_{};
    inline static std::size_t message_calls_{};
    inline static void* observed_run_{};
    inline static const void* observed_boss_{};
};

} // namespace azs
