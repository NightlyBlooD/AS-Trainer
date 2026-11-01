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

    /** @brief Exercise the computed lifesteal host/member lifecycle without native game code.
     * @param host_run Writable 0x2000-byte authoritative current-profile fixture.
     * @param other_run Separate writable allocation used to prove exact-context rejection.
     * @return None; checks owner-thread admission, cached publication, one-shot/rebuild,
     * Freeze/Start/reset, bounded GUID slots, profile support and unchanged fixture bytes.
     */
    static void test_lifesteal(void* host_run, void* other_run);

    /** @brief Probe a computed getter from a thread that does not own the admitted run.
     * @param run_address Exact otherwise valid owner fixture.
     * @param resolved Output whether the foreign thread obtained an override.
     * @return None; the spawning test joins this thread before reading the result.
     */
    static void lifesteal_foreign_thread(void* run_address, bool* resolved) noexcept;

    /** @brief Admit a sequential native update on a replacement owner thread and probe its getter.
     * @param run_address Exact current host fixture supplied by the verified update entry.
     * @param resolved Output whether admission and the computed getter both succeeded.
     * @return None; proves thread migration refreshes admission without adopting a new run.
     */
    static void lifesteal_admitted_thread(void* run_address, bool* resolved) noexcept;

    static inline int failures_{};
};

} // namespace azs
