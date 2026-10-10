// La porte du ciblage. Voir l'en-tête pour ce qu'elle regarde, et pour ce
// qu'elle se refuse à deviner.

#include "world/versus_gate.h"

#include "ro/sim/map_mode.h"
#include "ro/sim/world.h"

namespace moonclient {

VersusGate VersusGateOf(const ro::sim::World& world, bool shift_held, bool no_shift) {
  return VersusGate{.map_opens = ro::sim::IsVersusMap(world.map_mode()),
                    // ⚠ UN OU, ET JAMAIS UN OU EXCLUSIF : le réglage REMPLACE
                    // la touche, il ne l'inverse pas, comme chez le client
                    // d'origine, réglage 0x69 — tenir MAJ alors que
                    // `/noshift` est allumé ne referme donc rien.
                    .forced = shift_held || no_shift,
                    .battlefield = world.map_mode().battlefield ? &world.battlefield() : nullptr};
}

}  // namespace moonclient
