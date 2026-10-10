// LES ARCS QUI SERPENTENT : les noeuds du sous-type 70.
//
// 3 identifiants, UN SEUL batisseur, et l'octet du site choisit la
// teinte. Tout le reste se tire, piste par piste.
//
// ENGENDRE par scripts/gen_client_effect_snaking_arcs.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_snaking_arc_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSnakingArcPattern, 3> kSnakingArcs = {{
    {330,
     "Effect_Build_WhiteScatterParticles46",
     "data\\texture\\effect\\white02.bmp",
     0,
     {0, 3, 1}},  // ambre : rouge plein, vert deux cents, bleu qui monte
    {376,
     "Effect_Build_WhiteScatterParticles46",
     "data\\texture\\effect\\white02.bmp",
     2,
     {0, 3, 1}},  // rose : rouge plein, vert et bleu qui montent ensemble
    {887,
     "Effect_Build_WhiteScatterParticles46",
     "data\\texture\\effect\\white02.bmp",
     8,
     {0, 3, 1}},  // bleu : bleu plein, rouge et vert qui montent ensemble
}};

}  // namespace

std::span<const EffectSnakingArcPattern> EffectSnakingArcPatternTable() { return kSnakingArcs; }

}  // namespace ro::render
