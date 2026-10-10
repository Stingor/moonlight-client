#include "character/recent_characters.h"

#include <algorithm>
#include <string>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

// La clé de la séquence. ⚠ GRAVÉE : la changer perd la mémoire de tous les
// joueurs sans que rien ne le dise.
constexpr const char* kListField = "recently_played";

constexpr const char* kFileHeader =
    "# Les personnages que ce client a joues recemment, du plus recent au plus\n"
    "# ancien, par identifiant de personnage.\n"
    "#\n"
    "# Il ne sert qu'a designer le bon siege a l'ouverture de la tablee, pour\n"
    "# qu'Entree suffise. L'effacer ne perd rien d'autre que cette commodite.\n";

[[nodiscard]] YAML::Node ReadWhole(const std::filesystem::path& path) {
  try {
    if (!std::filesystem::exists(path)) {
      return YAML::Node(YAML::NodeType::Map);
    }
    YAML::Node whole = YAML::LoadFile(path.string());
    // ⚠ UNE RACINE QUI N'EST PAS UNE TABLE N'A PAS DE CHAMPS : l'indexer lève
    // chez l'appelant, hors de ce `try`.
    return whole.IsMap() ? whole : YAML::Node(YAML::NodeType::Map);
  } catch (const std::exception& trouble) {
    // ⚠ UN FICHIER ILLISIBLE N'EMPÊCHE PAS DE JOUER : on repart d'une mémoire
    // vide, et le prochain départ en jeu la réécrit proprement.
    ro::core::LogWarning("personnages récents : fichier illisible ({})", trouble.what());
    return YAML::Node(YAML::NodeType::Map);
  }
}

}  // namespace

std::filesystem::path RecentCharactersPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kRecentCharactersFileName;
}

std::vector<std::uint32_t> LoadRecentCharacters(const std::filesystem::path& client_root) {
  std::vector<std::uint32_t> listed;
  const YAML::Node whole = ReadWhole(RecentCharactersPath(client_root));
  const YAML::Node mine = whole[kListField];
  if (!mine || !mine.IsSequence()) {
    return listed;
  }
  for (const YAML::Node& entry : mine) {
    // ⚠ ZÉRO N'EST PAS UN PERSONNAGE : c'est la valeur qu'une entrée illisible
    // rend, et la garder ferait chercher un siège qui n'existe pas.
    const auto character_id = entry.as<std::uint32_t>(0);
    if (character_id != 0 && listed.size() < kRecentCharactersMax) {
      listed.push_back(character_id);
    }
  }
  return listed;
}

bool NoteRecentCharacter(const std::filesystem::path& client_root, std::uint32_t character_id) {
  if (character_id == 0) {
    return false;
  }
  // ⚠ RELIRE AVANT D'ÉCRIRE : deux clients ouverts en même temps se
  // marcheraient dessus, et le second effacerait ce que le premier vient de
  // noter.
  std::vector<std::uint32_t> listed = LoadRecentCharacters(client_root);
  std::erase(listed, character_id);
  listed.insert(listed.begin(), character_id);
  if (listed.size() > kRecentCharactersMax) {
    listed.resize(kRecentCharactersMax);
  }

  YAML::Node whole(YAML::NodeType::Map);
  YAML::Node mine(YAML::NodeType::Sequence);
  for (const std::uint32_t kept : listed) {
    mine.push_back(kept);
  }
  whole[kListField] = mine;

  YAML::Emitter out;
  out << whole;
  const std::string body = std::string(kFileHeader) + out.c_str() + "\n";
  const std::filesystem::path path = RecentCharactersPath(client_root);
  std::error_code unused;
  std::filesystem::create_directories(path.parent_path(), unused);
  if (auto written = ro::core::WriteFileAtomically(path, body); !written) {
    ro::core::LogWarning("personnages récents : {}", ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

int PreferredCharacter(std::span<const std::uint32_t> recent,
                       std::span<const std::uint32_t> present) {
  if (present.empty()) {
    return -1;
  }
  for (const std::uint32_t wanted : recent) {
    for (int rank = 0; rank < static_cast<int>(present.size()); ++rank) {
      if (present[static_cast<std::size_t>(rank)] == wanted) {
        return rank;
      }
    }
  }
  // ⚠ LE PREMIER, ET JAMAIS « RIEN » : une tablée sans siège désigné oblige à
  // viser avant de pouvoir jouer.
  return 0;
}

}  // namespace moonclient
