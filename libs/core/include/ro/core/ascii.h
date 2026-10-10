#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ro::core {

// Abaisse les octets 0x41–0x5A, et EUX SEULS.
//
// Les noms de fichiers RO sont stockés en CP949, un encodage à deux octets. Un
// tolower() dépendant de la locale, ou appliqué aux octets >= 0x80, réécrirait
// des octets coréens et rendrait le fichier introuvable : c'est précisément le
// piège que cette fonction existe pour éviter.
//
// Un second octet CP949 PEUT tomber dans 0x41–0x5A, et il est alors abaissé lui
// aussi. Ce n'est pas un défaut : le client fait exactement pareil (_strlwr en
// locale C, quand il normalise un chemin d'archive), et comme la clé de
// recherche subit la même transformation que la clé stockée, la
// correspondance tient. Ce qui compte
// n'est pas de préserver le nom, c'est d'être d'accord avec le client.
[[nodiscard]] constexpr std::uint8_t AsciiLower(std::uint8_t byte) noexcept {
  return (byte >= 0x41 && byte <= 0x5A) ? static_cast<std::uint8_t>(byte + 0x20) : byte;
}

[[nodiscard]] std::string AsciiLower(std::string_view text);

// L'ORDRE ALPHABÉTIQUE QUI IGNORE LA CASSE, sans rien allouer.
//
// ⚠ ASCII SEULEMENT, comme tout ce qui est ici : un nom d'objet peut porter des
// octets qui n'en sont pas, et ils se comparent alors tels quels. C'est ce que
// fait le client, qui trie ses listes au `stricmp` de sa bibliothèque.
//
// ⚠ À ÉGALITÉ DE LETTRES, LE PLUS COURT PASSE DEVANT — « Potion » avant
// « Potion blanche ». Sans cette règle, deux noms dont l'un préfixe l'autre
// seraient déclarés équivalents, et leur ordre dépendrait de l'algorithme de
// tri plutôt que du nom.
[[nodiscard]] bool AsciiLess(std::string_view left, std::string_view right);

// L'ÉGALITÉ QUI IGNORE LA CASSE, ASCII seulement : « /BC » vaut « /bc », mais
// un octet hors ASCII ne se compare qu'à lui-même.
[[nodiscard]] bool AsciiEqualsFolded(std::string_view left, std::string_view right);

// `text` commence par `prefix`, à la casse ASCII près.
[[nodiscard]] bool AsciiStartsWithFolded(std::string_view text, std::string_view prefix);

}  // namespace ro::core
