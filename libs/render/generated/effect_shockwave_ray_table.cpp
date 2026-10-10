// LES RAIS DE L'ONDE DE CHOC : qui les pose.
//
// Le sous-type 54. 2 appels. Colonnes : identifiant, fonction, image,
// angle. L'image est ecrite en dur par le batisseur ; tout le reste aussi :
// voir effect_shockwave_ray_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_shockwave_rays.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_shockwave_ray_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectShockwaveRayPattern, 2> kShockwaveRays = {{
    {300, "Effect_Build_Shockwave", "data\\texture\\effect\\shockwave_c.bmp", 30},
    {300, "Effect_Build_Shockwave", "data\\texture\\effect\\shockwave_c.bmp", 60},
}};

}  // namespace

std::span<const EffectShockwaveRayPattern> EffectShockwaveRayPatternTable() {
  return kShockwaveRays;
}

}  // namespace ro::render
