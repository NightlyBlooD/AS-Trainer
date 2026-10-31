#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace azs {

class pattern_scanner;

enum class native_symbol : std::size_t {
    owner_frame, recalculate, create_speed_packet, grow_speed_packets,
    swap_player, flush_updates, route_queues, set_object_field,
    apply_choice, boss_loot, grow_loot, luck_reader, area_reader, pickup_reader,
    zone_table, count
};

class native_addresses final {
public:
    /** @brief Resolve a known build's native symbols before installing any trainer hook.
     * @param module_base Actual base of the running main module.
     * @param executable_path On-disk image whose hash was independently computed by the caller.
     * @param digest Exact 32-byte supported executable digest; unknown builds are refused.
     * @param error Output diagnostic for an unsupported hash, ambiguous signature or live mismatch.
     * @return True after selecting a verified immutable layout and all required addresses.
     * Current builds use embedded signatures; the legacy build retains its proven RVA profile.
     * An external scanner profile never grants runtime admission to an unknown game build.
     */
    static bool initialize(std::uintptr_t module_base, const std::filesystem::path& executable_path,
        const std::array<unsigned char, 32>& digest, std::string& error) noexcept;

    /** @brief Obtain one selected native image-relative address without scanning during gameplay.
     * @param symbol Required native helper, hook, reader instruction or immutable zone table.
     * @return Verified RVA; before initialization the original legacy profile is returned.
     */
    static std::uint32_t rva(native_symbol symbol) noexcept;

    /** @brief Report whether the selected build was resolved using the signature catalog.
     * @return True only after complete current-build signature and live-image verification.
     */
    static bool uses_patterns() noexcept;

    /** @brief Recheck every current-build code anchor before the first hooks are installed.
     * @return True when all live bytes still equal the verified file's anchors.
     * Legacy callers retain their existing exact prologue checks instead.
     */
    static bool verify_all() noexcept;

private:
    struct verification_chunk {
        std::uint32_t rva{};
        std::vector<std::uint8_t> bytes;
    };

    /** @brief Format a fixed SHA-256 as lowercase ASCII for profile matching.
     * @param digest Input digest from the file-hash guard; @return Exact 64-character key.
     */
    static std::string hash_text(const std::array<unsigned char, 32>& digest);

    /** @brief Resolve a stable catalog rule name to the required native symbol enum.
     * @param name ASCII signature-rule name; @param symbol Output enum on success.
     * @return True for a known native symbol; false for field-only or unknown rules.
     */
    static bool symbol_from_name(std::string_view name, native_symbol& symbol) noexcept;

    /** @brief Count bytes in an already validated whitespace-separated IDA pattern.
     * @param pattern Validated signature text; @return Number of fixed/wildcard byte tokens.
     */
    static std::size_t pattern_size(std::string_view pattern) noexcept;

    /** @brief Check that the selected offline file matches the running PE header identity.
     * @param module_base Actual main module; @param scanner Validated offline image.
     * @return True for matching AMD64 PE32+ image size and linker timestamp.
     */
    static bool matching_image(std::uintptr_t module_base, const pattern_scanner& scanner) noexcept;

    /** @brief Check committed image code and compare an anchor without changing page protection.
     * @param base Main image base; @param rva Code RVA; @param bytes Verified file bytes.
     * @param image_size Declared main image span.
     * @return True only for an in-range readable executable image with identical bytes.
     */
    static bool matching_live(std::uintptr_t base, std::uint32_t rva,
        std::span<const std::uint8_t> bytes, std::uint32_t image_size) noexcept;

    inline static std::array<std::uint32_t, static_cast<std::size_t>(native_symbol::count)> resolved_{};
    inline static std::vector<verification_chunk> verification_;
    inline static std::uintptr_t module_base_{};
    inline static std::uint32_t image_size_{};
    inline static bool patterns_{};
};

} // namespace azs
