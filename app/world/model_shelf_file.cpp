#include "world/model_shelf_file.h"

#include <algorithm>
#include <fstream>
#include <ios>
#include <iterator>
#include <system_error>

#include <yaml-cpp/yaml.h>

#include "ro/core/file_write.h"
#include "ro/core/utf8_path.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

// Les clés du fichier : en changer une rendrait illisibles les fichiers déjà
// écrits.
constexpr const char* kFavoritesField = "favorites";
constexpr const char* kRecentsField = "recents";
constexpr const char* kCellField = "cell_px";

constexpr const char* kFileHeader =
    "# La bibliotheque de modeles de l'editeur de carte de moonclient : les\n"
    "# favoris, les modeles poses recemment (le plus recent d'abord) et le cote\n"
    "# des vignettes. Chemins relatifs a data\\model\\.\n";

std::vector<std::string> ReadNames(const YAML::Node& list) {
  std::vector<std::string> names;
  if (!list || !list.IsSequence()) {
    return names;
  }
  for (const YAML::Node& entry : list) {
    if (entry.IsScalar() && !entry.Scalar().empty()) {
      names.push_back(entry.Scalar());
    }
  }
  return names;
}

}  // namespace

bool ToggleFavorite(ModelShelf& shelf, const std::string& file) {
  const auto at = std::ranges::lower_bound(shelf.favorites, file);
  if (at != shelf.favorites.end() && *at == file) {
    shelf.favorites.erase(at);
    return true;
  }
  if (shelf.favorites.size() >= ModelShelf::kFavoriteLimit) {
    return false;
  }
  shelf.favorites.insert(at, file);
  return true;
}

void RememberRecent(ModelShelf& shelf, const std::string& file) {
  std::erase(shelf.recents, file);
  shelf.recents.insert(shelf.recents.begin(), file);
  if (shelf.recents.size() > ModelShelf::kRecentLimit) {
    shelf.recents.resize(ModelShelf::kRecentLimit);
  }
}

std::filesystem::path ModelShelfFilePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kModelShelfFileName;
}

ro::core::Result<ModelShelf> ReadModelShelf(std::string_view text) {
  ModelShelf shelf;
  try {
    const YAML::Node base = YAML::Load(std::string(text));
    if (!base || !base.IsMap()) {
      return shelf;
    }
    for (const std::string& file : ReadNames(base[kFavoritesField])) {
      const auto at = std::ranges::lower_bound(shelf.favorites, file);
      if ((at == shelf.favorites.end() || *at != file) &&
          shelf.favorites.size() < ModelShelf::kFavoriteLimit) {
        shelf.favorites.insert(at, file);
      }
    }
    // Lus du plus ancien au plus récent, pour que chacun passe en tête à son
    // tour : un doublon garde sa place la plus récente.
    std::vector<std::string> recents = ReadNames(base[kRecentsField]);
    for (auto file = recents.rbegin(); file != recents.rend(); ++file) {
      RememberRecent(shelf, *file);
    }
    if (const YAML::Node cell = base[kCellField]; cell && cell.IsScalar()) {
      shelf.cell_px = std::max(cell.as<float>(0.0F), 0.0F);
    }
  } catch (const YAML::Exception& broken) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData, broken.what());
  }
  return shelf;
}

std::string WriteModelShelf(const ModelShelf& shelf) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kCellField << YAML::Value << shelf.cell_px;
  out << YAML::Key << kFavoritesField << YAML::Value << YAML::BeginSeq;
  for (const std::string& file : shelf.favorites) {
    out << file;
  }
  out << YAML::EndSeq;
  out << YAML::Key << kRecentsField << YAML::Value << YAML::BeginSeq;
  for (const std::string& file : shelf.recents) {
    out << file;
  }
  out << YAML::EndSeq << YAML::EndMap;
  return std::string(kFileHeader) + out.c_str() + "\n";
}

ro::core::Result<ModelShelf> LoadModelShelf(const std::filesystem::path& client_root) {
  const std::filesystem::path path = ModelShelfFilePath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return ModelShelf{};
  }
  // ⚠ LE FLUX S'OUVRE PAR LE CHEMIN LARGE : un dossier hors de la page ANSI
  // resterait introuvable par un nom étroit.
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return ro::core::Fail(ro::core::ErrorCode::kIoFailure,
                          ro::core::Utf8FromPath(path) + " ne s'ouvre pas");
  }
  const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
  return ReadModelShelf(text);
}

ro::core::Result<void> SaveModelShelf(const std::filesystem::path& client_root,
                                      const ModelShelf& shelf) {
  return ro::core::WriteFileAtomically(ModelShelfFilePath(client_root), WriteModelShelf(shelf));
}

}  // namespace moonclient
