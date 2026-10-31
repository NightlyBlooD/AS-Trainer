#include "simulation_clock.hpp"

#include <algorithm>
#include <cmath>

namespace azs {

frame_steps simulation_clock::split_frame(float delta_seconds, double scale) noexcept {
    if (!std::isfinite(delta_seconds) || delta_seconds <= 0) {
        return {1, 0};
    }
    if (!std::isfinite(scale) || scale < 1 || scale > 16) {
        scale = 1;
    }
    const auto count = static_cast<std::uint32_t>(std::ceil(scale));
    return {count, static_cast<float>(static_cast<double>(delta_seconds) * scale / count)};
}

} // namespace azs
