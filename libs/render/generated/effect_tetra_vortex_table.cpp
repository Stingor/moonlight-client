// LE TOURBILLON DE TETRA VORTEX : qui le pose.
//
// Sous-type 140 : le batisseur est appele avec l'octet 4.
//
// 5 site(s). Colonnes : identifiant, fonction. Tout le reste est ecrit en dur
// par le batisseur : voir effect_tetra_vortex_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_tetra_vortices.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_tetra_vortex_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectTetraVortexPattern, 5> kTetraVortices = {{
    {804, "Effect_InitTetraVortex"},
    {970, "Effect_InitTetraVortex"},
    {971, "Effect_InitTetraVortex"},
    {972, "Effect_InitTetraVortex"},
    {973, "Effect_InitTetraVortex"},
}};

}  // namespace

std::span<const EffectTetraVortexPattern> EffectTetraVortexPatternTable() { return kTetraVortices; }

}  // namespace ro::render
