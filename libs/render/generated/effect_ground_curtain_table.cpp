// LES TENTURES AU SOL : les noeuds du sous-type 48.
//
// 5 identifiants, UN SEUL batisseur. L'octet
// de son site choisit la DISPOSITION des quatre traits, leur alpha, le
// nombre de pistes vives et la teinte - quatre choses d'un coup.
//
// ENGENDRE par scripts/gen_client_effect_ground_curtains.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_ground_curtain_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectGroundCurtainPattern, 5> kGroundCurtains = {{
    {277, "Effect_Build_ScatterSprites48", "data\\texture\\effect\\ring_blue.tga",
     0},  // EF_BOTTOM_DISSONANCE, gerbe
    {287, "Effect_Build_ScatterSprites48", "data\\texture\\effect\\ring_red.tga",
     1},  // EF_BOTTOM_ASSASSINCROSS, treillis
    {290, "Effect_Build_ScatterSprites48", "data\\texture\\effect\\ring_red.tga",
     0},  // EF_BOTTOM_UGLYDANCE, gerbe
    {292, "Effect_Build_ScatterSprites48", "data\\texture\\effect\\magic_green.tga",
     2},  // EF_BOTTOM_DONTFORGETME, treillis
    {294, "Effect_Build_ScatterSprites48", "data\\texture\\effect\\safeline.bmp",
     4},  // EF_BOTTOM_SERVICEFORYOU, croix
}};

}  // namespace

std::span<const EffectGroundCurtainPattern> EffectGroundCurtainPatternTable() {
  return kGroundCurtains;
}

}  // namespace ro::render
