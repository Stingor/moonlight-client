// LES ANNEAUX D'UN PORTAIL DE TÉLÉPORTATION, sous-type 65.
//
// Le bâtisseur est rappelé à CHAQUE tic et l'effet est perpétuel — à
// l'âge 99999998 il remet son compteur à zéro.
//
// 🔴 C'EST CE QU'UN WARP MONTRE AU SOL. Le PNJ du portail, au mouvement 45,
// reçoit l'effet 321 dès qu'il paraît — une seule fois, derrière un
// loquet, et seulement s'il est visible.
//
// ⚠ DEUX ÉTAGES PAR EFFET, un au tic un et un au tic deux, décalés d'un degré
// d'azimut et d'un cinquième d'unité de rayon. C'est ce décalage qui épaissit
// les anneaux sans les dédoubler.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_effect_warp_rings.py — ne pas
// modifier à la main.

#include <array>

#include "ro/render/effect_warp_ring_seed.h"

namespace ro::render {
namespace {

// Triée par identifiant d'effet : la recherche est dichotomique.
constexpr std::array<EffectWarpRingPattern, 4> kWarpRings = {{
    {321,
     "effectcls_InitTieredWarpBurst",
     "data\\texture\\effect\\ring_blue.tga",
     11,
     1,
     {2.5F, 5.0F, 7.5F, 10.0F},
     {270, 0, 90, 180}},
    {321,
     "effectcls_InitTieredWarpBurst",
     "data\\texture\\effect\\ring_blue.tga",
     4,
     2,
     {2.7F, 5.2F, 7.7F, 10.2F},
     {271, 1, 91, 181}},
    {1185,
     "effectcls_InitTieredWarpBurst",
     "data\\texture\\effect\\ring_red.tga",
     18,
     1,
     {2.5F, 5.0F, 7.5F, 10.0F},
     {270, 0, 90, 180}},
    {1185,
     "effectcls_InitTieredWarpBurst",
     "data\\texture\\effect\\ring_red.tga",
     18,
     2,
     {2.7F, 5.2F, 7.7F, 10.2F},
     {271, 1, 91, 181}},
}};

}  // namespace

std::span<const EffectWarpRingPattern> EffectWarpRingPatternTable() { return kWarpRings; }

}  // namespace ro::render
