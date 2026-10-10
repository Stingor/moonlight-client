#pragma once

#include <cstdint>
#include <optional>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// « TA CIBLE EST TROP LOIN » : le seul mot que le serveur dit d'un coup refusé.
//
// 🔴 ET IL NE FAIT RIEN D'AUTRE. Pour un JOUEUR trop loin de sa cible,
// `unit_attack_timer_sub` (src/map/unit.cpp) envoie `clif_movetoattack` puis
// rend la main : aucune horloge d'attaque n'est armée, aucune marche n'est
// lancée. `unit_walktobl` — la poursuite — n'est appelée que pour un MONSTRE.
// Rapprocher le personnage est donc entièrement l'affaire du client.
//
// Le paquet porte de quoi le faire : la cible, SA case, la NÔTRE, et la portée
// de l'arme. Le client natif s'en sert pour recaler les deux positions (deux
// messages 61), noter la portée, puis RÉARMER son action en attente sur la
// cible (message 60) avec une horloge reculée, pour que le tic suivant reprenne
// la poursuite sans attendre.
//
// ⚠ LE RECALAGE N'EST NI UNE MARCHE, NI UN ARRÊT. Le message 61 écrit la case
// et la position dessinée telles quelles, sans construire de chemin et sans
// toucher à la motion — là où `ZC_STOPMOVE` bâtit un chemin, fait glisser
// l'acteur et le rend au repos. Qui marche encore garde sa marche, qui reprend
// la position au pas suivant : voir `sim::World::Reposition`. Y voir un arrêt
// figerait une cible qui s'éloigne, et changerait le repos de qui attendait.
//
// 🔴 CE QUI EST UN FAIT DU MONDE S'ARRÊTE LÀ : les deux positions et la portée.
// Le RÉARMEMENT, lui, n'en est pas un — le serveur ne sait rien de ce que le
// client attend —, et il n'a donc rien à faire dans `sim::World`. C'est
// `TargetTooFar` qui le porte, et `app` qui le mène.
[[nodiscard]] FrameOutcome ApplyAttackReachFrame(const Frame& frame, sim::World& world,
                                                 std::uint32_t now_ms);

// La cible d'un refus de portée, ou rien si cette trame n'en est pas un.
//
// Elle sert à `app`, qui tient l'action en attente et reprend alors la
// poursuite. ⚠ ELLE NE DÉCIDE RIEN : la cible peut avoir disparu entre-temps,
// et c'est à l'appelant de le voir.
[[nodiscard]] std::optional<std::uint32_t> TargetTooFar(const Frame& frame);

}  // namespace ro::net
