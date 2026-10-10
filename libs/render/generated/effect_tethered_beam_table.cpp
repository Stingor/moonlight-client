// LES TRAITS TENDUS : les noeuds du sous-type 38.
//
// 5 identifiants, TROIS batisseurs, AUCUN tirage : l'alpha de depart,
// le rayon de l'ame et la variante sont poses en dur par le site.
//
// ENGENDRE par scripts/gen_client_effect_tethered_beams.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_tethered_beam_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectTetheredBeamPattern, 5> kTetheredBeams = {{
    {232, "Effect_Build_CloudOnActor", "data\\texture\\effect\\cloud11.tga", 45, 0.4F,
     0.0F},  // ame JAUNE, gaine BLEUE, additives - et l'alpha scintille
    {384, "Effect_Build_AlphaCenterOnActor", "data\\texture\\effect\\alpha_center.tga", 0, 0.2F,
     2.0F},  // ame 0/100/150, gaine 255/50/0 - l'alpha monte, tient, puis tombe
    {395, "Effect_Build_AlphaCenterOnActor2", "data\\texture\\effect\\alpha_center.tga", 0, 0.7F,
     3.0F},  // ame et gaine 255/89/182 - et le rayon RESPIRE
    {819, "Effect_Build_CloudOnActor", "data\\texture\\effect\\cloud11.tga", 45, 0.4F,
     4.0F},  // ame et gaine 50/50/50, melange ordinaire, alpha DEMI
    {1210, "Effect_Build_CloudOnActor", "data\\texture\\effect\\cloud11.tga", 45, 0.4F,
     0.0F},  // ame JAUNE, gaine BLEUE, additives - et l'alpha scintille
}};

}  // namespace

std::span<const EffectTetheredBeamPattern> EffectTetheredBeamPatternTable() {
  return kTetheredBeams;
}

}  // namespace ro::render
