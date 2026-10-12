#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/proto/pet_packets.h"
#include "ro/sim/pet.h"

namespace ro::net {

// ── LES TRAMES DU FAMILIER ─────────────────────────────────────────────────
//
// La fiche, les changements d'état, la liste d'éclosion, le résultat d'une
// évolution, l'accusé d'un repas — et, dans l'écho des réglages,
// l'auto-alimentation, que le lecteur des réglages du personnage laisse passer.
//
// ⚠ LE SPECTACLE N'Y CHANGE RIEN : il se lit par `ReadPetPerformance`.
// L'apparence passe par `ApplyPetAccessoryLook`, la parole par `ReadPetAct`.
[[nodiscard]] FrameOutcome ApplyPetFrame(const Frame& frame, sim::Pet& pet);

// LA PAROLE D'UN FAMILIER, le nôtre comme celui d'un voisin : son acteur, et
// l'entier que le client de son maître a composé (voir `ro/sim/pet_talk.h`).
struct PetActNotice {
  std::uint32_t actor_id = 0;
  std::int32_t code = 0;
};
// Rien pour une autre trame, ou une trame mal formée.
[[nodiscard]] std::optional<PetActNotice> ReadPetAct(const Frame& frame);

// LE SPECTACLE QU'UN FAMILIER DONNE : son acteur, et le numéro que le serveur
// a tiré. Rien pour une autre trame ou un autre changement d'état.
struct PetPerformanceNotice {
  std::uint32_t actor_id = 0;
  std::int32_t performance = 0;
};
[[nodiscard]] std::optional<PetPerformanceNotice> ReadPetPerformance(const Frame& frame);

// L'ACCESSOIRE D'UN PET, le nôtre comme celui d'un voisin : le changement
// d'état « accessoire » remplace la coiffe basse de son acteur, celle où
// l'apparition l'avait posé et que le dessin lit. Zéro le retire.
//
// Vrai quand un acteur visible a changé ; faux pour une autre trame, un autre
// changement d'état ou un acteur inconnu.
[[nodiscard]] bool ApplyPetAccessoryLook(const Frame& frame, sim::World& world);

// Un ordre au familier. ⚠ ÉMET ET S'ARRÊTE LÀ : les gardes du client — pas de
// nourriture en sac, courrier en rédaction — appartiennent à l'appelant.
[[nodiscard]] core::Result<void> RequestPetCommand(Connection& link, proto::PetCommand command);

// Le nouveau nom, en UTF-8 : il part transcodé dans la page de codes du fil.
// Refusé sans envoi s'il est vide ou trop long SOUS CETTE FORME.
[[nodiscard]] core::Result<void> RequestPetRename(Connection& link, std::string_view name);

// L'œuf choisi dans la liste d'éclosion, par la case que la liste a donnée.
[[nodiscard]] core::Result<void> RequestPetEgg(Connection& link, std::uint16_t client_slot);

// L'évolution vers l'œuf CIBLE.
[[nodiscard]] core::Result<void> RequestPetEvolution(Connection& link, std::uint32_t target_egg_id);

// L'auto-alimentation. ⚠ L'ÉTAT NE CHANGE QU'À L'ÉCHO du serveur.
[[nodiscard]] core::Result<void> RequestPetAutoFeed(Connection& link, bool enabled);

// Ce que notre familier dit ou montre. Le serveur le renvoie à tous, nous
// compris : c'est son écho qui le fait paraître.
[[nodiscard]] core::Result<void> RequestPetAct(Connection& link, std::int32_t code);

}  // namespace ro::net
