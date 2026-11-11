#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/style.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LES STYLES HORS DE VUE : la guilde, le groupe et les amis demandent ceux d'un
// membre connecté que la carte ne montre pas (`CZ 0x0F63`), et le serveur les
// rend sous son char_id (`ZC 0x0F64`). Voir l'ADR 0147 et
// docs/couleurs-du-personnage/conception.md, lot L13.
//
// LA DEMANDE : [op:2][taille:2][nombre:1] puis nombre × [char_id:4].
//
// LA RÉPONSE : [op:2][taille:2][résultat:1][attente_ms:2][drapeaux:1]
// [taille_brute:2] puis la charge, compressée comme celle du 0x0F5F. Un bloc
// par char_id demandé :
//   [char_id:4][état:1]
//   état 1 : [métier:2][sexe:1][coupe:2][couleur:2]
//            [corps:1] puis corps × une variante du lot 0x0F27 sans son gid
//            [nombre:1] puis nombre × entrée de calque, à la forme du 0x0F5F
// L'état 0 dit « rien à montrer », sans rien d'autre.
//
// 🔴 LE SERVEUR CHOISIT QUI : il ne répond que pour les membres de la guilde, du
// groupe et de la liste d'amis de la session, et rend l'état 0 à tout autre.
// Ces deux paquets voyagent sous le bit des calques, et la demande ne part
// qu'une fois la cadence dite par le serveur (règle 26).

// Les char_id d'une demande, au plus, quand le serveur ne le dit pas.
inline constexpr std::size_t kAbsentStyleIdsMost = 16;

// La demande, ou la raison de la refuser : aucun char_id, trop, un nul ou un
// doublon.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeAbsentStylesRequest(
    std::span<const std::uint32_t> char_ids);
[[nodiscard]] core::Result<void> RequestAbsentStyles(Connection& link,
                                                     std::span<const std::uint32_t> char_ids);

struct AbsentStyleBlock {
  std::uint32_t char_id = 0;
  sim::AbsentStyle style;
};

// 🔴 TOUT OU RIEN : un bloc qui déborde, ou un état inconnu, rend une erreur.
// Une entrée de calque ou une variante qu'on ne sait pas lire est enjambée, et
// celles au-delà des plafonds ignorées.
[[nodiscard]] core::Result<std::vector<AbsentStyleBlock>> ReadAbsentStylePayload(
    std::span<const std::uint8_t> raw);

// `ZC 0x0F64` : chaque bloc remplace ce que le livre savait de son personnage.
// Un refus se range au livre, avec son attente.
[[nodiscard]] FrameOutcome ApplyAbsentStylesFrame(const Frame& frame, sim::World& world,
                                                  std::uint32_t now_ms);

}  // namespace ro::net
