// LES TRAITS GAINES : les noeuds du sous-type 69.
//
// 5 identifiants, 5 batisseurs qui sont UN SEUL semis a sept tirages
// pres. Le site ne choisit ni le style, ni la teinte, ni la rampe d'alpha :
// tous trois suivent le batisseur.
//
// ENGENDRE par scripts/gen_client_effect_sheathed_bolts.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_sheathed_bolt_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSheathedBoltPattern, 5> kSheathedBolts = {{
    {329,
     "EffectInst_InitCloudBurstParticles",
     "data\\texture\\effect\\cloud11.tga",
     EffectSheathedBoltBuilder::kImpactYellow,
     {0, 1, 1}},  // EF_TRIPLEATTACK, ame JAUNE et gaine ivoire : aucun style nomme
    {388,
     "Effect_Build_AlphaCenterSpiral45",
     "data\\texture\\effect\\alpha_center.tga",
     EffectSheathedBoltBuilder::kAimedWhite,
     {0, 3, 1}},  // EF_TRIPLEATTACK2, blanc, vise
    {393,
     "Effect_Build_AlphaCenterSpiral45b",
     "data\\texture\\effect\\alpha_center.tga",
     EffectSheathedBoltBuilder::kAimedPink,
     {0, 8, 1}},  // EF_TRIPLEATTACK3, rose, vise, gaine DOUBLE
    {892,
     "Effect_InitDustCloudHit",
     "data\\texture\\effect\\cloud11.tga",
     EffectSheathedBoltBuilder::kImpactBlue,
     {0, 1, 1}},  // EF_TRIPLEATTACK4, bleu, portee posee - et DEVANT tout
    {1030,
     "Effect_InitDustCloudHit2",
     "data\\texture\\effect\\cloud11.tga",
     EffectSheathedBoltBuilder::kImpactDim,
     {0, 1, 1}},  // sans constante, sombre, recul de CENT - et DEVANT tout
}};

}  // namespace

std::span<const EffectSheathedBoltPattern> EffectSheathedBoltPatternTable() {
  return kSheathedBolts;
}

}  // namespace ro::render
