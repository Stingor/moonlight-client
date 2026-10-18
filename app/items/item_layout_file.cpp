#include "items/item_layout_file.h"

#include <cstdint>
#include <optional>

#include <yaml-cpp/yaml.h>

#include "game/save_folder.h"
#include "game/yaml_load.h"

namespace moonclient {
namespace {

// Les champs d'une entrée. ⚠ GRAVÉS : les changer rend à tous
// les joueurs une grille remise à plat, sans que rien ne le dise.
constexpr const char* kPlacesField = "places";
constexpr const char* kItemField = "item";
constexpr const char* kCellField = "cell";

constexpr const char* kFileHeader =
    "# Où le joueur a posé ses objets dans la grille du sac.\n"
    "#\n"
    "# « item » est l'identifiant de l'objet — celui des scripts et des bases du\n"
    "# serveur —, et « cell » la n-ième case de la grille, en comptant de zéro\n"
    "# depuis le coin haut-gauche.\n"
    "#\n"
    "# La case est un index ABSOLU : c'est le nombre de colonnes qui décide sur\n"
    "# quelle ligne elle tombe, donc la disposition ne tient que si la taille de\n"
    "# la fenêtre est verrouillée.\n"
    "#\n"
    "# Un objet absent d'ici remplit les trous, dans l'ordre où le serveur l'a\n"
    "# listé. Ce fichier ne sert que si « Placement libre » est coché.\n";

}  // namespace

std::filesystem::path ItemLayoutFilePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kItemLayoutFileName;
}

void LoadItemLayout(const std::filesystem::path& client_root, ItemLayout& into) {
  std::optional<YAML::Node> whole = LoadYamlMap(ItemLayoutFilePath(client_root), "placement du sac",
                                                "la grille se remplira dans l'ordre du serveur");
  if (!whole.has_value()) {
    return;
  }
  const YAML::Node listed = (*whole)[kPlacesField];
  if (!listed || !listed.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : listed) {
    if (!entry.IsMap()) {
      continue;
    }
    const YAML::Node item = entry[kItemField];
    const YAML::Node cell = entry[kCellField];
    if (!item || !item.IsScalar() || !cell || !cell.IsScalar()) {
      continue;
    }
    // ⚠ UNE ENTRÉE ILLISIBLE EST IGNORÉE, jamais fatale : une place perdue coûte
    // un objet mal rangé, un refus coûterait toute la disposition.
    into.Put(item.as<std::uint32_t>(0U), cell.as<int>(ItemLayout::kNowhere));
  }
}

bool SaveItemLayout(const std::filesystem::path& client_root, const ItemLayout& from) {
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kPlacesField << YAML::Value << YAML::BeginSeq;
  // ⚠ DANS L'ORDRE DES IDENTIFIANTS, que la table tient déjà : un fichier qui se
  // réordonne à chaque écriture est illisible en diff, et celui-ci se corrige à
  // la main.
  for (const auto& [item_id, cell] : from.places()) {
    out << YAML::Flow << YAML::BeginMap;
    out << YAML::Key << kItemField << YAML::Value << item_id;
    out << YAML::Key << kCellField << YAML::Value << cell;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;

  return SaveYamlFile(ItemLayoutFilePath(client_root), kFileHeader, out, "placement du sac",
                      "la disposition de cette partie ne sera pas retrouvée");
}

}  // namespace moonclient
