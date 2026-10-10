#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

// LA PAROLE NOMMÉE, partagée par les fichiers de paquets qui en émettent.
//
// En-tête PRIVÉ : il vit dans `src/`, parce que rien hors de cette bibliothèque
// n'a à fabriquer une trame de parole sans passer par sa requête.
namespace ro::proto {

// Le corps d'un message PARLÉ : « Nom : texte », puis un zéro. Longueur
// VARIABLE — la table dit moins un, et c'est la tête du paquet qui porte la
// longueur réelle ; le zéro terminal y compte, le serveur lisant une chaîne C.
// Toutes les paroles adressées à un public — tout le monde, le groupe, la
// guilde, le clan, le champ de bataille — ont cette forme, et le serveur les
// contrôle de la même façon.
//
// Échoue si le serveur ne déclare plus lire `opcode` par `handler` à ces
// décalages, ou si le texte dépasse ce qu'il accepte.
//
// ⚠ DEUX BORNES, et la plus serrée joue : le texte seul au rejet, « Nom :
// texte » à la diffusion. La seconde est propre à chaque relais, et absente
// (`body_max` vide) quand le relais ne coupe pas à cette taille.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeNamedChat(
    std::uint16_t opcode, std::string_view handler, std::span<const std::uint16_t> offsets,
    std::string_view speaker, std::string_view text, std::optional<std::size_t> body_max);

}  // namespace ro::proto
