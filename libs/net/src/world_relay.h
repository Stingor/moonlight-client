#pragma once

#include <cstdint>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

// LE RELAIS : ce qu'une trame de carte porte SANS que ce soit un fait
// d'acteur — un contenant, un coffre, une banque, un canal, un style.
//
// En-tête PRIVÉ : il vit dans `src/` et non dans `include/`, parce que
// personne hors de cette bibliothèque n'a à court-circuiter l'aiguillage. La
// séparation suit une frontière réelle et non le plafond de lignes qui l'a
// révélée : d'un côté un aiguillage qui DÉCODE les paquets du monde, de
// l'autre une chaîne qui ne fait que PASSER la trame à la famille qui sait la
// lire.
namespace ro::net {

// ⚠ L'ORDRE DE LA CHAÎNE COMPTE, et il va du plus précis au plus général :
// chacun rend `kUnhandled` sur ce qui n'est pas à lui, et le dernier attrape
// le reste.
[[nodiscard]] FrameOutcome RelayFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms);

}  // namespace ro::net
