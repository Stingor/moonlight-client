#include "quest/tracker_memory.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

// Les champs du fichier. ⚠ GRAVÉS : les changer fait oublier ses réglages.
constexpr const char* kShownField = "shown";
constexpr const char* kBackgroundField = "background";
constexpr const char* kLockedField = "locked";
constexpr const char* kClickThroughField = "click_through";
constexpr const char* kSplitField = "achievements_apart";
constexpr const char* kMostQuestsField = "quests_shown";
constexpr const char* kTrackNewField = "follow_new";
constexpr const char* kCurrentMapFirstField = "current_map_first";
constexpr std::array<const char*, kTrackerPanelCount> kXFields = {"x", "achievements_x"};
constexpr std::array<const char*, kTrackerPanelCount> kYFields = {"y", "achievements_y"};

constexpr const char* kFileHeader =
    "# Le panneau de suivi des quêtes et des succès.\n"
    "#\n"
    "# « shown » : le suivi est à l'écran ;\n"
    "# « background » 0 aucun, 1 léger, 2 sombre ; « locked » et « click_through » ;\n"
    "# « achievements_apart » : les succès dans leur propre panneau ;\n"
    "# « quests_shown » : le plus de quêtes affichées ;\n"
    "# « follow_new » : une quête reçue se suit d'elle-même ;\n"
    "# « current_map_first » : les quêtes de la carte courante en tête.\n"
    "# « x » et « y » (puis « achievements_x » et « achievements_y » pour le panneau séparé) :\n"
    "# sa place en pixels d'écran, absente tant qu'on ne l'a pas déplacé.\n";

}  // namespace

std::filesystem::path TrackerMemoryPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kTrackerMemoryFileName;
}

void ParseTrackerMemory(std::string_view text, TrackerMemory& into) {
  YAML::Node whole;
  try {
    whole = YAML::Load(std::string(text));
  } catch (const YAML::Exception& broken) {
    ro::core::LogWarning("suivi : fichier illisible ({}) — les défauts s'appliquent",
                         broken.what());
    return;
  }
  if (!whole.IsMap()) {
    return;
  }
  try {
    into.shown = whole[kShownField].as<bool>(true);
    const ro::ui::TrackerLook defaults;
    ro::ui::TrackerLook& look = into.look;
    const int background = whole[kBackgroundField].as<int>(0);
    look.background =
        background >= 0 && static_cast<std::size_t>(background) < ro::ui::kTrackerBackgroundCount
            ? static_cast<ro::ui::TrackerBackground>(background)
            : ro::ui::TrackerBackground::kNone;
    look.locked = whole[kLockedField].as<bool>(defaults.locked);
    look.click_through = whole[kClickThroughField].as<bool>(defaults.click_through);
    look.split = whole[kSplitField].as<bool>(defaults.split);
    look.most_quests = std::clamp(whole[kMostQuestsField].as<std::int32_t>(defaults.most_quests),
                                  ro::ui::kTrackerFewestQuests, ro::ui::kTrackerMostQuests);
    look.track_new_quests = whole[kTrackNewField].as<bool>(defaults.track_new_quests);
    look.current_map_first = whole[kCurrentMapFirstField].as<bool>(defaults.current_map_first);
    for (std::size_t panel = 0; panel < kTrackerPanelCount; ++panel) {
      // UNE PLACE N'EXISTE QU'ENTIÈRE : une coordonnée seule ne dit rien.
      if (const YAML::Node x = whole[kXFields[panel]], y = whole[kYFields[panel]]; x && y) {
        into.places[panel] = ro::ui::TrackerPlace{.x = x.as<float>(), .y = y.as<float>()};
      }
    }
  } catch (const YAML::Exception& broken) {
    ro::core::LogWarning("suivi : champ illisible ({})", broken.what());
  }
}

std::string TrackerMemoryText(const TrackerMemory& from) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kShownField << YAML::Value << from.shown;
  out << YAML::Key << kBackgroundField << YAML::Value << static_cast<int>(from.look.background);
  out << YAML::Key << kLockedField << YAML::Value << from.look.locked;
  out << YAML::Key << kClickThroughField << YAML::Value << from.look.click_through;
  out << YAML::Key << kSplitField << YAML::Value << from.look.split;
  out << YAML::Key << kMostQuestsField << YAML::Value << from.look.most_quests;
  out << YAML::Key << kTrackNewField << YAML::Value << from.look.track_new_quests;
  out << YAML::Key << kCurrentMapFirstField << YAML::Value << from.look.current_map_first;
  for (std::size_t panel = 0; panel < kTrackerPanelCount; ++panel) {
    if (from.places[panel].has_value()) {
      out << YAML::Key << kXFields[panel] << YAML::Value << from.places[panel]->x;
      out << YAML::Key << kYFields[panel] << YAML::Value << from.places[panel]->y;
    }
  }
  out << YAML::EndMap;
  return std::string(kFileHeader) + out.c_str() + "\n";
}

void LoadTrackerMemory(const std::filesystem::path& client_root, TrackerMemory& into) {
  const std::filesystem::path path = TrackerMemoryPath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return;
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    ro::core::LogWarning("suivi : {} ne s'ouvre pas — les défauts s'appliquent", path.string());
    return;
  }
  const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  ParseTrackerMemory(text, into);
}

bool SaveTrackerMemory(const std::filesystem::path& client_root, const TrackerMemory& from) {
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(TrackerMemoryPath(client_root), TrackerMemoryText(from));
      !written) {
    ro::core::LogWarning("suivi : écriture impossible ({})", ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
