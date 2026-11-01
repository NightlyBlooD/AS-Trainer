#pragma once

#include "god_mode_bindings.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace azs {

class god_mode_bindings_tests final {
public:
    /** @brief Exercise adaptive God binding certificates against test-owned synthetic PE bytes.
     * @return Zero when relocation, linked-function and atomic-refusal regressions pass, one otherwise.
     * No game code, executable, process, injected DLL or real native function is used.
     */
    static int run();

private:
    enum class certificate_fault { none, missing_dispatcher_call, invalid_operand, duplicate_family };

    struct fixture {
        std::vector<std::uint8_t> bytes;
        std::uint32_t text_rva{};
        std::uint32_t dispatcher_offset{};
        std::uint32_t helper_offset{};
        std::uint32_t constant_offset{};
        bool imported_helper{};
    };

    /** @brief Record a required property while allowing independent fixture cases to continue.
     * @param condition Expected true result; @param description Human-readable failure context.
     * @return None; increments the suite's failure count when condition is false.
     */
    static void check(bool condition, const char* description);

    /** @brief Write one unsigned little-endian value within a bounded test-owned PE allocation.
     * @tparam value_type Unsigned integer representation; @param bytes Owned file-layout bytes.
     * @param offset Destination byte offset; @param value Exact bits to encode.
     * @return None; records a failed assertion instead of accessing an invalid range.
     */
    template <typename value_type>
    static void put_unsigned(std::vector<std::uint8_t>& bytes, std::size_t offset, value_type value) {
        if (offset > bytes.size() || sizeof(value_type) > bytes.size() - offset) {
            check(false, "fixture scalar remains inside the owned PE bytes");
            return;
        }
        const auto bits = static_cast<std::uint64_t>(value);
        for (std::size_t index = 0; index < sizeof(value_type); ++index)
            bytes[offset + index] = static_cast<std::uint8_t>(bits >> (index * 8));
    }

    /** @brief Build two small unwind-covered functions, linked calls and immutable RIP constants.
     * @param text_rva Independently relocatable first section RVA.
     * @param image_base Independently relocatable preferred image base.
     * @param dispatcher_offset Location of the linked dispatcher relative to text_rva.
     * @param helper_offset Location of an external helper relative to text_rva.
     * @param constant_offset Additional displacement within the read-only constant section.
     * @param imported_helper True creates an FF25 helper thunk and an owned named import table.
     * @return Owned PE64 fixture; its function bytes are invented test data rather than game code.
     */
    static fixture make_fixture(std::uint32_t text_rva = 0x1000,
        std::uint64_t image_base = 0x140000000, std::uint32_t dispatcher_offset = 0x80,
        std::uint32_t helper_offset = 0x180, std::uint32_t constant_offset = 0,
        bool imported_helper = false);

    /** @brief Resolve one fixture against a trusted toy certificate constructed from separate owned bytes.
     * @param input Candidate PE to inspect; @param trusted Original unmodified certificate source.
     * @param output Candidate replaced only on complete acceptance; @param error Failure diagnostic.
     * @param fault Optional deliberately malformed trusted metadata for refusal regression checks.
     * @return True for a coherent linked certificate, false without replacing output.
     * The injected toy certificate is accessible only through this friend test suite and cannot
     * authorize runtime images through the production embedded-catalog entry point.
     */
    static bool resolve_fixture(const fixture& input, const fixture& trusted,
        god_mode_bindings::candidate& output, std::string& error,
        certificate_fault fault = certificate_fault::none);

    /** @brief Compare every published scalar and verification byte of two binding candidates.
     * @param left First candidate; @param right Second candidate.
     * @return True only when all fields and ordered proof chunks remain equal.
     */
    static bool same_candidate(const god_mode_bindings::candidate& left,
        const god_mode_bindings::candidate& right);

    /** @brief Assert a malformed image is refused atomically using the trusted original certificate.
     * @param input Mutated owned image; @param trusted Original unmodified certificate source.
     * @param previous Prior successful candidate; @param description Required refusal behavior.
     * @return None; checks both a nonempty diagnostic and unchanged prior output.
     */
    static void reject(const fixture& input, const fixture& trusted,
        const god_mode_bindings::candidate& previous, const char* description);

    /** @brief Verify unknown file identities and independent code/data relocations retain admission.
     * @return None; also proves invented toy bytes never pass the production certificate catalog.
     */
    static void test_relocation();

    /** @brief Refuse missing/ambiguous entries and changes to fixed code, scalar values or linked calls.
     * @return None; verifies a prior successful candidate remains unchanged after every refusal.
     */
    static void test_refusal();

    /** @brief Refuse incomplete reference metadata, out-of-body operands and competing valid families.
     * @return None; tests certificate coherence in addition to malformed candidate image bytes.
     */
    static void test_metadata();

    /** @brief Verify imported helper identities and bounded forwarding rather than generic thunk prefixes.
     * @return None; accepts relocated named imports and refuses renamed, missing or cyclic targets.
     */
    static void test_imports();

    static inline int failures_{};
};

} // namespace azs
