#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LE NOM D'UNE OPTION ALÉATOIRE — « MaxHP +%d », « Dégâts contre les boss
// +%d%% » — lu dans `data\yaml_files\random_options.yml`, que
// `scripts/yaml_tables/random_options.py` tire de la base des options du
// serveur et des libellés de l'installation :
//
//     version: 1
//     mode: Prerenewal
//     options:
//       1: {option: VAR_MAXHPAMOUNT, name: "MaxHP +%d"}
//
// La clé est l'identifiant que le paquet de l'item porte. Une option sans
// `name` s'écrit par son numéro.
struct RandomOptionNames {
  std::unordered_map<std::uint32_t, std::string> templates;

  // Le libellé de l'option `option_id` pour cette valeur, ou vide quand la
  // table ne la connaît pas.
  //
  // ⚠ LE GABARIT VIENT D'UN FICHIER : il ne passe JAMAIS à `printf`. Seuls
  // `%d` (la valeur, une fois) et `%%` s'y remplacent ; un gabarit qui porterait
  // autre chose rend vide, et l'appelant écrit alors son repli.
  [[nodiscard]] std::string Format(std::uint32_t option_id, int value) const;

  [[nodiscard]] bool empty() const noexcept { return templates.empty(); }
};

inline constexpr std::string_view kRandomOptionsPath = "data\\yaml_files\\random_options.yml";

// Remplit un gabarit : voir `RandomOptionNames::Format`.
[[nodiscard]] std::string FillOptionTemplate(std::string_view pattern, int value);

// Le texte de `random_options.yml`. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, sans table `options`, ou dont une option n'est pas une
// table ou porte un `name` qui n'est pas un texte.
[[nodiscard]] core::Result<RandomOptionNames> ParseRandomOptionNames(std::string_view yaml_text);

// Lit la table. ABSENTE, elle rend une table vide plutôt qu'une erreur : les
// options s'écrivent alors par leur numéro.
[[nodiscard]] core::Result<RandomOptionNames> ReadRandomOptionNames(Vfs& vfs);

}  // namespace ro::vfs
