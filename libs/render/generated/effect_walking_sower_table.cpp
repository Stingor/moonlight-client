// LE SEMEUR QUI MARCHE : qui le pose, et ce qu'il fait naitre.
//
// Le sous-type 120, qui ne lit de son cas qu'un octet.
//
// 3 site(s). Colonnes : identifiant, effet que le noeud fait naitre,
// fonction, octet. Tout le reste est ecrit en dur par le batisseur : voir
// effect_walking_sower_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_walking_sowers.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_walking_sower_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectWalkingSowerPattern, 3> kWalkingSowers = {{
    {551, 552, "Effect_Build_RisingParticles78", 0},
    {555, 556, "Effect_Build_RisingParticles78", 1},
    {1204, 1205, "Effect_Build_RisingParticles78", 99},
}};

}  // namespace

std::span<const EffectWalkingSowerPattern> EffectWalkingSowerPatternTable() {
  return kWalkingSowers;
}

}  // namespace ro::render
