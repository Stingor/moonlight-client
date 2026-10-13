#pragma once

#include <span>
#include <vector>

#include "ro/ui/menu_icons.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/bug_report_options.h"
#include "options/factory_layout.h"

namespace moonclient {

// ── LA PAGE « ICÔNES DU MENU » ──────────────────────────────────────────────
//
// Le verrou qui fait passer les icônes du clic à la pose, l'aimant entre elles,
// une case par icône pour la montrer ou la masquer, et le retour à la grille
// d'origine.
//
// ⚠ LA PLACE DE CHAQUE ICÔNE SE PERSISTE SANS PARAÎTRE ICI, comme celle des
// jauges : on la pose à la souris.
inline constexpr int kMenuIconsRowBase = 6200;

enum class MenuIconsRow : int {
  kLocked = 0,
  kSticky,
  kForgetPlaces,
  // Puis une case par commande du catalogue, au rang de la commande.
  kFirstIcon,
  kCount = kFirstIcon + static_cast<int>(ro::ui::kMenuCommandCount),
};

static_assert(kBugReportRowBase + static_cast<int>(BugReportRow::kCount) <= kMenuIconsRowBase,
              "la plage des icônes du menu chevauche celle du rapport de bug");

[[nodiscard]] constexpr int MenuIconsRowId(MenuIconsRow row) {
  return kMenuIconsRowBase + static_cast<int>(row);
}

class MenuIconsOptions {
 public:
  // ⚠ MUTABLE : les icônes écrivent leur place dedans quand on les déplace.
  [[nodiscard]] ro::ui::MenuIconsLook& look() noexcept { return look_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Refait les lignes, à chaque image où le panneau est ouvert.
  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

  // `/cashshop` : montre ou cache le bouton du Vote Shop, et rend s'il est
  // montré. ⚠ LA MÊME PLACE QUE SA CASE « Icônes montrées » : une commande et
  // une case pour un seul réglage, comme dans le jeu.
  bool ToggleCashShopButton() noexcept;

 private:
  ro::ui::MenuIconsLook look_ = FactoryMenuIcons();
  std::vector<ro::ui::OptionRowView> rows_;
};

}  // namespace moonclient
