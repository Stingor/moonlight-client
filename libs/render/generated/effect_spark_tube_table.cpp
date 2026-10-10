// LES TUBES D'ETINCELLE : les noeuds du sous-type 100.
//
// 4 identifiants, TROIS batisseurs. Chacun a sa propre FENETRE de tics,
// et c'est la que vit le nombre de noeuds : deux, trois, quatre ou cinq,
// et l'un d'eux ne commence qu'au sixieme tic.
//
// L'image n'est pas ici : les trois l'ecrivent en dur. Voir
// kSparkTubeTexture.
//
// REFUSE - l'identifiant 428 : EF_HITLINE3 passe l'octet ZERO au batisseur des particules blanches
// montantes, dont la fenetre de tics est alors VIDE : le client ne montre rien.
//
// ENGENDRE par scripts/gen_client_effect_spark_tubes.py - ne pas modifier
// a la main.

#include <array>

#include "ro/render/effect_spark_tube_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSparkTubePattern, 4> kSparkTubes = {{
    {429,
     "Effect_Build_WhiteRisingParticles_F6",
     EffectSparkTubeBuilder::kRisingLate,
     3,
     {6, 9, 1}},  // EF_HITLINE4
    {430,
     "Effect_Build_WhiteDirectedParticles",
     EffectSparkTubeBuilder::kDirected,
     3,
     {0, 2, 1}},  // EF_HITLINE5
    {431,
     "Effect_Build_WhiteRisingParticles",
     EffectSparkTubeBuilder::kRising,
     3,
     {0, 4, 1}},  // EF_HITLINE6
    {434,
     "Effect_Build_WhiteRisingParticles",
     EffectSparkTubeBuilder::kRising,
     1,
     {0, 1, 1}},  // EF_STORMKICK
}};

}  // namespace

std::span<const EffectSparkTubePattern> EffectSparkTubePatternTable() { return kSparkTubes; }

}  // namespace ro::render
