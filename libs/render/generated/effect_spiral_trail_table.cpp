// LES GERBES DE FEUX D'ARTIFICE : ce que leur batisseur ecrit en dur.
//
// Sous-type 52.
//
// 1 gerbe(s), et leurs 3 images viennent du BATISSEUR et non du site
// d'appel : celui-ci ne pousse rien. L'ordre des images compte :
// et la marque d'etincelle les indexe — voir effect_spiral_trail.h.
//
// LA VIE DU NOEUD N'EST PAS ICI : ce batisseur la COPIE de l'effet, et elle
// se demande a EffectNodeLifeFor, qui repond pour toutes les figures d'un
// coup. La porter aussi ici ferait deux tables pour une seule question.
//
// ENGENDRE par scripts/gen_client_effect_spiral_trails.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_spiral_trail_seed.h"

namespace ro::render {
namespace {

constexpr std::array<std::string_view, 3> kImages = {{
    "data\\texture\\effect\\pok1.tga",
    "data\\texture\\effect\\pok3.tga",
    "data\\texture\\effect\\pok2.tga",
}};

constexpr std::array<EffectSpiralTrailPattern, 1> kSpiralTrails = {{
    {297, "Effect_Build_PokRainFalling", kImages},
}};

}  // namespace

std::span<const EffectSpiralTrailPattern> EffectSpiralTrailPatternTable() { return kSpiralTrails; }

}  // namespace ro::render
