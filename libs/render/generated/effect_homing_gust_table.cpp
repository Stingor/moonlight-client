// LES BOURRASQUES QUI POURSUIVENT UN ACTEUR : les noeuds des
// sous-types 122 et 125.
//
// 5 appels, UNE LIGNE PAR APPEL DU SITE : l'octet est ce que le
// site passe, et le semis par coup dit qu'il boucle sur les coups.
//
// ENGENDRE par scripts/gen_client_effect_homing_gusts.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_homing_gust_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectHomingGustPattern, 5> kHomingGusts = {{
    {553, "Effect_Build_Scatter7aFromParam", "data\\texture\\effect\\270바람.tga",
     EffectHomingGustKind::kSteered, 0, false},
    {553, "Effect_Build_Scatter7aFromParam", "data\\texture\\effect\\270바람.tga",
     EffectHomingGustKind::kSteered, 1, false},
    {621, "Effect_Build_Scatter7dFromParam", "data\\texture\\effect\\270바람.tga",
     EffectHomingGustKind::kRandomSpread, 0, true},
    {1206, "Effect_Build_Scatter7aFromParam", "data\\texture\\effect\\270바람.tga",
     EffectHomingGustKind::kSteered, 4, false},
    {1206, "Effect_Build_Scatter7aFromParam", "data\\texture\\effect\\270바람.tga",
     EffectHomingGustKind::kSteered, 5, false},
}};

}  // namespace

std::span<const EffectHomingGustPattern> EffectHomingGustPatternTable() { return kHomingGusts; }

}  // namespace ro::render
