// LES GRAINS QUI DERIVENT : les noeuds du sous-type 96.
//
// 4 identifiants, UN SEUL batisseur.
// Son entier choisit le TIC du semis, le nombre de pistes vives et tous
// les nombres du grain.
//
// ENGENDRE par scripts/gen_client_effect_drifting_motes.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_drifting_mote_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectDriftingMotePattern, 4> kDriftingMotes = {{
    {411,
     "Effect_Build_RadialScatter60FromParam",
     "data\\texture\\effect\\whitelight.tga",
     0,
     {35, 35, 0},
     30},  // EF_PEONG, UN grain, plus gros, au tic 35
    {462,
     "Effect_Build_RadialScatter60FromParam",
     "data\\texture\\effect\\whitelight.tga",
     1,
     {0, 0, 0},
     30},  // EF_STORMKICK4, QUATRE grains, au tic 0
    {463,
     "Effect_Build_RadialScatter60FromParam",
     "data\\texture\\effect\\whitelight.tga",
     1,
     {0, 0, 0},
     30},  // EF_STORMKICK5, QUATRE grains, au tic 0
    {1150,
     "Effect_Build_RadialScatter60FromParam",
     "data\\texture\\effect\\whitelight.tga",
     1,
     {0, 0, 0},
     30},  // QUATRE grains, au tic 0
}};

}  // namespace

std::span<const EffectDriftingMotePattern> EffectDriftingMotePatternTable() {
  return kDriftingMotes;
}

}  // namespace ro::render
