#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/render/actor_pick.h"
#include "ro/ui/entity_sample.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/cast_bar_options.h"

namespace moonclient {

// ── LA PRÉCISION DU CIBLAGE, dans la page « Entités » ───────────────────────
//
// Fondue dans la page qui règle déjà ce qui se pose sur une entité : son cadre
// d'essai montre aussi la zone qui répond au clic. De combien le jeu ÉLARGIT la
// surface qui répond au clic de chaque entité. Le client porte toute zone
// cliquable plus petite qu'un plancher jusqu'à lui — quarante pixels d'une
// fenêtre de 640 de large pour un acteur, trente-quatre pour un item au sol ou
// une unité de compétence. Un œuf de vingt-cinq pixels se défend donc sur cent,
// et deux entités voisines se disputent le même clic.
//
// Le réglage divise ce plancher par deux, par cran : le contrat du patch WARP
// `TighterClickArea`, pour que les deux se décrivent pareil.
inline constexpr int kPickPrecisionRowBase = 7000;

enum class PickPrecisionRow : int {
  kShift = 0,
  kCount,
};

static_assert(kCastBarRowEnd <= kPickPrecisionRowBase,
              "la plage de la précision du ciblage chevauche celle de la barre de cast");

[[nodiscard]] constexpr int PickPrecisionRowId(PickPrecisionRow row) {
  return kPickPrecisionRowBase + static_cast<int>(row);
}

inline constexpr std::size_t kPickShiftCount =
    static_cast<std::size_t>(ro::render::kLargestPickFloorShift) + 1;

class PickPrecisionOptions {
 public:
  // Le cran : zéro pour le client tel quel, un pour la moitié, jusqu'au
  // seizième. ⚠ BORNÉ : un fichier retouché à la main peut porter n'importe quoi.
  [[nodiscard]] int floor_shift() const noexcept;

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Refait les lignes pour cette largeur de fenêtre : ses planchers sont les
  // chiffres que la page annonce. ⚠ ELLES S'AJOUTENT AUX RÉGLAGES COMMUNS de la
  // page « Entités » : elles valent pour toutes les espèces.
  void Compose(float viewport_width);

  // LES ZONES QUE LE CADRE D'ESSAI DESSINE pour cette famille — celle d'origine
  // et celle du réglage —, dans cette fenêtre.
  [[nodiscard]] ro::ui::EntityPickZones ZonesFor(float factor, float viewport_width) const;
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  int floor_shift_ = 0;
  // L'infobulle de la liste : les chiffres de CETTE fenêtre, puis la légende du
  // cadre. ⚠ ICI : la rangée n'en garde qu'un pointeur.
  std::string help_;
  // Les crans, chacun avec sa taille en pixels : « Moitié (50 px) » se compare,
  // « Moitié » se devine. ⚠ ICI ET NON SUR LA PILE : la rangée n'en garde
  // qu'une vue.
  std::array<std::string, kPickShiftCount> choice_texts_;
  std::array<const char*, kPickShiftCount> choices_{};
  std::vector<ro::ui::OptionRowView> rows_;
};

}  // namespace moonclient
