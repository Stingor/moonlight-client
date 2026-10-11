#pragma once

#include <cstdint>
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
// ⚠ LE SPECTACLE D'UN FAMILIER RESTE NON TRAITÉ : rien ici ne le lit encore.
// L'apparence passe par `ApplyPetAccessoryLook`.
[[nodiscard]] FrameOutcome ApplyPetFrame(const Frame& frame, sim::Pet& pet);

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

}  // namespace ro::net
