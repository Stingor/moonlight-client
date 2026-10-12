#pragma once

// LE TEXTE D'`item_info.yml`, lu sans bibliothèque YAML.
//
// La table pèse une douzaine de méga-octets : un analyseur YAML général la lit
// en plusieurs secondes, quand ce lecteur-ci la lit en une fraction. Il ne
// connaît que la forme que `scripts/yaml_tables/item_info.py` écrit, qui reste
// du YAML valide :
//
//   · des commentaires `#` et des lignes vides ;
//   · `version: <entier>`, puis `items: {` ;
//   · par objet, `<identifiant>: {<champ>: <valeur>, …}`, séparés par des
//     virgules, et la table fermée par `}` ;
//   · une valeur est un entier sans signe, `true` ou `false`, un texte entre
//     guillemets doubles aux échappements de JSON (`\"`, `\\`, `\/`, `\b`,
//     `\f`, `\n`, `\r`, `\t`, `\uXXXX`), ou une suite `[…]` de tels textes.
//
// ⚠ TOUT AUTRE TEXTE EST REFUSÉ, même du YAML valide : un texte sans
// guillemets, un échappement propre à YAML, un champ inconnu. Une table
// retouchée à la main se refuse donc au lieu de se lire de travers, et le
// message dit sa ligne.

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace ro::vfs {

// Un objet tel que la table l'écrit, avant que le lecteur n'y applique les
// règles du client. Un champ absent garde sa valeur par défaut.
struct ItemInfoRecord {
  std::uint32_t item_id = 0;
  std::string name;
  // En UTF-8, comme tout le texte.
  std::string resource;
  std::uint8_t slots = 0;
  std::uint16_t view = 0;
  bool costume = false;
  std::vector<std::string> description;
};

// La version de la forme que ce lecteur comprend.
inline constexpr std::uint32_t kItemInfoVersion = 1;

// Lit le texte et rend chaque objet à `take`, dans l'ordre du texte. Rend une
// erreur à la première forme inattendue, ou à la première erreur que `take`
// rend.
[[nodiscard]] core::Result<void> ReadItemInfoText(
    std::string_view text, const std::function<core::Result<void>(ItemInfoRecord&&)>& take);

}  // namespace ro::vfs
