#include "ro/vfs/res_name_table.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "ro/core/ascii.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {
namespace {

// Le fichier de l'installation, et la racine des fichiers d'une carte.
constexpr std::string_view kTablePath = "data\\resnametable.txt";
constexpr std::string_view kMapRoot = "data\\";

// Le séparateur du fichier. Une ligne porte `source#cible#`.
constexpr char kFieldEnd = '#';

}  // namespace

ResNameTable ResNameTable::Parse(std::string_view contents) {
  ResNameTable table;
  std::string pending_source;
  std::size_t token_start = 0;
  for (std::size_t at = 0; at < contents.size(); ++at) {
    const char letter = contents[at];
    // ⚠ UNE FIN DE LIGNE TERMINE LE JETON SANS OUBLIER LA SOURCE EN ATTENTE :
    // le client ne remet pas son compteur à zéro là, si bien qu'une ligne à un
    // seul dièse s'apparie avec la suivante. Aucune ligne du fichier livré n'en
    // porte un nombre autre que zéro ou deux, mais le porter autrement ferait
    // diverger les deux clients sur un fichier que l'exploitant peut éditer.
    if (letter == '\n' || letter == '\r') {
      token_start = at + 1;
      continue;
    }
    if (letter != kFieldEnd) {
      continue;
    }
    const std::string_view token = contents.substr(token_start, at - token_start);
    token_start = at + 1;
    if (pending_source.empty()) {
      pending_source = token;
      continue;
    }
    // LE PREMIER L'EMPORTE : le client insère dans une table ordonnée, qui
    // garde ce qu'elle a. Le fichier livré répète 97 de ses 2 274 entrées.
    table.clones_.emplace(std::move(pending_source), std::string(token));
    pending_source.clear();
  }
  return table;
}

ResNameTable ResNameTable::Load(Vfs& vfs) {
  auto bytes = vfs.Read(kTablePath);
  if (!bytes) {
    return {};
  }
  return Parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

std::string ResNameTable::CloneOf(std::string_view name) const {
  std::string lowered = core::AsciiLower(name);
  // ⚠ LA SECONDE CHANCE DU CLIENT N'EST PAS PORTÉE. Faute de clé, il retire
  // trois caractères quatre rangs avant le dernier `@` et cherche de nouveau.
  // Ce rattrapage vise les cartes d'INSTANCE d'une ancienne
  // nomenclature ; celle de ce serveur bâtit `%04u#%0*u`
  // (`instance_generate_mapname`, moonlight/src/map/instance.cpp:803) et ne
  // porte aucun `@`, si bien que la branche ne peut pas s'y allumer.
  const auto found = clones_.find(lowered);
  return found == clones_.end() ? lowered : found->second;
}

std::string ResNameTable::MapFilePath(std::string_view file_name) const {
  return std::string(kMapRoot) + CloneOf(file_name);
}

}  // namespace ro::vfs
