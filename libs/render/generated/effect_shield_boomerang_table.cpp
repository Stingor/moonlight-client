// LE BOUMERANG ET SON FRERE QUI ECLATE : ce que chaque site pose.
//
// Sous-type 42, deux batisseurs : le boumerang recoit son image du site,
// son frere qui eclate ecrit la sienne en dur.
//
// 4 sites. Colonnes : identifiant, fonction, image, tics de depart,
// alpha de tete, genre, rayon fixe et rayon par parametre de
// l'instance. La vitesse,
// l'elevation, la cle et la montee sont communes : voir
// effect_shield_boomerang_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_shield_boomerangs.py - ne pas
// modifier a la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_shield_boomerang_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectShieldBoomerangPattern, 4> kShieldBoomerangs = {{
    {249, "Effect_Build_ShieldBoomerang", "data\\texture\\effect\\shield_boomerang.bmp", 0, 255, 0,
     7.0F, 0.0F},
    {265, "Effect_Build_BlueIvyBeamWithSound", "data\\texture\\effect\\blue_ivy.bmp", -15, 5, 1,
     2.0F, 0.4F},
    {494, "Effect_Build_ShieldBoomerang", "data\\texture\\effect\\토마.bmp", 0, 255, 0, 7.0F, 0.0F},
    {774, "Effect_Build_ShieldBoomerang", "data\\texture\\유저인터페이스\\item\\배틀액스.bmp", 0,
     255, 0, 7.0F, 0.0F},
}};

}  // namespace

std::span<const EffectShieldBoomerangPattern> EffectShieldBoomerangPatternTable() {
  return kShieldBoomerangs;
}

}  // namespace ro::render
