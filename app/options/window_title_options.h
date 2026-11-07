#pragma once

#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/pet_options.h"

namespace moonclient {

// ── LE TITRE DE LA FENÊTRE, dans la section Interface ───────────────────────
//
// Deux cases. Le pseudo et la carte en tête du titre — « Stingor - prontera -
// Moonlight-Destiny a8de56e6c012 » —, lisibles dans la barre des tâches quand
// deux clients tournent. Le hash du commit bâti après le nom du jeu : il dit
// d'un coup d'œil de quel binaire vient ce qu'on regarde. Voir `WindowTitle`.
inline constexpr int kWindowTitleRowBase = 8000;

enum class WindowTitleRow : int {
  kNamesShown = 0,
  kRevisionShown,
  kCount,
};

static_assert(kPetRowBase + static_cast<int>(PetRow::kCount) <= kWindowTitleRowBase,
              "la plage du titre de la fenêtre chevauche celle de la fiche du pet");

class WindowTitleOptions {
 public:
  [[nodiscard]] bool names_shown() const noexcept { return names_shown_; }
  [[nodiscard]] bool revision_shown() const noexcept { return revision_shown_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Ajoute ses lignes à celles d'une section.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const;

  // Rend vrai quand la ligne était la sienne.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  bool names_shown_ = true;
  bool revision_shown_ = true;
};

}  // namespace moonclient
