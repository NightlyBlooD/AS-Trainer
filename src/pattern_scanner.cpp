#include "pattern_scanner.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>

namespace azs {

bool pattern_scanner::contains(std::size_t total, std::size_t offset, std::size_t size) noexcept {
    return size <= total && offset <= total - size;
}

void pattern_scanner::clear_image() noexcept {
    valid_ = false;
    image_base_ = 0;
    image_size_ = 0;
    header_size_ = 0;
    file_data_.clear();
    sections_.clear();
    functions_.clear();
}

bool pattern_scanner::fail(const char* message) const noexcept {
    error_ = message;
    return false;
}

bool pattern_scanner::load_file(const std::filesystem::path& path) noexcept {
    clear_image();
    error_ = "";
    try {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) return fail("Cannot open the offline PE file.");
        const auto end = input.tellg();
        if (end < 0 || static_cast<std::uint64_t>(end) > maximum_file_size)
            return fail("PE file size is invalid or exceeds 512 MiB.");
        const auto size = static_cast<std::size_t>(end);
        file_data_.resize(size);
        input.seekg(0);
        if (size != 0) input.read(reinterpret_cast<char*>(file_data_.data()), static_cast<std::streamsize>(size));
        if (!input || !parse_image()) {
            if (!input) fail("Cannot read the complete offline PE file.");
            clear_image();
            return false;
        }
        return true;
    } catch (...) {
        clear_image();
        return fail("Cannot allocate or parse the offline PE image.");
    }
}

bool pattern_scanner::load_bytes(std::span<const std::uint8_t> data) noexcept {
    error_ = "";
    if (data.size() > maximum_file_size) {
        clear_image();
        return fail("PE byte view exceeds 512 MiB.");
    }
    try {
        std::vector<std::uint8_t> copy(data.begin(), data.end());
        clear_image();
        file_data_ = std::move(copy);
        if (!parse_image()) {
            clear_image();
            return false;
        }
        return true;
    } catch (...) {
        clear_image();
        return fail("Cannot allocate or parse the offline PE byte view.");
    }
}

bool pattern_scanner::parse_image() {
    const std::span<const std::uint8_t> data(file_data_);
    std::uint16_t dos{}, machine{}, section_count{}, optional_size{}, magic{};
    std::uint32_t pe_offset{}, signature{}, directories{};
    if (!read_unsigned(data, 0, dos) || dos != 0x5a4d || !read_unsigned(data, 0x3c, pe_offset) ||
        pe_offset < 0x40 || !contains(data.size(), pe_offset, 24) ||
        !read_unsigned(data, pe_offset, signature) || signature != 0x00004550)
        return fail("Invalid or truncated DOS/PE signature.");
    read_unsigned(data, pe_offset + 4, machine);
    read_unsigned(data, pe_offset + 6, section_count);
    read_unsigned(data, pe_offset + 20, optional_size);
    const auto optional_offset = static_cast<std::size_t>(pe_offset) + 24;
    if (machine != 0x8664 || section_count == 0 || section_count > 96 || optional_size < 112 ||
        !contains(data.size(), optional_offset, optional_size) ||
        !read_unsigned(data, optional_offset, magic) || magic != 0x20b)
        return fail("Only bounded AMD64 PE32+ headers are supported.");
    read_unsigned(data, optional_offset + 24, image_base_);
    read_unsigned(data, optional_offset + 56, image_size_);
    read_unsigned(data, optional_offset + 60, header_size_);
    read_unsigned(data, optional_offset + 108, directories);
    const auto section_offset = optional_offset + optional_size;
    const auto section_bytes = static_cast<std::size_t>(section_count) * 40;
    if (image_size_ == 0 || header_size_ == 0 || header_size_ > data.size() || header_size_ > image_size_ ||
        image_base_ > std::numeric_limits<std::uint64_t>::max() - (image_size_ - 1) ||
        directories > (optional_size - 112u) / 8u ||
        !contains(data.size(), section_offset, section_bytes) ||
        !contains(header_size_, section_offset, section_bytes))
        return fail("PE optional header, image range or section table exceeds its bounds.");

    sections_.reserve(section_count);
    for (std::size_t index = 0; index < section_count; ++index) {
        const auto offset = section_offset + index * 40;
        pe_section section;
        std::size_t name_length = 0;
        while (name_length < 8 && data[offset + name_length] != 0) ++name_length;
        section.name.assign(reinterpret_cast<const char*>(data.data() + offset), name_length);
        std::uint32_t raw_offset{};
        read_unsigned(data, offset + 8, section.virtual_size);
        read_unsigned(data, offset + 12, section.rva);
        read_unsigned(data, offset + 16, section.raw_size);
        read_unsigned(data, offset + 20, raw_offset);
        read_unsigned(data, offset + 36, section.characteristics);
        section.file_offset = raw_offset;
        const auto mapped_size = std::max(section.virtual_size, section.raw_size);
        if (section.rva < header_size_ || section.rva > image_size_ ||
            static_cast<std::uint64_t>(section.rva) + mapped_size > image_size_ ||
            (section.raw_size != 0 && (section.file_offset < header_size_ ||
             !contains(data.size(), section.file_offset, section.raw_size))))
            return fail("PE section range is truncated, overflowing or overlaps its headers.");
        for (const auto& previous : sections_) {
            const auto previous_size = std::max(previous.virtual_size, previous.raw_size);
            const bool virtual_overlap = mapped_size != 0 && previous_size != 0 &&
                static_cast<std::uint64_t>(section.rva) < static_cast<std::uint64_t>(previous.rva) + previous_size &&
                static_cast<std::uint64_t>(previous.rva) < static_cast<std::uint64_t>(section.rva) + mapped_size;
            const bool raw_overlap = section.raw_size != 0 && previous.raw_size != 0 &&
                section.file_offset < previous.file_offset + previous.raw_size &&
                previous.file_offset < section.file_offset + section.raw_size;
            if (virtual_overlap || raw_overlap) return fail("PE section mappings are ambiguous or overlapping.");
        }
        sections_.push_back(std::move(section));
    }

    valid_ = true;
    if (directories <= 3) return true;
    std::uint32_t exception_rva{}, exception_size{};
    read_unsigned(data, optional_offset + 112 + 3 * 8, exception_rva);
    read_unsigned(data, optional_offset + 116 + 3 * 8, exception_size);
    if (exception_rva == 0 && exception_size == 0) return true;
    std::size_t exception_offset{};
    if (exception_rva == 0 || exception_size == 0 || exception_size % 12 != 0 ||
        exception_size / 12 > 1024u * 1024u ||
        !rva_to_file_offset(exception_rva, exception_offset, exception_size))
        return fail("PE exception directory is truncated or has an invalid entry count.");
    functions_.reserve(exception_size / 12);
    bool padding_seen = false;
    for (std::size_t offset = 0; offset < exception_size; offset += 12) {
        runtime_function function;
        read_unsigned(data, exception_offset + offset, function.begin);
        read_unsigned(data, exception_offset + offset + 4, function.end);
        read_unsigned(data, exception_offset + offset + 8, function.unwind);
        if (function.begin == 0 && function.end == 0 && function.unwind == 0) {
            padding_seen = true;
            continue;
        }
        std::size_t ignored{};
        if (padding_seen || function.begin >= function.end || function.end > image_size_ || function.unwind == 0 ||
            !rva_to_file_offset(function.begin, ignored, static_cast<std::size_t>(function.end - function.begin)) ||
            !rva_to_file_offset(function.unwind, ignored, 4) ||
            (!functions_.empty() && functions_.back().end > function.begin))
            return fail("PE runtime functions are invalid, overlapping, unsorted or unbacked.");
        functions_.push_back(function);
    }
    return true;
}

bool pattern_scanner::validate_pattern(std::string_view pattern, std::string& error) noexcept {
    try {
        pattern_scanner scanner;
        compiled_pattern compiled;
        if (!scanner.compile_pattern(pattern, compiled)) {
            error = scanner.error();
            return false;
        }
        error.clear();
        return true;
    } catch (...) {
        try { error = "Cannot allocate the bounded pattern parser."; } catch (...) {}
        return false;
    }
}

bool pattern_scanner::compile_pattern(std::string_view text, compiled_pattern& result) const {
    result = {};
    std::size_t position = 0;
    std::size_t run_start = 0;
    std::size_t run_length = 0;
    while (position < text.size()) {
        while (position < text.size() && (text[position] == ' ' || text[position] == '\t' ||
               text[position] == '\r' || text[position] == '\n')) ++position;
        if (position == text.size()) break;
        const auto start = position;
        while (position < text.size() && text[position] != ' ' && text[position] != '\t' &&
               text[position] != '\r' && text[position] != '\n') ++position;
        const auto token = text.substr(start, position - start);
        if (result.values.size() == 4096) return fail("Pattern exceeds 4096 bytes.");
        const bool wildcard = token == "?" || token == "??";
        std::uint8_t value{};
        if (!wildcard) {
            if (token.size() != 2) return fail("Pattern tokens must be hex byte pairs or ?/?? wildcards.");
            for (const auto character : token) {
                unsigned digit = 16;
                if (character >= '0' && character <= '9') digit = static_cast<unsigned>(character - '0');
                else if (character >= 'a' && character <= 'f') digit = static_cast<unsigned>(character - 'a') + 10;
                else if (character >= 'A' && character <= 'F') digit = static_cast<unsigned>(character - 'A') + 10;
                if (digit == 16) return fail("Pattern contains a nonhexadecimal byte token.");
                value = static_cast<std::uint8_t>((static_cast<unsigned>(value) << 4) | digit);
            }
        }
        result.values.push_back(value);
        result.fixed.push_back(wildcard ? 0 : 1);
        if (wildcard) {
            run_length = 0;
        } else {
            if (run_length == 0) run_start = result.values.size() - 1;
            ++run_length;
            if (run_length > result.anchor_length) {
                result.anchor_offset = run_start;
                result.anchor_length = run_length;
            }
        }
    }
    if (result.values.empty()) return fail("Pattern is empty.");
    if (result.anchor_length == 0) return fail("All-wildcard patterns are not accepted.");
    return true;
}

bool pattern_scanner::scan_sections(const compiled_pattern& pattern, std::string_view section,
    std::vector<pattern_match>& result, std::size_t limit) const {
    bool section_seen = section.empty() || section == "*";
    for (const auto& region : sections_) {
        if (!section.empty() && section != "*" && region.name != section) continue;
        section_seen = true;
        const auto length = pattern.values.size();
        if (region.raw_size < length) continue;
        const auto* data = file_data_.data() + region.file_offset;
        const auto* cursor = data + pattern.anchor_offset;
        const auto* search_end = data + (region.raw_size - length) + pattern.anchor_offset + 1;
        while (cursor < search_end) {
            const auto* anchor = static_cast<const std::uint8_t*>(std::memchr(cursor,
                pattern.values[pattern.anchor_offset], static_cast<std::size_t>(search_end - cursor)));
            if (anchor == nullptr) break;
            const auto* candidate = anchor - pattern.anchor_offset;
            bool matches = std::memcmp(anchor, pattern.values.data() + pattern.anchor_offset,
                pattern.anchor_length) == 0;
            for (std::size_t index = 0; matches && index < length; ++index)
                if (pattern.fixed[index] && candidate[index] != pattern.values[index]) matches = false;
            if (matches) {
                const auto offset = static_cast<std::size_t>(candidate - data);
                const auto rva = region.rva + static_cast<std::uint32_t>(offset);
                result.push_back({rva, image_base_ + rva, region.file_offset + offset});
                if (result.size() >= limit) return true;
            }
            cursor = anchor + 1;
        }
    }
    return section_seen || fail("Requested PE section does not exist.");
}

std::vector<pattern_match> pattern_scanner::find(std::string_view pattern, std::string_view section) const noexcept {
    error_ = "";
    try {
        std::vector<pattern_match> result;
        compiled_pattern compiled;
        if (!valid_) {
            fail("No valid offline PE image is loaded.");
            return result;
        }
        if (!compile_pattern(pattern, compiled) || !scan_sections(compiled, section, result, maximum_matches + 1)) return {};
        if (result.size() > maximum_matches) {
            fail("Pattern result count exceeds one million matches.");
            return {};
        }
        std::sort(result.begin(), result.end(), [](const pattern_match& left, const pattern_match& right) {
            return left.rva < right.rva;
        });
        return result;
    } catch (...) {
        fail("Cannot allocate the bounded pattern search result.");
        return {};
    }
}

bool pattern_scanner::find_unique(std::string_view pattern, pattern_match& result,
    std::string_view section) const noexcept {
    error_ = "";
    try {
        if (!valid_) return fail("No valid offline PE image is loaded.");
        compiled_pattern compiled;
        std::vector<pattern_match> found;
        found.reserve(2);
        if (!compile_pattern(pattern, compiled) || !scan_sections(compiled, section, found, 2)) return false;
        if (found.empty()) return fail("Pattern was not found.");
        if (found.size() != 1) return fail("Pattern is ambiguous: multiple matches were found.");
        result = found.front();
        return true;
    } catch (...) {
        return fail("Cannot allocate the unique pattern search result.");
    }
}

bool pattern_scanner::rva_to_file_offset(std::uint32_t rva, std::size_t& result, std::size_t size) const noexcept {
    if (!valid_ || size == 0 || !contains(image_size_, rva, size)) return false;
    if (contains(header_size_, rva, size)) {
        result = rva;
        return true;
    }
    for (const auto& section : sections_) {
        if (rva >= section.rva && contains(section.raw_size, static_cast<std::size_t>(rva - section.rva), size)) {
            result = section.file_offset + (rva - section.rva);
            return true;
        }
    }
    return false;
}

bool pattern_scanner::file_offset_to_rva(std::size_t offset, std::uint32_t& result) const noexcept {
    if (!valid_ || offset >= file_data_.size()) return false;
    if (offset < header_size_) {
        result = static_cast<std::uint32_t>(offset);
        return true;
    }
    for (const auto& section : sections_) {
        if (offset >= section.file_offset && offset - section.file_offset < section.raw_size) {
            result = section.rva + static_cast<std::uint32_t>(offset - section.file_offset);
            return true;
        }
    }
    return false;
}

bool pattern_scanner::rva_to_va(std::uint32_t rva, std::uint64_t& result) const noexcept {
    if (!valid_ || rva >= image_size_) return false;
    result = image_base_ + rva;
    return true;
}

bool pattern_scanner::va_to_rva(std::uint64_t va, std::uint32_t& result) const noexcept {
    if (!valid_ || va < image_base_ || va - image_base_ >= image_size_) return false;
    result = static_cast<std::uint32_t>(va - image_base_);
    return true;
}

std::span<const std::uint8_t> pattern_scanner::view_rva(std::uint32_t rva, std::size_t size) const noexcept {
    std::size_t offset{};
    return rva_to_file_offset(rva, offset, size) ? bytes().subspan(offset, size) : std::span<const std::uint8_t>{};
}

bool pattern_scanner::function_start(std::uint32_t rva, std::uint32_t& result) const noexcept {
    std::uint32_t end{};
    return function_range(rva, result, end);
}

bool pattern_scanner::function_range(std::uint32_t rva, std::uint32_t& begin,
    std::uint32_t& end) const noexcept {
    error_ = "";
    const auto after = std::upper_bound(functions_.begin(), functions_.end(), rva,
        [](std::uint32_t address, const runtime_function& function) { return address < function.begin; });
    if (after == functions_.begin()) return fail("Matched RVA has no runtime function entry.");
    auto function = *std::prev(after);
    if (rva >= function.end) return fail("Matched RVA is outside its runtime function range.");
    std::array<std::uint32_t, 32> visited{};
    for (std::size_t depth = 0; depth < visited.size(); ++depth) {
        for (std::size_t previous = 0; previous < depth; ++previous)
            if (visited[previous] == function.unwind) return fail("Unwind chain contains a cycle.");
        visited[depth] = function.unwind;
        const auto header = view_rva(function.unwind, 4);
        if (header.empty()) return fail("Unwind info header is truncated.");
        const auto version = header[0] & 7u;
        const auto flags = header[0] >> 3;
        if ((version != 1 && version != 2) || (flags & ~7u) != 0 || ((flags & 4u) && (flags & 3u)))
            return fail("Unwind info version or flags are unsupported.");
        const auto extra_offset = 4u + ((static_cast<std::size_t>(header[2]) + 1u) / 2u) * 4u;
        const auto required = extra_offset + ((flags & 4u) ? 12u : (flags & 3u) ? 4u : 0u);
        const auto info = view_rva(function.unwind, required);
        if (info.empty()) return fail("Unwind codes or chained function data are truncated.");
        if ((flags & 4u) == 0) {
            begin = function.begin;
            end = function.end;
            return true;
        }
        read_unsigned(info, extra_offset, function.begin);
        read_unsigned(info, extra_offset + 4, function.end);
        read_unsigned(info, extra_offset + 8, function.unwind);
        std::size_t ignored{};
        if (function.begin >= function.end || function.end > image_size_ || function.unwind == 0 ||
            !rva_to_file_offset(function.begin, ignored, static_cast<std::size_t>(function.end - function.begin)))
            return fail("Chained runtime function range is invalid.");
    }
    return fail("Unwind chain exceeds 32 entries.");
}

bool pattern_scanner::resolve(const pattern_match& match, pattern_resolve_mode mode,
    pattern_resolution& result, std::size_t displacement_offset, std::size_t instruction_size) const noexcept {
    error_ = "";
    std::size_t offset{};
    std::uint64_t address{};
    if (!rva_to_file_offset(match.rva, offset) || offset != match.file_offset ||
        !rva_to_va(match.rva, address) || address != match.va)
        return fail("Pattern match does not belong to this PE image mapping.");
    pattern_resolution resolved{match.rva, match.va, 0};
    if (mode == pattern_resolve_mode::function_start) {
        if (!function_start(match.rva, resolved.rva)) return false;
        resolved.va = image_base_ + resolved.rva;
    } else if (mode == pattern_resolve_mode::rip_rel32 || mode == pattern_resolve_mode::field_disp32) {
        if (displacement_offset > image_size_ - match.rva)
            return fail("Displacement offset overflows the image range.");
        const auto displacement_rva = match.rva + static_cast<std::uint32_t>(displacement_offset);
        const auto displacement = view_rva(displacement_rva, 4);
        std::uint32_t bits{};
        if (!read_unsigned(displacement, 0, bits)) return fail("Displacement bytes are not completely file-backed.");
        const auto signed_value = std::bit_cast<std::int32_t>(bits);
        if (mode == pattern_resolve_mode::field_disp32) {
            resolved.field_displacement = signed_value;
        } else {
            if (instruction_size > image_size_ - match.rva ||
                displacement_offset > instruction_size || instruction_size - displacement_offset < 4)
                return fail("RIP instruction end does not cover its displacement or exceeds the image.");
            const auto target = static_cast<std::int64_t>(match.rva) +
                static_cast<std::int64_t>(instruction_size) + signed_value;
            if (target < 0 || static_cast<std::uint64_t>(target) >= image_size_)
                return fail("RIP-relative target is outside the PE image.");
            resolved.rva = static_cast<std::uint32_t>(target);
            resolved.va = image_base_ + resolved.rva;
        }
    } else if (mode != pattern_resolve_mode::match) {
        return fail("Unknown pattern resolution mode.");
    }
    result = resolved;
    return true;
}

std::span<const std::uint8_t> pattern_scanner::bytes() const noexcept { return file_data_; }
std::span<const pe_section> pattern_scanner::sections() const noexcept { return sections_; }
bool pattern_scanner::valid() const noexcept { return valid_; }
std::uint64_t pattern_scanner::image_base() const noexcept { return image_base_; }
std::uint32_t pattern_scanner::image_size() const noexcept { return image_size_; }
std::string_view pattern_scanner::error() const noexcept { return error_; }

} // namespace azs
