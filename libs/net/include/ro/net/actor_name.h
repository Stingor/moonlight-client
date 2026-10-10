#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LE NOM AU-DESSUS D'UN ACTEUR, ET CE QUE LE SERVEUR ÉCRIT SOUS LUI.
//
// 🔴 CE N'EST PAS LE NOM DE L'APPARITION. Les trois paquets d'unité portent
// déjà un nom — celui que la base contient —, et c'est celui-là que le monde
// pose en voyant entrer un acteur. Ce paquet-ci porte ce que le serveur veut
// MONTRER, et les deux diffèrent dès que quelque chose se met devant : un
// `@fakename`, l'étiquette d'absence que Moonlight colle devant le pseudo, le
// nom masqué d'un siège. Il porte en outre ce qu'aucune apparition ne dit —
// groupe, guilde, rang, titre.
//
// 🔴 L'ÉTIQUETTE D'ABSENCE NE RENOMME PAS L'ACTEUR : « [AFK] Nom » va dans
// `NameCard::shown_name`, et `Actor::name` garde le nom du personnage. Le
// serveur cherche un joueur invité par son nom EXACT ; lui envoyer le libellé
// répond « pas connecté ou n'existe pas » tant que l'autre reste absent.
//
// DEUX PAQUETS, ET LE SERVEUR CHOISIT PAR L'ESPÈCE (`clif_name`) :
//
//   · `ZC 0x0A30` — un JOUEUR, un monstre de garnison, et TOUT monstre dès que
//     `show_mob_info` est posé. Nom, trois chaînes, un numéro de titre ;
//   · `ZC 0x0ADF` — un PNJ, un familier, un homoncule, un mercenaire, un
//     élémentaire, et un monstre quand `show_mob_info` est à zéro. Nom et
//     titre en toutes lettres, rien d'autre.
//
// ⚠ CE SERVEUR-CI POSE `show_mob_info: 6` : ses monstres passent donc par
// 0x0A30, et leur niveau et leur vie arrivent DANS LE CHAMP DU GROUPE. Voir
// `sim::NameCard`, qui nomme ces champs par leur PLACE et non par leur sens.
//
// ⚠ UN ACTEUR DÉGUISÉ EST NOMMÉ SOUS L'OPPOSÉ DE SON IDENTIFIANT, et cela
// tombe juste sans rien faire : `disguised_bl_id` rend `-(int32)id`, et le
// paquet d'apparition emploie EXACTEMENT le même calcul. Les deux se
// retrouvent donc sous la même valeur non signée.
//
// ── CE QUI N'EST PAS TRAITÉ ICI, ET POURQUOI ────────────────────────────────
//
// `ZC 0x0AF7` `ZC_ACK_REQNAME_BYGID` répond lui aussi par un nom, mais sous un
// identifiant de PERSONNAGE — pas celui d'un acteur du monde. Le serveur s'en
// sert pour remplir la fenêtre de groupe et l'aide à la saisie d'un murmure,
// jamais pour une étiquette. Le monde n'indexe rien par cet identifiant-là :
// le brancher ici ne trouverait personne. C'est `ro/net/item_facts.h` qui le
// lit, pour nommer le forgeron d'un objet.
[[nodiscard]] FrameOutcome ApplyActorNameFrame(const Frame& frame, sim::World& world);

// « Qui est-ce ? »
//
// 🔴 LE SERVEUR NE POUSSE PAS LA CARTE D'UN JOUEUR À L'ENTRÉE EN VUE. Il ne
// l'envoie qu'aux instants où elle CHANGE — métier, titre, guilde, absence —
// et un joueur déjà posé quand on arrive n'en enverra donc jamais. C'est au
// client de la réclamer, une fois par entité ; le client natif le fait
// paresseusement, au moment où il a besoin d'écrire l'étiquette.
//
// ⚠ UN MONSTRE N'A PAS BESOIN QU'ON DEMANDE : `clif_getareachar_unit` diffuse
// sa carte à chaque entrée en vue. C'est même l'essentiel du trafic — quatre
// cent quatre-vingt-quatre trames en trois minutes et demie sur une carte
// peuplée.
[[nodiscard]] core::Result<void> RequestActorName(Connection& link, std::uint32_t actor_id);

}  // namespace ro::net
