#pragma once

#include <cstdint>

#include "ro/sim/actor.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/world_fwd.h"

#include "effects/effect_draw_gate.h"

// LE JUGEMENT D'UN PORTEUR À LA PORTE DE DESSIN : voir `EffectDrawGate`.
namespace moonclient {

// La porte d'un porteur, pour ce monde et ce groupe. Zéro, ou un porteur que le
// monde ne connaît plus, se montre : ses effets s'en vont avec lui.
[[nodiscard]] BearerGate BearerGateOf(std::uint32_t bearer_id, const ro::sim::World& world,
                                      const ro::sim::PartyRoster* party);

// VRAI POUR UN BOSS QUE SES OPTIONS CACHENT : le seul porteur que la porte du
// signe du vent écoute. Voir `EffectDrawGate::DrawsWindSign`.
[[nodiscard]] bool IsConcealedBoss(const ro::sim::Actor& actor, const ro::sim::World& world);

}  // namespace moonclient
