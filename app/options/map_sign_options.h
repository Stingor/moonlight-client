#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/menu_icons_options.h"
#include "world/map_sign_stage.h"

namespace moonclient {

// ── LE BANDEAU DU NOM DE LA CARTE, dans la section Interface ────────────────
//
// Une liste : toutes les cartes qui ont un titre (le défaut), celles que la
// table annonce — comme le jeu —, ou aucune. Voir `MapSignMode`.
inline constexpr int kMapSignRowBase = 6300;

enum class MapSignRow : int {
  kMode = 0,
  kCount,
};

static_assert(kMenuIconsRowBase + static_cast<int>(MenuIconsRow::kCount) <= kMapSignRowBase,
              "la plage du bandeau de carte chevauche celle des icônes du menu");

inline constexpr std::size_t kMapSignModeCount = static_cast<std::size_t>(MapSignMode::kCount);

class MapSignOptions {
 public:
  // ⚠ BORNÉ : un rang hors liste venu du fichier vaut le défaut.
  [[nodiscard]] MapSignMode mode() const noexcept;

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Ajoute sa ligne à celles d'une section.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows);

  // Rend vrai quand la ligne était la sienne.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  int mode_ = static_cast<int>(MapSignMode::kEveryTitled);
  // Les libellés de la liste, traduits à chaque composition. ⚠ ICI ET NON SUR
  // LA PILE : la ligne n'en garde qu'une vue, lue après la composition.
  std::array<const char*, kMapSignModeCount> mode_names_{};
};

}  // namespace moonclient
