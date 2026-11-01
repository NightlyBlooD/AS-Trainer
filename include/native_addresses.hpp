#pragma once

#include "game_layout.hpp"

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
struct pattern_match;
struct signature_profile;
struct signature_rule;

enum class native_symbol : std::size_t {
    owner_frame, recalculate, create_speed_packet, grow_speed_packets,
    swap_player, flush_updates, route_queues, set_object_field,
    apply_choice, boss_loot, grow_loot, luck_reader, area_reader, pickup_reader,
    zone_table, lifesteal, count
};

class native_addresses final {
public:
    enum class object_field_abi { bits, float_value };

    struct verification_chunk {
        std::uint32_t rva{};
        std::vector<std::uint8_t> bytes;
    };

    struct candidate {
        std::array<std::uint32_t, static_cast<std::size_t>(native_symbol::count)> symbols{};
        std::vector<layout_offset> run_offsets;
        std::vector<layout_offset> member_offsets;
        std::size_t participant_stride{};
        std::uint32_t image_size{};
        std::string image_sha256;
        layout_id layout{layout_id::legacy};
        object_field_abi setter_abi{object_field_abi::bits};
        std::vector<verification_chunk> verification;
    };

    /** @brief Resolve and validate one complete trusted catalog against immutable offline PE bytes.
     * @param scanner Already validated AMD64 file-layout image; no process is read or executed.
     * @param profile Parsed embedded catalog with layout current or adaptive. Current requires its
     * exact admitted digest; adaptive requires every native symbol, canonical run/member key,
     * participant stride, fixed ABI guard and exactly one typed object-field ABI guard.
     * @param result Complete candidate, replaced only after every contract passes.
     * @param error Output diagnostic, cleared on success.
     * @return True for a complete offline certificate; false preserves result and global bindings.
     * Unknown builds never inherit static offsets. Intrinsic object tags at offset zero are the
     * only mappings that need no instruction. Callable targets require unwind entry proof, or a
     * certified direct E8 call target for the explicitly supported leaf helpers. This does not
     * grant a live hook: initialize additionally verifies the loaded image and all code bytes.
     */
    static bool resolve_file(const pattern_scanner& scanner, const signature_profile& profile,
        candidate& result, std::string& error) noexcept;

    /** @brief Select the embedded exact-build or adaptive catalog and produce its offline proof.
     * @param scanner Validated immutable offline PE image.
     * @param result Complete candidate, unchanged after any failure.
     * @param error Output failure reason or an empty string after success.
     * @return True for the exact legacy/current builds or one fully certified adaptive binding.
     * Hashes identify files and preserve existing exact-build behavior; an unknown hash can pass
     * only the embedded adaptive catalog's complete layout/ABI proof. External profiles do not
     * become runtime admission authorities through this automatic path.
     */
    static bool resolve_auto(const pattern_scanner& scanner, candidate& result,
        std::string& error) noexcept;

    /** @brief Resolve a known build's native symbols before installing any trainer hook.
     * @param module_base Actual base of the running main module.
     * @param executable_path On-disk image whose hash was independently computed by the caller.
     * @param digest Exact 32-byte digest independently read from the process's executable file.
     * @param error Output diagnostic for an unsupported hash, ambiguous signature or live mismatch.
     * @return True after selecting a verified immutable layout and all required addresses.
     * Exact builds retain their existing paths. An unknown build needs a complete embedded
     * adaptive certificate, matching file identity, intact live code and a coherent typed layout.
     */
    static bool initialize(std::uintptr_t module_base, const std::filesystem::path& executable_path,
        const std::array<unsigned char, 32>& digest, std::string& error) noexcept;

    /** @brief Obtain one selected native image-relative address without scanning during gameplay.
     * @param symbol Required native helper, hook, reader instruction or immutable zone table.
     * @return Verified RVA; before initialization the original legacy profile is returned.
     */
    static std::uint32_t rva(native_symbol symbol) noexcept;

    /** @brief Obtain the stable catalog name used for one native binding and offline reports.
     * @param symbol Native helper, hook, reader instruction or table identifier.
     * @return Canonical immutable ASCII name, or an empty view for an invalid enum.
     */
    static std::string_view symbol_name(native_symbol symbol) noexcept;

    /** @brief Report whether the selected build was resolved using the signature catalog.
     * @return True only after complete current-build signature and live-image verification.
     */
    static bool uses_patterns() noexcept;

    /** @brief Distinguish the certified modern binding family from the exact legacy ABI.
     * @return True after complete current/adaptive publication, false for legacy/uninitialized.
     * This does not choose the object-field calling convention; use object_field_uses_float().
     */
    static bool uses_current_abi() noexcept;

    /** @brief Select the separately certified native object-field value calling convention.
     * @return True for a float argument in XMM2; false for raw value bits in R8D.
     * The choice comes from the exact current build or an explicit linked adaptive mode guard,
     * never from signature usage, layout identity or a guessed version relationship.
     */
    static bool object_field_uses_float() noexcept;

    /** @brief Check whether a verified native float-return lifesteal getter is available.
     * @return True only after publication of a nonzero getter binding; legacy stays unsupported.
     */
    static bool supports_lifesteal() noexcept;

    /** @brief Recheck every current-build code anchor before the first hooks are installed.
     * @return True when all live bytes still equal the verified file's anchors.
     * Legacy callers retain their existing exact prologue checks instead.
     */
    static bool verify_all() noexcept;

private:
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

    /** @brief Verify that a bounded offline range belongs to one section with the required access.
     * @param scanner Validated immutable PE; @param rva First byte; @param size Complete range.
     * @param executable True requires readable executable code; false requires read-only data.
     * @return True for one fully file-backed section with appropriate PE memory attributes.
     */
    static bool section_range(const pattern_scanner& scanner, std::uint32_t rva,
        std::size_t size, bool executable) noexcept;

    /** @brief Prove that a resolved code address is its unwind-described function entry.
     * @param scanner Validated image; @param rva Required entry address.
     * @return True when bounded unwind-chain resolution returns exactly rva.
     */
    static bool function_entry(const pattern_scanner& scanner, std::uint32_t rva) noexcept;

    /** @brief Prove a direct near-call entry from a trusted instruction anchor.
     * @param scanner Validated image; @param rule RIP-rel32 rule; @param match Matched anchor RVA.
     * @param target Resolved expected function entry.
     * @return True for an E8 instruction ending at instruction_size inside an unwind-described
     * caller, with its signed rel32 pointing exactly to target. No code is executed.
     */
    static bool direct_call_entry(const pattern_scanner& scanner, const signature_rule& rule,
        const pattern_match& match, std::uint32_t target) noexcept;

    /** @brief Decode a stable hexadecimal canonical-offset rule name.
     * @param name Catalog rule name; @param prefix Required run_ or member_ prefix.
     * @param offset Output canonical offset, unchanged when the suffix is malformed.
     * @return True for a complete bounded hexadecimal suffix, optionally beginning with 0x.
     */
    static bool offset_key(std::string_view name, std::string_view prefix,
        std::size_t& offset) noexcept;

    /** @brief Validate one live read-only image data target without comparing relocated pointers.
     * @param base Actual main-module base; @param rva Required data range; @param size Byte count.
     * @param image_size Declared image size.
     * @return True only for committed readable nonwritable nonexecutable image pages.
     */
    static bool matching_live_data(std::uintptr_t base, std::uint32_t rva,
        std::size_t size, std::uint32_t image_size) noexcept;

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
    inline static bool current_abi_{};
    inline static object_field_abi setter_abi_{object_field_abi::bits};
    inline static bool initialized_{};
};

} // namespace azs
