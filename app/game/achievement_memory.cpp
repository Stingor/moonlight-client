#include "game/achievement_memory.h"

#include <algorithm>
#include <system_error>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/sim/achievement_book.h"

#include "game/yaml_load.h"

namespace moonclient {
namespace {

// Les champs du fichier. ⚠ GRAVÉS : les changer fait oublier à chacun ses
// succès suivis, sans que rien ne le dise.
constexpr const char* kBannerField = "banner";
constexpr const char* kCharactersField = "characters";
constexpr const char* kCharacterField = "character";
constexpr const char* kTrackedField = "tracked";
constexpr const char* kTabField = "tab";
constexpr const char* kSubtabField = "subtab";
constexpr const char* kFilterField = "filter";
constexpr const char* kSortField = "sort";
constexpr const char* kSelectedField = "selected";

constexpr const char* kFileHeader =
    "# Les succès du joueur, tels que le client les retient.\n"
    "#\n"
    "# « banner » : 0 celle du jeu, 1 discrète, 2 aucune. Le panneau de suivi,\n"
    "# commun aux quêtes, se règle dans « suivi.yml ». Sous « characters »,\n"
    "# pour chacun : les succès suivis, et la fenêtre telle qu'il l'a laissée —\n"
    "# onglet, sous-onglet (-1 : l'onglet entier), filtre, ordre, succès choisi.\n"
    "#\n"
    "# ⚠ TOUT CECI EST AU CLIENT : le serveur n'en sait rien.\n";

// Le plus grand sous-onglet que l'arbre range, tous onglets confondus : Aventure
// en a six. ⚠ L'ÉTAGE BORNE ENSUITE À L'ONGLET RESTAURÉ, qui peut en avoir moins.
constexpr int kMostSubtab = 5;

template <typename Enum>
[[nodiscard]] Enum Bounded(int value, std::size_t count, Enum fallback) {
  return value >= 0 && static_cast<std::size_t>(value) < count ? static_cast<Enum>(value)
                                                               : fallback;
}

[[nodiscard]] RememberedAchievements ReadOne(const YAML::Node& entry) {
  RememberedAchievements kept;
  kept.character = entry[kCharacterField].as<std::string>(std::string());
  if (const YAML::Node tracked = entry[kTrackedField]; tracked && tracked.IsSequence()) {
    for (const YAML::Node& id : tracked) {
      const auto achievement_id = id.as<std::uint32_t>(ro::sim::kNoAchievement);
      if (achievement_id != ro::sim::kNoAchievement &&
          kept.tracked.size() < ro::sim::kMostTrackedAchievements &&
          std::ranges::find(kept.tracked, achievement_id) == kept.tracked.end()) {
        kept.tracked.push_back(achievement_id);
      }
    }
  }
  const int tab = entry[kTabField].as<int>(ro::ui::kAchievementSummaryTab);
  if (tab >= ro::ui::kAchievementSummaryTab && tab < ro::ui::kAchievementTabCount) {
    kept.tab = tab;
    const int subtab = entry[kSubtabField].as<int>(ro::ui::kAchievementWholeTab);
    kept.subtab = subtab >= ro::ui::kAchievementWholeTab && subtab <= kMostSubtab
                      ? subtab
                      : ro::ui::kAchievementWholeTab;
  }
  kept.filter = Bounded(entry[kFilterField].as<int>(0), ro::ui::kAchievementFilterCount,
                        ro::ui::AchievementFilter::kAll);
  kept.sort = Bounded(entry[kSortField].as<int>(0), ro::ui::kAchievementSortCount,
                      ro::ui::AchievementSort::kGame);
  if (const auto selected = entry[kSelectedField].as<std::uint32_t>(ro::sim::kNoAchievement);
      selected != ro::sim::kNoAchievement) {
    kept.selected = selected;
  }
  return kept;
}

}  // namespace

const RememberedAchievements* AchievementMemory::Of(std::string_view character) const {
  const auto found = std::ranges::find(characters, character, &RememberedAchievements::character);
  return found == characters.end() ? nullptr : &*found;
}

void AchievementMemory::Remember(RememberedAchievements kept) {
  std::erase_if(characters, [&kept](const RememberedAchievements& other) {
    return other.character == kept.character;
  });
  characters.push_back(std::move(kept));
}

std::filesystem::path AchievementMemoryPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kAchievementMemoryFileName;
}

void LoadAchievementMemory(const std::filesystem::path& client_root, AchievementMemory& into) {
  const std::filesystem::path path = AchievementMemoryPath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return;
  }
  YAML::Node whole;
  try {
    whole = LoadYamlFile(path);
  } catch (const YAML::Exception& broken) {
    ro::core::LogWarning("succès : fichier illisible ({}) — les défauts s'appliquent",
                         broken.what());
    return;
  }
  // ⚠ UNE RACINE QUI N'EST PAS UNE TABLE N'A PAS DE CHAMPS : l'indexer lèverait
  // hors du `try`.
  if (!whole.IsMap()) {
    return;
  }
  try {
    into.banner = Bounded(whole[kBannerField].as<int>(0), ro::ui::kAchievementBannerStyleCount,
                          ro::ui::AchievementBannerStyle::kNative);
    const YAML::Node listed = whole[kCharactersField];
    if (!listed || !listed.IsSequence()) {
      return;
    }
    for (const YAML::Node& entry : listed) {
      if (!entry.IsMap()) {
        continue;
      }
      RememberedAchievements kept = ReadOne(entry);
      if (!kept.character.empty()) {
        into.Remember(std::move(kept));
      }
    }
  } catch (const YAML::Exception& broken) {
    // Un champ d'un autre type : ce qui a été lu reste, le reste aux défauts.
    ro::core::LogWarning("succès : champ illisible ({})", broken.what());
  }
}

bool SaveAchievementMemory(const std::filesystem::path& client_root,
                           const AchievementMemory& from) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kBannerField << YAML::Value << static_cast<int>(from.banner);
  out << YAML::Key << kCharactersField << YAML::Value << YAML::BeginSeq;
  for (const RememberedAchievements& kept : from.characters) {
    out << YAML::BeginMap;
    out << YAML::Key << kCharacterField << YAML::Value << kept.character;
    out << YAML::Key << kTrackedField << YAML::Value << YAML::Flow << YAML::BeginSeq;
    for (const std::uint32_t achievement_id : kept.tracked) {
      out << achievement_id;
    }
    out << YAML::EndSeq;
    out << YAML::Key << kTabField << YAML::Value << kept.tab;
    out << YAML::Key << kSubtabField << YAML::Value << kept.subtab;
    out << YAML::Key << kFilterField << YAML::Value << static_cast<int>(kept.filter);
    out << YAML::Key << kSortField << YAML::Value << static_cast<int>(kept.sort);
    if (kept.selected.has_value()) {
      out << YAML::Key << kSelectedField << YAML::Value << *kept.selected;
    }
    out << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;

  const std::string text = std::string(kFileHeader) + out.c_str() + "\n";
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(AchievementMemoryPath(client_root), text);
      !written) {
    ro::core::LogWarning("succès : écriture impossible ({})", ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
