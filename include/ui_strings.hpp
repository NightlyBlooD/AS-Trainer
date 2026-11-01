#pragma once

#include "localization.hpp"
#include <cstddef>

namespace azs {

enum class ui_text_id : std::size_t {
    overlay_create_failed,
    graphics_failed,
    overlay_stopped,
    title,
    tab_parameters,
    tab_automation,
    run_active_format,
    next_run,
    not_ready,
    hotkeys,
    reset,
    reset_short,
    reset_done,
    reset_hint,
    hide,
    hold_start_hint,
    configs,
    filename_too_long,
    no_configs,
    refresh,
    filename_hint,
    load,
    save,
    save_hint,
    automation,
    party_toggle,
    party_status_format,
    party_wait,
    party_help,
    auto_spells,
    auto_jewelry,
    legendary_toggle,
    legendary_help,
    choice_delay,
    game_time,
    speed_16x,
    time_help,
    performance_stats,
    priority_help,
    spell_priority_title,
    jewelry_priority_title,
    priority_limits,
    apply_rules,
    rules_applied,
    rules_rejected,
    priority_key_long,
    priority_too_many,
    col_parameter,
    col_current,
    col_desired,
    col_freeze,
    col_start,
    range_format,
    unavailable,
    yes,
    no,
    pending,
    apply,
    value_sent,
    value_prepared,
    value_rejected,
    freeze_rejected,
    start_rejected,
    start_hint,
    unit_multiplier_one,
    unit_cooldown,
    unit_yards,
    unit_bonus_fraction,
    unit_coins,
    unit_chest_coins,
    unit_remaining,
    unit_skip_flag,
    unit_current_health,
    unit_health,
    unit_fraction,
    unit_multiplier,
    unit_armor,
    unit_health_per_second,
    unit_count,
    unit_health_per_kill,
    unit_dot_limit,
    unit_dot_haste,
    unit_lifesteal,
    field_damage,
    field_run_speed,
    field_cooldown,
    field_pickup,
    field_area,
    field_luck,
    field_gold,
    field_chest_cost,
    field_rerolls,
    field_skip,
    field_health,
    field_max_health,
    field_crit_chance,
    field_crit_damage,
    field_experience,
    field_armor,
    field_regeneration,
    field_gold_gain,
    field_targets,
    field_echo,
    field_kill_heal,
    field_thorns,
    field_dot_stacks,
    field_dot_haste,
    field_buff_power,
    field_lives,
    field_lifesteal,
    god_mode_toggle,
    god_mode_help,
    count
};

class ui_strings final {
public:
    /** @brief Read one immutable UI translation in the current client language.
     * @param id Catalog entry, including stable ImGui ID suffixes where required.
     * @return Process-lifetime UTF-8 text, or an empty string for an invalid ID.
     */
    static const char* get(ui_text_id id) noexcept;

    /** @brief Read a translation for an explicit language without changing global state.
     * @param id Catalog entry; @param language Any supported client language; invalid values select English.
     * @return Immutable UTF-8 text; invalid language values fall back to English.
     */
    static const char* get(ui_text_id id, client_language language) noexcept;

    /** @brief Relocalize a previously stored UI status after a client-language change.
     * @param message UTF-8 literal matching any catalog language, or arbitrary diagnostic text.
     * @return Current-language static text on an exact match; otherwise the unchanged pointer.
     * Null input returns empty static text; unknown messages retain their original lifetime.
     */
    static const char* translate_message(const char* message) noexcept;

    /** @brief Resolve the localized display name of a stable editable-field key.
     * @param key Null-terminated ASCII key supplied by field_descriptor.
     * @return Localized name, the unchanged key if unknown, or empty text for null.
     */
    static const char* field_name(const char* key) noexcept;

    /** @brief Resolve a field display name in an explicit language for read-only callers.
     * @param key Stable descriptor key; @param language Desired catalog language.
     * @return Immutable translated name, or the unchanged key when unknown.
     */
    static const char* field_name(const char* key, client_language language) noexcept;

    /** @brief Resolve localized units while keeping the numeric storage scale unchanged.
     * @param key Null-terminated ASCII key supplied by field_descriptor.
     * @return Localized unit/scale explanation, or an empty string for an unknown key.
     */
    static const char* field_unit(const char* key) noexcept;

    /** @brief Resolve a field's numeric-scale explanation in an explicit language.
     * @param key Stable descriptor key; @param language Desired catalog language.
     * @return Immutable translated unit, or empty text when unknown.
     */
    static const char* field_unit(const char* key, client_language language) noexcept;

private:
    struct field_entry {
        const char* key;
        ui_text_id name;
        ui_text_id unit;
    };

    /** @brief Binary-search the small sorted immutable field translation map.
     * @param key Stable field key, or null for no field.
     * @return Matching static entry, or nullptr when the key is not present.
     */
    static const field_entry* find_field(const char* key) noexcept;

    static const field_entry field_entries_[26];
};

} // namespace azs
