// LES ECLAIRS : les noeuds du sous-type 101.
//
// 3 identifiants et 6 lignes : chacun pose DEUX eventails, l'un vers
// l'avant et l'autre vers l'arriere.
//
// ENGENDRE par scripts/gen_client_effect_lightning_fans.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_lightning_fan_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectLightningFanPattern, 6> kLightningFans = {{
    {432,
     "Effect_Build_ElectricArcFan_Elec1_F20",
     "data\\texture\\effect\\elec1.tga",
     0,
     0,
     false,
     {0, 3, 1}},  // teinte en degrade, du sombre au blanc
    {432,
     "Effect_Build_ElectricArcFan_Elec1_F20",
     "data\\texture\\effect\\elec1.tga",
     180,
     0,
     false,
     {0, 3, 1}},  // teinte en degrade, du sombre au blanc
    {781,
     "Effect_Build_ElectricArcFan_Elec1_F0",
     "data\\texture\\effect\\elec1.tga",
     0,
     3,
     true,
     {0, 3, 1}},  // teinte TIREE AU SORT a chaque quad
    {781,
     "Effect_Build_ElectricArcFan_Elec1_F0",
     "data\\texture\\effect\\elec1.tga",
     180,
     3,
     true,
     {0, 3, 1}},  // teinte TIREE AU SORT a chaque quad
    {888,
     "Effect_Build_ElectricArcFan_Elec1_F11",
     "data\\texture\\effect\\elec1.tga",
     0,
     0,
     false,
     {0, 3, 1}},  // teinte en degrade, du sombre au blanc
    {888,
     "Effect_Build_ElectricArcFan_Elec1_F11",
     "data\\texture\\effect\\elec1.tga",
     180,
     0,
     false,
     {0, 3, 1}},  // teinte en degrade, du sombre au blanc
}};

}  // namespace

std::span<const EffectLightningFanPattern> EffectLightningFanPatternTable() {
  return kLightningFans;
}

}  // namespace ro::render
