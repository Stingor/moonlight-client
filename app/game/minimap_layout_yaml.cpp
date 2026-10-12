#include "game/minimap_layout_yaml.h"

#include <algorithm>
#include <cstdint>

namespace moonclient {
namespace {

// Les champs. ⚠ GRAVÉS : la section « minimap » de `navigation.yml` les porte
// encore chez un joueur existant, et un profil sans minimap en hérite.
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

constexpr int kMostPercent = 100;

template <typename T>
void Read(const YAML::Node& node, const char* field, T& into) {
  if (const YAML::Node value = node[field]; value && value.IsScalar()) into = value.as<T>(into);
}

}  // namespace

void ReadMinimapLayout(const YAML::Node& node, ro::ui::MinimapLayout& into) {
  ro::ui::MinimapSettings& minimap = into.settings;
  Read(node, kShownField, into.shown);
  Read(node, kPosXField, minimap.pos_x);
  Read(node, kPosYField, minimap.pos_y);
  Read(node, kSideField, minimap.side_px);
  Read(node, kClickThroughField, minimap.click_through);
  Read(node, kZoomField, minimap.zoom);
  Read(node, kBackdropField, minimap.backdrop_percent);
  Read(node, kMapField, minimap.map_percent);
  Read(node, kSmoothField, minimap.smooth);
  Read(node, kArrowHalfField, minimap.arrow_half_px);
  Read(node, kArrowTintField, minimap.arrow_tint);
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

void WriteMinimapLayout(YAML::Emitter& out, const ro::ui::MinimapLayout& from) {
  const ro::ui::MinimapSettings& minimap = from.settings;
  out << YAML::BeginMap;
  out << YAML::Key << kShownField << YAML::Value << from.shown;
  out << YAML::Key << kPosXField << YAML::Value << minimap.pos_x;
  out << YAML::Key << kPosYField << YAML::Value << minimap.pos_y;
  out << YAML::Key << kSideField << YAML::Value << minimap.side_px;
  out << YAML::Key << kClickThroughField << YAML::Value << minimap.click_through;
  out << YAML::Key << kZoomField << YAML::Value << minimap.zoom;
  out << YAML::Key << kBackdropField << YAML::Value << minimap.backdrop_percent;
  out << YAML::Key << kMapField << YAML::Value << minimap.map_percent;
  out << YAML::Key << kSmoothField << YAML::Value << minimap.smooth;
  out << YAML::Key << kArrowHalfField << YAML::Value << minimap.arrow_half_px;
  out << YAML::Key << kArrowTintField << YAML::Value << minimap.arrow_tint;
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
}

}  // namespace moonclient
