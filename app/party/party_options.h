#pragma once

#include <span>
#include <vector>

#include "ro/ui/options_window.h"
#include "ro/ui/party_friend_window.h"
#include "ro/ui/party_grid.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LES DEUX PAGES DU GROUPE : « GROUPE / AMIS » ET « GROUPE (GRILLE) » ─────
//
// La grille se déplace à la souris : sa place et son nombre de
// colonnes se persistent sans paraître ici.
//
// ⚠ LES LIGNES PORTENT UN IDENTIFIANT COMPOSÉ, hors de `GameOption`, dans deux
// plages qui suivent celle du bandeau de carte (voir `option_pages.h`).
inline constexpr int kPartyFriendRowBase = 6400;
inline constexpr int kPartyGridRowBase = 6500;

enum class PartyFriendRow : int {
  kLockSize = 0,
  kJobIcon,
  kHeadMode,
  kLevel,
  kMapMode,
  kTooltip,
  kClickTargets,
  kBuffs,
  kBuffPreview,
  kBuffPixels,
  kBuffMost,
  kBuffRows,
  kBuffOpacity,
  kBuffTime,
  kBuffTimeAuto,
  kBuffTimePixels,
  kBuffTimeBackground,
  kBuffIconBackground,
  kBuffSweep,
  kBuffSweepColor,
  kIconPixels,
  kRowSpacing,
  kHpBar,
  kHpText,
  kSpBar,
  kSpText,
  kBarWidth,
  kBarHeight,
  kBarsStacked,
  kTextInBars,
  kTextPixels,
  kCount,
};
static_assert(kPartyFriendRowBase + static_cast<int>(PartyFriendRow::kCount) <= kPartyGridRowBase,
              "la plage de la fenêtre Groupe / Amis chevauche celle de la grille");

enum class PartyGridRow : int {
  kShown = 0,
  kLocked,
  kFakeMembers,
  kColumns,
  kTileWidth,
  kTileHeight,
  kGap,
  kJobIcon,
  kSelf,
  kOffline,
  kLevel,
  kHpText,
  kTextPixels,
  kTooltip,
  kSp,
  kSpBar,
  kCastOnTile,
  kClickable,
  kStatuses,
  kStatusPreview,
  kStatusPixels,
  kStatusMost,
  kStatusRows,
  kStatusOpacity,
  kStatusTime,
  kStatusTimeAuto,
  kStatusTimePixels,
  kStatusTimeBackground,
  kStatusIconBackground,
  kStatusSweep,
  kStatusSweepColor,
  kFrameBackground,
  kTileBackground,
  kHpHigh,
  kHpMid,
  kHpLow,
  kSpColor,
  kText,
  kFarText,
  kOfflineText,
  kSelfBorder,
  kMidPercent,
  kLowPercent,
  kCount,
};

[[nodiscard]] constexpr int PartyFriendRowId(PartyFriendRow row) {
  return kPartyFriendRowBase + static_cast<int>(row);
}
[[nodiscard]] constexpr int PartyGridRowId(PartyGridRow row) {
  return kPartyGridRowBase + static_cast<int>(row);
}

class PartyOptions {
 public:
  // ⚠ MUTABLES : la grille écrit sa place quand on la déplace, et la fenêtre
  // allume la grille depuis sa propre case.
  [[nodiscard]] ro::ui::PartyFriendLook& window() noexcept { return window_; }
  [[nodiscard]] const ro::ui::PartyFriendLook& window() const noexcept { return window_; }
  [[nodiscard]] ro::ui::PartyGridLook& grid() noexcept { return grid_; }
  [[nodiscard]] const ro::ui::PartyGridLook& grid() const noexcept { return grid_; }

  // Les faux états, le temps de régler. ⚠ JAMAIS PERSISTÉS : retrouvés
  // allumés, ils feraient croire à des buffs qu'on n'a pas.
  [[nodiscard]] bool window_preview() const noexcept { return window_preview_; }
  [[nodiscard]] bool grid_preview() const noexcept { return grid_preview_; }
  // Le mode réglage de la grille : des faux membres dans les cases. ⚠ JAMAIS
  // PERSISTÉ non plus.
  [[nodiscard]] bool grid_fake_members() const noexcept { return grid_fake_members_; }

  // L'onglet ouvert de la fenêtre, retrouvé d'une partie à l'autre. Rend vrai
  // quand il a changé : il faut alors toucher le fichier.
  [[nodiscard]] ro::ui::PartyFriendTab tab() const noexcept;
  [[nodiscard]] bool NoteTab(ro::ui::PartyFriendTab tab) noexcept;

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut. EN
  // DEUX, une par page du panneau : la fenêtre, puis la grille.
  void DeclareWindow(SettingsFile& into);
  void DeclareGrid(SettingsFile& into);

  // Refait les lignes des deux pages, à chaque image où le panneau est ouvert.
  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> window_rows() const noexcept {
    return window_rows_;
  }
  [[nodiscard]] std::span<const ro::ui::OptionRowView> grid_rows() const noexcept {
    return grid_rows_;
  }

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  void ComposeWindow();
  void ComposeGrid();
  [[nodiscard]] bool ChangeWindow(PartyFriendRow row, const ro::ui::OptionsWindowAction& asked);
  [[nodiscard]] bool ChangeGrid(PartyGridRow row, const ro::ui::OptionsWindowAction& asked);

  ro::ui::PartyFriendLook window_;
  ro::ui::PartyGridLook grid_;
  bool window_preview_ = false;
  bool grid_preview_ = false;
  bool grid_fake_members_ = false;
  int tab_ = static_cast<int>(ro::ui::PartyFriendTab::kParty);

  std::vector<ro::ui::OptionRowView> window_rows_;
  std::vector<ro::ui::OptionRowView> grid_rows_;
  // Refaits à chaque composition : le pointeur d'un texte traduit meurt au
  // changement de langue.
  std::vector<const char*> head_names_;
  std::vector<const char*> map_names_;
  std::vector<const char*> gauge_names_;
  std::vector<const char*> sweep_names_;
};

}  // namespace moonclient
