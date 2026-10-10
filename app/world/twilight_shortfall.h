#pragma once

#include <cstdint>
#include <vector>

#include "ro/sim/inventory.h"

namespace moonclient {

// CE QUI MANQUE À UNE TWILIGHT ALCHEMY POUR RÉUSSIR.
//
// Le serveur refuse les trois Twilight par un échec SANS MOTIF ni objet : il
// tente sa fabrication et, faute de matière, dit seulement « échec ». Le
// client, lui, connaît la recette et le sac : il peut nommer ce qui manque.
//
// Les quantités sont celles que le serveur exige, et non une recette par
// potion : deux cents White Potion ou Slim White Potion d'un coup, et pour la
// troisième, cinquante Fire Bottle et cinquante Acid Bottle avec au moins cent
// Empty Bottle au total.

// UN BESOIN : l'objet, combien il en faut, combien le sac en porte.
struct TwilightNeed {
  std::uint32_t item_id = 0;
  std::int32_t required = 0;
  std::int32_t owned = 0;
};

// Vrai pour les trois Twilight Alchemy.
[[nodiscard]] bool IsTwilightAlchemy(std::uint16_t skill_id) noexcept;

// Les besoins NON couverts par le sac, dans l'ordre de la recette ; vide quand
// rien ne manque, ou pour une autre compétence.
[[nodiscard]] std::vector<TwilightNeed> TwilightShortfall(std::uint16_t skill_id,
                                                          const ro::sim::Inventory& inventory);

}  // namespace moonclient
