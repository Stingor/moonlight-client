// LES DEUX RAYONS POINTES VERS LA CIBLE : qui les pose.
//
// Le sous-type 154. 5 appels. Colonnes : identifiant, fonction, image,
// octet. Tout le reste est ecrit en dur par le batisseur : voir
// effect_aimed_spoke_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_aimed_spokes.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_aimed_spoke_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectAimedSpokePattern, 5> kAimedSpokes = {{
    {854, "Effect_InitDirectedSprite", "data\\texture\\effect\\line3.tga", 0},
    {858, "Effect_InitDirectedSprite", "data\\texture\\effect\\line3.tga", 1},
    {863, "Effect_InitDirectedSprite", "data\\texture\\effect\\line3.tga", 2},
    {1584, "Effect_InitDirectedSprite",
     "data\\texture\\effect\\mineffect\\new_metalicsound\\new_metalicsound_line\\met_damege_line_1."
     "tga",
     2},
    {1584, "Effect_InitDirectedSprite",
     "data\\texture\\effect\\new_metalicsound\\new_metalicsound_line\\met_damege_line_1.tga", 2},
}};

}  // namespace

std::span<const EffectAimedSpokePattern> EffectAimedSpokePatternTable() { return kAimedSpokes; }

}  // namespace ro::render
