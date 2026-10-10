#include "party/party_options.h"

#include <cmath>

namespace moonclient {
namespace {

using ro::ui::OptionsWindowAction;
using ro::ui::PartyFriendTab;

[[nodiscard]] int Rounded(float value) { return static_cast<int>(std::lround(value)); }

}  // namespace

PartyFriendTab PartyOptions::tab() const noexcept {
  return tab_ == static_cast<int>(PartyFriendTab::kFriends) ? PartyFriendTab::kFriends
                                                            : PartyFriendTab::kParty;
}

bool PartyOptions::NoteTab(PartyFriendTab tab) noexcept {
  const int seen = static_cast<int>(tab);
  if (seen == tab_) {
    return false;
  }
  tab_ = seen;
  return true;
}

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
void PartyOptions::DeclareWindow(SettingsFile& into) {
  into.Declare("party_tab", &tab_);
  into.Declare("party_size_locked", &window_.lock_size);
  into.Declare("party_icon_class", &window_.show_job_icon);
  into.Declare("party_head", &window_.head_mode);
  into.Declare("party_level", &window_.show_level);
  into.Declare("party_map", &window_.map_mode);
  into.Declare("party_tooltip", &window_.show_tooltip);
  into.Declare("party_click_target", &window_.click_targets);
  into.Declare("party_statuses", &window_.show_buffs);
  into.Declare("party_statuses_size", &window_.buff_px);
  into.Declare("party_statuses_number", &window_.buff_max);
  into.Declare("party_statuses_lines", &window_.buff_rows);
  into.Declare("party_statuses_time", &window_.buff_time);
  into.Declare("party_statuses_greying", &window_.buff_sweep);
  into.Declare("party_statuses_opacity", &window_.buff_opacity_percent);
  into.Declare("party_statuses_time_auto", &window_.buff_time_auto);
  into.Declare("party_statuses_time_size", &window_.buff_time_px);
  into.Declare("party_statuses_time_background", &window_.buff_time_background);
  into.Declare("party_statuses_background_icon", &window_.buff_icon_background);
  into.Declare("party_statuses_veil", &window_.buff_sweep_color);
  into.Declare("party_icon_size", &window_.icon_px);
  into.Declare("party_line_spacing", &window_.row_spacing);
  into.Declare("party_bar_hp", &window_.show_hp_bar);
  into.Declare("party_text_hp", &window_.hp_text_mode);
  into.Declare("party_bar_sp", &window_.show_sp);
  into.Declare("party_text_sp", &window_.sp_text_mode);
  into.Declare("party_gauges_width", &window_.bar_w);
  into.Declare("party_gauges_height", &window_.bar_h);
  into.Declare("party_gauges_stuck", &window_.bars_stacked);
  into.Declare("party_text_within_gauges", &window_.text_in_bars);
  into.Declare("party_text_size", &window_.text_px);
}

void PartyOptions::DeclareGrid(SettingsFile& into) {
  into.Declare("grid_party", &grid_.shown);
  into.Declare("grid_party_locked", &grid_.locked);
  into.Declare("grid_party_x", &grid_.rect.x);
  into.Declare("grid_party_y", &grid_.rect.y);
  into.Declare("grid_party_width", &grid_.rect.width);
  into.Declare("grid_party_height", &grid_.rect.height);
  into.Declare("grid_party_anchor_x", &grid_.rect.across);
  into.Declare("grid_party_anchor_y", &grid_.rect.down);
  into.Declare("grid_party_columns", &grid_.columns);
  into.Declare("grid_party_tile_width", &grid_.tile_width_px);
  into.Declare("grid_party_tile_height", &grid_.tile_height_px);
  into.Declare("grid_party_spacing", &grid_.gap_px);
  into.Declare("grid_party_self", &grid_.show_self);
  into.Declare("grid_party_out_of_line", &grid_.show_offline);
  into.Declare("grid_party_icon_class", &grid_.show_job_icon);
  into.Declare("grid_party_level", &grid_.show_level);
  into.Declare("grid_party_text_hp", &grid_.hp_text);
  into.Declare("grid_party_text_size", &grid_.text_px);
  into.Declare("grid_party_spell_tile", &grid_.cast_on_tile);
  into.Declare("grid_party_clickable", &grid_.clickable);
  into.Declare("grid_party_sp", &grid_.show_sp);
  into.Declare("grid_party_tooltip", &grid_.show_tooltip);
  into.Declare("grid_party_sp_height", &grid_.sp_bar_px);
  into.Declare("grid_party_statuses", &grid_.show_statuses);
  into.Declare("grid_party_statuses_size", &grid_.status_px);
  into.Declare("grid_party_statuses_number", &grid_.status_most);
  into.Declare("grid_party_statuses_lines", &grid_.status_rows);
  into.Declare("grid_party_statuses_time", &grid_.status_countdown);
  into.Declare("grid_party_statuses_greying", &grid_.status_sweep);
  into.Declare("grid_party_statuses_opacity", &grid_.status_opacity_percent);
  into.Declare("grid_party_statuses_time_auto", &grid_.status_countdown_auto);
  into.Declare("grid_party_statuses_time_size", &grid_.status_countdown_px);
  into.Declare("grid_party_statuses_time_background", &grid_.status_countdown_background);
  into.Declare("grid_party_statuses_background_icon", &grid_.status_icon_background);
  into.Declare("grid_party_statuses_veil", &grid_.status_sweep_color);
  into.Declare("grid_party_background_frame", &grid_.frame_background);
  into.Declare("grid_party_background_tile", &grid_.tile_background);
  into.Declare("grid_party_life_high", &grid_.hp_high);
  into.Declare("grid_party_life_average", &grid_.hp_mid);
  into.Declare("grid_party_life_low", &grid_.hp_low);
  into.Declare("grid_party_color_sp", &grid_.sp);
  into.Declare("grid_party_text", &grid_.text);
  into.Declare("grid_party_text_far", &grid_.far_text);
  into.Declare("grid_party_text_out_of_line", &grid_.offline_text);
  into.Declare("grid_party_edge_self", &grid_.self_border);
  into.Declare("grid_party_threshold_medium", &grid_.hp_mid_percent);
  into.Declare("grid_party_threshold_bottom", &grid_.hp_low_percent);
}

bool PartyOptions::Change(const OptionsWindowAction& asked) {
  if (asked.changed >= kPartyFriendRowBase &&
      asked.changed < kPartyFriendRowBase + static_cast<int>(PartyFriendRow::kCount)) {
    return ChangeWindow(static_cast<PartyFriendRow>(asked.changed - kPartyFriendRowBase), asked);
  }
  if (asked.changed >= kPartyGridRowBase &&
      asked.changed < kPartyGridRowBase + static_cast<int>(PartyGridRow::kCount)) {
    return ChangeGrid(static_cast<PartyGridRow>(asked.changed - kPartyGridRowBase), asked);
  }
  return false;
}

bool PartyOptions::ChangeWindow(PartyFriendRow row, const OptionsWindowAction& asked) {
  switch (row) {
    case PartyFriendRow::kLockSize:
      window_.lock_size = asked.on;
      break;
    case PartyFriendRow::kJobIcon:
      window_.show_job_icon = asked.on;
      break;
    case PartyFriendRow::kHeadMode:
      window_.head_mode = asked.chosen;
      break;
    case PartyFriendRow::kLevel:
      window_.show_level = asked.on;
      break;
    case PartyFriendRow::kMapMode:
      window_.map_mode = asked.chosen;
      break;
    case PartyFriendRow::kTooltip:
      window_.show_tooltip = asked.on;
      break;
    case PartyFriendRow::kClickTargets:
      window_.click_targets = asked.on;
      break;
    case PartyFriendRow::kBuffs:
      window_.show_buffs = asked.on;
      break;
    case PartyFriendRow::kBuffPreview:
      window_preview_ = asked.on;
      break;
    case PartyFriendRow::kBuffPixels:
      window_.buff_px = Rounded(asked.value);
      break;
    case PartyFriendRow::kBuffMost:
      window_.buff_max = Rounded(asked.value);
      break;
    case PartyFriendRow::kBuffRows:
      window_.buff_rows = Rounded(asked.value);
      break;
    case PartyFriendRow::kBuffTime:
      window_.buff_time = asked.on;
      break;
    case PartyFriendRow::kBuffSweep:
      window_.buff_sweep = asked.chosen;
      break;
    case PartyFriendRow::kBuffOpacity:
      window_.buff_opacity_percent = Rounded(asked.value);
      break;
    case PartyFriendRow::kBuffTimeAuto:
      window_.buff_time_auto = asked.on;
      break;
    case PartyFriendRow::kBuffTimePixels:
      window_.buff_time_px = Rounded(asked.value);
      break;
    case PartyFriendRow::kBuffTimeBackground:
      window_.buff_time_background = asked.color;
      break;
    case PartyFriendRow::kBuffIconBackground:
      window_.buff_icon_background = asked.color;
      break;
    case PartyFriendRow::kBuffSweepColor:
      window_.buff_sweep_color = asked.color;
      break;
    case PartyFriendRow::kIconPixels:
      window_.icon_px = Rounded(asked.value);
      break;
    case PartyFriendRow::kRowSpacing:
      window_.row_spacing = Rounded(asked.value);
      break;
    case PartyFriendRow::kHpBar:
      window_.show_hp_bar = asked.on;
      break;
    case PartyFriendRow::kHpText:
      window_.hp_text_mode = asked.chosen;
      break;
    case PartyFriendRow::kSpBar:
      window_.show_sp = asked.on;
      break;
    case PartyFriendRow::kSpText:
      window_.sp_text_mode = asked.chosen;
      break;
    case PartyFriendRow::kBarWidth:
      window_.bar_w = Rounded(asked.value);
      break;
    case PartyFriendRow::kBarHeight:
      window_.bar_h = Rounded(asked.value);
      break;
    case PartyFriendRow::kBarsStacked:
      window_.bars_stacked = asked.on;
      break;
    case PartyFriendRow::kTextInBars:
      window_.text_in_bars = asked.on;
      break;
    case PartyFriendRow::kTextPixels:
      window_.text_px = Rounded(asked.value);
      break;
    case PartyFriendRow::kCount:
      return false;
  }
  return true;
}

bool PartyOptions::ChangeGrid(PartyGridRow row, const OptionsWindowAction& asked) {
  switch (row) {
    case PartyGridRow::kShown:
      grid_.shown = asked.on;
      break;
    case PartyGridRow::kLocked:
      grid_.locked = asked.on;
      break;
    case PartyGridRow::kColumns:
      grid_.columns = Rounded(asked.value);
      break;
    case PartyGridRow::kTileWidth:
      grid_.tile_width_px = Rounded(asked.value);
      break;
    case PartyGridRow::kTileHeight:
      grid_.tile_height_px = Rounded(asked.value);
      break;
    case PartyGridRow::kGap:
      grid_.gap_px = Rounded(asked.value);
      break;
    case PartyGridRow::kJobIcon:
      grid_.show_job_icon = asked.on;
      break;
    case PartyGridRow::kSelf:
      grid_.show_self = asked.on;
      break;
    case PartyGridRow::kOffline:
      grid_.show_offline = asked.on;
      break;
    case PartyGridRow::kLevel:
      grid_.show_level = asked.on;
      break;
    case PartyGridRow::kHpText:
      grid_.hp_text = asked.chosen;
      break;
    case PartyGridRow::kTextPixels:
      grid_.text_px = Rounded(asked.value);
      break;
    case PartyGridRow::kTooltip:
      grid_.show_tooltip = asked.on;
      break;
    case PartyGridRow::kSp:
      grid_.show_sp = asked.on;
      break;
    case PartyGridRow::kSpBar:
      grid_.sp_bar_px = Rounded(asked.value);
      break;
    case PartyGridRow::kCastOnTile:
      grid_.cast_on_tile = asked.on;
      break;
    case PartyGridRow::kClickable:
      grid_.clickable = asked.on;
      break;
    case PartyGridRow::kStatuses:
      grid_.show_statuses = asked.on;
      break;
    case PartyGridRow::kFakeMembers:
      grid_fake_members_ = asked.on;
      break;
    case PartyGridRow::kStatusPreview:
      grid_preview_ = asked.on;
      break;
    case PartyGridRow::kStatusPixels:
      grid_.status_px = Rounded(asked.value);
      break;
    case PartyGridRow::kStatusMost:
      grid_.status_most = Rounded(asked.value);
      break;
    case PartyGridRow::kStatusRows:
      grid_.status_rows = Rounded(asked.value);
      break;
    case PartyGridRow::kStatusTime:
      grid_.status_countdown = asked.on;
      break;
    case PartyGridRow::kStatusSweep:
      grid_.status_sweep = asked.chosen;
      break;
    case PartyGridRow::kStatusOpacity:
      grid_.status_opacity_percent = Rounded(asked.value);
      break;
    case PartyGridRow::kStatusTimeAuto:
      grid_.status_countdown_auto = asked.on;
      break;
    case PartyGridRow::kStatusTimePixels:
      grid_.status_countdown_px = Rounded(asked.value);
      break;
    case PartyGridRow::kStatusTimeBackground:
      grid_.status_countdown_background = asked.color;
      break;
    case PartyGridRow::kStatusIconBackground:
      grid_.status_icon_background = asked.color;
      break;
    case PartyGridRow::kStatusSweepColor:
      grid_.status_sweep_color = asked.color;
      break;
    case PartyGridRow::kFrameBackground:
      grid_.frame_background = asked.color;
      break;
    case PartyGridRow::kTileBackground:
      grid_.tile_background = asked.color;
      break;
    case PartyGridRow::kHpHigh:
      grid_.hp_high = asked.color;
      break;
    case PartyGridRow::kHpMid:
      grid_.hp_mid = asked.color;
      break;
    case PartyGridRow::kHpLow:
      grid_.hp_low = asked.color;
      break;
    case PartyGridRow::kSpColor:
      grid_.sp = asked.color;
      break;
    case PartyGridRow::kText:
      grid_.text = asked.color;
      break;
    case PartyGridRow::kFarText:
      grid_.far_text = asked.color;
      break;
    case PartyGridRow::kOfflineText:
      grid_.offline_text = asked.color;
      break;
    case PartyGridRow::kSelfBorder:
      grid_.self_border = asked.color;
      break;
    case PartyGridRow::kMidPercent:
      grid_.hp_mid_percent = Rounded(asked.value);
      break;
    case PartyGridRow::kLowPercent:
      grid_.hp_low_percent = Rounded(asked.value);
      break;
    case PartyGridRow::kCount:
      return false;
  }
  return true;
}

}  // namespace moonclient
