// LES ICONES QUI FLOTTENT : les noeuds du sous-type 123.
//
// 3 identifiants, UN SEUL batisseur, un appel par octet. L'octet ne
// choisit que l'image ; tout le reste est en dur. Voir
// effect_drifting_icon_seed.h.
//
// ENGENDRE par scripts/gen_client_effect_drifting_icons.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_drifting_icon_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectDriftingIconPattern, 3> kDriftingIcons = {{
    {573,
     "EffectInst_InitScatterParticlesCamAnchored",
     0,
     {"data\\texture\\유저인터페이스\\item\\하얀허브.bmp"}},
    {574,
     "EffectInst_InitScatterParticlesCamAnchored",
     1,
     {"data\\texture\\유저인터페이스\\item\\화이트슬림포션.bmp"}},
    {575,
     "EffectInst_InitScatterParticlesCamAnchored",
     2,
     {"data\\texture\\유저인터페이스\\item\\화염병.bmp",
      "data\\texture\\유저인터페이스\\item\\염산병.bmp",
      "data\\texture\\유저인터페이스\\item\\알코올.bmp"}},
}};

}  // namespace

std::span<const EffectDriftingIconPattern> EffectDriftingIconPatternTable() {
  return kDriftingIcons;
}

}  // namespace ro::render
