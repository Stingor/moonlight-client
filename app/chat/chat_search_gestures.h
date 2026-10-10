#pragma once

#include <cstdint>
#include <string>

#include "ro/ui/chat_link.h"

#include "chat/chat_monster_link.h"
#include "chat/chat_navi_search_link.h"

namespace moonclient {

// ── CE QUE DEMANDENT LES GESTES QUI CHERCHENT ───────────────────────────────
//
// Le nom d'un monstre, d'où qu'il vienne — le fil, la saisie, une table de
// tirage, le carnet de chasse, la navigation —, et une recherche `<NAVS>`. Ce
// qui se décide sans la partie est ici, pour s'essayer sur un banc.

// LA COMMANDE DU SERVEUR qu'un geste sur un monstre envoie, comme une ligne
// tapée : « @mobinfo 1039 », « @whereis 1039 ». Vide pour tout autre geste, et
// sans identifiant.
[[nodiscard]] std::string MonsterCommandLine(ro::ui::ChatLinkGesture gesture,
                                             std::uint32_t monster_id);

// L'IDENTIFIANT DE LA BASE qu'un nom de monstre porte, ou zéro quand il n'en
// porte qu'une CLASSE DE SPRITE (`by_view`) — celle d'un acteur ou d'une donnée
// de navigation, que rien chez le client ne ramène à la base. La fiche remonte
// d'une classe, le serveur faisant la correspondance ; le bestiaire du site et
// les deux commandes, eux, ne lisent que la base, et un monstre maison dont la
// classe est celle d'un autre leur ferait décrire cet autre.
[[nodiscard]] std::uint32_t MonsterDatabaseId(const MonsterLink& monster, bool by_view);

// LA RECHERCHE « OÙ LE TROUVER » D'UN MONSTRE : sur son NOM, le seul que les
// données de navigation connaissent, et SANS carte — on veut tous ses lieux
// d'apparition. Terme vide sans nom.
[[nodiscard]] NaviSearchLink WhereToFindSearch(const MonsterLink& monster);

// LA CARTE OÙ MÈNE « Y ALLER » : le terme pour une carte, qui est son propre
// lieu ; le contexte pour un PNJ ou un monstre. Vide quand la recherche n'a pas
// de lieu.
[[nodiscard]] std::string NaviSearchDestination(const NaviSearchLink& link);

}  // namespace moonclient
