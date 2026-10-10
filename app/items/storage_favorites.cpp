#include "items/storage_favorites.h"

#include <string>
#include <system_error>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

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
  const std::filesystem::path path = StorageFavoritesPath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return;  // première partie : personne n'a encore rien marqué
  }
  YAML::Node whole;
  try {
    whole = YAML::LoadFile(path.string());
  } catch (const YAML::Exception& broken) {
    // La seule exception qu'on attrape, et pour la même raison qu'ailleurs : un
    // fichier abîmé ne doit pas empêcher de jouer.
    ro::core::LogWarning(
        "favoris du coffre : fichier illisible ({}) — l'onglet des favoris sera vide",
        broken.what());
    return;
  }
  // ⚠ UNE RACINE QUI N'EST PAS UNE TABLE N'A PAS DE CHAMPS : l'indexer lève
  // hors du `try`, et le jeu ne se lancerait pas.
  if (!whole.IsMap()) {
    return;
  }
  const YAML::Node listed = whole[kFavoritesField];
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

  const std::string text = std::string(kFileHeader) + out.c_str() + "\n";
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(StorageFavoritesPath(client_root), text);
      !written) {
    ro::core::LogWarning(
        "favoris du coffre : écriture impossible ({}) — les favoris de cette partie ne seront "
        "pas retrouvés",
        ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
