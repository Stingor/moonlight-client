// LA LUEUR BLANCHE QUI PASSE EN ARC : qui la pose.
//
// Le sous-type 109, qui ne lit de son cas que le nom de son image.
//
// 1 site(s). Colonnes : identifiant, fonction, image. Tout le reste est
// ecrit en dur par le batisseur : voir effect_arcing_light_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_arcing_lights.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_arcing_light_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectArcingLightPattern, 1> kArcingLights = {{
    {503, "Effect_Build_Sprite6dFromParam", "data\\texture\\effect\\whitelight.tga"},
}};

}  // namespace

std::span<const EffectArcingLightPattern> EffectArcingLightPatternTable() { return kArcingLights; }

}  // namespace ro::render
