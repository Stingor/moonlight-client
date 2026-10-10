// LES RONDES : les noeuds du sous-type 27.
//
// 3 identifiants, UN SEUL batisseur, deux octets par site : une
// hauteur et une teinte. Les huit angles se tirent, piste par piste.
//
// ENGENDRE par scripts/gen_client_effect_spinning_rounds.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_spinning_round_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSpinningRoundPattern, 3> kSpinningRounds = {{
    {651, "Effect_Build_SpinScatter1bFromParam", "data\\texture\\effect\\mpa3.tga", 2,
     false},  // blanche
    {695, "Effect_Build_SpinScatter1bFromParam", "data\\texture\\effect\\mpa3.tga", 2,
     true},  // verte
    {2402, "Effect_Build_SpinScatter1bFromParam", "data\\texture\\effect\\mpa3.tga", 2,
     true},  // verte
}};

}  // namespace

std::span<const EffectSpinningRoundPattern> EffectSpinningRoundPatternTable() {
  return kSpinningRounds;
}

}  // namespace ro::render
