// LES BOURRASQUES AU SOL : les noeuds du sous-type 119.
//
// 1 identifiant, UN SEUL batisseur, UN SEUL site : tout le reste
// - rayon, vitesse, retard, teinte - est en dur dans le batisseur.
//
// ENGENDRE par scripts/gen_client_effect_ground_gusts.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_ground_gust_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectGroundGustPattern, 1> kGroundGusts = {{
    {547, "Effect_Build_Scatter77FromParam",
     "data\\texture\\effect\\270바람.tga"},  // sans constante
}};

}  // namespace

std::span<const EffectGroundGustPattern> EffectGroundGustPatternTable() { return kGroundGusts; }

}  // namespace ro::render
