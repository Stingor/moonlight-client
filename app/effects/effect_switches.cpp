// LES DEUX BASCULES D'EFFET DU JEU — `/effect` et `/mineffect`.
//
// Sorties de `effect_view.cpp` au plafond de huit cents lignes, et la coupure
// suit une famille : ces trois méthodes ne servent qu'aux deux réglages 8 et
// 229 du client, et elles sont les seules du chantier des effets à ne rien
// faire naître.
//
// 🔴 CE NE SONT PAS DEUX BASCULES JUMELLES, et les confondre est l'erreur
// plausible. `/effect` crible l'AFFICHAGE : le portier du client saute le
// dessin, sauf pour cent quatorze identifiants qu'une liste blanche protège.
// `/mineffect` choisit une RESSOURCE : il fait prendre une variante allégée là
// où le client en connaît une — c'est son drapeau le plus lu, deux cent trente
// sites contre treize.
//
// ⚠ AUCUNE DES DEUX NE TUE QUOI QUE CE SOIT. Le client ne parcourt pas ses
// effets vivants quand on bascule ; ses portiers relisent le drapeau à chaque
// image, et ce qui était caché revient si l'on rebascule.

#include "ro/render/effect_visibility.h"
#include "ro/sim/world.h"

#include "effects/effect_bearer_gate.h"
#include "effects/effect_node_view.h"
#include "effects/effect_view.h"

namespace moonclient {

void EffectsInPlay::SetEffectSwitches(bool shown, bool minimal) {
  effects_shown_ = shown;
  minimal_effects_ = minimal;
  // ⚠ LA TROUPE PROCÉDURALE LES VEUT AUSSI, et par le même geste : elle sème et
  // se dessine seule, et deux exemplaires qu'on poserait séparément finiraient
  // par diverger d'une image.
  nodes_->SetEffectSwitches(shown, minimal);
}

bool EffectsInPlay::ShowsEffect(std::uint32_t effect_id) const {
  return effects_shown_ || ro::render::IsForcedVisibleEffect(effect_id);
}

void EffectsInPlay::NoticeDrawGate(const ro::sim::World& world, const ro::sim::PartyRoster* party) {
  draw_gate_ = EffectDrawGate{};
  for (const auto& [id, actor] : world.actors()) {
    draw_gate_.present.insert(id);
    if (IsConcealedBoss(actor, world)) {
      draw_gate_.concealed_bosses.insert(id);
    }
    if (const BearerGate gate = BearerGateOf(id, world, party); gate != BearerGate::kShown) {
      draw_gate_.concealed.emplace(id, gate);
    }
  }
  // ⚠ LA TROUPE PROCÉDURALE ET LES CLASSES LA VEULENT AUSSI, par le même geste
  // que les bascules.
  nodes_->NoticeDrawGate(draw_gate_);
  classes_.NoticeDrawGate(draw_gate_);
}

}  // namespace moonclient
