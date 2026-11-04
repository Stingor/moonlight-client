#pragma once

#include <cstdint>
#include <vector>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"

namespace moonclient {

// L'OBJECTIF DU JOUEUR : un sort épinglé au Codex. Ce qui y MÈNE, c'est le sort
// lui-même et, pour un évolué, ce qu'exige son évolution — son sort de base,
// sa carte ou le second sort de son union —, en remontant chaque évolué
// jusqu'aux sorts que la réserve offre.
struct SurvivorsObjectivePath {
  std::vector<std::uint16_t> skill_ids;
  std::vector<std::uint16_t> card_item_ids;
};

// Le chemin vers `objective_skill_id` ; vide pour 0 ou un sort inconnu.
[[nodiscard]] SurvivorsObjectivePath SurvivorsObjectivePathOf(const ro::sim::SurvivorsRules& rules,
                                                              std::uint16_t objective_skill_id);

// Vrai quand prendre cette offre avance sur le chemin : un sort ou une carte
// qui y figure. Jamais un soin ni des zenys.
[[nodiscard]] bool OfferServesObjective(const SurvivorsObjectivePath& path,
                                        const ro::sim::SurvivorsOffer& offer);

}  // namespace moonclient
