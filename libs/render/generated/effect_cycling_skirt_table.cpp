// LES JUPES QUI DEFILENT : ce que chaque site d'appel passe a son batisseur.
//
// 2 jupes pour 1 effets, toutes du sous-type 144. L'IMAGE VIENT DU
// BATISSEUR — il en formate vingt-sept ou vingt-neuf selon le style — et le
// deuxieme argument du site ne sert a rien : il n'est lu nulle part. Ce que
// le style et l'alpha decident vit dans le semis, voir
// effect_cycling_skirt_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_cycling_skirts.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_cycling_skirt_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectCyclingSkirtPattern, 2> kCyclingSkirts = {{
    {830, "Effect_InitWaterBubblesOrTop", 45, 1},
    {830, "Effect_InitWaterBubblesOrTop", 25, 0},
}};

}  // namespace

std::span<const EffectCyclingSkirtPattern> EffectCyclingSkirtPatternTable() {
  return kCyclingSkirts;
}

}  // namespace ro::render
