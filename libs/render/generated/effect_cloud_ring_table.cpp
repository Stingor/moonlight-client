// L'ANNEAU DE NUAGE : les noeuds du sous-type 62.
//
// 9 identifiants et 13 lignes : quatre d'entre eux appellent leur
// batisseur DEUX FOIS, avec deux octets differents.
//
// ⚠ 4 AUTRES sont ECARTES, chacun pour une raison mesuree - voir le
// generateur. Le sous-type en compte 13 en tout.
//
// ENGENDRE par scripts/gen_client_effect_cloud_rings.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_cloud_ring_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectCloudRingPattern, 13> kCloudRings = {{
    {224,
     "effectcls_InitHealCloud",
     "data\\texture\\effect\\cloud11.tga",
     0,
     4,
     EffectCloudRingBuilder::kHealCloud,
     {0, 0, 0}},
    // le frere, aucun octet, quatre pistes ecrites en toutes lettres
    {317,
     "Effect_BuildUpdate_VioletRedRingsCloud",
     "data\\texture\\effect\\cloud11.tga",
     0,
     4,
     EffectCloudRingBuilder::kVioletRedCloud,
     {2, 2, 0}},
    // style 0 : la nappe, au tic 2
    {341,
     "Effect_BuildUpdate_VioletRedRingsCloud",
     "data\\texture\\effect\\cloud11.tga",
     3,
     10,
     EffectCloudRingBuilder::kVioletRedCloud,
     {2, 2, 0}},
    // style 3 : la nappe, au tic 2
    {389,
     "Effect_Build_CloudRings4",
     "data\\texture\\effect\\cloud11.tga",
     0,
     5,
     EffectCloudRingBuilder::kCloudRings4,
     {0, 0, 0}},
    // octet 0 : style LENT, cle 2r+5, rayon tire, variante cinq
    {402,
     "Effect_Build_CloudRings4",
     "data\\texture\\effect\\cloud11.tga",
     1,
     8,
     EffectCloudRingBuilder::kCloudRings4,
     {0, 0, 0}},
    // octet 1 : style LENT, cle 2r+3, rayon tire, variante HUIT
    {435,
     "Effect_Build_CloudRings4",
     "data\\texture\\effect\\cloud11.tga",
     2,
     5,
     EffectCloudRingBuilder::kCloudRings4,
     {0, 0, 0}},
    // octet 2 : style LARGE, cle 48-5r, rayon 14-2r
    {435,
     "Effect_Build_CloudRings4",
     "data\\texture\\effect\\cloud11.tga",
     3,
     5,
     EffectCloudRingBuilder::kCloudRings4,
     {0, 0, 0}},
    // octet 3 : style LARGE, cle 28-5r, rayon 6-r
    {459,
     "Effect_Build_CloudRings4",
     "data\\texture\\effect\\cloud11.tga",
     2,
     5,
     EffectCloudRingBuilder::kCloudRings4,
     {0, 0, 0}},
    // octet 2 : style LARGE, cle 48-5r, rayon 14-2r
    {459,
     "Effect_Build_CloudRings4",
     "data\\texture\\effect\\cloud11.tga",
     3,
     5,
     EffectCloudRingBuilder::kCloudRings4,
     {0, 0, 0}},
    // octet 3 : style LARGE, cle 28-5r, rayon 6-r
    {460,
     "Effect_Build_CloudRings4",
     "data\\texture\\effect\\cloud11.tga",
     2,
     5,
     EffectCloudRingBuilder::kCloudRings4,
     {0, 0, 0}},
    // octet 2 : style LARGE, cle 48-5r, rayon 14-2r
    {460,
     "Effect_Build_CloudRings4",
     "data\\texture\\effect\\cloud11.tga",
     3,
     5,
     EffectCloudRingBuilder::kCloudRings4,
     {0, 0, 0}},
    // octet 3 : style LARGE, cle 28-5r, rayon 6-r
    {461,
     "Effect_Build_CloudRings4",
     "data\\texture\\effect\\cloud11.tga",
     2,
     5,
     EffectCloudRingBuilder::kCloudRings4,
     {0, 0, 0}},
    // octet 2 : style LARGE, cle 48-5r, rayon 14-2r
    {461,
     "Effect_Build_CloudRings4",
     "data\\texture\\effect\\cloud11.tga",
     3,
     5,
     EffectCloudRingBuilder::kCloudRings4,
     {0, 0, 0}},
    // octet 3 : style LARGE, cle 28-5r, rayon 6-r
}};

}  // namespace

std::span<const EffectCloudRingPattern> EffectCloudRingPatternTable() { return kCloudRings; }

}  // namespace ro::render
