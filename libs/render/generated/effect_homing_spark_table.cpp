// LES ETINCELLES QUI CONVERGENT : les noeuds du sous-type 159.
//
// 4 identifiants, UN SEUL batisseur, et l'octet du site ne choisit que la
// teinte des deux lames. Tout le reste est commun.
//
// ENGENDRE par scripts/gen_client_effect_homing_sparks.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_homing_spark_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectHomingSparkPattern, 4> kHomingSparks = {{
    {882, "Effect_InitSphericalSpread", "data\\texture\\effect\\alpha_center.tga",
     0},  // EF_PRIMECHARGE, lame fine cyan, lame large rouge
    {908, "Effect_InitSphericalSpread", "data\\texture\\effect\\alpha_center.tga",
     1},  // EF_PRIMECHARGE2, lame fine blanche, lame large jaune
    {909, "Effect_InitSphericalSpread", "data\\texture\\effect\\alpha_center.tga",
     2},  // EF_PRIMECHARGE3, lame fine rouge, lame large rose
    {910, "Effect_InitSphericalSpread", "data\\texture\\effect\\alpha_center.tga",
     3},  // EF_PRIMECHARGE4, lame fine bleu pale, lame large bleue
}};

}  // namespace

std::span<const EffectHomingSparkPattern> EffectHomingSparkPatternTable() { return kHomingSparks; }

}  // namespace ro::render
