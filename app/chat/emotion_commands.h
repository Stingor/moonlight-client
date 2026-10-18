#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace moonclient {

// UNE COMMANDE D'ÉMOTICÔNE ET LE NUMÉRO QU'ELLE ENVOIE.
//
// Un agrégat plutôt qu'une paire : le constructeur d'une paire prend ses
// arguments par valeur, et un littéral entier y passe par un int qu'il faut
// rétrécir. Écrit ainsi, chaque numéro est vérifié à la compilation.
struct EmotionCommandRow {
  std::string_view name;
  std::uint16_t emotion;
};

// L'émoticône qu'une ligne demande, ou rien si elle n'en demande aucune.
//
// Les commandes viennent du jeu lui-même : celles que le serveur documente dans
// son énumération emotion_type (src/map/clif.hpp), celles de la liste
// classique dans le même ordre qu'elle, et celles que le client du joueur
// nomme d'un numéro (`/e1` … `/e33`) ou d'un mot (`/sorry`, `/dice`). Une
// commande inconnue n'est PAS envoyée au serveur — le jeu les traite chez lui.
//
// ⚠ LA LIGNE ENTIÈRE, À LA CASSE PRÈS : « /X » est une émoticône, « /x » non.
[[nodiscard]] std::optional<std::uint16_t> EmotionCommand(std::string_view line);

// LE CHEMIN INVERSE : la commande qui joue cette émoticône-là, ou une vue vide.
//
// 🔴 LA MÊME TABLE QUE CI-DESSUS, LUE À L'ENVERS, et c'est ce qui les garde
// d'accord. Une seconde liste finirait par proposer une commande que le routage
// ne reconnaît pas — donc une macro qui ne joue rien et se dit tout haut.
//
// ⚠ LA PREMIÈRE FORME L'EMPORTE quand plusieurs nomment la même : « /ho » avant
// « /delight », « /rice » avant « /e2 ». C'est la forme lisible, celle que le
// client écrit lui-même.
//
// ⚠ ET UNE ÉMOTICÔNE PEUT N'EN AVOIR AUCUNE : le sprite en porte près de cent,
// la liste du jeu n'en nomme qu'une partie. Ce qui n'a pas de commande n'a rien
// à faire dans une macro — elle ne jouerait rien chez les autres clients.
[[nodiscard]] std::string_view EmotionCommandOf(std::uint16_t emotion);

// CE QUE `/emotion` ÉCRIT SOUS SON TITRE : une commande par ligne, dans l'ordre
// où le client du joueur les énumère. Les noms ne se traduisent pas.
[[nodiscard]] std::span<const std::string_view> EmotionListLines();

// TOUTE LA TABLE, dans son ordre : la liste des commandes du chat la propose
// telle quelle, la première forme de chaque émoticône en tête, les autres en
// alias. ⚠ LA COMMANDE NUMÉROTÉE « /e <n> » N'Y EST PAS : elle se lit à part.
[[nodiscard]] std::span<const EmotionCommandRow> EmotionCommandRows();

}  // namespace moonclient
