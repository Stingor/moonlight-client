// LES COURONNES DE MAGNUM BREAK : qui les pose.
//
// Le sous-type 73. 1 appel(s). Colonnes : identifiant, fonction, image.
// Tout le reste est ecrit en dur par le batisseur : voir
// effect_break_ring_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_break_rings.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_break_ring_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectBreakRingPattern, 1> kBreakRings = {{
    {339, "Effect_BuildUpdate_MagnumBreakRings", "data\\texture\\effect\\ring_yellow.tga"},
}};

}  // namespace

std::span<const EffectBreakRingPattern> EffectBreakRingPatternTable() { return kBreakRings; }

}  // namespace ro::render
