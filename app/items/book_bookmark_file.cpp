#include "items/book_bookmark_file.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

constexpr const char* kBookmarksField = "book_bookmarks";
constexpr const char* kCharacterField = "cid";
constexpr const char* kBookField = "id";
constexpr const char* kPageField = "page";

constexpr const char* kFileHeader =
    "# Les signets des livres de moonclient, tous personnages confondus.\n"
    "#\n"
    "# cid est le personnage, id l'objet du livre, page celle ou il a ete\n"
    "# referme (a partir de 1). Relu quand \"Livres : ouvrir a la premiere\n"
    "# page\" est eteint.\n";

// Un signet tel que le fichier le range : avec son personnage.
struct Stored {
  std::uint32_t character_id = 0;
  std::uint32_t item_id = 0;
  int page = 0;
};

template <typename T>
[[nodiscard]] T ScalarOr(const YAML::Node& node, T fallback) {
  if (!node || !node.IsScalar()) {
    return fallback;
  }
  return node.as<T>(fallback);
}

// Une entrée sans livre ou sans page valide se saute.
[[nodiscard]] std::vector<Stored> ReadList(const YAML::Node& list) {
  std::vector<Stored> all;
  if (!list || !list.IsSequence()) {
    return all;
  }
  for (const YAML::Node& entry : list) {
    if (!entry.IsMap()) {
      continue;
    }
    const Stored kept{.character_id = ScalarOr<std::uint32_t>(entry[kCharacterField], 0),
                      .item_id = ScalarOr<std::uint32_t>(entry[kBookField], 0),
                      .page = ScalarOr<int>(entry[kPageField], 0)};
    if (kept.item_id != 0 && kept.page > 0) {
      all.push_back(kept);
    }
  }
  return all;
}

enum class FileState : std::uint8_t { kMissing, kBroken, kRead };

struct FileRead {
  FileState state = FileState::kMissing;
  std::vector<Stored> bookmarks;
};

// Un fichier abîmé n'empêche pas de jouer : son exception s'attrape.
[[nodiscard]] FileRead ReadStored(const std::filesystem::path& client_root) {
  const std::filesystem::path path = BookBookmarkFilePath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return {};
  }
  try {
    const YAML::Node whole = YAML::LoadFile(path.string());
    if (!whole.IsMap()) {
      return FileRead{.state = FileState::kRead};
    }
    return FileRead{.state = FileState::kRead, .bookmarks = ReadList(whole[kBookmarksField])};
  } catch (const YAML::Exception& broken) {
    ro::core::LogWarning("signets des livres : {} illisible ({})", path.string(), broken.what());
    return FileRead{.state = FileState::kBroken};
  }
}

void Emit(YAML::Emitter& out, std::uint32_t character_id, std::uint32_t item_id, int page) {
  out << YAML::BeginMap << YAML::Key << kCharacterField << YAML::Value << character_id << YAML::Key
      << kBookField << YAML::Value << item_id << YAML::Key << kPageField << YAML::Value << page
      << YAML::EndMap;
}

}  // namespace

std::filesystem::path BookBookmarkFilePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kBookBookmarkFileName;
}

BookBookmarks LoadBookBookmarks(const std::filesystem::path& client_root,
                                std::uint32_t character_id) {
  BookBookmarks mine;
  for (const Stored& kept : ReadStored(client_root).bookmarks) {
    if (kept.character_id == character_id) {
      mine[kept.item_id] = kept.page;
    }
  }
  return mine;
}

bool SaveBookBookmarks(const std::filesystem::path& client_root, std::uint32_t character_id,
                       const BookBookmarks& bookmarks) {
  const FileRead stored = ReadStored(client_root);
  if (stored.state == FileState::kBroken) {
    ro::core::LogWarning(
        "signets des livres : {} n'est pas réécrit tant qu'il reste illisible — il porte "
        "ceux des autres personnages",
        BookBookmarkFilePath(client_root).string());
    return false;
  }
  // Les siens par livre croissant : le fichier ne se rebat pas d'une écriture à
  // l'autre au gré du hachage.
  std::vector<std::pair<std::uint32_t, int>> mine(bookmarks.begin(), bookmarks.end());
  std::ranges::sort(mine);
  // Ses signets prennent la place du premier des anciens : un personnage ne
  // saute pas d'un bout du fichier à l'autre à chaque enregistrement.
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kBookmarksField << YAML::Value << YAML::BeginSeq;
  bool placed = false;
  const auto emit_mine = [&] {
    for (const auto& [item_id, page] : mine) {
      if (item_id != 0 && page > 0) {
        Emit(out, character_id, item_id, page);
      }
    }
    placed = true;
  };
  for (const Stored& kept : stored.bookmarks) {
    if (kept.character_id != character_id) {
      Emit(out, kept.character_id, kept.item_id, kept.page);
    } else if (!placed) {
      emit_mine();
    }
  }
  if (!placed) {
    emit_mine();
  }
  out << YAML::EndSeq << YAML::EndMap;
  const std::string text = std::string(kFileHeader) + out.c_str() + "\n";
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(BookBookmarkFilePath(client_root), text);
      !written) {
    ro::core::LogWarning("signets des livres : écriture impossible ({})",
                         ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
