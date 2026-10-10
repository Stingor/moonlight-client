// LE VOILE TEINTE : qui le pose.
//
// Le sous-type 172, qui lit de son cas un octet de rythme et une teinte.
//
// 2 appel(s). Colonnes : identifiant, fonction, octet, teinte, vie de
// l'instance. Tout le reste est ecrit en dur par le batisseur : voir
// effect_colored_veil.h.
//
// ENGENDRE par scripts/gen_client_effect_colored_veils.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_colored_veil.h"

namespace ro::render {
namespace {

constexpr std::array<EffectColoredVeilPattern, 2> kColoredVeils = {{
    {669, "Effect_Build_WhiteColoredSprite_ac", 1, {255, 0, 0}, 70},
    {673, "Effect_Build_WhiteColoredSprite_ac", 2, {0, 0, 255}, 99999999},
}};

}  // namespace

std::span<const EffectColoredVeilPattern> EffectColoredVeilPatternTable() { return kColoredVeils; }

}  // namespace ro::render
