// LES CARRÉS QUI MONTENT : les nœuds du sous-type 49.
//
// UN SEUL bâtisseur ; chaque site lui passe une image et un mode. La figure
// est celle du carré teinté ; c'est le MOUVEMENT qui fait cette famille.
//
// ENGENDRÉ par scripts/gen_client_effect_rising_quads.py — ne pas modifier
// à la main.

#include <array>

#include "ro/render/effect_rising_quad.h"

namespace ro::render {
namespace {

// Triée par identifiant d'effet : la recherche est dichotomique.
constexpr std::array<EffectRisingQuadPattern, 1> kRisingQuads = {{
    {283, "data\\texture\\effect\\safeline.bmp", 0},
}};

}  // namespace

std::span<const EffectRisingQuadPattern> EffectRisingQuadPatternTable() { return kRisingQuads; }

}  // namespace ro::render
