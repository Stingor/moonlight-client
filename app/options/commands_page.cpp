#include "options/commands_page.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "ro/net/command_settings.h"
#include "ro/net/item_facts.h"
#include "ro/net/player_settings.h"
#include "ro/vfs/item_name.h"

namespace moonclient {
namespace {

using ro::ui::CommandsField;

// Le réglage du fil que chaque case désigne, et le champ du miroir qu'il pose.
struct FieldWire {
  CommandsField field;
  std::uint16_t id;
  bool ro::sim::CommandSettings::* flag;
};

// ⚠ LES BASCULES SEULES : les réglages chiffrés ont chacun leur conversion, et
// se traitent à part.
constexpr FieldWire kToggles[] = {
    {CommandsField::kShowExp, ro::net::kSettingShowExp, &ro::sim::CommandSettings::show_exp},
    {CommandsField::kShowZeny, ro::net::kSettingShowZeny, &ro::sim::CommandSettings::show_zeny},
    {CommandsField::kShowMobInfo, ro::net::kSettingShowMobInfo,
     &ro::sim::CommandSettings::show_mob_info},
    {CommandsField::kSeparateKills, ro::net::kSettingSeparateKills,
     &ro::sim::CommandSettings::separate_kills},
    {CommandsField::kBlockExp, ro::net::kSettingBlockExp, &ro::sim::CommandSettings::block_exp},
    {CommandsField::kShowSkillDelay, ro::net::kSettingShowSkillDelay,
     &ro::sim::CommandSettings::show_skill_delay},
    {CommandsField::kShowSpeed, ro::net::kSettingShowSpeed, &ro::sim::CommandSettings::show_speed},
    {CommandsField::kSellStuff, ro::net::kSettingSellStuff, &ro::sim::CommandSettings::sell_stuff},
    {CommandsField::kSellItem, ro::net::kSettingSellItem, &ro::sim::CommandSettings::sell_item},
    {CommandsField::kNoAsk, ro::net::kSettingNoAsk, &ro::sim::CommandSettings::no_ask},
    {CommandsField::kWings, ro::net::kSettingWings, &ro::sim::CommandSettings::wings},
    {CommandsField::kFlyWingMarker, ro::net::kSettingFlyWingMarker,
     &ro::sim::CommandSettings::fly_wing_marker},
    {CommandsField::kAutolootRares, ro::net::kSettingAutolootRares,
     &ro::sim::CommandSettings::autoloot_rares},
    {CommandsField::kAutolootMvp, ro::net::kSettingAutolootMvp,
     &ro::sim::CommandSettings::autoloot_mvp},
    {CommandsField::kAutolootMvpRewards, ro::net::kSettingAutolootMvpRewards,
     &ro::sim::CommandSettings::autoloot_mvp_rewards},
};

[[nodiscard]] ro::net::AutolootPresetOrder OrderOf(ro::ui::CommandsPresetGesture gesture) {
  switch (gesture) {
    case ro::ui::CommandsPresetGesture::kSave:
      return ro::net::AutolootPresetOrder::kSave;
    case ro::ui::CommandsPresetGesture::kLoad:
      return ro::net::AutolootPresetOrder::kLoad;
    case ro::ui::CommandsPresetGesture::kDelete:
      return ro::net::AutolootPresetOrder::kDelete;
    case ro::ui::CommandsPresetGesture::kAutoload:
      return ro::net::AutolootPresetOrder::kAutoload;
    case ro::ui::CommandsPresetGesture::kRename:
    case ro::ui::CommandsPresetGesture::kNone:
      break;
  }
  return ro::net::AutolootPresetOrder::kRename;
}

// La liste sans son ordre : deux listes aux mêmes items sont la même liste.
[[nodiscard]] std::vector<std::uint32_t> Sorted(std::vector<std::uint32_t> ids) {
  std::ranges::sort(ids);
  return ids;
}

}  // namespace

void CommandsPage::Compose(const ro::sim::ItemFacts* facts, const ro::vfs::ItemTable* items,
                           const ro::vfs::ItemWords& words) {
  FollowPresetAnnounce();
  const auto name_of = [&](std::uint32_t item_id) {
    if (items == nullptr) {
      return std::string();
    }
    return ro::vfs::ItemNameText(
        ro::vfs::ComposeItemName(ro::vfs::ItemNameFacts{.item_id = item_id}, *items, words));
  };
  if (last_viewed_named_ != last_viewed_item_) {
    last_viewed_named_ = last_viewed_item_;
    last_viewed_name_ = name_of(last_viewed_item_);
  }

  const std::vector<std::uint32_t> empty;
  const std::vector<std::uint32_t>& listed = facts != nullptr ? facts->autoloot() : empty;
  names_.clear();
  names_.reserve(listed.size());
  for (const std::uint32_t item_id : listed) {
    names_.push_back(name_of(item_id));
  }
  // ⚠ LES VUES APRÈS LES TAMPONS : une vue prise pendant le remplissage pendrait
  // à la première réallocation.
  entries_.clear();
  entries_.reserve(listed.size());
  for (std::size_t rank = 0; rank < listed.size(); ++rank) {
    entries_.push_back(ro::ui::CommandsAutolootEntry{
        .item_id = listed[rank], .name = names_[rank].empty() ? nullptr : names_[rank].c_str()});
  }
  presets_.clear();
  presets_.reserve(settings_.presets.size());
  for (const ro::sim::AutolootPreset& preset : settings_.presets) {
    presets_.push_back(ro::ui::CommandsPresetView{
        .slot = preset.slot, .name = preset.name.c_str(), .autoload = preset.autoload});
  }

  const ro::sim::CommandSettings& told = settings_;
  view_ = ro::ui::CommandsPageView{
      // ⚠ LA LISTE AUSSI DOIT AVOIR ÉTÉ DITE : sans elle, « Clear » et « Add »
      // parleraient d'une liste que personne ne connaît.
      .told = told.told && facts != nullptr && facts->autoloot_told(),
      .show_exp = told.show_exp,
      .show_zeny = told.show_zeny,
      .show_mob_info = told.show_mob_info,
      .separate_kills = told.separate_kills,
      .block_exp = told.block_exp,
      .show_skill_delay = told.show_skill_delay,
      .show_speed = told.show_speed,
      .sell_stuff = told.sell_stuff,
      .sell_item = told.sell_item,
      .no_ask = told.no_ask,
      .wings = told.wings,
      .fly_wing_marker = told.fly_wing_marker,
      .kill_steal_guard = static_cast<int>(told.kill_steal_guard),
      .autoloot_rate_percent = told.autoloot_rate_percent,
      .autoloot_min_zeny = told.autoloot_min_zeny,
      .autoloot_type_mask = told.autoloot_type_mask,
      .autoloot_rares = told.autoloot_rares,
      .autoloot_mvp = told.autoloot_mvp,
      .autoloot_mvp_rewards = told.autoloot_mvp_rewards,
      .autoloot = entries_,
      .autoloot_unsaved = told.active_preset != 0 && Sorted(listed) != Sorted(told.saved_autoloot),
      .presets = presets_,
      .active_preset = told.active_preset,
      .last_viewed_item = last_viewed_item_,
      .last_viewed_name = last_viewed_name_.empty() ? nullptr : last_viewed_name_.c_str(),
  };
}

void CommandsPage::NoteDescribed(std::uint32_t item_id) {
  if (item_id != 0) {
    last_viewed_item_ = item_id;
  }
}

ro::ui::OptionSectionBody CommandsPage::Body() {
  return [this](const ro::ui::SkinArt& art, ro::ui::OptionBodyTabs& tabs) {
    if (tabs.wanted >= 0 && tabs.wanted < ro::ui::kCommandsTabCount) {
      state_.tab_wanted = static_cast<ro::ui::CommandsTab>(tabs.wanted);
    }
    view_.offers_links = tabs.offers_links;
    asked_ = ro::ui::DrawCommandsPage(view_, state_, art);
    if (asked_.link_tab.has_value()) {
      tabs.link = static_cast<int>(*asked_.link_tab);
    }
  };
}

void CommandsPage::FollowPresetAnnounce() {
  if (settings_.presets_told_count == presets_seen_) {
    return;
  }
  presets_seen_ = settings_.presets_told_count;
  // LE PRESET CHARGÉ SE CHOISIT DE LUI-MÊME, et son nom remplit le champ :
  // « Sauvegarder » le met alors à jour au lieu d'en créer un autre.
  state_.selected_preset = settings_.active_preset;
  for (const ro::sim::AutolootPreset& preset : settings_.presets) {
    if (preset.slot == settings_.active_preset) {
      std::snprintf(state_.preset_name.data(), state_.preset_name.size(), "%s",
                    preset.name.c_str());
      break;
    }
  }
}

void CommandsPage::ApplySetting(ro::net::Connection& link, CommandsField field, int value) {
  for (const FieldWire& toggle : kToggles) {
    if (toggle.field == field) {
      ro::net::SendPlayerSetting(link, toggle.id, value != 0 ? 1U : 0U);
      settings_.*toggle.flag = value != 0;
      return;
    }
  }
  switch (field) {
    case CommandsField::kKillStealGuard:
      ro::net::SendPlayerSetting(link, ro::net::kSettingKillStealGuard,
                                 static_cast<std::uint32_t>(value));
      settings_.kill_steal_guard = static_cast<ro::sim::KillStealGuard>(value);
      return;
    case CommandsField::kAutolootRate:
      ro::net::SendPlayerSetting(link, ro::net::kSettingAutolootRate,
                                 static_cast<std::uint32_t>(value));
      settings_.autoloot_rate_percent = value;
      return;
    case CommandsField::kAutolootMinZeny:
      ro::net::SendPlayerSetting(
          link, ro::net::kSettingAutolootMinZeny,
          static_cast<std::uint32_t>(value / ro::net::kAutolootMinZenyWireUnit));
      settings_.autoloot_min_zeny = value;
      return;
    case CommandsField::kAutolootTypes:
      ro::net::SendPlayerSetting(link, ro::net::kSettingAutolootTypes,
                                 static_cast<std::uint32_t>(value));
      settings_.autoloot_type_mask = static_cast<std::uint16_t>(value);
      return;
    default:
      return;
  }
}

void CommandsPage::Apply(ro::net::Connection& link, ro::sim::ItemFacts* facts) {
  const ro::ui::CommandsPageAction asked = std::exchange(asked_, ro::ui::CommandsPageAction{});
  if (!link.is_open() || !view_.told || facts == nullptr) {
    return;
  }
  if (asked.field != CommandsField::kNone) {
    ApplySetting(link, asked.field, asked.value);
  }
  // LA LISTE : le serveur n'accuse pas un ajout accepté, donc on le note d'avance
  // — et c'est lui qui démentira s'il refuse.
  if (asked.autoloot_clear) {
    ro::net::SendPlayerSetting(link, ro::net::kSettingAutolootAdd, 0);
    facts->TellAutoloot({});
  }
  if (asked.autoloot_add != 0) {
    ro::net::RequestAutoloot(link, asked.autoloot_add, true);
    facts->NoteAutoloot(asked.autoloot_add, true);
  }
  if (asked.autoloot_remove != 0) {
    ro::net::RequestAutoloot(link, asked.autoloot_remove, false);
    facts->NoteAutoloot(asked.autoloot_remove, false);
  }
  if (asked.preset != ro::ui::CommandsPresetGesture::kNone) {
    ro::net::SendAutolootPresetOrder(link, OrderOf(asked.preset), asked.preset_slot,
                                     asked.preset_name);
  }
}

}  // namespace moonclient
