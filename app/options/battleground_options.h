#pragma once

#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LE CHAMP DE BATAILLE, DANS « GAMEPLAY » ─────────────────────────────────
//
// Ce que le combat montre : le score et le surlignage de notre camp, les
// coéquipiers sur la minimap, la marque au-dessus des ennemis. Tout est montré
// par défaut.
//
// ⚠ LES LIGNES PORTENT UN IDENTIFIANT COMPOSÉ, hors de `GameOption`. La plage
// est chaînée à celle du carnet MVP dans `option_pages.h` : deux pages sur la
// même plage se volent leurs lignes.
inline constexpr int kBattlegroundRowBase = 7800;

enum class BattlegroundRow : int {
  kScoreShown = 0,
  kOwnSideHighlighted,
  kMinimapMates,
  kEnemyMarker,
  kCount,
};

[[nodiscard]] constexpr int BattlegroundRowId(BattlegroundRow row) {
  return kBattlegroundRowBase + static_cast<int>(row);
}

struct BattlegroundSettings {
  bool score_shown = true;
  bool own_side_highlighted = true;
  bool minimap_mates = true;
  bool enemy_marker = true;
};

class BattlegroundOptions {
 public:
  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);
  // Ajoute ses lignes à celles d'une section.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const;
  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);
  [[nodiscard]] const BattlegroundSettings& settings() const noexcept { return settings_; }

 private:
  BattlegroundSettings settings_;
};

}  // namespace moonclient
