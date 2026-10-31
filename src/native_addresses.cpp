#include "native_addresses.hpp"
#include "embedded_signatures.hpp"
#include "game_layout.hpp"
#include "pattern_scanner.hpp"
#include "signature_profile.hpp"

#include <Windows.h>

#include <cstring>
#include <limits>

namespace azs {
namespace {
constexpr std::array<std::uint32_t, static_cast<std::size_t>(native_symbol::count)> legacy_rvas = {
    0x5cbb3e0, 0x5caaf00, 0x5b54d60, 0x60c5a60, 0x5ce2c10,
    0x5ca6240, 0x5c9e280, 0x5b4cc40, 0x5d2d750, 0x5cc4370,
    0x60f4e60, 0x5d5f1b7, 0x5d1978f, 0x5cd87f1, 0x70a2e08
};
constexpr std::array<const char*, static_cast<std::size_t>(native_symbol::count)> symbol_names = {
    "owner_frame", "recalculate", "create_speed_packet", "grow_speed_packets", "swap_player",
    "flush_updates", "route_queues", "set_object_field", "apply_choice", "boss_loot",
    "grow_loot", "luck_reader", "area_reader", "pickup_reader", "zone_table"
};
constexpr const char* legacy_hash = "5dd6819d836748ccf809ab76fe6509c9ee793e087939b5d036c430d835a14e63";
constexpr const char* current_hash = "e60b03aae379c9c3c4c728bba1b3dbaeac161007e34a8c09fc61c061f9cceb9c";
}

std::string native_addresses::hash_text(const std::array<unsigned char, 32>& digest) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(64, '0');
    for (std::size_t index = 0; index < digest.size(); ++index) {
        result[index * 2] = digits[digest[index] >> 4];
        result[index * 2 + 1] = digits[digest[index] & 15];
    }
    return result;
}

bool native_addresses::symbol_from_name(std::string_view name, native_symbol& symbol) noexcept {
    for (std::size_t index = 0; index < symbol_names.size(); ++index) {
        if (name == symbol_names[index]) {
            symbol = static_cast<native_symbol>(index);
            return true;
        }
    }
    return false;
}

std::size_t native_addresses::pattern_size(std::string_view pattern) noexcept {
    std::size_t count = 0;
    bool token = false;
    for (const char value : pattern) {
        const bool space = value == ' ' || value == '\t' || value == '\r' || value == '\n';
        if (!space && !token) ++count;
        token = !space;
    }
    return count;
}

bool native_addresses::matching_image(std::uintptr_t module_base, const pattern_scanner& scanner) noexcept {
    const auto raw = scanner.bytes();
    if (raw.size() < 0x40) return false;
    std::uint32_t pe_offset{};
    std::memcpy(&pe_offset, raw.data() + 0x3c, sizeof(pe_offset));
    if (pe_offset > raw.size() || raw.size() - pe_offset < sizeof(IMAGE_NT_HEADERS64)) return false;
    IMAGE_NT_HEADERS64 offline{};
    std::memcpy(&offline, raw.data() + pe_offset, sizeof(offline));
    __try {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module_base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x100000) return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(module_base + dos->e_lfanew);
        return nt->Signature == IMAGE_NT_SIGNATURE && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
            nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
            nt->FileHeader.TimeDateStamp == offline.FileHeader.TimeDateStamp &&
            nt->OptionalHeader.SizeOfImage == scanner.image_size();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool native_addresses::matching_live(std::uintptr_t base, std::uint32_t rva,
    std::span<const std::uint8_t> bytes, std::uint32_t image_size) noexcept {
    if (bytes.empty() || rva >= image_size || bytes.size() > image_size - rva || base > UINTPTR_MAX - rva) return false;
    auto cursor = base + rva;
    if (bytes.size() > UINTPTR_MAX - cursor) return false;
    const auto end = cursor + bytes.size();
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region) ||
            region.State != MEM_COMMIT || region.Type != MEM_IMAGE || (region.Protect & PAGE_GUARD) != 0) return false;
        const auto protection = region.Protect & 0xff;
        if (protection != PAGE_EXECUTE_READ && protection != PAGE_EXECUTE_READWRITE &&
            protection != PAGE_EXECUTE_WRITECOPY) return false;
        const auto next = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
        if (next <= cursor) return false;
        cursor = next;
    }
    __try {
        return std::memcmp(reinterpret_cast<const void*>(base + rva), bytes.data(), bytes.size()) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool native_addresses::initialize(std::uintptr_t module_base, const std::filesystem::path& executable_path,
    const std::array<unsigned char, 32>& digest, std::string& error) noexcept {
    try {
        const auto hash = hash_text(digest);
        if (hash == legacy_hash) {
            game_layout::select(layout_id::legacy);
            resolved_ = legacy_rvas;
            patterns_ = false;
            verification_.clear();
            error.clear();
            return true;
        }
        if (hash != current_hash) {
            error = "unsupported executable SHA-256; use signature_scan for a read-only compatibility report";
            return false;
        }
        signature_profile profile;
        if (!signature_profiles::parse(current_signature_profile, profile, error) ||
            profile.sha256 != hash || profile.layout != "current") {
            if (error.empty()) error = "embedded signature profile does not match the admitted build";
            return false;
        }
        pattern_scanner scanner;
        if (!scanner.load_file(executable_path)) {
            error = std::string(scanner.error());
            return false;
        }
        if (signature_profiles::sha256(scanner.bytes()) != hash || !matching_image(module_base, scanner)) {
            error = "on-disk executable and running image identity differ";
            return false;
        }
        std::array<std::uint32_t, static_cast<std::size_t>(native_symbol::count)> resolved{};
        std::array<bool, static_cast<std::size_t>(native_symbol::count)> seen{};
        std::array<bool, 3> fields_seen{};
        std::vector<verification_chunk> chunks;
        for (const auto& rule : profile.rules) {
            pattern_match match;
            pattern_resolution target;
            if (!scanner.find_unique(rule.pattern, match, rule.section) ||
                !scanner.resolve(match, rule.resolve, target, rule.displacement_offset, rule.instruction_size)) {
                error = rule.name + ": " + std::string(scanner.error());
                return false;
            }
            const auto size = pattern_size(rule.pattern);
            const auto bytes = scanner.view_rva(match.rva, size);
            if (bytes.size() != size || !matching_live(module_base, match.rva, bytes, scanner.image_size())) {
                error = rule.name + ": live code differs from its verified signature anchor";
                return false;
            }
            chunks.push_back({match.rva, {bytes.begin(), bytes.end()}});
            native_symbol symbol{};
            if (symbol_from_name(rule.name, symbol)) {
                if (rule.resolve == pattern_resolve_mode::field_disp32) {
                    error = rule.name + ": native address cannot be a field displacement";
                    return false;
                }
                const auto adjusted = static_cast<std::int64_t>(target.rva) + rule.addend;
                if (adjusted < 0 || adjusted >= scanner.image_size()) {
                    error = rule.name + ": resolved target is outside the image";
                    return false;
                }
                const auto index = static_cast<std::size_t>(symbol);
                if (seen[index]) { error = "duplicate required native symbol"; return false; }
                resolved[index] = static_cast<std::uint32_t>(adjusted);
                seen[index] = true;
                if (symbol != native_symbol::zone_table) {
                    const auto prologue = scanner.view_rva(resolved[index], 24);
                    if (prologue.size() != 24 || !matching_live(module_base, resolved[index], prologue, scanner.image_size())) {
                        error = rule.name + ": resolved target is not intact executable image code";
                        return false;
                    }
                    chunks.push_back({resolved[index], {prologue.begin(), prologue.end()}});
                }
            } else {
                const std::array<std::string_view, 3> names{"field_luck", "field_area", "field_pickup"};
                const std::array<std::int32_t, 3> offsets{0x4fc, 0x4f8, 0x4e8};
                bool known = false;
                for (std::size_t index = 0; index < names.size(); ++index) {
                    if (rule.name != names[index]) continue;
                    known = rule.resolve == pattern_resolve_mode::field_disp32 &&
                        target.field_displacement == offsets[index] && rule.addend == 0;
                    fields_seen[index] = known;
                }
                if (!known) { error = rule.name + ": unknown or incompatible field rule"; return false; }
            }
        }
        for (const bool present : seen) if (!present) { error = "required native symbol is missing"; return false; }
        for (const bool present : fields_seen) if (!present) { error = "required field fingerprint is missing"; return false; }
        resolved_ = resolved;
        verification_ = std::move(chunks);
        module_base_ = module_base;
        image_size_ = scanner.image_size();
        game_layout::select(layout_id::current);
        patterns_ = true;
        error.clear();
        return true;
    } catch (...) {
        try { error = "native signature resolution failed"; } catch (...) {}
        return false;
    }
}

std::uint32_t native_addresses::rva(native_symbol symbol) noexcept {
    const auto index = static_cast<std::size_t>(symbol);
    if (index >= legacy_rvas.size()) return 0;
    return patterns_ ? resolved_[index] : legacy_rvas[index];
}

bool native_addresses::uses_patterns() noexcept { return patterns_; }

bool native_addresses::verify_all() noexcept {
    if (!patterns_ || verification_.empty()) return false;
    for (const auto& chunk : verification_)
        if (!matching_live(module_base_, chunk.rva, chunk.bytes, image_size_)) return false;
    return true;
}

} // namespace azs
