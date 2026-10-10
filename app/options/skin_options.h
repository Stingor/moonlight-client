#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/minimap_panel_options.h"

namespace moonclient {

// ── LE SKIN, dans la section Interface ──────────────────────────────────────
//
// Une liste : les dossiers de `skin\` de l'installation, le skin d'origine en
// tête. Le choix se lit avant la fenêtre — l'écran de mise à jour le montre
// déjà — et s'applique au montage des archives : il vaut donc au démarrage
// SUIVANT, et la ligne le dit.
inline constexpr int kSkinRowBase = 8200;

enum class SkinRow : int {
  kChoice = 0,
  kCount,
};

static_assert(kMinimapPanelRowBase + static_cast<int>(MinimapPanelRow::kCount) <= kSkinRowBase,
              "la plage du skin chevauche celle de la minimap");

class SkinOptions {
 public:
  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Les skins de l'installation, le skin d'origine en tête. Voir
  // `InstalledSkins`.
  void SeeInstalled(std::vector<std::string> names);

  // LE SKIN À PORTER : celui du fichier s'il est installé, sinon celui
  // d'origine — un dossier retiré à la main ne laisse pas l'interface sans
  // rien. Tant que la liste n'est pas vue, celui du fichier.
  [[nodiscard]] std::string_view active() const;

  // Ajoute sa ligne à celles d'une section.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows);

  // Rend vrai quand la ligne était la sienne.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  std::string name_;
  std::vector<std::string> installed_;
  // Les libellés de la liste. ⚠ ICI ET NON SUR LA PILE : la ligne n'en garde
  // qu'une vue, lue après la composition.
  std::vector<const char*> choices_;
};

}  // namespace moonclient
