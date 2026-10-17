#include "character/stat_preset_file.h"

#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

// Les clés du fichier : en changer une rendrait illisibles les fichiers déjà
// écrits chez le joueur.
constexpr const char* kAccountsField = "stat_presets";
constexpr const char* kAccountField = "aid";
constexpr const char* kPresetsField = "presets";
constexpr const char* kNameField = "name";
constexpr const char* kStatsField = "stats";

constexpr const char* kFileHeader =
    "# Les prereglages de stats de moonclient, par compte.\n"
    "#\n"
    "# aid est l'identifiant du compte ; stats les six valeurs de base visees,\n"
    "# dans l'ordre STR, AGI, VIT, INT, DEX, LUK.\n";

[[nodiscard]] std::optional<ro::sim::StatPreset> ReadPreset(const YAML::Node& entry) {
  if (!entry.IsMap()) {
    return std::nullopt;
  }
  const YAML::Node name = entry[kNameField];
  const YAML::Node stats = entry[kStatsField];
  if (!name || !name.IsScalar() || name.Scalar().empty() || !stats || !stats.IsSequence() ||
      stats.size() != ro::sim::kBaseStatCount) {
    return std::nullopt;
  }
  ro::sim::StatPreset preset;
  preset.name = ro::sim::TidyStatPresetName(name.Scalar());
  if (preset.name.empty()) {
    return std::nullopt;
  }
  for (std::size_t rank = 0; rank < ro::sim::kBaseStatCount; ++rank) {
    if (!stats[rank].IsScalar()) {
      return std::nullopt;
    }
    preset.stats[rank] = stats[rank].as<std::int64_t>(0);
  }
  return preset;
}

[[nodiscard]] std::vector<ro::sim::StatPreset> ReadPresets(const YAML::Node& list) {
  std::vector<ro::sim::StatPreset> presets;
  if (!list || !list.IsSequence()) {
    return presets;
  }
  for (const YAML::Node& entry : list) {
    if (std::optional<ro::sim::StatPreset> preset = ReadPreset(entry); preset.has_value()) {
      presets.push_back(std::move(*preset));
    }
  }
  return presets;
}

enum class FileState : std::uint8_t { kMissing, kBroken, kRead };

struct FileRead {
  FileState state = FileState::kMissing;
  StatPresetShelf shelf;
};

[[nodiscard]] FileRead ReadFile(const std::filesystem::path& client_root) {
  const std::filesystem::path path = StatPresetFilePath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return {};
  }
  // ⚠ LE FLUX S'OUVRE PAR LE CHEMIN LARGE : un dossier hors de la page ANSI
  // resterait introuvable par un nom étroit.
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    ro::core::LogWarning("préréglages de stats : {} ne s'ouvre pas", ro::core::Utf8FromPath(path));
    return FileRead{.state = FileState::kBroken};
  }
  const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
  ro::core::Result<StatPresetShelf> shelf = ReadStatPresetShelf(text);
  if (!shelf) {
    ro::core::LogWarning("préréglages de stats : {} illisible ({})", ro::core::Utf8FromPath(path),
                         ro::core::Describe(shelf.error()));
    return FileRead{.state = FileState::kBroken};
  }
  return FileRead{.state = FileState::kRead, .shelf = std::move(*shelf)};
}

}  // namespace

std::filesystem::path StatPresetFilePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kStatPresetFileName;
}

ro::core::Result<StatPresetShelf> ReadStatPresetShelf(std::string_view text) {
  StatPresetShelf shelf;
  try {
    const YAML::Node base = YAML::Load(std::string(text));
    // Un fichier vide, ou dont la racine n'est pas une table, ne range rien.
    if (!base || !base.IsMap()) {
      return shelf;
    }
    const YAML::Node accounts = base[kAccountsField];
    if (!accounts || !accounts.IsSequence()) {
      return shelf;
    }
    for (const YAML::Node& entry : accounts) {
      if (!entry.IsMap() || !entry[kAccountField] || !entry[kAccountField].IsScalar()) {
        continue;
      }
      const auto account_id = entry[kAccountField].as<std::uint32_t>(0);
      if (account_id == 0) {
        continue;
      }
      std::vector<ro::sim::StatPreset>& kept = shelf[account_id];
      for (ro::sim::StatPreset& preset : ReadPresets(entry[kPresetsField])) {
        kept.push_back(std::move(preset));
      }
    }
  } catch (const YAML::Exception& broken) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData, broken.what());
  }
  return shelf;
}

std::string WriteStatPresetShelf(const StatPresetShelf& shelf) {
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kAccountsField << YAML::Value << YAML::BeginSeq;
  for (const auto& [account_id, presets] : shelf) {
    if (presets.empty()) {
      continue;
    }
    out << YAML::BeginMap << YAML::Key << kAccountField << YAML::Value << account_id << YAML::Key
        << kPresetsField << YAML::Value << YAML::BeginSeq;
    for (const ro::sim::StatPreset& preset : presets) {
      out << YAML::BeginMap << YAML::Key << kNameField << YAML::Value << preset.name << YAML::Key
          << kStatsField << YAML::Value << YAML::Flow << YAML::BeginSeq;
      for (const std::int64_t value : preset.stats) {
        out << value;
      }
      out << YAML::EndSeq << YAML::EndMap;
    }
    out << YAML::EndSeq << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;
  return std::string(kFileHeader) + out.c_str() + "\n";
}

std::vector<ro::sim::StatPreset> LoadStatPresets(const std::filesystem::path& client_root,
                                                 std::uint32_t account_id) {
  FileRead read = ReadFile(client_root);
  const auto mine = read.shelf.find(account_id);
  if (mine == read.shelf.end()) {
    return {};
  }
  return std::move(mine->second);
}

bool SaveStatPresets(const std::filesystem::path& client_root, std::uint32_t account_id,
                     std::span<const ro::sim::StatPreset> presets) {
  FileRead read = ReadFile(client_root);
  if (read.state == FileState::kBroken) {
    ro::core::LogWarning(
        "préréglages de stats : {} n'est pas réécrit tant qu'il reste illisible — il porte "
        "ceux des autres comptes",
        ro::core::Utf8FromPath(StatPresetFilePath(client_root)));
    return false;
  }
  read.shelf[account_id].assign(presets.begin(), presets.end());
  if (const ro::core::Result<void> written = ro::core::WriteFileAtomically(
          StatPresetFilePath(client_root), WriteStatPresetShelf(read.shelf));
      !written) {
    ro::core::LogWarning("préréglages de stats : écriture impossible ({})",
                         ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
