// LES LUCIOLES : les noeuds du sous-type 130.
//
// 3 identifiants, UN SEUL batisseur, UN SEUL octet par site - et cet
// octet porte tout : le nombre de pistes, le mouvement et la teinte.
//
// ENGENDRE par scripts/gen_client_effect_wandering_motes.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_wandering_motes_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectWanderingMotePattern, 3> kWanderingMotes = {{
    {713, "Effect_InitWhiteLightSpread", "data\\texture\\effect\\whitelight.tga",
     0},  // quatre pistes, seize lucioles qui errent, jaunes
    {714, "Effect_InitWhiteLightSpread", "data\\texture\\effect\\whitelight.tga",
     1},  // UNE piste, UNE luciole qui MONTE, jaune, et repart tous les 3000 tics
    {946, "Effect_InitWhiteLightSpread", "data\\texture\\effect\\whitelight.tga",
     2},  // quatre pistes, seize lucioles qui errent, BLANCHES
}};

}  // namespace

std::span<const EffectWanderingMotePattern> EffectWanderingMotePatternTable() {
  return kWanderingMotes;
}

}  // namespace ro::render
