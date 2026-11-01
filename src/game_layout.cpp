#include "game_layout.hpp"

#include <iterator>
#include <algorithm>

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

constexpr auto run_keys = [] {
    std::array<std::size_t, std::size(run_offsets)> result{};
    for (std::size_t index = 0; index < result.size(); ++index) result[index] = run_offsets[index][0];
    return result;
}();
constexpr auto member_keys = [] {
    std::array<std::size_t, std::size(member_offsets)> result{};
    for (std::size_t index = 0; index < result.size(); ++index) result[index] = member_offsets[index][0];
    return result;
}();

} // namespace

const game_layout::profile game_layout::legacy_profile_{layout_id::legacy, 0xa38, 0xdac, 0xda4};
const game_layout::profile game_layout::current_profile_{layout_id::current, 0xad0, 0xecc, 0xec4};
std::atomic<const game_layout::profile*> game_layout::active_{&legacy_profile_};
game_layout::profile game_layout::adaptive_profile_{layout_id::adaptive, 0, 0, 0};
std::array<layout_offset, 128> game_layout::adaptive_run_offsets_{};
std::array<layout_offset, 16> game_layout::adaptive_member_offsets_{};
std::atomic<std::uint64_t> game_layout::generation_{};

bool game_layout::select(layout_id id) noexcept {
    if (id == layout_id::legacy) {
        active_.store(&legacy_profile_, std::memory_order_release);
        generation_.fetch_add(1, std::memory_order_release);
        return true;
    }
    if (id == layout_id::current) {
        active_.store(&current_profile_, std::memory_order_release);
        generation_.fetch_add(1, std::memory_order_release);
        return true;
    }
    return false;
}

std::span<const std::size_t> game_layout::required_run_offsets() noexcept { return run_keys; }
std::span<const std::size_t> game_layout::required_member_offsets() noexcept { return member_keys; }
std::uint64_t game_layout::generation() noexcept { return generation_.load(std::memory_order_acquire); }

std::size_t game_layout::run_field_width(std::size_t key) noexcept {
    switch (key) {
    case 0: case 0x2ca: case 0x354: case 0xd9e: case 0xda1: case 0xda3: case 0xda4: return 1;
    case 0x2d0: case 0x2d8: case 0x378: case 0x380: case 0x4b0:
    case 0x670: case 0x678: case 0x680: case 0x688: case 0x690: case 0x698:
    case 0x6a0: case 0x6a8: case 0x6b0: case 0x6d0: case 0x6d8: case 0x6e0:
    case 0xa68: case 0xa70: case 0xa78: case 0xc98: return 8;
    case 0xb28: case 0xb40: return 24;
    case 0xc78: return 32;
    default: return 4;
    }
}

bool game_layout::validate_resolved(std::span<const layout_offset> runs,
    std::span<const layout_offset> members, std::size_t stride) noexcept {
    if (runs.size() != run_keys.size() || members.size() != member_keys.size()
        || runs.size() > adaptive_run_offsets_.size() || members.size() > adaptive_member_offsets_.size()
        || stride < 0x100 || stride > 0x10000 || (stride & 7) != 0) return false;
    for (std::size_t index = 0; index < runs.size(); ++index) {
        const auto& item = runs[index];
        if (item.legacy_offset != run_keys[index] || item.resolved_offset >= 0x10000
            || ((item.legacy_offset == 0) != (item.resolved_offset == 0))) return false;
        const auto width = run_field_width(item.legacy_offset);
        const auto alignment = width == 1 ? 1u : width == 4 ? 4u : 8u;
        if ((item.resolved_offset & (alignment - 1)) != 0) return false;
        for (std::size_t previous = 0; previous < index; ++previous) {
            const auto previous_end = runs[previous].resolved_offset + run_field_width(runs[previous].legacy_offset);
            if (item.resolved_offset < previous_end && runs[previous].resolved_offset < item.resolved_offset + width)
                return false;
        }
    }
    for (std::size_t index = 0; index < members.size(); ++index) {
        const auto& item = members[index];
        if (item.legacy_offset != member_keys[index] || item.resolved_offset >= stride
            || ((item.legacy_offset == 0) != (item.resolved_offset == 0))) return false;
        const auto width = item.legacy_offset == 0 || item.legacy_offset == 0xa34 ? 1u
            : item.legacy_offset == 0x4b0 ? 8u : 4u;
        if (width > stride - item.resolved_offset
            || (item.legacy_offset != 0 && (item.resolved_offset & (width - 1)) != 0)) return false;
        for (std::size_t previous = 0; previous < index; ++previous) {
            const auto previous_width = members[previous].legacy_offset == 0 || members[previous].legacy_offset == 0xa34
                ? 1u : members[previous].legacy_offset == 0x4b0 ? 8u : 4u;
            if (item.resolved_offset < members[previous].resolved_offset + previous_width
                && members[previous].resolved_offset < item.resolved_offset + width) return false;
        }
    }
    const auto get = [runs](std::size_t key) noexcept {
        for (const auto& item : runs) if (item.legacy_offset == key) return item.resolved_offset;
        return invalid_offset;
    };
    const auto scalar_span = get(0xda3) + 1;
    const auto host_span = get(0xda8) + 4;
    if (host_span > 0x10000 || scalar_span > host_span || get(0xda4) >= host_span) return false;
    for (const auto& item : runs) {
        const auto span = item.legacy_offset == 0xda4 || item.legacy_offset == 0xda8 ? host_span : scalar_span;
        if (item.resolved_offset >= span || run_field_width(item.legacy_offset) > span - item.resolved_offset)
            return false;
    }
    constexpr std::size_t vectors[]{0x2d0, 0x378, 0x670, 0x688, 0x6a0, 0x6d0, 0xa68};
    for (const auto key : vectors) {
        if (get(key) + 8 != get(key + 8)) return false;
        if (key != 0x2d0 && key != 0x378 && get(key) + 16 != get(key + 16)) return false;
    }
    return true;
}

bool game_layout::select_resolved(std::span<const layout_offset> runs,
    std::span<const layout_offset> members, std::size_t stride) noexcept {
    if (!validate_resolved(runs, members, stride)) return false;
    const auto get = [runs](std::size_t key) noexcept {
        for (const auto& item : runs) if (item.legacy_offset == key) return item.resolved_offset;
        return invalid_offset;
    };
    std::copy(runs.begin(), runs.end(), adaptive_run_offsets_.begin());
    std::copy(members.begin(), members.end(), adaptive_member_offsets_.begin());
    adaptive_profile_ = {layout_id::adaptive, stride, get(0xda8) + 4, get(0xda3) + 1,
        adaptive_run_offsets_.data(), runs.size(), adaptive_member_offsets_.data(), members.size()};
    active_.store(&adaptive_profile_, std::memory_order_release);
    generation_.fetch_add(1, std::memory_order_release);
    return true;
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
    const auto* selected = active_.load(std::memory_order_acquire);
    if (selected->id == layout_id::legacy) return legacy_offset;
    if (selected->id == layout_id::current) return lookup(legacy_offset, run_offsets, std::size(run_offsets));
    std::size_t first = 0, last = selected->run_count;
    while (first < last) {
        const auto middle = first + (last - first) / 2;
        const auto& item = selected->run_offsets[middle];
        if (item.legacy_offset == legacy_offset) return item.resolved_offset;
        if (legacy_offset < item.legacy_offset) last = middle; else first = middle + 1;
    }
    return invalid_offset;
}

std::size_t game_layout::member_offset(std::size_t legacy_offset) noexcept {
    const auto* selected = active_.load(std::memory_order_acquire);
    if (selected->id == layout_id::legacy) return legacy_offset;
    if (selected->id == layout_id::current) return lookup(legacy_offset, member_offsets, std::size(member_offsets));
    for (std::size_t index = 0; index < selected->member_count; ++index)
        if (selected->member_offsets[index].legacy_offset == legacy_offset)
            return selected->member_offsets[index].resolved_offset;
    return invalid_offset;
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
