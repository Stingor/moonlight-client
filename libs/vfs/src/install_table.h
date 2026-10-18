#pragma once

// ── LES TABLES DE L'INSTALLATION, CONVERTIES EN YAML ────────────────────────
//
// Le client d'origine lit treize petites tables en texte dans `data\` ;
// `scripts/yaml_tables/install_text_tables.py` les convertit sous
// `data\yaml_files\`, en gardant ce que leur lecteur en tirait. Ce que les
// lecteurs de `vfs` partagent pour les lire est ici.
//
// En-tête PRIVÉ de `libs/vfs/src`.

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/vfs.h"

#include "yaml_required.h"

namespace ro::vfs {

inline constexpr int kInstallTableVersion = 1;

[[nodiscard]] inline core::Error RefusedTable(std::string_view table_path, std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     std::string(table_path) + " : " + std::move(detail)};
}

// Le texte d'une table, ou rien quand l'installation ne la porte pas. Une autre
// panne de lecture est une erreur.
[[nodiscard]] inline core::Result<std::optional<std::string>> ReadOptionalTable(
    Vfs& vfs, std::string_view table_path) {
  auto bytes = vfs.Read(table_path);
  if (!bytes) {
    if (bytes.error().code == core::ErrorCode::kNotFound) {
      return std::optional<std::string>();
    }
    return std::unexpected(
        core::Error{bytes.error().code, std::string(table_path) + " : " + bytes.error().detail});
  }
  return std::optional<std::string>(std::string(bytes->begin(), bytes->end()));
}

// LES OCTETS D'ARCHIVE QU'UN TEXTE DE LA TABLE DÉSIGNE. Le YAML porte de
// l'UTF-8 ; le client gardait les octets CP949 du fichier d'origine, et c'est
// eux qu'il rend. Un texte qui ne s'écrit pas en CP949 lève l'exception de
// yaml-cpp.
[[nodiscard]] inline std::string ArchivedText(const YAML::Node& node) {
  auto archived = Utf8ToCp949(node.as<std::string>());
  if (!archived) {
    throw YAML::RepresentationException(node.Mark(), "un texte qui ne s'écrit pas en CP949");
  }
  return std::move(*archived);
}

// Un identifiant d'objet, sur 32 bits.
[[nodiscard]] inline std::uint32_t ItemIdOf(const YAML::Node& node) {
  return static_cast<std::uint32_t>(Whole(node, 0, std::numeric_limits<std::uint32_t>::max()));
}

// LA SECTION `section` D'UNE TABLE VERSIONNÉE, passée à `visit`, qui la
// parcourt. Un texte qui n'est pas du YAML, une version non gérée, une section
// absente ou d'une autre forme que `form`, et toute exception levée par `visit`
// se rendent en refus, sous le nom de la table.
template <typename Visit>
[[nodiscard]] core::Result<void> VisitTableSection(std::string_view yaml_text,
                                                   std::string_view table_path, const char* section,
                                                   YAML::NodeType::value form, Visit&& visit) {
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const auto root = VersionedRoot(yaml_text, kInstallTableVersion);
    if (!root) {
      return std::unexpected(RefusedTable(table_path, root.error()));
    }
    const YAML::Node found = Required(*root, section);
    if (found.Type() != form) {
      return std::unexpected(
          RefusedTable(table_path, std::string("« ") + section + " » n'a pas la forme attendue"));
    }
    visit(found);
  } catch (const YAML::Exception& failure) {
    return std::unexpected(RefusedTable(table_path, failure.what()));
  }
  return {};
}

}  // namespace ro::vfs
