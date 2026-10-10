#pragma once

// UN SKILL DE COMPAGNON QUI ATTEND SA CIBLE.
//
// Lancé depuis l'onglet ou une barre, un skill d'homoncule ou de mercenaire qui
// vise un acteur ou une case ne part pas tout de suite : il s'ARME, et le clic
// gauche suivant le donne à l'IA du compagnon, qui s'approche jusqu'à la portée
// mesurée depuis LUI. Le personnage ne marche pas.

#include <cstdint>

#include "ro/sim/companion_ai.h"

namespace moonclient {

struct CompanionAim {
  ro::sim::AiCompanion companion = ro::sim::AiCompanion::kHomunculus;
  std::uint16_t skill_id = 0;
  // Le niveau qui partira, déjà borné à l'appris.
  std::uint16_t skill_level = 0;
  int range_cells = 0;
  // Il vise une case ; sinon un acteur.
  bool on_ground = false;

  [[nodiscard]] friend bool operator==(const CompanionAim&, const CompanionAim&) = default;
};

}  // namespace moonclient
