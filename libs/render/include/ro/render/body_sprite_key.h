#pragma once

#include <cstdint>
#include <string_view>

namespace ro::render {

// L'IDENTITÉ D'UN CORPS, RÉDUITE À TRENTE-DEUX BITS : FNV-1a sur le chemin de
// son `.spr`.
//
// 🔴 C'EST LE SERVEUR QUI L'IMPOSE, et tous les clients doivent en trouver le
// même pour le même corps : c'est lui qui désigne, dans une recette de
// couleurs, DE QUEL corps on parle. Un joueur a une recette par corps — à
// pied, sur sa monture, en costume — parce qu'un index de palette ne veut
// rien dire hors du sprite où il a été mesuré.
//
// Le chemin lui-même ne conviendrait pas : jusqu'à quatre-vingts octets, et du
// CP949 au milieu. D'où la normalisation — séparateurs unifiés, extension
// `.spr` ignorée (le même corps se nomme tantôt par sa base, tantôt par son
// fichier), et repli en minuscules des SEULS 'A' à 'Z'.
//
// ⚠ LE REPLI ASCII N'EST PAS UN DÉTAIL : un `tolower` de la bibliothèque sur
// un octet CP949 dépend de la locale, et abîmerait les dossiers coréens du
// chemin. Deux clients de locales différentes ne calculeraient plus la même
// clé, donc ne choisiraient plus la même variante.
//
// Rend zéro sur un chemin vide — la sentinelle « corps inconnu », qui ne
// désigne jamais une variante ; un chemin qui condenserait à zéro rend un.
[[nodiscard]] std::uint32_t BodySpriteKey(std::string_view sprite_path);

}  // namespace ro::render
