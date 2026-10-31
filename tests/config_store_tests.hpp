#pragma once

#include "trainer_state.hpp"

#include <string>

namespace azs {

class config_store_tests final {
public:
    /** @brief Verify strict preset parsing and atomic local file save/load behavior.
     * @return Zero when all focused checks pass, one otherwise.
     * Creates only a uniquely named test preset beside this test executable and removes
     * that owned file afterwards; never creates or loads default.ini or touches a game process.
     */
    static int run();

private:
    /** @brief Record one independent regression assertion.
     * @param condition Required true condition; @param description Failure context.
     * @return None; increments failures_ only when false.
     */
    static void check(bool condition, const char* description);

    /** @brief Build a representative finite preset using stable keys from the real descriptors.
     * @return Fields in deliberately noncanonical order plus bounded automation priorities.
     */
    static trainer_preset sample_preset();

    /** @brief Test malformed text without allowing output or global settings mutation.
     * @param text Invalid INI input; @param description Failure context for the assertion.
     * @return None; checks parse failure, output preservation and a nonempty diagnostic.
     */
    static void reject(const std::string& text, const char* description);

    inline static int failures_{};
};

} // namespace azs
