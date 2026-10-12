#include "items/item_layout_file.h"

#include <cstdint>
#include <string>
#include <system_error>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

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
  const std::filesystem::path path = ItemLayoutFilePath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return;  // première partie : le remplissage automatique fait l'affaire
  }
  YAML::Node whole;
  try {
    whole = LoadYamlFile(path);
  } catch (const YAML::Exception& broken) {
    // La seule exception qu'on attrape, et pour la même raison qu'ailleurs : un
    // fichier abîmé ne doit pas empêcher de jouer.
    ro::core::LogWarning(
        "placement du sac : fichier illisible ({}) — la grille se remplira dans l'ordre du "
        "serveur",
        broken.what());
    return;
  }
  // ⚠ UNE RACINE QUI N'EST PAS UNE TABLE N'A PAS DE CHAMPS : l'indexer lève
  // hors du `try`, et le jeu ne se lancerait pas.
  if (!whole.IsMap()) {
    return;
  }
  const YAML::Node listed = whole[kPlacesField];
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

  const std::string text = std::string(kFileHeader) + out.c_str() + "\n";
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(ItemLayoutFilePath(client_root), text);
      !written) {
    ro::core::LogWarning(
        "placement du sac : écriture impossible ({}) — la disposition de cette partie ne sera "
        "pas retrouvée",
        ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
