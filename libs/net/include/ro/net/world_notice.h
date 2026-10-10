#pragma once

#include <cstdint>
#include <optional>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// CE QUE LE SERVEUR DIT, par opposition à ce qu'il fait au monde.
//
// La parole, les bulles au-dessus des têtes, les annonces, l'heure et le
// battement de cœur. Aucun de ces paquets ne déplace un corps, n'en change
// l'apparence ni n'en pose l'état : ils portent ce que le serveur ÉCRIT.
//
// Ils vivent à part de `ApplyFrame` pour la même raison que les compétences :
// ils forment une famille, et `world_update.cpp` touchait son plafond de
// lignes. Un fichier qui ne porte qu'un sujet se relit.
//
// Même contrat que `ApplyFrame` : un acteur inconnu n'est pas inventé, et
// `now_ms` est l'horloge de l'APPELANT.
[[nodiscard]] FrameOutcome ApplyNoticeFrame(const Frame& frame, sim::World& world,
                                            std::uint32_t now_ms);

// LE MOTIF D'UNE ÉVICTION (`SC_NOTIFY_BAN`), ou rien si cette trame n'en est pas
// une. Le serveur coupe la liaison aussitôt après : c'est à `app` de fermer le
// jeu en disant pourquoi — voir `BanText`.
[[nodiscard]] std::optional<std::uint8_t> EvictionOf(const Frame& frame);

}  // namespace ro::net
