#include "items/storage_favorites.h"

#include <optional>

#include <yaml-cpp/yaml.h>

#include "game/yaml_load.h"

namespace moonclient {
namespace {

// Le champ du fichier. ⚠ GRAVÉ : le changer rend à tous les joueurs un onglet de
// favoris vide, sans que rien ne le dise.
constexpr const char* kFavoritesField = "favoris";

constexpr const char* kFileHeader =
    "# Les objets marqués favoris dans le coffre.\n"
    "#\n"
    "# Une liste d'identifiants d'objets — ceux des scripts et des bases du\n"
    "# serveur. Toutes les piles d'un même objet sont favorites ensemble, et un\n"
    "# objet sorti puis remis retrouve son étoile.\n"
    "#\n"
    "# ⚠ CES FAVORIS-CI SONT AU CLIENT. Le serveur n'en connaît aucun dans un\n"
    "# coffre — contrairement au sac, dont il retient le drapeau lui-même.\n";

}  // namespace

bool StorageFavorites::Toggle(std::uint32_t item_id) {
  if (item_id == 0) {
    return false;
  }
  if (const auto found = ids_.find(item_id); found != ids_.end()) {
    ids_.erase(found);
    return false;
  }
  ids_.insert(item_id);
  return true;
}

void StorageFavorites::Add(std::uint32_t item_id) {
  if (item_id != 0) {
    ids_.insert(item_id);
  }
}

std::filesystem::path StorageFavoritesPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kStorageFavoritesFileName;
}

void LoadStorageFavorites(const std::filesystem::path& client_root, StorageFavorites& into) {
  std::optional<YAML::Node> whole = LoadYamlMap(
      StorageFavoritesPath(client_root), "favoris du coffre", "l'onglet des favoris sera vide");
  if (!whole.has_value()) {
    return;
  }
  const YAML::Node listed = (*whole)[kFavoritesField];
  if (!listed || !listed.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : listed) {
    if (entry.IsScalar()) {
      into.Add(entry.as<std::uint32_t>(0U));
    }
  }
}

bool SaveStorageFavorites(const std::filesystem::path& client_root, const StorageFavorites& from) {
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kFavoritesField << YAML::Value << YAML::Flow
      << YAML::BeginSeq;
  // ⚠ DANS L'ORDRE DES IDENTIFIANTS, que l'ensemble tient déjà : un fichier qui
  // se réordonne à chaque écriture est illisible en diff, et celui-ci se corrige
  // à la main.
  for (const std::uint32_t item_id : from.ids()) {
    out << item_id;
  }
  out << YAML::EndSeq << YAML::EndMap;

  return SaveYamlFile(StorageFavoritesPath(client_root), kFileHeader, out, "favoris du coffre",
                      "les favoris de cette partie ne seront pas retrouvés");
}

}  // namespace moonclient
