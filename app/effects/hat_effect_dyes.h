#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "ro/render/effect_dye.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

class EffectsInPlay;

// LA TEINTURE D'UN EFFET DE CHAPEAU QU'UN ACTEUR AFFICHE, telle que le livre des
// styles la tient : sous le gid du porteur, au calque 8, sous la clé de son
// numéro. Rien quand le livre n'en a pas pour ce numéro.
struct WornHatEffectDye {
  std::uint32_t bearer_id = 0;
  std::uint16_t hat_effect_id = 0;
  std::optional<ro::render::EffectDye> dye;
};

// Une ligne par effet de chapeau que chaque acteur affiche. ⚠ SEULS LES
// NUMÉROS AFFICHÉS : une ligne du livre sous un numéro que le porteur ne
// montre plus ne teint rien.
[[nodiscard]] std::vector<WornHatEffectDye> WornHatEffectDyes(const ro::sim::World& world);

// Pose ces teintures sur les effets en jeu, et ôte celles que le livre ne
// porte plus.
void NoticeHatEffectDyes(EffectsInPlay& effects, const ro::sim::World& world);

}  // namespace moonclient
