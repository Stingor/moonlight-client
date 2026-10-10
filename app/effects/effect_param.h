#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "ro/sim/combat_notice.h"

#include "effects/effect_node_view.h"

namespace moonclient {

// LES SORTES DE COUP QUI PORTENT PLUSIEURS COUPS, telles que le serveur les
// numérote (e_damage_type) : multi-coups, multi-coups encaissé, multi-coups
// critique. Pour elles seules, le gestionnaire de coup du client pose le
// NOMBRE DE COUPS du paquet dans l'instance de l'effet ; pour toute autre
// sorte, il pose un.
inline constexpr std::array<std::uint8_t, 3> kMultiHitDamageTypes = {8, 9, 13};
// ⚠ AU-DELÀ DE TRENTE-SIX COUPS, LE COMPTE RETOMBE À UN, et ne s'arrête pas à
// trente-six : le client écarte le nombre qu'il ne croit pas, comme il écarte
// zéro.
inline constexpr std::int32_t kEffectParamCap = 36;

// CE QUE LE CLIENT POSE COMME PARAMÈTRE D'INSTANCE POUR CE COUP. Soul Strike y lit
// combien de boules lancer : cinq au niveau dix, où le serveur frappe cinq
// fois, une au niveau un. C'est aussi l'ARGUMENT que la suite d'une compétence
// reçoit au coup, et ce que l'effet d'emploi du coup reçoit.
[[nodiscard]] inline std::int32_t EffectParamOfStrike(const ro::sim::Strike& strike) {
  if (std::ranges::find(kMultiHitDamageTypes, strike.type) == kMultiHitDamageTypes.end()) {
    return kRawEffectParam;
  }
  const std::int32_t hits = strike.hits;
  return hits < kRawEffectParam || hits > kEffectParamCap ? kRawEffectParam : hits;
}

}  // namespace moonclient
