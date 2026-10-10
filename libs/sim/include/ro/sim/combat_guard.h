#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "ro/sim/actor.h"

namespace ro::sim {

// LA GARDE — arme levée, sur place — que prend qui vient de se battre.
//
// 🔴 CE N'EST PAS UNE POSE QUE QUELQU'UN DEMANDE, C'EST UN REPOS. Chez le
// client, le retour à l'état zéro pose d'abord le rang de l'ATTENTE — puis, si
// l'état QUITTÉ est l'un de ceux ci-dessous, il le remplace aussitôt par le rang
// QUATRE, la garde. On repart donc du combat EN GARDE, et du reste à l'attente.
//
// ⚠ L'ÉTAT QUITTÉ EST CELUI DU DERNIER GESTE, et c'est lui seul qui décide : un
// coup porté ou encaissé, une incantation ordinaire — finie ou interrompue —,
// cinq poses de compétence. L'emploi ordinaire d'une compétence, le ramassage,
// la marche, l'assise ramènent à l'attente, même après un combat. C'est
// `app`, qui sait lire l'état d'un geste dans les tables du client, qui dit
// lequel a été quitté ; ici ne vit que la liste.
//
// ⚠ ET ELLE TIENT JUSQU'AU GESTE SUIVANT. Rien ne la retire au bout d'un temps :
// c'est le passage par un AUTRE état qui en sort, et marcher en est un. L'arrêt
// qui suit une marche quitte l'état de marche, qui n'est pas dans la liste, et
// retombe donc à l'attente ordinaire.
//
// 🔴 LA SECONDE PORTE EST LE MODE PK. Sortant de n'importe quel état, le client
// prend aussi la garde si l'acteur porte le mode PK — le karma que le serveur
// écrit dans ses paquets d'unité. Un tel joueur ne se repose jamais qu'en garde.

// Les états d'action du client que l'on quitte pour la GARDE : l'attaque, la
// blessure, l'incantation ordinaire, et cinq poses de compétence.
inline constexpr std::array<std::int8_t, 8> kStatesLeftInGuard = {2, 4, 8, 52, 53, 54, 58, 59};

// Vrai si le retour au repos, quittant cet état, prend la GARDE plutôt que
// l'attente.
[[nodiscard]] inline bool LeavingStateGuards(std::int8_t left_state) noexcept {
  return std::ranges::find(kStatesLeftInGuard, left_state) != kStatesLeftInGuard.end();
}

// Vrai si cet acteur, quittant cet état, se tient EN GARDE plutôt qu'au repos :
// l'état l'exige, ou le mode PK.
[[nodiscard]] inline bool GuardsLeaving(const Actor& actor, std::int8_t left_state) noexcept {
  return LeavingStateGuards(left_state) || actor.pk_mode;
}

}  // namespace ro::sim
