#include "shell/login_settings.h"

#include "ro/core/i18n.h"

#include "game/game_settings.h"
#include "options/display_options.h"
#include "options/game_option_ids.h"
#include "options/game_options.h"
#include "options/option_pages.h"
#include "options/sprite_sound_options.h"

namespace moonclient {
namespace {

// Le rang d'une section parmi celles que montre le login.
[[nodiscard]] constexpr std::size_t ShownRank(OptionSection section) {
  for (std::size_t rank = 0; rank < kLoginSettingsSections.size(); ++rank) {
    if (kLoginSettingsSections[rank] == section) {
      return rank;
    }
  }
  return kLoginSettingsSections.size();
}

constexpr std::size_t kDisplayRank = ShownRank(OptionSection::kDisplay);
constexpr std::size_t kSoundRank = ShownRank(OptionSection::kSound);
constexpr std::size_t kSkinRank = ShownRank(OptionSection::kSkin);
static_assert(kDisplayRank < kLoginSettingsSections.size() &&
                  kSoundRank < kLoginSettingsSections.size() &&
                  kSkinRank < kLoginSettingsSections.size(),
              "les pages que le login recompose doivent en faire partie");

}  // namespace

std::span<const ro::ui::OptionSectionView> LoginSettings::Compose(GameSettings& settings,
                                                                  std::uint32_t now_ms) {
  GameOptions& options = settings.options();
  window_.revert_seconds = options.revert_seconds(now_ms);
  if (!window_.open) {
    return {};
  }
  options.Refresh(true, window_.section_tab);
  const std::span<const ro::ui::OptionSectionView> all = options.sections();
  for (std::size_t rank = 0; rank < kLoginSettingsSections.size(); ++rank) {
    sections_[rank] = all[Rank(kLoginSettingsSections[rank])];
  }

  // L'AFFICHAGE, PUIS L'ÉCHELLE : la rangée garde son intertitre « Fenêtres ».
  display_rows_.assign(sections_[kDisplayRank].rows.begin(), sections_[kDisplayRank].rows.end());
  for (const ro::ui::OptionRowView& row : all[Rank(OptionSection::kInterface)].rows) {
    if (row.id == static_cast<int>(GameOption::kInterfaceScale)) {
      display_rows_.push_back(row);
    }
  }
  sections_[kDisplayRank].rows = display_rows_;

  // LE SON, SANS LES MONSTRES NI LES PETS : on ne les entend qu'en jeu.
  sound_rows_.clear();
  for (const ro::ui::OptionRowView& row : sections_[kSoundRank].rows) {
    if (!IsSpriteSoundRow(row.id)) {
      sound_rows_.push_back(row);
    }
  }
  sections_[kSoundRank].rows = sound_rows_;

  // ⚠ LE TITRE DE FAMILLE EST PORTÉ PAR LE PREMIER MEMBRE, et celui du skin
  // — « Interface » — n'est pas montré ici.
  sections_[kSkinRank].group = ro::core::Tr("UI");
  return sections_;
}

void LoginSettings::RestoreShown(std::size_t shown_rank, GameSettings& settings) {
  SettingsFile& file = settings.file();
  settings.options().pages().RestorePage(SettingsPage(kLoginSettingsSections[shown_rank]), file,
                                         PageRestore::kShownAtLogin);
  if (shown_rank == kDisplayRank) {
    file.RestoreDefault(kInterfaceScaleKey);
  }
}

void LoginSettings::Route(const ro::ui::OptionsWindowAction& asked, GameSettings& settings,
                          const OptionTargets& to, std::uint32_t now_ms) {
  GameOptions& options = settings.options();
  if (asked.keep_display) {
    options.KeepDisplay();
  }
  if (asked.revert_display) {
    options.RevertDisplay(to);
  }
  if (asked.restore_defaults) {
    for (std::size_t rank = 0; rank < kLoginSettingsSections.size(); ++rank) {
      RestoreShown(rank, settings);
    }
    options.ApplyAll(to);
    settings.file().Touch();
  } else if (asked.restore_section >= 0 &&
             static_cast<std::size_t>(asked.restore_section) < kLoginSettingsSections.size()) {
    const auto rank = static_cast<std::size_t>(asked.restore_section);
    RestoreShown(rank, settings);
    // ⚠ L'ÉCHELLE TIENT À LA MACHINE : l'affichage la repousse avec l'écran.
    options.ApplyAll(to,
                     settings.file().PageHoldsMachine(SettingsPage(kLoginSettingsSections[rank])));
    settings.file().Touch();
  } else {
    options.Change(asked, to, now_ms);
  }
}

}  // namespace moonclient
