#include "game/minigame_file.h"

#include <algorithm>
#include <optional>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#include "game/save_folder.h"
#include "game/yaml_load.h"

namespace moonclient {
namespace {

// Les clés du fichier : en changer une rendrait illisibles les fichiers déjà
// écrits chez le joueur.
constexpr const char* kRojeweledBestField = "rojeweled_best";
constexpr const char* kRoggleBestField = "roggle_best";
constexpr const char* kTrophiesField = "trophies";

constexpr const char* kFileHeader =
    "# Les meilleurs scores et les trophees des mini-jeux de moonclient.\n"
    "#\n"
    "# trophies liste les cles des trophees gagnes.\n";

// Un meilleur score ; zéro quand le champ manque ou ne se lit pas.
[[nodiscard]] int BestIn(const YAML::Node& whole, const char* field) {
  const YAML::Node best = whole[field];
  if (!best || !best.IsScalar()) {
    return 0;
  }
  return std::max(0, best.as<int>(0));
}

}  // namespace

std::filesystem::path MinigameFilePath(const std::filesystem::path& client_root) {
  return client_root / ro::core::PathFromUtf8(kSaveFolder) /
         ro::core::PathFromUtf8(kMinigameFileName);
}

ro::core::Result<ro::sim::MinigameRecord> ReadMinigameRecord(std::string_view text) {
  ro::sim::MinigameRecord record;
  try {
    const YAML::Node whole = YAML::Load(std::string(text));
    if (whole.IsNull()) {
      return record;
    }
    if (!whole.IsMap()) {
      return ro::core::Fail(ro::core::ErrorCode::kCorruptData, "la racine n'est pas une table");
    }
    record.rojeweled_best = BestIn(whole, kRojeweledBestField);
    record.roggle_best = BestIn(whole, kRoggleBestField);
    const YAML::Node trophies = whole[kTrophiesField];
    if (trophies && trophies.IsSequence()) {
      const auto catalog = ro::sim::MinigameTrophies();
      for (const YAML::Node& key : trophies) {
        if (!key.IsScalar()) {
          continue;
        }
        const auto known =
            std::ranges::find_if(catalog, [&key](const ro::sim::MinigameTrophyText& text_of) {
              return key.Scalar() == text_of.key;
            });
        if (known == catalog.end()) {
          record.foreign_trophy_keys.push_back(key.Scalar());
        } else {
          record.unlocked[static_cast<std::size_t>(known->trophy)] = true;
        }
      }
    }
  } catch (const YAML::Exception& broken) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData, broken.what());
  }
  return record;
}

std::string WriteMinigameRecord(const ro::sim::MinigameRecord& record) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kRojeweledBestField << YAML::Value << record.rojeweled_best;
  out << YAML::Key << kRoggleBestField << YAML::Value << record.roggle_best;
  out << YAML::Key << kTrophiesField << YAML::Value << YAML::BeginSeq;
  for (const ro::sim::MinigameTrophyText& text : ro::sim::MinigameTrophies()) {
    if (record.Has(text.trophy)) {
      out << text.key;
    }
  }
  for (const std::string& key : record.foreign_trophy_keys) {
    out << key;
  }
  out << YAML::EndSeq << YAML::EndMap;
  return std::string(kFileHeader) + out.c_str() + "\n";
}

MinigameRecordHeld LoadMinigameRecord(const std::filesystem::path& client_root) {
  const std::filesystem::path path = MinigameFilePath(client_root);
  const std::optional<std::string> text = ReadFileText(path);
  if (!text.has_value()) {
    ro::core::LogWarning("mini-jeux : {} ne s'ouvre pas — il ne sera réécrit qu'une fois relu",
                         ro::core::Utf8FromPath(path));
    return {.file_unread = true};
  }
  ro::core::Result<ro::sim::MinigameRecord> read = ReadMinigameRecord(*text);
  if (!read) {
    ro::core::LogWarning("mini-jeux : {} abîmé ({}) — il ne sera réécrit qu'une fois relu",
                         ro::core::Utf8FromPath(path), ro::core::Describe(read.error()));
    return {.file_unread = true};
  }
  return {.record = std::move(*read)};
}

bool SaveMinigameRecord(const std::filesystem::path& client_root, MinigameRecordHeld& held) {
  if (held.file_unread) {
    const std::optional<std::string> text = ReadFileText(MinigameFilePath(client_root));
    if (!text.has_value()) {
      return false;
    }
    const ro::core::Result<ro::sim::MinigameRecord> on_disk = ReadMinigameRecord(*text);
    if (!on_disk) {
      return false;
    }
    ro::sim::MergeMinigameRecord(held.record, *on_disk);
    held.file_unread = false;
  }
  if (const ro::core::Result<void> written = ro::core::WriteFileAtomically(
          MinigameFilePath(client_root), WriteMinigameRecord(held.record));
      !written) {
    ro::core::LogWarning("mini-jeux : écriture impossible ({})",
                         ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
