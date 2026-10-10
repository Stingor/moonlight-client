#include "world/navigation_compose.h"

#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/sim/walk_path.h"

namespace moonclient {
namespace {

using ro::formats::NaviData;
using ro::sim::NaviHitKind;

constexpr std::string_view kMapFolder = "map\\";
constexpr std::string_view kBitmapExtension = ".bmp";
constexpr std::string_view kTrailIconPrefix = "navigation_interface3\\btn_roadiocn_select";
constexpr std::string_view kTrailIconSuffix = "_normal.bmp";

[[nodiscard]] std::string_view MapNameOf(const NaviData& data, ro::formats::NaviMapIndex map) {
  return map < data.maps.size() ? std::string_view(data.maps[map].name) : std::string_view();
}

[[nodiscard]] std::string_view MapLabelOf(const NaviData& data, ro::formats::NaviMapIndex map) {
  return map < data.maps.size() ? std::string_view(data.maps[map].display_name)
                                : std::string_view();
}

[[nodiscard]] ro::ui::NavigationEntryView EntryOf(const NaviData& data,
                                                  const ro::sim::NaviHit& hit) {
  ro::ui::NavigationEntryView entry;
  switch (hit.kind) {
    case NaviHitKind::kMap: {
      const auto map = static_cast<ro::formats::NaviMapIndex>(hit.rank);
      entry.kind = ro::ui::NavigationKind::kMap;
      entry.name = MapLabelOf(data, map);
      entry.map_name = MapNameOf(data, map);
      entry.map_label = entry.name;
      break;
    }
    case NaviHitKind::kNpc: {
      const ro::formats::NaviNpc& npc = data.npcs[hit.rank];
      entry.kind = ro::ui::NavigationKind::kNpc;
      entry.name = npc.name;
      entry.map_name = MapNameOf(data, npc.map);
      entry.map_label = MapLabelOf(data, npc.map);
      entry.has_cell = npc.cell_x > 0 && npc.cell_y > 0;
      entry.x = npc.cell_x;
      entry.y = npc.cell_y;
      entry.is_shop = npc.is_shop();
      break;
    }
    case NaviHitKind::kSpawn: {
      const ro::formats::NaviSpawn& spawn = data.spawns[hit.rank];
      entry.kind = ro::ui::NavigationKind::kMonster;
      entry.name = spawn.name;
      entry.map_name = MapNameOf(data, spawn.map);
      entry.map_label = MapLabelOf(data, spawn.map);
      entry.level = spawn.level;
      entry.amount = spawn.amount;
      entry.has_stats = spawn.packed_stats != 0;
      entry.element = static_cast<int>(spawn.element());
      entry.element_level = static_cast<int>(spawn.element_level());
      entry.size = static_cast<int>(spawn.size());
      entry.race = static_cast<int>(spawn.race());
      entry.is_mvp = spawn.is_boss();
      entry.monster_id = spawn.sprite_class;
      break;
    }
  }
  return entry;
}

// La classe de sprite d'un résultat : zéro pour une carte, ou pour une
// sentinelle de portail, qui n'est pas un personnage.
[[nodiscard]] std::uint32_t SpriteClassOf(const NaviData& data, const ro::sim::NaviHit& hit) {
  std::int32_t sprite_class = 0;
  if (hit.kind == NaviHitKind::kNpc && hit.rank < data.npcs.size())
    sprite_class = data.npcs[hit.rank].sprite_class;
  if (hit.kind == NaviHitKind::kSpawn && hit.rank < data.spawns.size())
    sprite_class = data.spawns[hit.rank].sprite_class;
  if (sprite_class <= 0 || sprite_class == ro::formats::kNaviWarpPortalSprite) return 0;
  return static_cast<std::uint32_t>(sprite_class);
}

[[nodiscard]] ro::ui::NavigationKind KindOf(NaviHitKind kind) {
  switch (kind) {
    case NaviHitKind::kMap:
      return ro::ui::NavigationKind::kMap;
    case NaviHitKind::kNpc:
      return ro::ui::NavigationKind::kNpc;
    case NaviHitKind::kSpawn:
      return ro::ui::NavigationKind::kMonster;
  }
  return ro::ui::NavigationKind::kMap;
}

}  // namespace

void NavigationComposer::Search(const NaviData& data, std::string_view term,
                                ro::formats::NaviMapIndex only_map,
                                ro::formats::NaviMapIndex prefer_map) {
  ro::sim::NaviSearchQuery query;
  query.term = term;
  query.only_map = only_map;
  query.prefer_map = prefer_map;
  groups_ = ro::sim::SearchNavi(data, query);
  Rebuild(data);
}

void NavigationComposer::ShowContents(const NaviData& data, ro::formats::NaviMapIndex map) {
  groups_ = ro::sim::ContentsOfMap(data, map);
  // Le contenu d'une carte n'est pas une recherche : ses groupes se nomment
  // par leur nature.
  for (ro::sim::NaviHitGroup& group : groups_)
    group.name = group.kind == NaviHitKind::kNpc ? std::string("NPC")
                                                 : std::string(ro::core::Tr("Monstres"));
  Rebuild(data);
}

void NavigationComposer::Clear() {
  groups_.clear();
  entries_.clear();
  group_views_.clear();
  ++serial_;
}

void NavigationComposer::Rebuild(const NaviData& data) {
  entries_.clear();
  entries_.reserve(groups_.size());
  for (const ro::sim::NaviHitGroup& group : groups_) {
    std::vector<ro::ui::NavigationEntryView>& rows = entries_.emplace_back();
    rows.reserve(group.hits.size());
    for (const ro::sim::NaviHit& hit : group.hits) rows.push_back(EntryOf(data, hit));
  }
  group_views_.clear();
  for (std::size_t rank = 0; rank < groups_.size(); ++rank)
    group_views_.push_back({.name = groups_[rank].name,
                            .kind = KindOf(groups_[rank].kind),
                            .entries = entries_[rank]});
  ++serial_;
}

std::optional<ro::sim::NaviHit> NavigationComposer::HitAt(ro::ui::NavigationRowRef row) const {
  if (row.group >= groups_.size() || row.entry >= groups_[row.group].hits.size())
    return std::nullopt;
  return groups_[row.group].hits[row.entry];
}

const ro::ui::NavigationView& NavigationComposer::Compose(const NavigationSources& from) {
  view_ = ro::ui::NavigationView{};
  view_.groups = group_views_;
  view_.results_serial = serial_;
  view_.current_map = from.current_map;
  view_.current_map_label = from.current_map_label;
  view_.bound_map_label = from.bound_map_label;
  view_.graph_map_count = from.navi != nullptr ? static_cast<int>(from.navi->maps.size()) : 0;

  if (from.image_at) {
    for (std::vector<ro::ui::NavigationEntryView>& rows : entries_) {
      for (ro::ui::NavigationEntryView& entry : rows) {
        if (entry.map_name.empty()) continue;
        std::string path(kMapFolder);
        path += entry.map_name;
        path += kBitmapExtension;
        entry.map_image = from.image_at(path);
      }
    }
    for (int icon = 1; icon <= ro::ui::kNavigationTrailIconCount; ++icon) {
      std::string path(kTrailIconPrefix);
      path += std::to_string(icon);
      path += kTrailIconSuffix;
      view_.trail_icons[static_cast<std::size_t>(icon - 1)] = from.image_at(path);
    }
  }

  // ── L'aperçu de la ligne choisie ─────────────────────────────────────────
  //
  // ⚠ UN PORTAIL N'EST PAS UN PERSONNAGE : sa classe est une sentinelle, et
  // l'aperçu retombe alors sur le plan de la carte. Une classe qui s'est déjà
  // montrée sans image aussi : ce qu'on attend d'elle ne viendra pas.
  const bool can_show = from.sprite_at && from.navi != nullptr;
  for (std::size_t group = 0; group < entries_.size(); ++group) {
    for (std::size_t row = 0; row < entries_[group].size(); ++row) {
      ro::ui::NavigationEntryView& entry = entries_[group][row];
      entry.sprite = {};
      const std::uint32_t sprite_class =
          can_show ? SpriteClassOf(*from.navi, groups_[group].hits[row]) : 0;
      entry.sprite_expected = sprite_class != 0 && !spriteless_.contains(sprite_class);
    }
  }
  if (can_show && from.selected.has_value() && from.preview_side_px > 0) {
    if (const auto hit = HitAt(*from.selected)) {
      ro::ui::NavigationEntryView& entry = entries_[from.selected->group][from.selected->entry];
      if (entry.sprite_expected) {
        const std::uint32_t sprite_class = SpriteClassOf(*from.navi, *hit);
        entry.sprite = from.sprite_at(sprite_class, from.preview_side_px);
        if (!entry.sprite.usable()) {
          spriteless_.insert(sprite_class);
          entry.sprite_expected = false;
        }
      }
    }
  }

  // ── L'itinéraire ─────────────────────────────────────────────────────────
  step_labels_.clear();
  steps_.clear();
  const ro::sim::NaviGuide* guide = from.guide;
  if (guide != nullptr && from.navi != nullptr) {
    const NaviData& data = *from.navi;
    const auto& guide_steps = guide->steps();
    // Les passages d'abord, tous : la vue garde des `string_view` dessus.
    step_labels_.reserve(guide_steps.size());
    for (const ro::sim::NaviGuideStep& step : guide_steps) {
      const ro::formats::NaviLink* link = data.FindLink(step.link_id);
      std::string passage;
      if (link != nullptr && link->sprite_class != ro::formats::kNaviWarpPortalSprite &&
          !link->name.empty()) {
        passage = ro::core::Tr("Parler à");
        passage += ' ';
        passage += link->name;
      }
      step_labels_.push_back(std::move(passage));
    }
    for (std::size_t rank = 0; rank < guide_steps.size(); ++rank)
      steps_.push_back({.map_label = MapLabelOf(data, guide_steps[rank].from_map),
                        .passage = step_labels_[rank],
                        .current = rank == guide->current_step()});
    if (!guide_steps.empty() || guide->active())
      steps_.push_back({.map_label = MapLabelOf(data, guide->goal_map()),
                        .current = guide->current_step() >= guide_steps.size()});
    if (guide_steps.empty() && steps_.size() == 1) steps_.clear();
    const ro::sim::NaviGuideStatus status = guide->status();
    view_.route.guiding = guide->active();
    view_.route.no_path = status == ro::sim::NaviGuideStatus::kNoRoute;
    view_.route.already_here = status == ro::sim::NaviGuideStatus::kAlreadyHere;
    if (guide->active() && guide->estimated_cost() > 0)
      view_.route.distance_cells = static_cast<int>(guide->estimated_cost() / ro::sim::kMoveCost);
  }
  view_.route.steps = steps_;
  return view_;
}

}  // namespace moonclient
