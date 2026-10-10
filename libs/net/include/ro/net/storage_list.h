#pragma once

#include <cstddef>
#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LES COFFRES DE CE PERSONNAGE — `ZC 0x0F1E`, et la demande d'ouverture
// `CZ 0x0F1D`. Tous deux réservés aux clients ayant donné leur poignée de main
// (ADR 0040).
//
// 🔴 CELA DÉFAIT UNE PHRASE DE `ro/net/inventory.h`, ET C'EST UNE BONNE
// NOUVELLE. Il y est écrit qu'un coffre « ne s'ouvre pas d'ici », que le
// serveur n'expose son ouverture par AUCUN paquet — un PNJ, une compétence, ou
// « @storage » tapé dans la conversation. C'était vrai de rAthena. Moonlight a
// ajouté ce paquet-ci, et il fait mieux que la commande : les `@storagealt`
// BASCULENT — ils referment quand un coffre est déjà ouvert —, si bien que
// passer d'un coffre à l'autre coûte deux commandes. Celui-ci enchaîne la
// fermeture et l'ouverture, ce qu'un onglet doit faire en un clic.
//
// ⚠ ET IL N'OUVRE RIEN DE PLUS QUE LA COMMANDE. Le serveur vérifie les MÊMES
// droits — `pc_can_use_command` sur « storage » et « storagealtN » —, et sa
// liste est déjà filtrée par eux. Un onglet ne donne accès à rien que le joueur
// ne puisse déjà taper.
//
// ⚠ LA LISTE ARRIVE AVEC L'OUVERT, et c'est ce qui les garde d'accord : le
// serveur la renvoie à chaque ouverture, en nommant le coffre qu'il vient
// d'ouvrir. Au login, il l'envoie avec `0xFF` — aucun.
[[nodiscard]] FrameOutcome ApplyStorageListFrame(const Frame& frame, sim::World& world);

// « Ouvre-moi ce coffre-là. »
//
// ⚠ LE SILENCE EST UNE RÉPONSE. Un numéro absent de `inter_server.yml` est
// ignoré SANS un mot — le serveur y voit une liste périmée chez nous, et il a
// raison de ne pas afficher une erreur que le joueur ne peut pas corriger. Un
// refus de droits, lui, arrive dans la conversation.
//
// ⚠ ET RECLIQUER LE COFFRE DÉJÀ OUVERT NE FAIT RIEN, délibérément : les
// `@storagealt` referment dans ce cas, et un onglet qui referme la fenêtre
// serait un piège.
[[nodiscard]] core::Result<void> RequestOpenStorage(Connection& link, std::uint8_t storage_id);

// ── CE QUE LA STRUCTURE ENGENDRÉE NE PORTE PAS ──────────────────────────────
//
// Le générateur s'arrête au bloc fixe : le paquet déclare un `count` et pas ses
// entrées. Les décalages se nomment donc ici.
inline constexpr std::size_t kStorageListFixedSize = 6;

// `NAME_LENGTH`, dans `src/common/mmo.hpp`. ⚠ VINGT-QUATRE, là où un nom de
// CANAL en fait vingt : les deux listes se ressemblent et n'ont pas la même
// mesure. Se tromper décale toute la liste dès la seconde entrée.
inline constexpr std::size_t kStorageNameSize = 24;

inline constexpr std::size_t kStorageListEntrySize = 1 + kStorageNameSize;

}  // namespace ro::net
