// LES ETINCELLES QUI JAILLISSENT DE LA CIBLE : qui les pose.
//
// Sous-type 45 : un seul batisseur, appele sans argument.
//
// 2 sites. Colonnes : identifiant, fonction. Tout le reste est ecrit en dur
// par le batisseur : voir effect_thunder_scatter_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_thunder_scatters.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_thunder_scatter_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectThunderScatterPattern, 2> kThunderScatters = {{
    {264, "Effect_Build_ThunderCenterScatter2d"},
    {275, "Effect_Build_ThunderCenterScatter2d"},
}};

}  // namespace

std::span<const EffectThunderScatterPattern> EffectThunderScatterPatternTable() {
  return kThunderScatters;
}

}  // namespace ro::render
