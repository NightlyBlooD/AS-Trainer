#pragma once

#include <cstddef>

namespace azs {

class trainer_test_suite final {
public:
    /** @brief Exercise scalar commands, freezes, object replacement and memory guards.
     * @return Zero when all assertions pass; one after printing failed conditions.
     */
    static int run();

private:
    /** @brief Initialize a synthetic run object using only verified numeric fields.
     * @param memory A writable allocation of at least 0x2000 bytes.
     * @return None; sets class, level, phase, and bounded baseline field values.
     */
    static void initialize_run(void* memory);

    /** @brief Record an assertion and print its purpose if it fails.
     * @param condition Result to check; @param description Name of the required behavior.
     * @return None; increments an internal failure count on failure.
     */
    static void check(bool condition, const char* description);

    /** @brief Find the descriptor for a particular verified byte offset.
     * @param offset Field's offset relative to the synthetic run allocation.
     * @return Descriptor index, or field_count() if missing.
     */
    static std::size_t find_field(std::size_t offset);

    /** @brief Verify independent party acknowledgements against three owned fake runs.
     * @param host_run Writable 0x2000-byte authoritative host fixture.
     * @param first_member First writable 0x2000-byte member fixture with its own GUID.
     * @param second_member Separate writable member fixture with a different GUID.
     * @return None; records failures for one-shot/freeze isolation, phase guards,
     * reset/new-run lifecycle, data-only speed rollback and unchanged host snapshots.
     */
    static void test_members(void* host_run, void* first_member, void* second_member);

    static inline int failures_{};
};

} // namespace azs
