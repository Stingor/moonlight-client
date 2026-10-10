#pragma once

#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/damage_meter_options.h"

namespace moonclient {

// ── LES ÉCHOPPES DES AUTRES : comment on les ouvre, et si on les voit ────────
//
// Deux bascules du jeu, sous leurs commandes :
//
//   · `/shopping` (ou `/sh`) : allumée, un simple clic sur une enseigne ouvre
//     l'échoppe, et un clic droit sur une enseigne referme ce qu'on visitait ;
//     éteinte, il faut un double-clic, et le clic droit ne fait rien ;
//   · `/showshop` : éteinte, les enseignes d'échoppe disparaissent — celles des
//     salons restent.
//
// ⚠ TOUTES DEUX ALLUMÉES PAR DÉFAUT, comme au client d'origine.
inline constexpr int kShopRowBase = 7200;

enum class ShopRow : int {
  kOneClick = 0,
  kSignboardsShown,
  kCount,
};

static_assert(kDamageMeterRowBase + static_cast<int>(DamageMeterRow::kCount) <= kShopRowBase,
              "la plage des échoppes chevauche celle du compteur de dégâts");

class ShopOptions {
 public:
  [[nodiscard]] bool one_click() const noexcept { return one_click_; }
  [[nodiscard]] bool signboards_shown() const noexcept { return signboards_shown_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Ajoute ses lignes à celles d'une section.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const;

  // Rend vrai quand la ligne était la sienne.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

  // La bascule d'une commande : rend la nouvelle valeur.
  bool Toggle(ShopRow row);

 private:
  bool one_click_ = true;
  bool signboards_shown_ = true;
};

}  // namespace moonclient
