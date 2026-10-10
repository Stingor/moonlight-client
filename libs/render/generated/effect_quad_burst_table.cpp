// LES RAFALES DE QUADS TEINTES : les noeuds du sous-type 29.
//
// 8 identifiants, UN SEUL batisseur.
// Son octet choisit SIX choses : l'image, la marque, la taille, la vitesse,
// le nombre de pistes vives et la hauteur de depart des quatre particules.
//
// Les huit sites sont tous ceux qui appellent le batisseur ; le releve des
// noeuds n'en attache que cinq. Voir effect_quad_burst_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_quad_bursts.py - ne pas modifier
// a la main.

#include <array>

#include "ro/render/effect_quad_burst_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectQuadBurstPattern, 8> kQuadBursts = {{
    {202, "Effect_Build_GhostFreezeParticles", 0},  // EF_LEVEL99_3, freezing_circle, marque 0
    {362, "Effect_Build_GhostFreezeParticles",
     1},  // EF_LEVEL99_4, whitelight, marque 1 - bleu double
    {678, "Effect_Build_GhostFreezeParticles", 2},  // EF_GREEN99_3, whitelight, marque 0, echelle 7
    {692, "Effect_Build_GhostFreezeParticles", 3},  // EF_MAP_GHOST, ghost, marque 11 - UNE piste
    {703, "Effect_Build_GhostFreezeParticles",
     4},  // EF_CIRCLELIGHT, whitelight, marque 14 - la colonne
    {710, "Effect_Build_GhostFreezeParticles", 5},  // EF_MAP_GREENLIGHT, whitelight, marque 15
    {711, "Effect_Build_GhostFreezeParticles",
     6},  // EF_MAP_MAGICWALL, whitelight, marque 16 - disperse
    {712, "Effect_Build_GhostFreezeParticles",
     7},  // EF_MAP_GREENLIGHT2, whitelight, marque 15 - DEUX pistes
}};

}  // namespace

std::span<const EffectQuadBurstPattern> EffectQuadBurstPatternTable() { return kQuadBursts; }

}  // namespace ro::render
