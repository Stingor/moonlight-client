#pragma once

// LE TEXTE D'`item_info.yml`, lu et écrit sans bibliothèque YAML.
//
// La table pèse une douzaine de méga-octets : un analyseur YAML général la lit
// en plusieurs secondes, quand ce lecteur-ci la lit en une fraction. Il ne
// connaît que la forme que l'écrivain ci-dessous produit — la même que
// `scripts/yaml_tables/item_info.py` —, qui reste du YAML valide :
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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace ro::vfs {

// Un objet tel que la table l'écrit, avant que le lecteur n'y applique les
// règles du client. Un champ absent garde sa valeur par défaut, et l'écrivain
// omet un champ qui la garde.
struct ItemInfoRecord {
  std::uint32_t item_id = 0;
  std::string name;
  // En UTF-8, comme tout le texte.
  std::string resource;
  std::uint8_t slots = 0;
  std::uint16_t view = 0;
  bool costume = false;
  // L'effet de l'objet au sol : la table le porte, le client ne le montre pas.
  std::uint32_t effect = 0;
  // Les lignes BRUTES : le `#` final qui vaut trait de séparation y reste.
  std::vector<std::string> description;

  [[nodiscard]] bool operator==(const ItemInfoRecord& other) const = default;
};

// La version de la forme que ce lecteur comprend.
inline constexpr std::uint32_t kItemInfoVersion = 1;

// Lit le texte et rend chaque objet à `take`, dans l'ordre du texte. Rend une
// erreur à la première forme inattendue, ou à la première erreur que `take`
// rend.
[[nodiscard]] core::Result<void> ReadItemInfoText(
    std::string_view text, const std::function<core::Result<void>(ItemInfoRecord&&)>& take);

// L'EN-TÊTE DU TEXTE : ses lignes de commentaire et ses lignes vides de tête,
// fins de ligne comprises, jusqu'à la première ligne de données. C'est ce que
// l'écrivain recopie tel quel.
[[nodiscard]] std::string_view ItemInfoHeader(std::string_view text);

// ÉCRIT LA TABLE : l'en-tête tel quel, puis une ligne par objet, PAR
// IDENTIFIANT CROISSANT quel que soit l'ordre reçu.
//
// 🔴 OCTET POUR OCTET LA FORME DU SCRIPT D'IMPORT, et un essai le tient : les
// champs dans l'ordre `name`, `resource`, `slots`, `view`, `costume`, `effect`,
// `description` ; un champ nul, faux ou vide omis ; les textes entre
// guillemets à la manière de JSON sans échappement des caractères non ASCII —
// seuls `"`, `\`, les fins de ligne, la tabulation et les autres caractères de
// contrôle s'échappent, ceux-ci en `\u00xx` minuscule. Une table relue puis
// réécrite rend donc ses propres octets.
[[nodiscard]] std::string WriteItemInfoText(std::string_view header,
                                            std::span<const ItemInfoRecord> records);

}  // namespace ro::vfs
