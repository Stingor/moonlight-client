#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LE NOM D'UNE OPTION ALÉATOIRE — « MaxHP +%d », « Dégâts contre les boss
// +%d%% ».
//
// Le client le demande à sa fonction Lua `GetVarOptionName`, qui ne fait que
// lire la table `NameTable_VAR` : un gabarit par identifiant d'option, indexé
// par les constantes de `EnumVAR` (la première valeur de chaque paire). Les deux
// fichiers sont en clair, et se lisent dans cet ordre.
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

inline constexpr std::string_view kEnumVarPath =
    "data\\luafiles514\\lua files\\datainfo\\enumvar.lub";
inline constexpr std::string_view kRandomOptionNamesPath =
    "data\\luafiles514\\lua files\\datainfo\\addrandomoptionnametable.lub";

// Remplit un gabarit : voir `RandomOptionNames::Format`.
[[nodiscard]] std::string FillOptionTemplate(std::string_view pattern, int value);

// Lit les deux tables. ABSENTES, elles rendent une table vide plutôt qu'une
// erreur : les options s'écrivent alors par leur numéro.
[[nodiscard]] core::Result<RandomOptionNames> ReadRandomOptionNames(Vfs& vfs);

}  // namespace ro::vfs
