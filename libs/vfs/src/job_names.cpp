#include "ro/vfs/job_names.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "ro/formats/lua_table.h"
#include "ro/vfs/encoding.h"

namespace ro::vfs {
namespace {

// Lit un fichier de la chaîne PAR-DESSUS les globales déjà acquises. Une absence
// n'est pas une erreur : la table restera vide, et la fiche écrira ses niveaux.
[[nodiscard]] core::Result<bool> ReadInto(Vfs& vfs, std::string_view internal_path,
                                          formats::LuaTable& globals) {
  auto bytes = vfs.Read(std::string(internal_path));
  if (!bytes) {
    if (bytes.error().code == core::ErrorCode::kNotFound) {
      return false;
    }
    return std::unexpected(
        core::Error{bytes.error().code, std::string(internal_path) + " : " + bytes.error().detail});
  }
  // 🔴 LES CLÉS QU'ON NE SAIT PAS RÉSOUDRE SE SAUTENT ICI, et seulement ici :
  // `pcjobname` indexe des constantes que le CLIENT pose — les formes MONTÉES,
  // au-delà de sa cent cinquante-quatrième entrée. Refuser coûterait les cent
  // cinquante-quatre pour vingt-cinq que le serveur n'envoie jamais.
  auto read = formats::ReadLuaFile(*bytes, std::move(globals), formats::UnnamedKey::kSkipEntry);
  if (!read) {
    return std::unexpected(
        core::Error{read.error().code, std::string(internal_path) + " : " + read.error().detail});
  }
  globals = std::move(*read);
  return true;
}

}  // namespace

std::string_view JobNames::Find(std::uint32_t job_id) const {
  const auto found = by_id.find(job_id);
  return found != by_id.end() ? std::string_view(found->second) : std::string_view();
}

core::Result<JobNames> ReadJobNames(Vfs& vfs) {
  JobNames names;
  formats::LuaTable globals;

  auto identity = ReadInto(vfs, kJobNameFiles[0], globals);
  if (!identity) {
    return std::unexpected(identity.error());
  }
  if (!*identity) {
    return names;
  }
  // 🔴 LE NOM QUE LA TABLE EMPLOIE N'EST PAS CELUI QU'ELLE DÉFINIT. `jobidentity`
  // écrit `JTtbl` ; `pcjobname` indexe `JOBID`. Rien dans l'installation ne pose
  // le second — relevé sur les soixante-dix tables de `datainfo`, `admin` et
  // `cls` : deux fichiers l'emploient, AUCUN ne l'écrit. C'est le client qui le
  // crée avant d'évaluer les siens, et c'est ce que cette ligne rejoue.
  //
  // ⚠ SANS ELLE LA CHAÎNE SE REFUSE ENTIÈRE, avec « JOBID n'est pas une table ».
  static_cast<void>(formats::AliasGlobal(globals, kJobIdentityGlobal, kJobIdentityAlias));

  auto shown = ReadInto(vfs, kJobNameFiles[1], globals);
  if (!shown) {
    return std::unexpected(shown.error());
  }
  if (!*shown) {
    return names;
  }

  const formats::LuaValue* table = globals.Find(kJobNameGlobal);
  if (table == nullptr || table->table == nullptr) {
    return names;
  }
  for (const auto& [key, name] : table->table->by_index) {
    const std::optional<std::uint32_t> job_id =
        formats::LuaWholeNumber(key, std::numeric_limits<std::uint32_t>::max());
    if (name.kind != formats::LuaValue::Kind::kString || !job_id.has_value()) {
      continue;
    }
    // ⚠ TRANSCODÉ EN TOLÉRANT, comme les noms d'objets : ces tables viennent
    // d'une traduction communautaire, et un octet douteux dans un nom ne doit pas
    // coûter la table entière.
    names.by_id[*job_id] = Cp949ToUtf8Lossy(name.text);
  }
  return names;
}

}  // namespace ro::vfs
