#pragma once

#include <span>
#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LA PAGE « FICHE DE MONSTRE » ────────────────────────────────────────────
//
// Les deux cases de la fiche de monstre.
//
// ⚠ LES LIGNES PORTENT UN IDENTIFIANT COMPOSÉ, hors de `GameOption`, comme
// celles des graphismes, dont la plage précède la sienne.
inline constexpr int kMonsterSheetRowBase = 5600;

enum class MonsterSheetRow : int {
  kIntro = 0,
  kAnimate,
  kGuardians,
  kCount,
};

[[nodiscard]] constexpr int MonsterSheetRowId(MonsterSheetRow row) {
  return kMonsterSheetRowBase + static_cast<int>(row);
}

class MonsterSheetOptions {
 public:
  // Faire tourner l'animation d'attente du sprite. Le jeu d'origine en fige la
  // première image ; ALLUMÉ par défaut — c'est l'intérêt.
  [[nodiscard]] bool animate() const noexcept { return animate_; }
  // Lever le refus du jeu de montrer les cinq Gardiens de forteresse. ÉTEINT
  // par défaut : ce filtre est une règle de jeu, pas un défaut d'interface.
  [[nodiscard]] bool show_guardians() const noexcept { return show_guardians_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Refait les lignes, à chaque image où le panneau est ouvert.
  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  bool animate_ = true;
  bool show_guardians_ = false;
  std::vector<ro::ui::OptionRowView> rows_;
};

}  // namespace moonclient
