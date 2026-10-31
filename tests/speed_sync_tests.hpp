#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace azs {

class speed_sync_tests final {
public:
    /** @brief Exercise real runtime/state speed and frame paths against owned fake run memory.
     * @return Zero when all focused regression checks pass, one otherwise.
     * No game process is opened, no hooks installed, and native calls use test stubs.
     */
    static int run();

private:
    enum class grow_mode { normal, corrupt_count, newer_command };

    /** @brief Record one focused assertion and preserve execution of later checks.
     * @param condition Expected true condition.
     * @param description Human-readable failure context.
     * @return None; increments the suite failure count only when false.
     */
    static void check(bool condition, const char* description);

    /** @brief Find an editable descriptor using its verified game-data offset.
     * @param offset Byte offset from the fake run base.
     * @return Descriptor index, or field_count() when no descriptor matches.
     */
    static std::size_t find_field(std::size_t offset);

    /** @brief Allocate and track writable fixture storage for later cleanup.
     * @param size Number of virtual bytes required by a run or packet buffer.
     * @return Owned writable memory, or null on allocation/tracking failure.
     */
    static void* allocate(std::size_t size);

    /** @brief Release a tracked fixture allocation, including a relocated old queue.
     * @param address Exact VirtualAlloc base previously returned by allocate().
     * @return None; ignores null and untracked addresses.
     */
    static void release(void* address);

    /** @brief Reset the fake run and bridge to an active, command-free baseline.
     * @param run_address Owned writable run storage of at least 0x2000 bytes.
     * @return None; installs test-only native stubs and publishes the initial snapshot.
     */
    static void prepare_run(void* run_address);

    /** @brief Set the fake native queue metadata without calling game code.
     * @param run_address Fixture base; @param capacity Allocated packet slots.
     * @param data Packet buffer or an intentionally malformed pointer.
     * @param count Number of initialized packets.
     * @return None; writes only the owned fixture's three queue words.
     */
    static void set_queue(void* run_address, std::size_t capacity, void* data, std::size_t count);

    /** @brief Exercise current-build offsets and both exact object-field setter ABIs.
     * @param run_address Owned writable 0x2000-byte run fixture.
     * @param packet_buffer Owned writable buffer containing at least four packet slots.
     * @param speed_field Stable speed descriptor index cached while legacy is selected.
     * @return None; records assertions, restores native stubs and selects legacy on exit.
     * Uses preallocated packet capacity and never calls the game or native allocator.
     */
    static void test_current_layout(void* run_address, void* packet_buffer,
        std::size_t speed_field);

    /** @brief Record the current-build float-typed native setter's exact argument bits.
     * @param map Opaque map identity forwarded by the adapter.
     * @param field_id Native uint16 field key; @param value Value passed in XMM2.
     * @return map unchanged; stores bit_cast<uint32_t>(value) without numeric conversion.
     */
    static void* __fastcall set_field_float(void* map, std::uint16_t field_id,
        float value) noexcept;

    /** @brief Record the legacy native setter's exact integer argument bits.
     * @param map Opaque map identity forwarded by the adapter.
     * @param field_id Native uint16 field key; @param value_bits Value passed in R8D.
     * @return map unchanged; records every bit of value_bits for ABI assertions.
     */
    static void* __fastcall set_field_bits(void* map, std::uint16_t field_id,
        std::uint32_t value_bits) noexcept;

    /** @brief Stub the exact packet-constructor ABI and record every input argument.
     * @param output Writable 32-byte packet result.
     * @param guid Player GUID; @param revision Old network revision.
     * @param world_speed Requested movement speed in world units.
     * @return output, matching the native constructor's returned pointer.
     */
    static void* __fastcall create_packet(void* output, std::uint64_t guid,
        std::uint32_t revision, float world_speed);

    /** @brief Replace the native owner update while exercising the real tick hook.
     * @param run_address Owned phase-1 fixture whose simulation clock is advanced.
     * @param delta_seconds Exact interval supplied by game_runtime::tick_hook.
     * @return None; records call count/total time and updates only fixture run+0xce0.
     */
    static void __fastcall owner_frame(void* run_address, float delta_seconds) noexcept;

    /** @brief Stub native 32-byte Vec growth, including allocation relocation.
     * @param queue_address Fixture Vec header with cap/data/len words.
     * @return None; selected fault modes change metadata or enqueue a newer command.
     */
    static void __fastcall grow_queue(void* queue_address);

    /** @brief Read a trivial scalar from test-owned memory without aliasing casts.
     * @tparam value_type Trivially copyable scalar/result type.
     * @param address Fixture base; @param offset Byte offset of the value.
     * @return Copied test value; callers guarantee the owned range is accessible.
     */
    template <typename value_type>
    static value_type read_value(const void* address, std::size_t offset) noexcept {
        value_type value{};
        std::memcpy(&value, static_cast<const unsigned char*>(address) + offset, sizeof(value));
        return value;
    }

    /** @brief Store a trivial scalar in test-owned memory without aliasing casts.
     * @tparam value_type Trivially copyable scalar/result type.
     * @param address Fixture base; @param offset Byte offset.
     * @param value Test value to store.
     * @return None; callers guarantee the fixture range is accessible.
     */
    template <typename value_type>
    static void write_value(void* address, std::size_t offset, value_type value) noexcept {
        std::memcpy(static_cast<unsigned char*>(address) + offset, &value, sizeof(value));
    }

    inline static int failures_{};
    inline static std::array<void*, 32> allocations_{};
    inline static grow_mode growth_mode_{};
    inline static bool raise_packet_exception_{};
    inline static std::size_t packet_calls_{};
    inline static std::size_t grow_calls_{};
    inline static std::uint64_t observed_guid_{};
    inline static std::uint32_t observed_revision_{};
    inline static float observed_world_speed_{};
    inline static std::size_t owner_calls_{};
    inline static double owner_total_seconds_{};
    inline static std::size_t float_setter_calls_{};
    inline static std::size_t bits_setter_calls_{};
    inline static void* observed_map_{};
    inline static std::uint16_t observed_field_id_{};
    inline static std::uint32_t observed_value_bits_{};
};

} // namespace azs
