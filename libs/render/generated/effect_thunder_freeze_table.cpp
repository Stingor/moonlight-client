// LE COEUR QUI ERRE ET SES ECLATS DE GEL : qui les pose.
//
// Sous-type 145. Le batisseur ne lit rien de son cas.
//
// 1 site(s). Colonnes : identifiant, fonction. Tout le reste est ecrit en dur
// par le batisseur : voir effect_thunder_freeze_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_thunder_freezes.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_thunder_freeze_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectThunderFreezePattern, 1> kThunderFreezes = {{
    {833, "Effect_InitThunderFreeze"},
}};

}  // namespace

std::span<const EffectThunderFreezePattern> EffectThunderFreezePatternTable() {
  return kThunderFreezes;
}

}  // namespace ro::render
