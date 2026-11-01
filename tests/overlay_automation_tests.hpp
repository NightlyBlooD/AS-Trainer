#pragma once

namespace azs {

class overlay_automation_tests final {
public:
    /** @brief Verify field auto-application and independent option/priority publication.
     * @return Zero when all regression checks pass; one on any failure or exception.
     * Exercises the production overlay command paths without a game or graphics device.
     */
    static int run() noexcept;

private:
    /** @brief Record one required invariant of automation settings or draft preservation.
     * @param condition True when the observed state matches the required behavior.
     * @param description Human-readable diagnostic printed only for a failed invariant.
     * @return None; failures increment the suite's final exit-status counter.
     */
    static void check(bool condition, const char* description) noexcept;

    /** @brief Verify edited values and start toggles use the current accepted field bridge.
     * @return None; checks numeric/boolean commits, start reapplication, disable behavior
     * and invalid-draft preservation using queued state without native game memory.
     */
    static void check_field_edits();

    inline static int failures_{};
};

} // namespace azs
