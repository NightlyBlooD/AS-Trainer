#include "trainer_state.hpp"
#include "game_layout.hpp"
#include "localization.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cmath>
#include <cstring>
#include <mutex>

namespace azs {
namespace {

constexpr field_descriptor field_definitions[] = {
    {"Урон", "множитель, 1 = обычный", 0x4cc, value_kind::real, 0.01, 1000, 0.5, "damage"},
    {"Скорость бега", "множитель, 1 = обычная", 0x4dc, value_kind::real, 0.1, 10, 0.1, "run_speed"},
    {"Перезарядка заклинаний", "время: 0.5 = вдвое быстрее", 0x4d0, value_kind::real, 0.01, 10, 0.05, "cooldown"},
    {"Радиус сбора", "ярды", 0x4e0, value_kind::real, 1, 1000, 10, "pickup"},
    {"Область действия", "множитель, 1 = обычная", 0x4f0, value_kind::real, 0.1, 100, 0.5, "area"},
    {"Удача", "доля: 1 = +100%", 0x4f4, value_kind::real, 0, 1000, 10, "luck"},
    {"Золото забега", "монеты", 0x544, value_kind::unsigned_integer, 0, 1000000000, 1000, "gold"},
    {"Цена открытия сундука", "монеты; 0 = бесплатно", 0xd40, value_kind::unsigned_integer, 0, 1000000000, 1, "chest_cost"},
    {"Бесплатные рероллы", "оставшееся количество", 0x59c, value_kind::unsigned_integer, 0, 1000000, 10, "rerolls"},
    {"Разрешить скип", "скипы не расходуются по счётчику", 0x2ca, value_kind::boolean, 0, 1, 1, "skip"},
    {"Здоровье", "текущее", 0x538, value_kind::real, 1, 1000000, 100, "health"},
    {"Максимум здоровья", "единицы здоровья", 0x4c8, value_kind::real, 1, 1000000, 100, "max_health"},
    {"Шанс критического удара", "доля: 1 = 100%", 0x4d4, value_kind::real, 0, 10, 0.1, "crit_chance"},
    {"Урон критического удара", "множитель", 0x4d8, value_kind::real, 1, 100, 0.5, "crit_damage"},
    {"Получаемый опыт", "множитель, 1 = обычный", 0x4e4, value_kind::real, 0.1, 100, 0.5, "experience"},
    {"Снижение входящего урона", "доля; обычный предел 0.6", 0x4e8, value_kind::real, 0, 0.99, 0.1, "armor"},
    {"Восстановление здоровья", "здоровья в секунду", 0x4ec, value_kind::real, 0, 100000, 10, "regeneration"},
    {"Получаемое золото", "множитель, 1 = обычный", 0x4f8, value_kind::real, 0.1, 100, 0.5, "gold_gain"},
    {"Дополнительные цели", "количество", 0x4fc, value_kind::unsigned_integer, 0, 100, 1, "targets"},
    {"Повторное заклинание", "доля: 1 = 100%", 0x500, value_kind::real, 0, 10, 0.1, "echo"},
    {"Лечение за убийство", "здоровья за врага", 0x504, value_kind::real, 0, 100000, 10, "kill_heal"},
    {"Отражение урона", "доля: 1 = 100%", 0x508, value_kind::real, 0, 100, 0.1, "thorns"},
    {"Дополнительные стаки DoT", "игра учитывает не более 3", 0x50c, value_kind::unsigned_integer, 0, 3, 1, "dot_stacks"},
    {"Сила баффов", "доля: 1 = +100%", 0x510, value_kind::real, 0, 100, 0.1, "buff_power"},
    {"Возрождения", "количество", 0x598, value_kind::unsigned_integer, 0, 1000, 1, "lives"},
};
static_assert(std::size(field_definitions) <= 64);

constexpr auto current_field_definitions = [] {
    auto fields = std::to_array(field_definitions);
    constexpr std::size_t offsets[] = {0x4d4, 0x4e4, 0x4d8, 0x4e8, 0x4f8, 0x4fc, 0x54c, 0xe60, 0x5a4, 0x2d2, 0x540, 0x4d0, 0x4dc, 0x4e0, 0x4ec, 0x4f0, 0x4f4, 0x500, 0x504, 0x508, 0x50c, 0x510, 0x514, 0x518, 0x5a0};
    static_assert(std::size(offsets) == fields.size());
    for (std::size_t index = 0; index < fields.size(); ++index) {
        fields[index].offset = offsets[index];
    }
    return fields;
}();


std::mutex message_mutex;
std::string status_message = "Waiting for game verification...";
const auto default_automation = std::make_shared<const automation_settings>();

} // namespace

trainer_state& trainer_state::instance() noexcept {
    static trainer_state state;
    return state;
}

std::size_t trainer_state::field_count() noexcept {
    return std::size(field_definitions);
}

const field_descriptor& trainer_state::field_at(std::size_t index) noexcept {
    const auto safe_index = index < field_count() ? index : 0;
    return game_layout::current_id() == layout_id::current
        ? current_field_definitions[safe_index] : field_definitions[safe_index];
}

field_snapshot trainer_state::get_field(std::size_t index) const noexcept {
    if (index >= field_count()) {
        return {};
    }
    const auto& field = fields_[index];
    return {field.current_value.load(), field.desired_value.load(),
            field.available.load() && get_runtime().run_active,
            field.frozen.load(), field.command_id.load() != field.applied_id.load(),
            field.configured.load(), field.apply_on_start.load()};
}

bool trainer_state::valid_value(std::size_t index, double value) noexcept {
    if (index >= field_count() || !std::isfinite(value)) {
        return false;
    }
    const auto& descriptor = field_at(index);
    return value >= descriptor.minimum && value <= descriptor.maximum &&
           (descriptor.kind == value_kind::real || std::floor(value) == value);
}

bool trainer_state::apply(std::size_t index, double value) noexcept {
    std::lock_guard lock(controls_mutex_);
    if (!valid_value(index, value)) {
        return false;
    }
    fields_[index].desired_value.store(value);
    fields_[index].configured.store(true);
    fields_[index].active_for_run.store(true);
    if (!get_runtime().run_active) {
        fields_[index].apply_on_start.store(true);
    }
    fields_[index].command_id.fetch_add(1);
    return true;
}

bool trainer_state::freeze(std::size_t index, bool enabled, double value) noexcept {
    std::lock_guard lock(controls_mutex_);
    if (index >= field_count()) {
        return false;
    }
    if (!enabled) {
        fields_[index].frozen.store(false);
        return true;
    }
    if (!valid_value(index, value)) {
        return false;
    }
    fields_[index].desired_value.store(value);
    fields_[index].configured.store(true);
    fields_[index].active_for_run.store(true);
    fields_[index].apply_on_start.store(true);
    fields_[index].command_id.fetch_add(1);
    fields_[index].frozen.store(true);
    return true;
}

void trainer_state::reset_all() noexcept {
    std::lock_guard lock(controls_mutex_);
    reset_locked();
    automation_.store(default_automation);
}

void trainer_state::reset_locked() noexcept {
    for (std::size_t index = 0; index < field_count(); ++index) {
        fields_[index].frozen.store(false);
        fields_[index].apply_on_start.store(false);
        fields_[index].active_for_run.store(false);
        fields_[index].applied_id.store(fields_[index].command_id.load());
    }
}

bool trainer_state::set_apply_on_start(std::size_t index, bool enabled) noexcept {
    std::lock_guard lock(controls_mutex_);
    if (index >= field_count() || !fields_[index].configured.load() ||
        !valid_value(index, fields_[index].desired_value.load())) {
        return false;
    }
    fields_[index].apply_on_start.store(enabled);
    return true;
}

bool trainer_state::valid_automation(const automation_settings& settings) noexcept {
    if (settings.choice_delay_ms < 50 || settings.choice_delay_ms > 5000 ||
        !std::isfinite(settings.time_scale) || settings.time_scale < 1 || settings.time_scale > 16) {
        return false;
    }
    for (const auto* priorities : {&settings.spell_priorities, &settings.jewelry_priorities}) {
        if (priorities->size() > 64) {
            return false;
        }
        for (const auto& key : *priorities) {
            if (key.empty() || key.size() > 128) {
                return false;
            }
            for (const auto character : key) {
                const auto byte = static_cast<unsigned char>(character);
                if ((byte < 'a' || byte > 'z') && (byte < 'A' || byte > 'Z') &&
                    (byte < '0' || byte > '9') && byte != '_' && byte != '.' &&
                    byte != ':' && byte != '-') {
                    return false;
                }
            }
        }
    }
    return true;
}

std::shared_ptr<const automation_settings> trainer_state::get_automation() const noexcept {
    return automation_.load();
}

bool trainer_state::set_automation(const automation_settings& settings) noexcept {
    if (!valid_automation(settings)) {
        return false;
    }
    try {
        const auto next = std::make_shared<const automation_settings>(settings);
        std::lock_guard lock(controls_mutex_);
        automation_.store(next);
        return true;
    } catch (...) {
        return false;
    }
}

trainer_preset trainer_state::export_preset() const {
    std::lock_guard lock(controls_mutex_);
    trainer_preset preset;
    for (std::size_t index = 0; index < field_count(); ++index) {
        const auto& field = fields_[index];
        if (field.configured.load()) {
            preset.fields.push_back({field_at(index).key, field.desired_value.load(),
                field.frozen.load(), field.apply_on_start.load()});
        }
    }
    preset.automation = *automation_.load();
    return preset;
}

bool trainer_state::import_preset(const trainer_preset& preset) noexcept {
    if (!valid_automation(preset.automation) || preset.fields.size() > field_count()) {
        return false;
    }
    std::array<bool, 64> seen{};
    std::array<std::size_t, 64> indices{};
    for (std::size_t entry = 0; entry < preset.fields.size(); ++entry) {
        const auto& field = preset.fields[entry];
        std::size_t index = 0;
        for (; index < field_count(); ++index) {
            if (field.key == field_at(index).key) {
                break;
            }
        }
        if (index == field_count() || seen[index] || !valid_value(index, field.value)) {
            return false;
        }
        seen[index] = true;
        indices[entry] = index;
    }
    try {
        const auto next_automation = std::make_shared<const automation_settings>(preset.automation);
        std::lock_guard lock(controls_mutex_);
        reset_locked();
        for (std::size_t index = 0; index < field_count(); ++index) {
            fields_[index].configured.store(false);
        }
        for (std::size_t entry = 0; entry < preset.fields.size(); ++entry) {
            auto& field = fields_[indices[entry]];
            const auto& input = preset.fields[entry];
            field.desired_value.store(input.value);
            field.configured.store(true);
            field.active_for_run.store(true);
            field.frozen.store(input.frozen);
            field.apply_on_start.store(input.apply_on_start);
            field.command_id.fetch_add(1);
        }
        automation_.store(next_automation);
        return true;
    } catch (...) {
        return false;
    }
}

runtime_snapshot trainer_state::get_runtime() const noexcept {
    const auto timestamp = last_update_ms_.load();
    const auto ready = hooks_ready_.load();
    return {ready, ready && timestamp != 0 && GetTickCount64() - timestamp < 5000,
            run_address_.load(), level_.load(), timestamp,
            party_host_.load(), party_members_.load(), party_writes_.load()};
}

void trainer_state::set_party_status(bool hosting, std::uint32_t member_count,
    std::uint64_t writes) noexcept {
    party_host_.store(hosting);
    party_members_.store(member_count);
    party_writes_.store(writes);
}

void trainer_state::set_message(const char* message) noexcept {
    try {
        std::lock_guard lock(message_mutex);
        status_message = message ? message : "";
    } catch (...) {
        OutputDebugStringW(L"Azeroth overlay: status allocation failed\n");
    }
}

std::string trainer_state::get_message() const {
    std::lock_guard lock(message_mutex);
    return localization::translate(status_message.c_str());
}

void trainer_state::set_hooks_ready(bool ready) noexcept {
    hooks_ready_.store(ready);
    if (!ready) {
        reset_all();
    }
}

bool trainer_state::valid_run(void* run_address) noexcept {
    const auto start = reinterpret_cast<std::uintptr_t>(run_address);
    if (start < 0x10000 || (start & 7) != 0 || start > UINTPTR_MAX - game_layout::scalar_size()) {
        return false;
    }
    auto cursor = start;
    const auto end = start + game_layout::scalar_size();
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<void*>(cursor), &region, sizeof(region)) != sizeof(region)) {
            return false;
        }
        const auto protection = region.Protect & 0xff;
        if (region.State != MEM_COMMIT || (region.Protect & PAGE_GUARD) != 0 ||
            (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY)) {
            return false;
        }
        const auto region_end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
        if (region_end <= cursor) {
            return false;
        }
        cursor = region_end;
    }
    __try {
        const auto* bytes = static_cast<const std::byte*>(run_address);
        const auto class_id = *reinterpret_cast<const std::uint8_t*>(bytes + game_layout::run_offset(0x354));
        const auto level = *reinterpret_cast<const std::uint32_t*>(bytes + game_layout::run_offset(0x530));
        const auto maximum_hp = *reinterpret_cast<const float*>(bytes + game_layout::run_offset(0x4c8));
        const auto phase = *reinterpret_cast<const std::uint8_t*>(bytes + game_layout::run_offset(0xda3));
        const bool valid_class = class_id == 1 || class_id == 2 || class_id == 3 || class_id == 4 ||
                                 class_id == 5 || class_id == 7 || class_id == 8 || class_id == 9 || class_id == 11;
        return valid_class && level >= 1 && level <= 100000 && phase <= 8 &&
               std::isfinite(maximum_hp) && maximum_hp >= 1 && maximum_hp <= 1000000000;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool trainer_state::read_field(void* run_address, std::size_t index, double& value) noexcept {
    const auto& descriptor = field_at(index);
    const auto* address = static_cast<const std::byte*>(run_address) + descriptor.offset;
    __try {
        if (descriptor.kind == value_kind::real) {
            value = *reinterpret_cast<const float*>(address);
        } else if (descriptor.kind == value_kind::unsigned_integer) {
            value = *reinterpret_cast<const std::uint32_t*>(address);
        } else {
            value = *reinterpret_cast<const std::uint8_t*>(address) != 0 ? 1 : 0;
        }
        return std::isfinite(value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool trainer_state::write_field(void* run_address, std::size_t index, double value) noexcept {
    const auto& descriptor = field_at(index);
    auto* address = static_cast<std::byte*>(run_address) + descriptor.offset;
    __try {
        if (descriptor.kind == value_kind::real) {
            *reinterpret_cast<float*>(address) = static_cast<float>(value);
        } else if (descriptor.kind == value_kind::unsigned_integer) {
            *reinterpret_cast<std::uint32_t*>(address) = static_cast<std::uint32_t>(value);
        } else {
            *reinterpret_cast<std::uint8_t*>(address) = value != 0 ? 1 : 0;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool trainer_state::read_level(void* run_address, std::uint32_t& level) noexcept {
    __try {
        level = *reinterpret_cast<std::uint32_t*>(static_cast<std::byte*>(run_address) + game_layout::run_offset(0x530));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool trainer_state::read_clock(void* run_address, float& elapsed) noexcept {
    __try {
        elapsed = *reinterpret_cast<float*>(static_cast<std::byte*>(run_address) + game_layout::run_offset(0xce0));
        return std::isfinite(elapsed) && elapsed >= 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool trainer_state::read_phase(void* run_address, std::uint8_t& phase) noexcept {
    __try {
        phase = *reinterpret_cast<std::uint8_t*>(static_cast<std::byte*>(run_address) + game_layout::run_offset(0xda3));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool trainer_state::process_run(void* run_address, bool publish, speed_update* speed_result) noexcept {
    if (speed_result) {
        *speed_result = {};
    }
    std::lock_guard lock(controls_mutex_);
    if (!hooks_ready_.load() || !valid_run(run_address)) {
        return false;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(run_address);
    std::uint8_t phase{};
    if (!read_phase(run_address, phase) || phase < 1 || phase > 3) {
        if (publish) {
            last_update_ms_.store(0);
        }
        return false;
    }
    float elapsed{};
    if (!read_clock(run_address, elapsed)) {
        return false;
    }
    if (run_address_.load() != address || elapsed + 0.0001f < last_game_time_.load()) {
        if (!publish) {
            return false;
        }
        run_address_.store(address);
        last_update_ms_.store(0);
        for (std::size_t index = 0; index < field_count(); ++index) {
            auto& field = fields_[index];
            field.available.store(false);
            if (field.configured.load() && field.apply_on_start.load()) {
                field.active_for_run.store(true);
                field.command_id.fetch_add(1);
            } else {
                field.active_for_run.store(false);
                field.frozen.store(false);
                field.applied_id.store(field.command_id.load());
            }
        }
    }
    for (std::size_t index = 0; index < field_count(); ++index) {
        auto& control = fields_[index];
        const auto command = control.command_id.load();
        const auto pending = command != control.applied_id.load();
        const auto desired = control.desired_value.load();
        if ((pending || control.frozen.load()) && valid_value(index, desired)) {
            const auto is_speed = field_at(index).offset == game_layout::run_offset(0x4dc);
            double previous = 0;
            if (is_speed && !read_field(run_address, index, previous)) {
                control.frozen.store(false);
                control.available.store(false);
                continue;
            }
            if (write_field(run_address, index, desired)) {
                control.applied_id.store(command);
                if (is_speed && speed_result) {
                    *speed_result = {true, static_cast<float>(previous), static_cast<float>(desired), command};
                }
            } else {
                control.frozen.store(false);
                control.available.store(false);
            }
        }
        if (publish) {
            double current = 0;
            const auto read_ok = read_field(run_address, index, current);
            if (read_ok) {
                control.current_value.store(current);
                if (!control.configured.load() && !control.available.load() && !pending && !control.frozen.load()) {
                    control.desired_value.store(current);
                }
            }
            control.available.store(read_ok);
        }
    }
    if (publish) {
        std::uint32_t level{};
        if (!read_level(run_address, level)) {
            return false;
        }
        level_.store(level);
        last_game_time_.store(elapsed);
        last_update_ms_.store(GetTickCount64());
    }
    return true;
}

bool trainer_state::process_member(void* run_address, member_commands& commands,
    speed_update* speed_result) noexcept {
    if (speed_result) {
        *speed_result = {};
    }
    std::lock_guard lock(controls_mutex_);
    if (!hooks_ready_.load() || !valid_run(run_address)) {
        return false;
    }
    std::uint8_t phase{};
    if (!read_phase(run_address, phase) || phase < 1 || phase > 3) {
        return false;
    }
    for (std::size_t index = 0; index < field_count(); ++index) {
        const auto& control = fields_[index];
        if (!control.configured.load() || !control.active_for_run.load()) {
            continue;
        }
        const auto command = control.command_id.load();
        if (commands.applied_ids[index] == command && !control.frozen.load()) {
            continue;
        }
        const auto is_speed = field_at(index).offset == game_layout::run_offset(0x4dc);
        if (is_speed && commands.rejected_speed_command == command) {
            continue;
        }
        const auto desired = control.desired_value.load();
        if (!valid_value(index, desired)) {
            continue;
        }
        double previous = 0;
        if (is_speed && !read_field(run_address, index, previous)) {
            continue;
        }
        if (write_field(run_address, index, desired)) {
            commands.applied_ids[index] = command;
            if (is_speed && speed_result) {
                *speed_result = {true, static_cast<float>(previous), static_cast<float>(desired), command};
            }
        }
    }
    return true;
}

bool trainer_state::reject_member_speed_update(void* run_address, const speed_update& update) noexcept {
    std::lock_guard lock(controls_mutex_);
    if (!update.written || !valid_run(run_address) || !std::isfinite(update.previous_value)) {
        return false;
    }
    for (std::size_t index = 0; index < field_count(); ++index) {
        if (field_at(index).offset == game_layout::run_offset(0x4dc)) {
            double current{};
            return read_field(run_address, index, current) &&
                current == static_cast<double>(update.requested_value) &&
                write_field(run_address, index, update.previous_value);
        }
    }
    return false;
}

bool trainer_state::reject_speed_update(void* run_address, const speed_update& update) noexcept {
    std::lock_guard lock(controls_mutex_);
    if (!update.written || run_address_.load() != reinterpret_cast<std::uintptr_t>(run_address) ||
        !valid_run(run_address) || !std::isfinite(update.previous_value)) {
        return false;
    }
    for (std::size_t index = 0; index < field_count(); ++index) {
        if (field_at(index).offset != game_layout::run_offset(0x4dc)) {
            continue;
        }
        auto& control = fields_[index];
        double current{};
        const auto restored = read_field(run_address, index, current) &&
            current == static_cast<double>(update.requested_value) &&
            write_field(run_address, index, update.previous_value);
        if (control.command_id.load() == update.command_id) {
            control.frozen.store(false);
            control.applied_id.store(update.command_id);
        }
        if (restored) {
            control.current_value.store(update.previous_value);
        }
        return restored;
    }
    return false;
}

} // namespace azs
