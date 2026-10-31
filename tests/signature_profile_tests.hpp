#pragma once

#include "signature_profile.hpp"

#include <string>
#include <string_view>

namespace azs {

class signature_profile_tests final {
public:
    /** @brief Verify strict atomic profile parsing, owned file loading and standard SHA-256 vectors.
     * @return Zero on complete success, one on any regression.
     * No game, executable code, native hook, network or existing user profile is modified.
     */
    static int run();

private:
    /** @brief Record an assertion while allowing independent checks to continue.
     * @param condition Expected true result; @param description Failure context.
     * @return None; increments the suite failure count on false.
     */
    static void check(bool condition, const char* description);

    /** @brief Create a complete profile document around caller-supplied symbol sections.
     * @param rules Symbol sections, or empty to use one minimal match rule.
     * @return UTF-8 INI with a deliberately uppercase hash and an analysis layout identifier.
     */
    static std::string profile_text(std::string_view rules = {});

    /** @brief Compare every public field without adding equality operators to production structs.
     * @param left First candidate; @param right Expected profile.
     * @return True only when all metadata, rules, modes, offsets and addends match exactly.
     */
    static bool same_profile(const signature_profile& left, const signature_profile& right);

    /** @brief Verify malformed text cannot partially replace an existing valid profile.
     * @param text Invalid bounded document; @param description Failure context.
     * @return None; requires false, a diagnostic and unchanged metadata/rules after parsing.
     */
    static void reject(std::string_view text, const char* description);

    /** @brief Cover every resolution mode, numeric boundary, Unicode metadata and safe default.
     * @return None; verifies uppercase hash normalization and non-runtime analysis layouts.
     */
    static void test_valid_profiles();

    /** @brief Cover unknown/duplicate content, invalid Unicode, numeric overflow and resource limits.
     * @return None; every rejection begins with a nonempty prior output to prove atomicity.
     */
    static void test_rejections();

    /** @brief Read and reject only uniquely owned INI files in the test working directory.
     * @return None; valid, malformed and missing file cases preserve their documented output semantics.
     */
    static void test_file_load();

    /** @brief Check BCrypt output against standard empty-input and abc SHA-256 test vectors.
     * @return None; verifies exact lowercase 64-character output without reading a game executable.
     */
    static void test_hashes();

    inline static int failures_{};
};

} // namespace azs
