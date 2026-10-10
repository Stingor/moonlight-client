// L'itinéraire et les dirigeables de la carte du monde. Voir l'en-tête.

#include "world/world_map_routes.h"

#include <algorithm>

#include "ro/formats/navi_tables.h"

namespace moonclient {
namespace {

// LA LIGNE INTERNATIONALE : Einbroch, Lighthalzen, Einbroch, Juno, Hugel,
// Juno, puis Einbroch de nouveau.
constexpr std::array<std::string_view, 7> kFromEinbroch = {
    "einbroch", "lighthalzen", "einbroch", "yuno", "hugel", "yuno", "einbroch"};
constexpr std::array<std::string_view, 7> kFromLighthalzen = {
    "lighthalzen", "einbroch", "yuno", "hugel", "yuno", "einbroch", "lighthalzen"};
constexpr std::array<std::string_view, 7> kFromHugel = {
    "hugel", "yuno", "einbroch", "lighthalzen", "einbroch", "yuno", "hugel"};
constexpr std::array<std::string_view, 7> kFromJunoAbroad = {
    "yuno", "einbroch", "lighthalzen", "einbroch", "yuno", "hugel", "yuno"};
constexpr std::array<WorldAirTourSpec, 4> kAbroad = {
    WorldAirTourSpec{.start = "einbroch", .stops = kFromEinbroch},
    WorldAirTourSpec{.start = "lighthalzen", .stops = kFromLighthalzen},
    WorldAirTourSpec{.start = "hugel", .stops = kFromHugel},
    WorldAirTourSpec{.start = "yuno", .stops = kFromJunoAbroad},
};

// LA LIGNE INTÉRIEURE : Izlude, Juno, ra_fild12, puis Izlude de nouveau.
constexpr std::array<std::string_view, 4> kFromIzlude = {"izlude", "yuno", "ra_fild12", "izlude"};
constexpr std::array<std::string_view, 4> kFromRachelField = {"ra_fild12", "izlude", "yuno",
                                                              "ra_fild12"};
constexpr std::array<std::string_view, 4> kFromJunoHome = {"yuno", "ra_fild12", "izlude", "yuno"};
constexpr std::array<WorldAirTourSpec, 3> kHome = {
    WorldAirTourSpec{.start = "izlude", .stops = kFromIzlude},
    WorldAirTourSpec{.start = "ra_fild12", .stops = kFromRachelField},
    WorldAirTourSpec{.start = "yuno", .stops = kFromJunoHome},
};

constexpr std::array<std::span<const WorldAirTourSpec>, ro::ui::kWorldMapAirLineCount> kAirLines = {
    kAbroad, kHome};

}  // namespace

std::vector<std::string> WorldRouteMaps(const ro::sim::NaviGuide* guide) {
  std::vector<std::string> maps;
  if (guide == nullptr || !guide->active() || guide->data() == nullptr) {
    return maps;
  }
  const std::vector<ro::formats::NaviMap>& known = guide->data()->maps;
  const auto add = [&known, &maps](ro::formats::NaviMapIndex index) {
    if (index >= known.size() || std::ranges::find(maps, known[index].name) != maps.end()) {
      return;
    }
    maps.push_back(known[index].name);
  };
  for (const ro::sim::NaviGuideStep& step : guide->steps()) {
    add(step.from_map);
  }
  add(guide->goal_map());
  return maps;
}

std::string WorldRouteStepMap(const ro::sim::NaviGuide* guide) {
  if (guide == nullptr || !guide->active() || guide->data() == nullptr) {
    return {};
  }
  const std::vector<ro::formats::NaviMap>& known = guide->data()->maps;
  const std::size_t step = guide->current_step();
  const ro::formats::NaviMapIndex index =
      step < guide->steps().size() ? guide->steps()[step].from_map : guide->goal_map();
  return index < known.size() ? known[index].name : std::string();
}

std::vector<WorldRouteStep> WorldRouteSteps(const ro::vfs::WorldViewTables& tables,
                                            std::span<const std::string> maps) {
  std::vector<WorldRouteStep> steps;
  const std::vector<ro::vfs::WorldView>& worlds = tables.worlds();
  for (const std::string& map_name : maps) {
    for (std::size_t world = 0; world < worlds.size(); ++world) {
      // La première carte de ce nom sur ce monde décide : un étage de donjon
      // renvoie au monde suivant.
      const auto area =
          std::ranges::find(worlds[world].areas, map_name, &ro::vfs::WorldArea::map_name);
      if (area != worlds[world].areas.end() && area->group >= kWorldFieldGroupFloor) {
        steps.push_back(WorldRouteStep{.map_name = map_name, .world = world, .rect = area->rect});
        break;
      }
    }
  }
  return steps;
}

std::span<const WorldAirTourSpec> WorldAirLine(std::size_t line) {
  return line < kAirLines.size() ? kAirLines[line] : std::span<const WorldAirTourSpec>();
}

}  // namespace moonclient
