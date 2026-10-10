// LES DEUX DISQUES QUI S'ETEIGNENT : les noeuds du sous-type 161.
//
// 1 ligne(s). La derniere colonne est le plancher de vie qu'un batisseur
// voisin pose a l'instance AVANT que celui-ci ne la copie dans son noeud.
//
// ENGENDRE par scripts/gen_client_effect_fading_twin_discs.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_fading_twin_disc_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectFadingTwinDiscPattern, 1> kFadingTwinDiscs = {{
    {831, "Effect_InitScaledSprite2", "data\\texture\\effect\\cir0002.bmp", 3,
     120},  // plancher pose par un batisseur voisin
}};

}  // namespace

std::span<const EffectFadingTwinDiscPattern> EffectFadingTwinDiscPatternTable() {
  return kFadingTwinDiscs;
}

}  // namespace ro::render
