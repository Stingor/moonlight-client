// LES CARRES QUI FILENT EN TOURNOYANT : les noeuds du sous-type 116.
//
// 1 identifiant, UN SEUL batisseur, UN SEUL site : tout le reste
// - rayon, alpha, vitesse, hauteur, horaire des cinq semis - est en dur.
//
// ENGENDRE par scripts/gen_client_effect_swirl_trails.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_swirl_trail_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSwirlTrailPattern, 1> kSwirlTrails = {{
    {520, "EffectUpdate_CruShieldBoomerang", "data\\texture\\effect\\shield_boomerang.bmp"},
}};

}  // namespace

std::span<const EffectSwirlTrailPattern> EffectSwirlTrailPatternTable() { return kSwirlTrails; }

}  // namespace ro::render
