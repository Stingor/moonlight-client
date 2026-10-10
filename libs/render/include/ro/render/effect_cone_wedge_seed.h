#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_cone_wedge.h"
#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU COIN DE CÔNE, et les huit effets qui l'emploient par le
// sous-type 89.
//
// ⚠ UN EFFET POSE PLUSIEURS NŒUDS, et c'est la particularité de cette famille :
// son site d'appel BOUCLE, et passe son index au bâtisseur. Cet index entre dans
// l'INCLINAISON, si bien que les nœuds d'un même effet pointent chacun dans une
// direction — c'est l'éventail. Le nombre de tours de la boucle dit combien de
// nœuds chaque effet pose.

// UN ÉVENTAIL QU'UN EFFET POSE.
struct EffectConeWedgePattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // La variante, que la figure et le mouvement lisent tous deux.
  std::uint8_t variant = 0;
  // COMBIEN DE NŒUDS le site pose : le nombre de tours de sa boucle.
  std::uint8_t node_count = 1;
};

// Tous les éventails connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectConeWedgePattern> EffectConeWedgePatternTable();

// TOUS les éventails d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectConeWedgePattern> EffectConeWedgePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UN NŒUD, toutes vives.
//
// `spoke` est le tour de boucle du site — l'index qui décale l'inclinaison.
//
// ⚠ TROIS VARIANTES PARTENT AU TIC ZÉRO, toutes les autres à MOINS VINGT-QUATRE :
// elles attendent donc presque une demi-seconde avant de paraître.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectConeWedgeTracks(
    const EffectConeWedgePattern& pattern, std::uint8_t spoke, std::uint32_t& generator);

// LE NŒUD d'un tour de boucle, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectConeWedgeNode(const EffectConeWedgePattern& pattern,
                                                      std::uint8_t spoke, glm::vec3 anchor,
                                                      std::int32_t effect_life_ticks,
                                                      std::uint32_t& generator);

}  // namespace ro::render
