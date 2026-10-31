#include "game_layout.hpp"

#include <iterator>

namespace azs {
namespace {

constexpr std::size_t run_offsets[][2] = {
    {0x0, 0x0},
    {0x2ca, 0x2d2},
    {0x2d0, 0x2d8},
    {0x2d8, 0x2e0},
    {0x354, 0x35c},
    {0x378, 0x380},
    {0x380, 0x388},
    {0x4b0, 0x4b8},
    {0x4c8, 0x4d0},
    {0x4cc, 0x4d4},
    {0x4d0, 0x4d8},
    {0x4d4, 0x4dc},
    {0x4d8, 0x4e0},
    {0x4dc, 0x4e4},
    {0x4e0, 0x4e8},
    {0x4e4, 0x4ec},
    {0x4e8, 0x4f0},
    {0x4ec, 0x4f4},
    {0x4f0, 0x4f8},
    {0x4f4, 0x4fc},
    {0x4f8, 0x500},
    {0x4fc, 0x504},
    {0x500, 0x508},
    {0x504, 0x50c},
    {0x508, 0x510},
    {0x50c, 0x514},
    {0x510, 0x518},
    {0x530, 0x538},
    {0x538, 0x540},
    {0x544, 0x54c},
    {0x598, 0x5a0},
    {0x59c, 0x5a4},
    {0x5a4, 0x5ac},
    {0x5a8, 0x5b0},
    {0x670, 0x678},
    {0x678, 0x680},
    {0x680, 0x688},
    {0x688, 0x690},
    {0x690, 0x698},
    {0x698, 0x6a0},
    {0x6a0, 0x6a8},
    {0x6a8, 0x6b0},
    {0x6b0, 0x6b8},
    {0x6d0, 0x6d8},
    {0x6d8, 0x6e0},
    {0x6e0, 0x6e8},
    {0x83c, 0x8bc},
    {0xa68, 0xb00},
    {0xa70, 0xb08},
    {0xa78, 0xb10},
    {0xb28, 0xc08},
    {0xb40, 0xc20},
    {0xc78, 0xd98},
    {0xc98, 0xdb8},
    {0xce0, 0xe00},
    {0xd0c, 0xe2c},
    {0xd40, 0xe60},
    {0xd9e, 0xebe},
    {0xda1, 0xec1},
    {0xda3, 0xec3},
    {0xda4, 0xec4},
    {0xda8, 0xec8},
};

constexpr std::size_t member_offsets[][2] = {
    {0x0, 0x0},
    {0x4b0, 0x4b8},
    {0xa24, 0xabc},
    {0xa34, 0xacc},
};

} // namespace

const game_layout::profile game_layout::legacy_profile_{layout_id::legacy, 0xa38, 0xdac, 0xda4};
const game_layout::profile game_layout::current_profile_{layout_id::current, 0xad0, 0xecc, 0xec4};
std::atomic<const game_layout::profile*> game_layout::active_{&legacy_profile_};

bool game_layout::select(layout_id id) noexcept {
    if (id == layout_id::legacy) {
        active_.store(&legacy_profile_, std::memory_order_release);
        return true;
    }
    if (id == layout_id::current) {
        active_.store(&current_profile_, std::memory_order_release);
        return true;
    }
    return false;
}

layout_id game_layout::current_id() noexcept {
    return active_.load(std::memory_order_acquire)->id;
}

std::size_t game_layout::lookup(std::size_t legacy_offset, const std::size_t (*pairs)[2],
                                std::size_t count) noexcept {
    std::size_t first = 0;
    while (first < count) {
        const std::size_t middle = first + (count - first) / 2;
        if (pairs[middle][0] == legacy_offset) {
            return pairs[middle][1];
        }
        if (legacy_offset < pairs[middle][0]) {
            count = middle;
        } else {
            first = middle + 1;
        }
    }
    return invalid_offset;
}

std::size_t game_layout::run_offset(std::size_t legacy_offset) noexcept {
    return current_id() == layout_id::legacy ? legacy_offset
        : lookup(legacy_offset, run_offsets, std::size(run_offsets));
}

std::size_t game_layout::member_offset(std::size_t legacy_offset) noexcept {
    return current_id() == layout_id::legacy ? legacy_offset
        : lookup(legacy_offset, member_offsets, std::size(member_offsets));
}

std::size_t game_layout::participant_stride() noexcept {
    return active_.load(std::memory_order_acquire)->stride;
}

std::size_t game_layout::run_size() noexcept {
    return active_.load(std::memory_order_acquire)->host_span;
}

std::size_t game_layout::scalar_size() noexcept {
    return active_.load(std::memory_order_acquire)->scalar_span;
}

} // namespace azs
