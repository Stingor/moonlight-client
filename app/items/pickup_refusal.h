#pragma once

#include <cstdint>
#include <optional>

#include "ro/vfs/item_words.h"

namespace moonclient {

// POURQUOI LE SERVEUR A REFUSÉ UN RAMASSAGE — la traduction de son code en sens.
//
// 🔴 C'EST ICI QUE VIT LA CORRESPONDANCE, et nulle part ailleurs. `net` décode
// le paquet mais ne voit pas les textes de l'installation ; `vfs` porte les
// textes mais ne connaît aucun protocole. Le pont est donc dans `app`, et il est
// en bibliothèque pour la raison habituelle : cette table est MUETTE en jeu. Un
// code rangé sous le mauvais message donne une phrase plausible qui dit le
// contraire de ce qui s'est passé — « votre sac est plein » sur une surcharge —,
// et rien à l'écran ne le trahit.
//
// ⚠ DEUX CODES PARTAGENT UN MESSAGE : le client écrit le même « You cannot get
// the item. » pour le refus sans motif et pour le refus temporaire.
//
// ⚠ TOUT CODE INCONNU EST SILENCIEUX, et c'est le comportement du client : il
// n'écrit rien plutôt que d'inventer une raison. Rendre « rien » plutôt qu'un
// message par défaut est donc un choix, pas un oubli.

// Les codes que ce serveur émet, tels que `e_additem_result` les numérote
// (moonlight, src/map/pc.hpp). Ils sont nommés ici parce qu'un `case 2` ne dit
// rien à personne.
//
// ⚠ DEUX VALEURS DE L'ÉNUMÉRATION DU SERVEUR MANQUENT À CETTE LISTE, et c'est
// vérifié plutôt que supposé : `ADDITEM_ITEM` (3) n'est retourné nulle part
// dans son code, et aucun appel n'émet 9 — le message de partage de groupe que
// le client sait pourtant écrire n'a donc pas d'émetteur ici. Les ajouter
// reviendrait à écrire du code que rien n'atteint.
enum class PickupResult : std::uint8_t {
  kGranted = 0,
  kInvalid = 1,
  kOverweight = 2,
  kTooManyItems = 4,
  kOverAmount = 5,
  kRefusedForNow = 6,
  kStackLimit = 7,
};

// Le sens du refus, ou rien quand le code n'en a pas — un succès compris.
[[nodiscard]] std::optional<ro::vfs::ItemPickupRefusal> PickupRefusalOf(std::uint8_t result);

}  // namespace moonclient
