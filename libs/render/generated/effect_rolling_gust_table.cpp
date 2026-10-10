// LES ROUES DE VENT : les noeuds du sous-type 126.
//
// 1 identifiant, UN SEUL batisseur, UN SEUL site : tout le reste
// - rayon, vitesse, retard, croix des cinq noeuds - est en dur.
//
// ENGENDRE par scripts/gen_client_effect_rolling_gusts.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_rolling_gust_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectRollingGustPattern, 1> kRollingGusts = {{
    {624, "Effect_Build_ScatterCross7eFromParam", "data\\texture\\effect\\270바람.tga"},
}};

}  // namespace

std::span<const EffectRollingGustPattern> EffectRollingGustPatternTable() { return kRollingGusts; }

}  // namespace ro::render
