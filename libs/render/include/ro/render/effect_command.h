#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ro::render {

// Le chemin d'effet qu'une ligne tapée demande, ou rien si elle n'en demande
// aucun.
//
// « /effet <nom> » sert à REGARDER un effet sans attendre que le jeu le joue :
// c'est ainsi qu'on juge sa géométrie, son mélange et sa cadence à sa vraie
// place, plutôt que sur une planche de vignettes. Un nom seul suffit — il est
// cherché dans le dossier des effets, et l'extension est sous-entendue.
//
// Un argument tout en CHIFFRES est un identifiant d'effet, celui que le serveur
// emploie, et non un nom de fichier : c'est ce qui sépare « /effet 100 » — le
// centième effet — de « /effet 100.str », le fichier de ce nom. Les deux
// existent, et rien d'autre ne les distinguerait.
//
// Un chemin VIDE dit que la ligne était bien la nôtre mais que cet identifiant
// n'a pas de fichier ; l'appelant sait alors quoi répondre, et à qui.
//
// ⚠ SANS LE C, et la lettre compte. Le jeu réserve cent soixante-quatorze
// commandes en barre oblique, rangées dans une même table du client, dont
// /effect — qui coupe l'affichage des effets de skills au
// sol — et /str+, qui monte la force. Une commande à nous qui reprendrait un de
// ces noms empêcherait d'implémenter celle du jeu, et SILENCIEUSEMENT : rien
// n'avertit qu'une commande en recouvre une autre. Avant d'en ajouter une,
// consulter cette table ; certaines entrées portent un signe, et une recherche
// qui ne l'attend pas les manque.
[[nodiscard]] std::optional<std::string> EffectCommand(std::string_view line, std::uint32_t draw);

}  // namespace ro::render
