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

#include "game/minimap_layout_yaml.h"
#include "game/save_folder.h"
#include "game/yaml_load.h"

namespace moonclient {
namespace {

constexpr const char* kMinimapField = "minimap";
constexpr const char* kPosXField = "x";
constexpr const char* kPosYField = "y";
constexpr const char* kTrailIconField = "trail_icon";
constexpr const char* kGroundTrailField = "ground_trail";
constexpr const char* kMemosField = "memos";
constexpr const char* kNameField = "name";

// Les bornes que le fichier, édité à la main, peut dépasser.
constexpr int kLeastTrailIcon = 1;
constexpr int kMostTrailIcon = ro::ui::kNavigationTrailIconCount;
constexpr int kLongestMemoName = 63;

constexpr const char* kFileHeader =
    "# La minimap et la navigation : ce que le joueur a réglé, et ses marqueurs.\n"
    "#\n"
    "# « minimap », s'il est là, est la présentation qu'avait la minimap avant de\n"
    "# suivre le profil de disposition (ui_layouts.yml) : un profil qui n'en a pas\n"
    "# encore la reprend. Le client ne la change plus.\n"
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
      ReadMinimapLayout(minimap, into.legacy_minimap.emplace());
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
  YAML::Emitter out;
  out << YAML::BeginMap;
  if (from.legacy_minimap.has_value()) {
    out << YAML::Key << kMinimapField << YAML::Value;
    WriteMinimapLayout(out, *from.legacy_minimap);
  }
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

ro::ui::MinimapLayout InheritedMinimap(const std::filesystem::path& client_root) {
  NaviPrefs prefs;
  LoadNaviPrefs(client_root, prefs);
  return prefs.legacy_minimap.value_or(ro::ui::MinimapLayout{});
}

}  // namespace moonclient
