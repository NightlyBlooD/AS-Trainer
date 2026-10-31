#pragma once

#include <cstdint>

namespace azs {

struct frame_steps {
    std::uint32_t count{1};
    float seconds_per_step{};
};

class simulation_clock final {
public:
    /** @brief Split an accelerated owner frame into no more than sixteen native steps.
     * @param delta_seconds Original game update interval; invalid/negative input becomes zero.
     * @param scale Finite requested simulation multiplier from 1 to 16; invalid input uses 1.
     * @return Steps whose sum is delta*scale and each interval is no larger than delta.
     * A zero interval still produces one call so menus/choices continue to be processed.
     */
    static frame_steps split_frame(float delta_seconds, double scale) noexcept;
};

} // namespace azs
