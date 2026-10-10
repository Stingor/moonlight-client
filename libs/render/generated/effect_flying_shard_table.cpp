// LES ECLATS QUI FUIENT : les noeuds du sous-type 44.
//
// 4 identifiants, DEUX batisseurs qui sont UN SEUL semis a quatre nombres
// pres : le retard, la vitesse d'eloignement, la demi-largeur et
// l'elancement. L'octet du site, lui, ne choisit que la teinte.
//
// ENGENDRE par scripts/gen_client_effect_flying_shards.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_flying_shard_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectFlyingShardPattern, 4> kFlyingShards = {{
    {262, "Effect_Build_RisingScatter2cFromParam", "data\\texture\\effect\\alpha_center.tga",
     EffectFlyingShardBuilder::kSlender, 0, 20,
     20},  // EF_TEIHIT1, or 225/225/80, 20 noeuds (20 simplifie)
    {266, "Effect_Build_RisingScatter2cFromParam", "data\\texture\\effect\\lens1.tga",
     EffectFlyingShardBuilder::kSlender, 1, 24,
     24},  // EF_TEIHIT1X, blanc, 24 noeuds (24 simplifie)
    {276, "Effect_Build_RisingScatter2cFromParam_b", "data\\texture\\effect\\lens2.tga",
     EffectFlyingShardBuilder::kStout, 2, 20,
     20},  // EF_TEIHIT3, ciel 65/184/240 - et DEVANT tout, 20 noeuds (20 simplifie)
    {889, "Effect_Build_RisingScatter2cFromParam", "data\\texture\\effect\\alpha_center.tga",
     EffectFlyingShardBuilder::kSlender, 2, 20,
     6},  // EF_TEIHIT1T, ciel 65/184/240 - et DEVANT tout, 20 noeuds (6 simplifie)
}};

}  // namespace

std::span<const EffectFlyingShardPattern> EffectFlyingShardPatternTable() { return kFlyingShards; }

}  // namespace ro::render
