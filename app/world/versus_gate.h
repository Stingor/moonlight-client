#pragma once

#include <cstdint>

#include "ro/sim/battlefield.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// ── CE QUI OUVRE LE CIBLAGE D'UN AUTRE JOUEUR ───────────────────────────────
//
// 🔴 À PART, ET DANS SA PROPRE BIBLIOTHÈQUE, POUR ÊTRE ÉPROUVÉ. C'est la règle
// centrale de la fiche 0055, et elle vivait dans un fichier de l'exécutable —
// donc hors d'atteinte de tout essai. Le dépôt fait ainsi pour tout ce qu'il
// vérifie : `armed_cursor` a sa bibliothèque pour la même raison.
//
// 🔴 DEUX SOURCES, ET C'EST UNE DISJONCTION : la CARTE ouvre, ou le GESTE force.
// Le client d'origine en fait autant — sa cascade de survol essaie le champ de
// bataille, puis le groupe, puis la guilde, et retombe sur la réunion de la
// zone PVP, de la zone GvG et du drapeau DISABLE_LOCKON ; MAJ, lui, est lu en
// tête et entre dans la même disjonction.
//
// 🔴 ET C'EST LA CARTE QU'ON REGARDE, JAMAIS L'INDIVIDU. Le serveur ne dit pas
// qui est vraiment frappable — `pvp_noguild` et `pvp_noparty` n'ont aucun bit,
// et six états individuels ne voyagent dans aucun paquet. On peint donc
// l'intention, et le serveur refuse ce qu'il doit refuser.
struct VersusGate {
  // La carte fait des autres joueurs des adversaires : `ro::sim::IsVersusMap`.
  bool map_opens = false;
  // MAJ tenu À L'INSTANT DU CLIC, ou `/noshift` allumé. ⚠ LE RÉGLAGE NE FAIT
  // QUE REMPLACER LA TOUCHE, il ne l'inverse pas : voir `GameOptions::no_shift`.
  bool forced = false;
  // LES CAMPS DU CHAMP DE BATAILLE, sur une telle carte seulement ; nul ailleurs.
  const ro::sim::Battlefield* battlefield = nullptr;

  [[nodiscard]] constexpr bool opens() const noexcept { return map_opens || forced; }

  // LA PORTE POUR UN ACTEUR PRÉCIS. Sur un champ de bataille, la carte n'ouvre
  // que sur l'autre camp : un coéquipier ne s'attaque pas sans le geste, et le
  // clic sur lui descend au sol comme en ville. Un camp que le serveur n'a pas
  // dit ouvre, comme le reste de la carte.
  [[nodiscard]] bool opens_on(std::uint32_t actor_id) const {
    if (forced) return true;
    return map_opens && (battlefield == nullptr || !battlefield->IsAlly(actor_id));
  }
};

// La porte, composée de ce que la CARTE dit et de ce que le GESTE demande.
//
// 🔴 UN SEUL ENDROIT, PARCE QU'IL Y A DEUX LECTEURS : le curseur, à chaque
// image, et le clic, à l'instant où il part. Les deux doivent voir la MÊME
// porte — un curseur qui promet l'épée pendant qu'un clic descend au sol est la
// pire des deux erreurs possibles, et elle ne se verrait qu'en jeu.
//
// ⚠ `shift_held` SE RELÈVE AU BON MOMENT PAR CHAQUE APPELANT, et ce n'est pas
// le même : le curseur veut l'état COURANT — presser MAJ change sa forme sans
// bouger la souris —, le clic veut celui de L'APPUI. Cette fonction ne lit
// aucun clavier ; elle ne fait que réunir.
[[nodiscard]] VersusGate VersusGateOf(const ro::sim::World& world, bool shift_held, bool no_shift);

// ── ET CE QUE L'AUTRE TOUCHE DÉCIDE : UN COUP, OU LA POURSUITE ──────────────
//
// 🔴 MÊME FORME QUE LA PORTE, ET POUR LA MÊME RAISON : CTRL tenu, ou `/noctrl`
// allumé. Le réglage REMPLACE la touche, il ne l'inverse pas, comme chez le
// client d'origine.
//
// ⚠ MAIS UN DÉFAUT INVERSE DE CELUI DE `/noshift` : `/noctrl` est ALLUMÉ
// d'usine. Sans relâche est donc l'ordinaire, et ce que la commande éteinte
// rend possible est le coup UNIQUE. Le libellé « pas de Ctrl » se lit à
// l'envers de son effet, comme celui de sa sœur.
//
// ⚠ ET CETTE DÉCISION NE TOUCHE PAS LE CURSEUR, contrairement à la porte : sa
// forme est la même qu'on frappe une fois ou dix. Elle n'a donc qu'un seul
// lecteur, le clic — mais elle se nomme ici, à côté de sa sœur, parce que les
// deux touches se lisent ensemble et que leurs défauts s'opposent.
[[nodiscard]] constexpr bool AttackRepeats(bool ctrl_held, bool no_ctrl) noexcept {
  return ctrl_held || no_ctrl;
}

}  // namespace moonclient
