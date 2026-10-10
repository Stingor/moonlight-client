// LE CARRE QUI TOURNE A PLAT : qui le pose.
//
// Le sous-type 143, qui lit de son cas un octet et un cap.
//
// 5 appel(s). Colonnes : identifiant, fonction, octet, cap, vie de
// l'instance. Les appels d'un meme identifiant gardent leur ordre. Tout le
// reste est ecrit en dur par le batisseur : voir effect_floor_spinner.h.
//
// ENGENDRE par scripts/gen_client_effect_floor_spinners.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_floor_spinner.h"

namespace ro::render {
namespace {

constexpr std::array<EffectFloorSpinnerPattern, 5> kFloorSpinners = {{
    {827, "Effect_InitTorchWaterDust", 0, 0, 29999},
    {829, "Effect_InitTorchWaterDust", 1, 0, 29999},
    {829, "Effect_InitTorchWaterDust", 1, 120, 29999},
    {829, "Effect_InitTorchWaterDust", 1, 240, 29999},
    {838, "Effect_InitTorchWaterDust", 2, 0, 99999999},
}};

}  // namespace

std::span<const EffectFloorSpinnerPattern> EffectFloorSpinnerPatternTable() {
  return kFloorSpinners;
}

}  // namespace ro::render
