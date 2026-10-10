#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/title_book.h"

namespace ro::net {

// LES TITRES : ceux qu'on possède, celui qu'on porte.
//
// TROIS PAQUETS, ET AUCUN NE NOMME UN TITRE POSSÉDÉ :
//
//   · `ZC 0x0A23`, la liste des succès — chaque succès RÉCLAMÉ y donne le
//     sien, que le client lit dans sa table ;
//   · `ZC 0x0A2F`, l'accusé d'un changement — accepté, il dit ce qu'on porte ;
//   · `ZC 0x0A30`, la carte de nom — quand elle est la NÔTRE, elle dit aussi ce
//     qu'on porte.
//
// 🔴 L'ACCUSÉ D'UNE RÉCLAMATION (`ZC 0x0A26`) NE DONNE AUCUN TITRE. Son octet
// vaut zéro pour un REFUS, et une réclamation qui rapporte un titre ne reçoit
// d'ailleurs pas d'accusé : le serveur répond par la liste complète. Prendre le
// titre sur un accusé nul offrirait un titre que le serveur refuse ensuite.
//
// ⚠ LA MISE À JOUR D'UN SEUL SUCCÈS (`ZC 0x0A24`) NE DONNE RIEN non plus, même
// quand elle le dit réclamé : le titre ne se prend qu'à la liste complète. Et un
// changement REFUSÉ ne fait rien du tout, pas même un message.
//
// ⚠ LA CARTE DE NOM EST AUSSI LUE PAR `ApplyActorNameFrame`, qui la pose sur
// l'acteur. Ce traducteur-ci la lit en plus, sans la lui prendre : c'est à
// l'appelant de les interroger tous les deux.
[[nodiscard]] FrameOutcome ApplyTitleFrame(const Frame& frame, sim::TitleBook& titles,
                                           std::uint32_t player_id);

// « Je porte ce titre » — zéro pour n'en porter aucun.
//
// ⚠ ÉMET, ET RIEN DE PLUS : le titre porté ne change qu'à l'accusé, parce que
// le serveur refuse un titre qu'il ne nous reconnaît pas.
[[nodiscard]] core::Result<void> RequestChangeTitle(Connection& link, std::int32_t title_id);

}  // namespace ro::net
