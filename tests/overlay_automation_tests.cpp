#include "overlay_automation_tests.hpp"
#include "overlay_window.hpp"
#include "trainer_state.hpp"
#include "ui_strings.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace azs {

void overlay_automation_tests::check(bool condition, const char* description) noexcept {
    if (!condition) {
        ++failures_;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

void overlay_automation_tests::check_field_edits() {
    trainer_state& state = trainer_state::instance();
    check(state.import_preset({}), "field fixture clears all previously configured values");
    std::size_t numeric = trainer_state::field_count();
    std::size_t boolean = trainer_state::field_count();
    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
        const auto& descriptor = trainer_state::field_at(index);
        if (std::strcmp(descriptor.key, "damage") == 0) {
            numeric = index;
        }
        if (descriptor.kind == value_kind::boolean) {
            boolean = index;
        }
    }
    check(numeric < trainer_state::field_count() && boolean < trainer_state::field_count(),
        "field fixture finds a numeric and a boolean descriptor");
    if (numeric >= trainer_state::field_count() || boolean >= trainer_state::field_count()) {
        return;
    }

    overlay_window overlay;
    overlay.edited_values_[numeric] = 2.5;
    overlay.dirty_values_[numeric] = true;
    check(overlay.apply_field_edit(numeric, false), "completed numeric edit queues its value without an apply button");
    const auto numeric_snapshot = state.get_field(numeric);
    check(numeric_snapshot.configured && numeric_snapshot.pending && numeric_snapshot.desired_value == 2.5
        && !overlay.dirty_values_[numeric]
        && state.get_message() == ui_strings::get(ui_text_id::value_prepared),
        "successful numeric edit clears dirty state and reports next-run preparation");

    overlay.edited_values_[boolean] = 1.0;
    overlay.dirty_values_[boolean] = true;
    check(overlay.apply_field_edit(boolean, true) && state.get_field(boolean).desired_value == 1.0
        && !overlay.dirty_values_[boolean]
        && state.get_message() == ui_strings::get(ui_text_id::value_sent),
        "boolean toggle queues immediately and reports a live update");

    state.reset_all();
    overlay.edited_values_[numeric] = 4.0;
    overlay.dirty_values_[numeric] = false;
    check(overlay.toggle_field_start(numeric, true, false)
        && state.get_field(numeric).desired_value == 4.0
        && state.get_field(numeric).pending && state.get_field(numeric).apply_on_start,
        "enabling start queues the editor value even for an already configured clean field");
    state.reset_all();
    check(!state.get_field(numeric).pending && overlay.toggle_field_start(numeric, true, true)
        && state.get_field(numeric).pending && state.get_field(numeric).apply_on_start,
        "enabling start requeues an unchanged configured value rather than only setting its flag");

    state.reset_all();
    check(state.set_apply_on_start(numeric, true), "disable fixture enables only the start flag");
    overlay.edited_values_[numeric] = 6.0;
    overlay.dirty_values_[numeric] = true;
    check(overlay.toggle_field_start(numeric, false, false)
        && !state.get_field(numeric).apply_on_start && !state.get_field(numeric).pending
        && state.get_field(numeric).desired_value == 4.0
        && overlay.dirty_values_[numeric] && overlay.edited_values_[numeric] == 6.0,
        "disabling start neither reapplies an edited value nor discards its draft");

    overlay.edited_values_[numeric] = -1.0;
    const auto before_invalid = state.get_field(numeric);
    check(!overlay.apply_field_edit(numeric, false) && overlay.dirty_values_[numeric]
        && overlay.edited_values_[numeric] == -1.0
        && state.get_field(numeric).desired_value == before_invalid.desired_value
        && state.get_field(numeric).pending == before_invalid.pending
        && state.get_field(numeric).apply_on_start == before_invalid.apply_on_start,
        "invalid numeric commit retains its draft and previously accepted field snapshot");
    check(!overlay.toggle_field_start(numeric, true, false) && overlay.dirty_values_[numeric]
        && overlay.edited_values_[numeric] == -1.0
        && state.get_field(numeric).desired_value == before_invalid.desired_value
        && state.get_field(numeric).pending == before_invalid.pending
        && state.get_field(numeric).apply_on_start == before_invalid.apply_on_start,
        "invalid start value cannot publish a value or enable its start flag");
    state.reset_all();
}

int overlay_automation_tests::run() noexcept {
    try {
        trainer_state& state = trainer_state::instance();
        state.reset_all();
        automation_settings original{};
        original.spell_priorities = {"wrath"};
        original.jewelry_priorities = {"ring"};
        check(state.set_automation(original), "fixture accepts existing priority lists");

        overlay_window overlay;
        overlay.refresh_automation_editor();
        const std::string invalid_draft(129, 'x');
        std::memcpy(overlay.spell_priority_text_.data(), invalid_draft.c_str(), invalid_draft.size() + 1);
        strcpy_s(overlay.jewelry_priority_text_.data(), overlay.jewelry_priority_text_.size(), "new_ring");
        overlay.apply_to_party_ = true;
        overlay.auto_choose_spells_ = true;
        overlay.auto_choose_jewelry_ = true;
        overlay.legendary_final_boss_ = true;
        overlay.god_mode_ = true;
        overlay.choice_delay_ms_ = 250;
        overlay.time_scale_ = 5.0;
        check(overlay.publish_automation_controls(), "invalid priority draft does not block immediate controls");
        const auto immediate = state.get_automation();
        check(immediate->apply_to_party && immediate->auto_choose_spells
            && immediate->auto_choose_jewelry && immediate->legendary_final_boss
            && immediate->god_mode
            && immediate->choice_delay_ms == 250 && immediate->time_scale == 5.0,
            "all immediate controls publish without applying priorities");
        check(immediate->spell_priorities == original.spell_priorities
            && immediate->jewelry_priorities == original.jewelry_priorities,
            "immediate controls preserve both accepted priority lists");
        check(invalid_draft == overlay.spell_priority_text_.data()
            && std::strcmp(overlay.jewelry_priority_text_.data(), "new_ring") == 0,
            "immediate controls preserve both unpublished priority drafts");
        check(!overlay.apply_automation_priorities() && state.get_automation() == immediate,
            "invalid priority application leaves the exact live settings snapshot unchanged");

        automation_settings newer_live = *immediate;
        newer_live.apply_to_party = false;
        newer_live.auto_choose_spells = false;
        newer_live.auto_choose_jewelry = false;
        newer_live.legendary_final_boss = false;
        newer_live.god_mode = false;
        newer_live.choice_delay_ms = 750;
        newer_live.time_scale = 2.0;
        check(state.set_automation(newer_live), "fixture updates live controls independently of cached editors");
        strcpy_s(overlay.spell_priority_text_.data(), overlay.spell_priority_text_.size(), " frostbolt,\nwrath ");
        check(overlay.apply_automation_priorities(), "valid priority drafts publish together");
        const auto prioritized = state.get_automation();
        check(!prioritized->apply_to_party && !prioritized->auto_choose_spells
            && !prioritized->auto_choose_jewelry && !prioritized->legendary_final_boss
            && !prioritized->god_mode
            && prioritized->choice_delay_ms == 750 && prioritized->time_scale == 2.0,
            "priority application preserves fresh live settings rather than stale UI controls");
        check(prioritized->spell_priorities == std::vector<std::string>{"frostbolt", "wrath"}
            && prioritized->jewelry_priorities == std::vector<std::string>{"new_ring"},
            "priority application trims separators and commits both ordered lists");

        strcpy_s(overlay.spell_priority_text_.data(), overlay.spell_priority_text_.size(), "unpublished_spell");
        overlay.refresh_automation_controls();
        check(!overlay.auto_choose_spells_ && !overlay.god_mode_ && overlay.time_scale_ == 2.0
            && std::strcmp(overlay.spell_priority_text_.data(), "unpublished_spell") == 0,
            "refreshing live controls preserves unfinished priority text");
        overlay.time_scale_ = 17.0;
        check(!overlay.publish_automation_controls() && overlay.time_scale_ == 2.0
            && state.get_automation() == prioritized
            && std::strcmp(overlay.spell_priority_text_.data(), "unpublished_spell") == 0,
            "rejected live controls roll back without publishing or losing priority drafts");

        strcpy_s(overlay.spell_priority_text_.data(), overlay.spell_priority_text_.size(), "accepted_spell");
        std::memcpy(overlay.jewelry_priority_text_.data(), invalid_draft.c_str(), invalid_draft.size() + 1);
        check(!overlay.apply_automation_priorities() && state.get_automation() == prioritized,
            "invalid second priority list cannot partially publish the first list");
        check_field_edits();
        state.reset_all();
    } catch (...) {
        std::fputs("FAIL: unexpected automation regression exception\n", stderr);
        return 1;
    }
    return failures_ == 0 ? 0 : 1;
}

} // namespace azs

int main() {
    return azs::overlay_automation_tests::run();
}
