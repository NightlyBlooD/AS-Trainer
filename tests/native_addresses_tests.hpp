#pragma once

#include "native_addresses.hpp"
#include "signature_profile.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace azs {

class native_addresses_tests final {
public:
    /** @brief Exercise offline adaptive certificates using owned synthetic PE bytes only.
     * @return Zero after every regression passes, otherwise one.
     * No game executable is loaded, mapped, executed, injected into or modified.
     */
    static int run() noexcept;

private:
    struct fixture {
        std::vector<std::uint8_t> bytes;
        signature_profile profile;
        std::uint32_t text_rva{};
        std::uint32_t data_rva{};
        std::array<std::uint32_t, static_cast<std::size_t>(native_symbol::count)> symbols{};
    };

    /** @brief Record a regression result while allowing independent checks to continue.
     * @param condition Required invariant; @param description Failure explanation.
     * @return None; increments the failure count when condition is false.
     */
    static void check(bool condition, const char* description) noexcept;

    /** @brief Encode one unsigned little-endian scalar inside an owned fixture.
     * @tparam value_type Unsigned scalar type; @param bytes Owned file-layout PE bytes.
     * @param offset Bounded file offset; @param value Scalar to encode.
     * @return None; records failure instead of writing outside the fixture.
     */
    template<class value_type>
    static void put_unsigned(std::vector<std::uint8_t>& bytes, std::size_t offset, value_type value) {
        if (offset > bytes.size() || sizeof(value_type) > bytes.size() - offset) {
            check(false, "fixture scalar remains inside owned bytes");
            return;
        }
        const auto bits = static_cast<std::uint64_t>(value);
        for (std::size_t index = 0; index < sizeof(value_type); ++index)
            bytes[offset + index] = static_cast<std::uint8_t>(bits >> (index * 8));
    }

    /** @brief Render exact bytes and an optional four-byte wildcard displacement as an IDA pattern.
     * @param bytes Bounded immutable sequence; @param wildcard_offset First wildcard byte or size_t(-1).
     * @return Space-separated hexadecimal pattern without changing input bytes.
     */
    static std::string pattern_text(std::span<const std::uint8_t> bytes,
        std::size_t wildcard_offset = static_cast<std::size_t>(-1));

    /** @brief Build a complete synthetic adaptive catalog and a matching AMD64 PE64 fixture.
     * @param text_rva Relocatable first section RVA; @param image_base Preferred PE image base.
     * @param float_setter True selects the separately certified float setter, otherwise raw bits.
     * @return Owned PE bytes, trusted toy rules, and independently expected native targets.
     * Required canonical maps are complete; damage alone moves nonuniformly to offset 0xd80.
     */
    static fixture make_fixture(std::uint32_t text_rva, std::uint64_t image_base, bool float_setter);

    /** @brief Compare complete certificates, including code verification bytes and typed ABI.
     * @param left First certificate; @param right Second certificate.
     * @return True only when all observable candidate fields are equal.
     */
    static bool same_candidate(const native_addresses::candidate& left,
        const native_addresses::candidate& right) noexcept;

    /** @brief Require a malformed catalog/image to fail without replacing a previous certificate.
     * @param input Owned fixture and mutated profile; @param previous Complete valid certificate.
     * @param description Failure context; @param error_fragment Optional diagnostic required for a specific gate.
     * @return None; also checks input bytes and global layout.
     */
    static void reject(const fixture& input, const native_addresses::candidate& previous,
        const char* description, std::string_view error_fragment = {});

    /** @brief Prove that unknown hashes and relocated functions can resolve a complete trusted catalog.
     * @return None; validates every address, complete field maps, separate setter modes and no publication.
     */
    static void test_complete_relocation();

    /** @brief Reject missing maps, absent native symbols, duplicate matches and nonentry targets atomically.
     * @return None; each failure begins with an existing complete candidate.
     */
    static void test_incomplete_and_ambiguous();

    /** @brief Refuse a native code target in read-only data and a falsely named setter ABI guard.
     * @return None; distinguishes trusted catalog resolution from permissive pattern discovery.
     */
    static void test_section_and_setter_contract();

    /** @brief Ensure the automatic runtime catalog path rejects unrelated toy bytes without stale output.
     * @return None; external toy catalogs do not become embedded runtime admission authorities.
     */
    static void test_automatic_refusal();

    inline static int failures_{};
};

} // namespace azs
