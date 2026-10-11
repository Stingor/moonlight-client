// LES EMPREINTES : les noeuds du sous-type 67.
//
// Colonnes : identifiant, image, octet du site, style, rayon, alpha.
//
// ENGENDRE par scripts/gen_client_effect_footprints.py - ne pas
// modifier a la main.

#include <array>
#include <cstdint>

#include "ro/render/effect_footprint_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectFootprintPattern, 4> kFootprints = {{
    {326, "data\\texture\\effect\\foot_l_b.tga", 0, 0, 5.0F, 150},
    {327, "data\\texture\\effect\\foot_r_b.tga", 0, 0, 5.0F, 150},
    {447, "data\\texture\\effect\\foot_l2.tga", 1, 100, 6.0F, 150},
    {448, "data\\texture\\effect\\foot_r2.tga", 1, 100, 6.0F, 150},
}};

}  // namespace

std::span<const EffectFootprintPattern> EffectFootprintPatternTable() { return kFootprints; }

}  // namespace ro::render
