// La vue de la carte du monde. Voir l'en-tête.

#include "world/world_map_compose.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/formats/navi_tables.h"

#include "world/world_map_routes.h"

namespace moonclient {
namespace {

constexpr std::string_view kGatExtension = ".gat";
// Les images de l'itinéraire et des dirigeables, sous la racine d'interface.
constexpr std::string_view kRouteLineImage = "worldview_interface\\wv_airf2.bmp";
constexpr std::string_view kRouteGoalImage = "worldview_interface\\wv_end.bmp";
constexpr std::string_view kAirLineImage = "worldview_interface\\wv_airf3.bmp";
constexpr std::string_view kAirTrailImage = "worldview_interface\\wv_airf.bmp";
constexpr std::string_view kAirshipImage = "worldview_interface\\wv_airplen32.bmp";
constexpr std::string_view kDungeonSpotImage = "worldview_interface\\wv_dunpos.bmp";
constexpr std::string_view kDungeonLabelImage = "worldview_interface\\wv_dunselect.bmp";
constexpr std::string_view kPartyFaceImage = "worldview_interface\\wv_face.bmp";
constexpr std::string_view kPartyFaceHereImage = "worldview_interface\\wv_face1.bmp";
constexpr std::string_view kStepMarkImage = "worldview_interface\\wv_select.bmp";
constexpr std::string_view kPreviewFolder = "map\\";
// Les flèches de la Kafra, sous la racine des textures.
constexpr std::string_view kKafraArrowBody = "maparrow_body.bmp";
constexpr std::string_view kKafraArrowHead = "maparrow_head.bmp";
// IZLUDE EST COLLÉE À PRONTERA : la flèche qui y mène part du bord droit de la
// ville, et vise le haut de sa case, aux quatre cinquièmes de sa largeur.
constexpr std::string_view kKafraNarrowTown = "izlude";
constexpr float kKafraNarrowAim = 0.8F;
// L'IMAGE D'UN PALIER DE RECHERCHE : la carte, cinq paliers de monstres, le PNJ
// seul puis avec cinq paliers de monstres.
constexpr std::string_view kSearchImagePrefix = "worldview_interface\\wv_sreach";
constexpr std::array<std::string_view, 6> kSearchMonsterImages = {"map",  "mob1", "mob2",
                                                                  "mob3", "mob4", "mob5"};

[[nodiscard]] ro::ui::WorldMapRect RectOf(const ro::vfs::WorldRect& rect) {
  return ro::ui::WorldMapRect{.left = static_cast<float>(rect.left),
                              .top = static_cast<float>(rect.top),
                              .right = static_cast<float>(rect.right),
                              .bottom = static_cast<float>(rect.bottom)};
}

[[nodiscard]] ImU32 ColorOf(const ro::vfs::WorldViewColor& color) {
  return IM_COL32(color.red, color.green, color.blue, color.alpha);
}

// Un texte de la carte, ses tailles comprises.
[[nodiscard]] ro::ui::WorldMapText TextOf(const ro::vfs::WorldViewText& text) {
  ro::ui::WorldMapText shown{.letter = ColorOf(text.letter), .shadow = ColorOf(text.shadow)};
  for (std::size_t tier = 0; tier < shown.sizes.size(); ++tier) {
    shown.sizes[tier] = static_cast<float>(text.sizes[tier]);
  }
  return shown;
}

// LA MINIMAP D'UNE CARTE, pour son aperçu, sous la racine d'interface.
[[nodiscard]] std::string PreviewImageOf(std::string_view map_name) {
  std::string path(kPreviewFolder);
  path += map_name;
  path += ".bmp";
  return path;
}

// Le nom de l'image d'un palier de recherche.
[[nodiscard]] std::string SearchImageOf(int shade) {
  const bool npc = shade >= kWorldSearchNpcShade;
  const auto tier = static_cast<std::size_t>(npc ? shade - kWorldSearchNpcShade : shade);
  std::string name(kSearchImagePrefix);
  if (npc) {
    name += "npc";
    if (tier > 0 && tier < kSearchMonsterImages.size()) {
      name += "_";
      name += kSearchMonsterImages[tier];
    }
  } else {
    name += tier < kSearchMonsterImages.size() ? kSearchMonsterImages[tier] : "noimage";
  }
  name += ".bmp";
  return name;
}

// Les paliers de monstres : à partir de ces nombres, le palier suivant.
constexpr std::array<std::int32_t, 4> kShadeFloors = {5, 20, 50, 100};

// La teinte d'un palier, dans celles du fichier.
[[nodiscard]] ImU32 SearchColorOf(int shade, const ro::vfs::WorldViewColors& colors) {
  if (shade == 0) {
    return ColorOf(colors.search_map);
  }
  if (shade == kWorldSearchNpcShade) {
    return ColorOf(colors.search_npc);
  }
  const int tier = shade > kWorldSearchNpcShade ? shade - kWorldSearchNpcShade : shade;
  const auto rank = static_cast<std::size_t>(
      std::clamp(tier, 1, static_cast<int>(colors.search_monsters.size())) - 1);
  return ColorOf(colors.search_monsters[rank]);
}

// Le compte de chaque carte trouvée par la recherche, par nom interne.
[[nodiscard]] std::map<std::string, std::int32_t, std::less<>> SearchCounts(
    const ro::formats::NaviData& data, std::span<const ro::sim::NaviHitGroup> groups) {
  std::map<std::string, std::int32_t, std::less<>> counts;
  const auto map_name = [&data](std::size_t index) -> std::string {
    return index < data.maps.size() ? data.maps[index].name : std::string();
  };
  for (const ro::sim::NaviHitGroup& group : groups) {
    for (const ro::sim::NaviHit& hit : group.hits) {
      std::string map;
      std::int32_t count = 0;
      switch (hit.kind) {
        case ro::sim::NaviHitKind::kMap:
          map = map_name(hit.rank);
          break;
        case ro::sim::NaviHitKind::kNpc:
          if (hit.rank < data.npcs.size()) {
            map = map_name(data.npcs[hit.rank].map);
          }
          count = kWorldSearchNpcMark;
          break;
        case ro::sim::NaviHitKind::kSpawn:
          if (hit.rank < data.spawns.size()) {
            map = map_name(data.spawns[hit.rank].map);
            count = data.spawns[hit.rank].amount;
          }
          break;
      }
      if (map.empty()) {
        continue;
      }
      if (const auto found = counts.find(map); found != counts.end()) {
        AddWorldSearchHit(found->second, count);
      } else {
        counts.emplace(std::move(map), count);
      }
    }
  }
  return counts;
}

// Le nom affiché d'une carte : celui de la table des noms, sinon celui de la
// carte du monde, sinon son nom interne.
[[nodiscard]] std::string_view ShownNameOf(const ro::vfs::WorldArea& area,
                                           const ro::vfs::MapDisplayNames* names) {
  if (names != nullptr) {
    if (const auto shown = names->DisplayNameOf(area.map_name + std::string(kGatExtension));
        shown.has_value()) {
      return *shown;
    }
  }
  return area.title.empty() ? std::string_view(area.map_name) : std::string_view(area.title);
}

}  // namespace

std::vector<WorldMapMember> WorldMapPartyMembers(const ro::sim::PartyRoster& party,
                                                 std::uint32_t own_account_id,
                                                 std::string_view own_name,
                                                 std::string_view here_map,
                                                 std::vector<std::string>& maps) {
  std::vector<WorldMapMember> members;
  maps.clear();
  if (!party.in_party()) {
    maps.emplace_back(here_map);
    members.push_back(WorldMapMember{.name = own_name, .map_name = maps.back(), .self = true});
    return members;
  }
  // ⚠ RÉSERVÉ D'AVANCE : les membres gardent une vue sur le nom de leur carte.
  maps.reserve(party.members().size());
  for (const ro::sim::PartyMember& member : party.members()) {
    if (member.offline) {
      continue;
    }
    std::string map = ro::core::AsciiLower(member.map);
    if (map.ends_with(kGatExtension)) {
      map.resize(map.size() - kGatExtension.size());
    }
    maps.push_back(std::move(map));
    members.push_back(WorldMapMember{
        .name = member.name, .map_name = maps.back(), .self = member.account_id == own_account_id});
  }
  return members;
}

void AddWorldSearchHit(std::int32_t& kept, std::int32_t count) {
  if (count == kWorldSearchNpcMark) {
    if (kept < kWorldSearchNpcMark) {
      kept += kWorldSearchNpcMark;
    }
  } else if (kept < kWorldSearchNpcMark) {
    kept = std::max(kept, count);
  } else if (count > kept - kWorldSearchNpcMark) {
    kept = count + kWorldSearchNpcMark;
  }
}

int WorldSearchShade(std::int32_t count) {
  const int base = count >= kWorldSearchNpcMark ? kWorldSearchNpcShade : 0;
  const std::int32_t monsters = count % kWorldSearchNpcMark;
  if (monsters <= 0) {
    return base;
  }
  int tier = 1;
  for (const std::int32_t floor : kShadeFloors) {
    if (monsters >= floor) {
      ++tier;
    }
  }
  return base + tier;
}

const ro::ui::WorldMapView& WorldMapComposer::Compose(const WorldMapSources& sources) {
  view_ = ro::ui::WorldMapView{};
  world_names_.clear();
  monsters_.clear();
  areas_.clear();
  floors_.clear();
  dungeons_.clear();
  member_names_.clear();
  members_.clear();
  party_.clear();
  kafra_legs_.clear();
  route_.clear();
  air_stops_.clear();
  air_tours_.clear();
  air_lines_.clear();
  if (sources.tables == nullptr || sources.tables->worlds().empty()) {
    return view_;
  }
  const std::vector<ro::vfs::WorldView>& worlds = sources.tables->worlds();
  for (const ro::vfs::WorldView& world : worlds) {
    world_names_.push_back(world.title.c_str());
  }
  std::size_t shown = sources.tables->WorldOf(sources.here_map).value_or(0);
  if (sources.chosen_world >= 0 && static_cast<std::size_t>(sources.chosen_world) < worlds.size()) {
    shown = static_cast<std::size_t>(sources.chosen_world);
  }
  const ro::vfs::WorldView& world = worlds[shown];
  const auto view_of = [](const ro::vfs::WorldArea& area) {
    return ro::ui::WorldMapAreaView{.rect = RectOf(area.rect),
                                    .map_name = area.map_name,
                                    .title = area.title,
                                    .level = area.level,
                                    .group = area.group};
  };
  // LES ÉTAGES, rangés donjon après donjon : chaque case en prend une tranche.
  std::vector<std::size_t> first_floor;
  for (const ro::vfs::WorldDungeon& dungeon : world.dungeons) {
    first_floor.push_back(floors_.size());
    for (const ro::vfs::WorldArea& area : world.areas) {
      if (area.group == dungeon.group) {
        floors_.push_back(view_of(area));
      }
    }
  }
  for (const ro::vfs::WorldArea& area : world.areas) {
    const bool floor = std::ranges::any_of(
        world.dungeons, [&area](const auto& dungeon) { return dungeon.group == area.group; });
    if (!floor) {
      areas_.push_back(view_of(area));
    }
  }
  for (std::size_t rank = 0; rank < world.dungeons.size(); ++rank) {
    const ro::vfs::WorldDungeon& dungeon = world.dungeons[rank];
    const std::size_t end = rank + 1 < first_floor.size() ? first_floor[rank + 1] : floors_.size();
    const std::span<const ro::ui::WorldMapAreaView> floors(floors_.data() + first_floor[rank],
                                                           end - first_floor[rank]);
    // L'ÉTIQUETTE EST LE RECTANGLE DU PREMIER ÉTAGE ; sans étage, le point.
    dungeons_.push_back(ro::ui::WorldMapDungeonView{
        .rect = RectOf(dungeon.rect),
        .label = floors.empty() ? RectOf(dungeon.rect) : floors.front().rect,
        .title = dungeon.title,
        .level = dungeon.level,
        .group = dungeon.group,
        .floors = floors});
  }
  ComposeRoute(sources, shown);
  ComposeAirLines(sources, world);
  ComposeParty(sources, shown);
  ComposeKafra(sources, world);

  view_.worlds = world_names_;
  view_.world = static_cast<int>(shown);
  if (sources.image_at) {
    view_.image = sources.image_at(world.image);
  }
  if (view_.image.usable()) {
    view_.image_size = ImVec2(view_.image.width, view_.image.height);
  }
  // LA RECHERCHE : chaque carte trouvée prend la teinte de son palier.
  if (sources.navi != nullptr && !sources.search.empty()) {
    const auto counts = SearchCounts(*sources.navi, sources.search);
    const auto shade = [&counts, &sources](ro::ui::WorldMapAreaView& area) {
      if (const auto found = counts.find(area.map_name); found != counts.end()) {
        const int tier = WorldSearchShade(found->second);
        area.search_color = SearchColorOf(tier, sources.tables->colors());
        if (sources.image_at) {
          area.search_image = sources.image_at(SearchImageOf(tier));
        }
      }
    };
    std::ranges::for_each(areas_, shade);
    std::ranges::for_each(floors_, shade);
  }
  // LES MONSTRES DE CHAQUE CARTE, une fois toutes les cartes rangées : le
  // tampon ne se réalloue plus quand les tranches s'y posent.
  if (sources.navi != nullptr) {
    std::vector<std::pair<std::size_t, std::size_t>> slices;
    for (const ro::ui::WorldMapAreaView& area : areas_) {
      const std::size_t first = monsters_.size();
      const ro::formats::NaviMapIndex index = sources.navi->FindMap(area.map_name);
      if (index != ro::formats::kNoNaviMap) {
        for (const std::uint32_t slot : sources.navi->SpawnsOn(index)) {
          const ro::formats::NaviSpawn& spawn = sources.navi->spawns[slot];
          monsters_.push_back(ro::ui::WorldMapMonsterView{.name = spawn.name,
                                                          .level = spawn.level,
                                                          .amount = spawn.amount,
                                                          .boss = spawn.is_boss()});
        }
      }
      // Les boss en tête, puis par niveau.
      std::stable_sort(monsters_.begin() + static_cast<std::ptrdiff_t>(first), monsters_.end(),
                       [](const auto& left, const auto& right) {
                         return left.boss != right.boss ? left.boss : left.level < right.level;
                       });
      slices.emplace_back(first, monsters_.size() - first);
    }
    for (std::size_t rank = 0; rank < areas_.size(); ++rank) {
      areas_[rank].monsters = std::span<const ro::ui::WorldMapMonsterView>(
          monsters_.data() + slices[rank].first, slices[rank].second);
    }
  }
  view_.areas = areas_;
  view_.dungeons = dungeons_;
  view_.dungeon_color = ColorOf(sources.tables->colors().dungeon_box);
  view_.route_color = ColorOf(sources.tables->colors().route);
  const ro::vfs::WorldViewColors& colors = sources.tables->colors();
  view_.select_color = ColorOf(colors.selected);
  view_.dungeon_name = TextOf(colors.dungeon_name);
  view_.field_edge = ColorOf(colors.field_edge);
  view_.field_name = TextOf(colors.field_name);
  view_.field_level = TextOf(colors.field_level);
  view_.select_box = ColorOf(colors.select_box);
  // L'APERÇU de la carte désignée : n'importe quelle carte de ce monde, un
  // étage de donjon compris.
  if (!sources.preview_map.empty()) {
    const auto area =
        std::ranges::find(world.areas, sources.preview_map, &ro::vfs::WorldArea::map_name);
    if (area != world.areas.end()) {
      view_.preview = ro::ui::WorldMapPreview{
          .map_name = area->map_name,
          .title = ShownNameOf(*area, sources.map_names),
          .image = sources.image_at ? sources.image_at(PreviewImageOf(area->map_name))
                                    : ro::ui::SkinImage{}};
    }
  }
  view_.route = route_;
  view_.air_lines = air_lines_;
  view_.party = party_;
  // LA COCHE DE L'ÉTAPE EN COURS, si c'est un champ ou une ville de ce monde.
  if (!sources.step_map.empty()) {
    const std::array<std::string, 1> step = {std::string(sources.step_map)};
    const std::vector<WorldRouteStep> found = WorldRouteSteps(*sources.tables, step);
    if (!found.empty() && found.front().world == shown) {
      view_.step_mark = RectOf(found.front().rect);
    }
  }
  if (sources.image_at) {
    view_.dungeon_spot = sources.image_at(kDungeonSpotImage);
    view_.dungeon_label = sources.image_at(kDungeonLabelImage);
    view_.party_face = sources.image_at(kPartyFaceImage);
    view_.party_face_here = sources.image_at(kPartyFaceHereImage);
    view_.step_mark_image = sources.image_at(kStepMarkImage);
    view_.route_line = sources.image_at(kRouteLineImage);
    view_.route_goal = sources.image_at(kRouteGoalImage);
    view_.air_line = sources.image_at(kAirLineImage);
    view_.air_trail = sources.image_at(kAirTrailImage);
    view_.airship = sources.image_at(kAirshipImage);
  }
  if (sources.texture_at) {
    view_.kafra_arrow_body = sources.texture_at(kKafraArrowBody);
    view_.kafra_arrow_head = sources.texture_at(kKafraArrowHead);
  }
  return view_;
}

void WorldMapComposer::ComposeRoute(const WorldMapSources& sources, std::size_t shown) {
  const std::vector<WorldRouteStep> steps = WorldRouteSteps(*sources.tables, sources.route);
  for (const WorldRouteStep& step : steps) {
    route_.push_back(
        ro::ui::WorldMapRouteStep{.rect = RectOf(step.rect), .shown = step.world == shown});
  }
  // LA MARQUE D'ARRIVÉE : la dernière étape est la destination — une
  // destination au fond d'un donjon n'en est pas une.
  view_.route_reaches_goal = !steps.empty() && !sources.route.empty() &&
                             steps.back().map_name == sources.route.back() &&
                             steps.back().world == shown;
}

void WorldMapComposer::ComposeParty(const WorldMapSources& sources, std::size_t shown) {
  // LES CARTES DU GROUPE, dans l'ordre où elles paraissent ; chacune garde ses
  // membres. Les noms sont recopiés : la vue survit à l'image où le groupe a
  // été lu.
  std::vector<std::string_view> maps;
  for (const WorldMapMember& member : sources.party) {
    if (!member.map_name.empty() && std::ranges::find(maps, member.map_name) == maps.end()) {
      maps.push_back(member.map_name);
    }
  }
  // ⚠ RÉSERVÉS D'AVANCE : les tranches des cartes se posent pendant qu'on
  // remplit, et une réallocation les laisserait pendre.
  member_names_.reserve(sources.party.size());
  members_.reserve(sources.party.size());
  for (const std::string_view map : maps) {
    for (const WorldMapMember& member : sources.party) {
      if (member.map_name == map) {
        member_names_.emplace_back(member.name);
      }
    }
  }
  std::size_t next_name = 0;
  for (const std::string_view map : maps) {
    const std::size_t first = members_.size();
    for (const WorldMapMember& member : sources.party) {
      if (member.map_name == map) {
        members_.push_back(
            ro::ui::WorldMapMemberView{.name = member_names_[next_name++], .self = member.self});
      }
    }
    // Sur le premier monde qui montre la carte ; ailleurs, elle ne se voit pas.
    const std::optional<std::size_t> world = sources.tables->WorldOf(map);
    if (!world.has_value() || *world != shown) {
      continue;
    }
    const std::vector<ro::vfs::WorldArea>& areas = sources.tables->worlds()[shown].areas;
    const auto area = std::ranges::find(areas, map, &ro::vfs::WorldArea::map_name);
    if (area == areas.end()) {
      continue;
    }
    party_.push_back(
        ro::ui::WorldMapPartyView{.rect = RectOf(area->rect),
                                  .map_name = area->map_name,
                                  .here = area->map_name == sources.here_map,
                                  .members = std::span<const ro::ui::WorldMapMemberView>(
                                      members_.data() + first, members_.size() - first)});
  }
}

void WorldMapComposer::ComposeKafra(const WorldMapSources& sources,
                                    const ro::vfs::WorldView& world) {
  if (sources.kafra_maps == nullptr || sources.preview_map.empty()) {
    return;
  }
  const std::span<const std::string> destinations =
      sources.kafra_maps->DestinationsOf(sources.preview_map);
  const auto town =
      std::ranges::find(world.areas, sources.preview_map, &ro::vfs::WorldArea::map_name);
  if (destinations.empty() || town == world.areas.end()) {
    return;
  }
  const auto center = [](const ro::vfs::WorldRect& rect) {
    return ImVec2(static_cast<float>(rect.right - rect.left) * 0.5F + static_cast<float>(rect.left),
                  static_cast<float>(rect.bottom - rect.top) * 0.5F + static_cast<float>(rect.top));
  };
  // LES DESTINATIONS SE CHERCHENT SUR LE PREMIER MONDE, celui des Kafra.
  const std::vector<ro::vfs::WorldArea>& first = sources.tables->worlds().front().areas;
  for (const std::string& destination : destinations) {
    const auto area = std::ranges::find(first, destination, &ro::vfs::WorldArea::map_name);
    if (area == first.end()) {
      continue;
    }
    ro::ui::WorldMapKafraLeg leg{.stop = {.rect = RectOf(area->rect),
                                          .map_name = area->map_name,
                                          .title = ShownNameOf(*area, sources.map_names)},
                                 .from = center(town->rect),
                                 .to = center(area->rect)};
    if (area->map_name == kKafraNarrowTown) {
      leg.from.x = static_cast<float>(town->rect.right);
      leg.to = ImVec2(static_cast<float>(area->rect.left) +
                          kKafraNarrowAim * static_cast<float>(area->rect.right - area->rect.left),
                      static_cast<float>(area->rect.top));
    }
    kafra_legs_.push_back(leg);
  }
  ro::ui::WorldMapKafraView kafra{
      .town_map = town->map_name, .town = RectOf(town->rect), .legs = kafra_legs_};
  if (sources.kafra_sprite != nullptr) {
    kafra.sprite = sources.kafra_sprite->image;
    kafra.sprite_feet = sources.kafra_sprite->feet;
    kafra.sprite_height = sources.kafra_sprite->height;
  }
  view_.kafra = kafra;
}

void WorldMapComposer::ComposeAirLines(const WorldMapSources& sources,
                                       const ro::vfs::WorldView& world) {
  // Les escales d'abord, toutes : les circuits n'en prennent des tranches
  // qu'une fois le tampon rempli.
  struct Slice {
    std::string_view start;
    std::size_t first = 0;
    std::size_t count = 0;
  };
  std::array<std::vector<Slice>, ro::ui::kWorldMapAirLineCount> slices;
  for (std::size_t line = 0; line < slices.size(); ++line) {
    for (const WorldAirTourSpec& spec : WorldAirLine(line)) {
      const std::size_t first = air_stops_.size();
      for (const std::string_view stop : spec.stops) {
        // Une escale que ce monde ne montre pas est sautée.
        const auto area = std::ranges::find(world.areas, stop, &ro::vfs::WorldArea::map_name);
        if (area != world.areas.end()) {
          air_stops_.push_back(
              ro::ui::WorldMapAirStop{.rect = RectOf(area->rect),
                                      .map_name = area->map_name,
                                      .title = ShownNameOf(*area, sources.map_names)});
        }
      }
      slices[line].push_back(
          Slice{.start = spec.start, .first = first, .count = air_stops_.size() - first});
    }
  }
  std::array<std::size_t, ro::ui::kWorldMapAirLineCount> first_tour{};
  for (std::size_t line = 0; line < slices.size(); ++line) {
    first_tour[line] = air_tours_.size();
    for (const Slice& slice : slices[line]) {
      air_tours_.push_back(
          ro::ui::WorldMapAirTour{.start = slice.start,
                                  .stops = std::span<const ro::ui::WorldMapAirStop>(
                                      air_stops_.data() + slice.first, slice.count)});
    }
  }
  for (std::size_t line = 0; line < slices.size(); ++line) {
    air_lines_.push_back(
        ro::ui::WorldMapAirLine{.tours = std::span<const ro::ui::WorldMapAirTour>(
                                    air_tours_.data() + first_tour[line], slices[line].size())});
  }
}

}  // namespace moonclient
