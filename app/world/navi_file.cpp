#include "world/navi_file.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/save_folder.h"
#include "game/yaml_load.h"

namespace moonclient {
namespace {

constexpr const char* kMinimapField = "minimap";
constexpr const char* kShownField = "shown";
constexpr const char* kPosXField = "x";
constexpr const char* kPosYField = "y";
constexpr const char* kSideField = "side";
constexpr const char* kClickThroughField = "click_through";
constexpr const char* kZoomField = "zoom";
constexpr const char* kBackdropField = "backdrop_opacity";
constexpr const char* kMapField = "map_opacity";
constexpr const char* kSmoothField = "smooth";
constexpr const char* kArrowHalfField = "arrow_px";
constexpr const char* kArrowTintField = "arrow_tint";
constexpr const char* kCoordsField = "coordinates";
constexpr const char* kMapNameField = "place_name";
constexpr const char* kPartyField = "party";
constexpr const char* kGuildField = "guild";
constexpr const char* kQuestsField = "quests";
constexpr const char* kTownField = "facilities";
constexpr const char* kPortalsField = "portals";
constexpr const char* kViewpointsField = "viewpoints";
constexpr const char* kBossField = "boss";
constexpr const char* kRouteField = "route";
constexpr const char* kTrailIconField = "trail_icon";
constexpr const char* kGroundTrailField = "ground_trail";
constexpr const char* kMemosField = "memos";
constexpr const char* kNameField = "name";

// Les bornes que le fichier, édité à la main, peut dépasser.
constexpr int kLeastTrailIcon = 1;
constexpr int kMostTrailIcon = ro::ui::kNavigationTrailIconCount;
constexpr int kMostPercent = 100;
constexpr int kLongestMemoName = 63;

constexpr const char* kFileHeader =
    "# La minimap et la navigation : ce que le joueur a réglé, et ses marqueurs.\n"
    "#\n"
    "# « memos » range, par nom de carte interne, les points posés d'un clic\n"
    "# droit sur la minimap : cellule x, y et nom.\n"
    "#\n"
    "# ⚠ TOUT CECI EST AU CLIENT. Le serveur ne connaît ni ces marqueurs ni ces\n"
    "# réglages.\n";

template <typename T>
void Read(const YAML::Node& node, const char* field, T& into) {
  if (const YAML::Node value = node[field]; value && value.IsScalar()) into = value.as<T>(into);
}

void ReadMinimap(const YAML::Node& node, NaviPrefs& into) {
  ro::ui::MinimapSettings& minimap = into.minimap;
  Read(node, kShownField, into.minimap_shown);
  Read(node, kPosXField, minimap.pos_x);
  Read(node, kPosYField, minimap.pos_y);
  Read(node, kSideField, minimap.side_px);
  Read(node, kClickThroughField, minimap.click_through);
  Read(node, kZoomField, minimap.zoom);
  Read(node, kBackdropField, minimap.backdrop_percent);
  Read(node, kMapField, minimap.map_percent);
  Read(node, kSmoothField, minimap.smooth);
  Read(node, kArrowHalfField, minimap.arrow_half_px);
  std::uint32_t tint = minimap.arrow_tint;
  Read(node, kArrowTintField, tint);
  minimap.arrow_tint = static_cast<ImU32>(tint);
  Read(node, kCoordsField, minimap.show_coords);
  Read(node, kMapNameField, minimap.show_map_name);
  Read(node, kPartyField, minimap.show_party);
  Read(node, kGuildField, minimap.show_guild);
  Read(node, kQuestsField, minimap.show_quests);
  Read(node, kTownField, minimap.show_town);
  Read(node, kPortalsField, minimap.show_portals);
  Read(node, kViewpointsField, minimap.show_viewpoints);
  Read(node, kBossField, minimap.show_boss);
  Read(node, kRouteField, minimap.show_route);
  // Un zoom écrit à la main peut valoir n'importe quoi, NaN compris.
  if (!(minimap.zoom >= ro::ui::kMinimapZoomLeast)) minimap.zoom = ro::ui::kMinimapZoomLeast;
  minimap.zoom = std::min(minimap.zoom, ro::ui::kMinimapZoomMost);
  minimap.backdrop_percent = std::clamp(minimap.backdrop_percent, 0, kMostPercent);
  minimap.map_percent = std::clamp(minimap.map_percent, 0, kMostPercent);
}

void ReadMemos(const YAML::Node& node, NaviPrefs& into) {
  for (const auto& map : node) {
    const std::string map_name = map.first.as<std::string>(std::string());
    if (map_name.empty() || !map.second.IsSequence()) continue;
    std::vector<NaviMemo> memos;
    for (const YAML::Node& entry : map.second) {
      if (!entry.IsMap()) continue;
      NaviMemo memo;
      Read(entry, kPosXField, memo.cell_x);
      Read(entry, kPosYField, memo.cell_y);
      Read(entry, kNameField, memo.name);
      if (memo.name.size() > static_cast<std::size_t>(kLongestMemoName))
        memo.name.resize(static_cast<std::size_t>(kLongestMemoName));
      if (memos.size() < static_cast<std::size_t>(ro::ui::kMinimapMemoCap))
        memos.push_back(std::move(memo));
    }
    if (!memos.empty()) into.memos[map_name] = std::move(memos);
  }
}

}  // namespace

std::filesystem::path NaviPrefsPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kNaviPrefsFileName;
}

void LoadNaviPrefs(const std::filesystem::path& client_root, NaviPrefs& into) {
  const std::filesystem::path path = NaviPrefsPath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) return;
  YAML::Node whole;
  try {
    whole = LoadYamlFile(path);
  } catch (const YAML::Exception& broken) {
    // La seule exception qu'on attrape : un fichier abîmé ne doit pas empêcher
    // de jouer.
    ro::core::LogWarning("navigation : fichier illisible ({}) — les réglages par défaut restent",
                         broken.what());
    return;
  }
  // ⚠ UNE RACINE QUI N'EST PAS UNE TABLE N'A PAS DE CHAMPS : l'indexer lèverait
  // hors du `try`.
  if (!whole.IsMap()) return;
  try {
    if (const YAML::Node minimap = whole[kMinimapField]; minimap && minimap.IsMap())
      ReadMinimap(minimap, into);
    Read(whole, kTrailIconField, into.trail_icon);
    into.trail_icon = std::clamp(into.trail_icon, kLeastTrailIcon, kMostTrailIcon);
    Read(whole, kGroundTrailField, into.ground_trail);
    if (const YAML::Node memos = whole[kMemosField]; memos && memos.IsMap()) ReadMemos(memos, into);
  } catch (const YAML::Exception& broken) {
    ro::core::LogWarning("navigation : valeur illisible ({}) — le reste garde son défaut",
                         broken.what());
  }
}

bool SaveNaviPrefs(const std::filesystem::path& client_root, const NaviPrefs& from) {
  const ro::ui::MinimapSettings& minimap = from.minimap;
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kMinimapField << YAML::Value << YAML::BeginMap;
  out << YAML::Key << kShownField << YAML::Value << from.minimap_shown;
  out << YAML::Key << kPosXField << YAML::Value << minimap.pos_x;
  out << YAML::Key << kPosYField << YAML::Value << minimap.pos_y;
  out << YAML::Key << kSideField << YAML::Value << minimap.side_px;
  out << YAML::Key << kClickThroughField << YAML::Value << minimap.click_through;
  out << YAML::Key << kZoomField << YAML::Value << minimap.zoom;
  out << YAML::Key << kBackdropField << YAML::Value << minimap.backdrop_percent;
  out << YAML::Key << kMapField << YAML::Value << minimap.map_percent;
  out << YAML::Key << kSmoothField << YAML::Value << minimap.smooth;
  out << YAML::Key << kArrowHalfField << YAML::Value << minimap.arrow_half_px;
  out << YAML::Key << kArrowTintField << YAML::Value
      << static_cast<std::uint32_t>(minimap.arrow_tint);
  out << YAML::Key << kCoordsField << YAML::Value << minimap.show_coords;
  out << YAML::Key << kMapNameField << YAML::Value << minimap.show_map_name;
  out << YAML::Key << kPartyField << YAML::Value << minimap.show_party;
  out << YAML::Key << kGuildField << YAML::Value << minimap.show_guild;
  out << YAML::Key << kQuestsField << YAML::Value << minimap.show_quests;
  out << YAML::Key << kTownField << YAML::Value << minimap.show_town;
  out << YAML::Key << kPortalsField << YAML::Value << minimap.show_portals;
  out << YAML::Key << kViewpointsField << YAML::Value << minimap.show_viewpoints;
  out << YAML::Key << kBossField << YAML::Value << minimap.show_boss;
  out << YAML::Key << kRouteField << YAML::Value << minimap.show_route;
  out << YAML::EndMap;
  out << YAML::Key << kTrailIconField << YAML::Value << from.trail_icon;
  out << YAML::Key << kGroundTrailField << YAML::Value << from.ground_trail;
  out << YAML::Key << kMemosField << YAML::Value << YAML::BeginMap;
  for (const auto& [map_name, memos] : from.memos) {
    if (memos.empty()) continue;
    out << YAML::Key << map_name << YAML::Value << YAML::BeginSeq;
    for (const NaviMemo& memo : memos) {
      out << YAML::Flow << YAML::BeginMap;
      out << YAML::Key << kPosXField << YAML::Value << memo.cell_x;
      out << YAML::Key << kPosYField << YAML::Value << memo.cell_y;
      out << YAML::Key << kNameField << YAML::Value << memo.name;
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;
  }
  out << YAML::EndMap << YAML::EndMap;

  const std::string text = std::string(kFileHeader) + out.c_str() + "\n";
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(NaviPrefsPath(client_root), text);
      !written) {
    ro::core::LogWarning(
        "navigation : écriture impossible ({}) — réglages et marqueurs de cette partie ne seront "
        "pas retrouvés",
        ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
