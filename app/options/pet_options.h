#pragma once

#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LE BLOC « FICHE DE PET » D'INTERFACE ────────────────────────────────────
//
// Les deux réglages de la fiche du pet : demander avant de remettre le pet dans
// son œuf, et animer son sprite. La puce de la barre de titre de la fiche mène
// à son intertitre, que porte la première rangée.
inline constexpr int kPetRowBase = 7900;

enum class PetRow : int {
  kConfirmReturnToEgg = 0,
  kAnimateSprite,
  kCount,
};

[[nodiscard]] constexpr int PetRowId(PetRow row) { return kPetRowBase + static_cast<int>(row); }

struct PetSettings {
  // ALLUMÉ par défaut : le jeu ne demandait pas, et l'erreur range le pet.
  bool confirm_return_to_egg = true;
  bool animate_sprite = true;
};

class PetOptions {
 public:
  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);
  // Ajoute ses lignes à celles d'Interface, à chaque image où le panneau est
  // ouvert.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const;
  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);
  [[nodiscard]] const PetSettings& settings() const noexcept { return settings_; }

 private:
  PetSettings settings_;
};

}  // namespace moonclient
