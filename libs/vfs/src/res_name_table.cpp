#include "ro/vfs/res_name_table.h"

#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/log.h"
#include "ro/vfs/vfs.h"

#include "install_table.h"

namespace ro::vfs {
namespace {

// La racine des fichiers d'une carte.
constexpr std::string_view kMapRoot = "data\\";

}  // namespace

core::Result<ResNameTable> ResNameTable::Parse(std::string_view yaml_text) {
  ResNameTable table;
  auto read = VisitTableSection(
      yaml_text, kResNamesPath, "files", YAML::NodeType::Map, [&table](const YAML::Node& files) {
        for (const auto& pair : files) {
          // LE PREMIER L'EMPORTE, comme dans la table ordonnée du
          // client, qui garde ce qu'elle a.
          table.clones_.emplace(ArchivedText(pair.first), ArchivedText(pair.second));
        }
      });
  if (!read) {
    return std::unexpected(read.error());
  }
  return table;
}

ResNameTable ResNameTable::Load(Vfs& vfs) {
  auto text = ReadOptionalTable(vfs, kResNamesPath);
  if (!text || !text->has_value()) {
    if (!text) {
      core::LogWarning("{} — les cartes se lisent sous leur propre nom",
                       core::Describe(text.error()));
    }
    return {};
  }
  auto table = Parse(**text);
  if (!table) {
    core::LogWarning("{} — les cartes se lisent sous leur propre nom",
                     core::Describe(table.error()));
    return {};
  }
  return std::move(*table);
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
