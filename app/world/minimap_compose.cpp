#include "world/minimap_compose.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/sim/movement.h"
#include "ro/sim/quest_markers.h"

namespace moonclient {
namespace {

// Les images de la minimap, sous la racine des skins.
constexpr std::string_view kMapFolder = "map\\";
constexpr std::string_view kBitmapExtension = ".bmp";
constexpr std::string_view kArrowPath = "map\\map_arrow.bmp";
constexpr std::string_view kMemoPointPath = "minimap\\memopoint_1.bmp";
constexpr std::string_view kBossPath = "map\\bossmonster.bmp";
constexpr std::string_view kStoreSearchPath = "basic_interface\\search_store.bmp";
// Une marque de quête : l'icône de sa COULEUR, et la flèche de sa direction
// quand elle est collée au bord — gauche, droite, haut, bas, numérotées de un.
constexpr std::string_view kQuestIconPrefix = "basic_interface\\quest_";
// L'icône de la grande carte : la sorte de quête (l'effet), puis la couleur.
constexpr std::string_view kQuestKindPrefix = "minimap\\Quest_";
constexpr std::array<std::string_view, ro::ui::kMinimapQuestArrowCount> kQuestArrows = {
    "basic_interface\\quest_arrow1.bmp", "basic_interface\\quest_arrow2.bmp",
    "basic_interface\\quest_arrow3.bmp", "basic_interface\\quest_arrow4.bmp"};
constexpr std::array<std::string_view, ro::ui::kMinimapButtonFaceCount> kSettingsFaces = {
    "minimap\\i_viewon_1.bmp", "minimap\\i_viewon_2.bmp", "minimap\\i_viewon_3.bmp"};
// L'icône de chaque commodité, par sa nature, puis celle d'un portail.
constexpr std::array<std::string_view, ro::vfs::kTownFacilityCount> kFacilityIcons = {
    "information\\store.bmp",  "information\\weaponshop.bmp", "information\\armorshops.bmp",
    "information\\smithy.bmp", "information\\guide.bmp",      "information\\inn.bmp",
    "information\\kafra.bmp",  "information\\style.bmp"};
constexpr std::string_view kPortalIcon = "information\\warp.bmp";

// Le pas d'une boussole, en degrés, et les écarts de case de chaque direction —
// zéro au nord, en tournant vers l'ouest.
constexpr float kCompassStepDeg = 45.0F;
constexpr std::array<std::int8_t, ro::sim::kDirectionCount> kCompassDx = {0, -1, -1, -1,
                                                                          0, 1,  1,  1};
constexpr std::array<std::int8_t, ro::sim::kDirectionCount> kCompassDy = {1,  1,  0, -1,
                                                                          -1, -1, 0, 1};

// La palette des membres : des teintes franches, qui se distinguent sur les
// cartes pâles comme sur les sombres.
constexpr std::array<ImU32, 8> kMatePalette = {
    IM_COL32(255, 96, 96, 255),  IM_COL32(96, 200, 255, 255),  IM_COL32(120, 230, 120, 255),
    IM_COL32(255, 200, 64, 255), IM_COL32(210, 120, 255, 255), IM_COL32(255, 150, 210, 255),
    IM_COL32(64, 230, 210, 255), IM_COL32(255, 255, 255, 255)};

constexpr std::uint32_t kAlphaShift = 24;
constexpr std::uint32_t kRedShift = 16;
constexpr std::uint32_t kGreenShift = 8;
constexpr std::uint32_t kChannelMask = 0xFF;

[[nodiscard]] ImU32 FromArgb(std::uint32_t argb) noexcept {
  return IM_COL32((argb >> kRedShift) & kChannelMask, (argb >> kGreenShift) & kChannelMask,
                  argb & kChannelMask, (argb >> kAlphaShift) & kChannelMask);
}

// « prontera.gat » → « prontera ».
[[nodiscard]] std::string_view WithoutExtension(std::string_view map) noexcept {
  const std::size_t dot = map.find('.');
  return dot == std::string_view::npos ? map : map.substr(0, dot);
}

}  // namespace

float MinimapFacingDegrees(std::uint8_t compass_direction) noexcept {
  const std::uint8_t direction = compass_direction % ro::sim::kDirectionCount;
  return ro::sim::ClientAimDegrees(static_cast<float>(kCompassDx[direction]),
                                   static_cast<float>(kCompassDy[direction]));
}

ImU32 MinimapMateColor(std::size_t rank) noexcept {
  return kMatePalette[rank % kMatePalette.size()];
}

const ro::ui::MinimapView& MinimapComposer::Compose(const MinimapSources& from) {
  view_ = ro::ui::MinimapView{};
  if (from.world == nullptr) return view_;
  map_name_ = from.world->map_name();
  view_.map_name = map_name_;
  view_.display_name = from.display_name;
  view_.cells_width = from.cells_width;
  view_.cells_height = from.cells_height;
  if (const ro::sim::Actor* player = from.world->player(); player != nullptr) {
    const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(*player, from.now_ms);
    view_.player_cell_x = cell.x;
    view_.player_cell_y = cell.y;
    view_.player_facing_deg = MinimapFacingDegrees(ro::sim::FacingAt(*player, from.now_ms));
  }
  if (from.image_at) {
    std::string path(kMapFolder);
    path += map_name_;
    path += kBitmapExtension;
    view_.map_image = from.image_at(path);
    view_.arrow = from.image_at(kArrowPath);
    view_.memo_point = from.image_at(kMemoPointPath);
    view_.boss_icon = from.image_at(kBossPath);
    view_.store_search_icon = from.image_at(kStoreSearchPath);
    for (std::size_t face = 0; face < kSettingsFaces.size(); ++face)
      view_.settings_button[face] = from.image_at(kSettingsFaces[face]);
    for (std::size_t arrow = 0; arrow < kQuestArrows.size(); ++arrow)
      view_.quest_arrows[arrow] = from.image_at(kQuestArrows[arrow]);
  }

  ComposeTown(from);
  ComposeQuests(from);
  ComposeMates(from);
  ComposeBattleMates(from);
  ComposeRoute(from);

  viewpoints_.clear();
  if (from.viewpoints != nullptr) {
    for (const ro::sim::Viewpoint& point : from.viewpoints->viewpoints())
      viewpoints_.push_back({.cell = {point.cell_x, point.cell_y},
                             .color = FromArgb(point.color_argb),
                             .age_ms = from.now_ms - point.placed_ms});
  }
  view_.viewpoints = viewpoints_;

  memos_.clear();
  for (const NaviMemo& memo : from.memos)
    memos_.push_back({.cell = {memo.cell_x, memo.cell_y}, .name = memo.name});
  view_.memos = memos_;
  if (from.store_search != nullptr) {
    if (const std::optional<ro::sim::StoreSearchCell> cell = from.store_search->marker();
        cell.has_value()) {
      view_.store_search = ro::ui::MinimapCell{.x = cell->x, .y = cell->y};
    }
  }
  // LES TOMBES, telles que le carnet de chasse les a filtrées.
  view_.tombs = from.tombs;
  // LE BOSS, tant que le serveur en donne la case : mort, il n'en a plus.
  view_.boss.reset();
  if (from.boss.has_value()) {
    view_.boss = ro::ui::MinimapCell{.x = from.boss->x, .y = from.boss->y};
  }
  return view_;
}

void MinimapComposer::ComposeQuests(const MinimapSources& from) {
  quests_.clear();
  if (from.image_at) {
    // Une marque sans couleur ne se montre qu'au-dessus du PNJ, et une marque
    // retirée n'a plus de case.
    for (const auto& entry : from.world->quest_markers().all()) {
      const ro::sim::QuestMarker& marker = entry.second;
      if (marker.color == ro::sim::kQuestMarkerNoColor || !marker.placed()) continue;
      std::string path(kQuestIconPrefix);
      path += std::to_string(marker.color);
      path += kBitmapExtension;
      std::string kind(kQuestKindPrefix);
      kind += std::to_string(marker.effect);
      kind += '_';
      kind += std::to_string(marker.color);
      kind += kBitmapExtension;
      quests_.push_back({.cell = {marker.cell_x, marker.cell_y},
                         .image = from.image_at(path),
                         .kind_image = from.image_at(kind)});
    }
  }
  view_.quests = quests_;
}

void MinimapComposer::ComposeTown(const MinimapSources& from) {
  town_.clear();
  portals_.clear();
  labels_.clear();
  if (!from.image_at) {
    view_.town = town_;
    view_.portals = portals_;
    return;
  }
  // Les libellés d'abord, tous : la vue garde des `string_view` dessus, et une
  // réallocation au milieu les rendrait caducs.
  struct Pending {
    ro::ui::MinimapCell cell;
    std::string_view icon;
    bool portal = false;
  };
  std::vector<Pending> pending;
  if (from.towns != nullptr) {
    for (const ro::vfs::TownFacilityEntry& entry : from.towns->On(map_name_)) {
      labels_.push_back(entry.name);
      pending.push_back(
          {{entry.cell_x, entry.cell_y}, kFacilityIcons[static_cast<std::size_t>(entry.facility)]});
    }
  }
  // Les PORTAILS du graphe : le client d'origine ne les montre que sur sa
  // grande carte. Ici ils sont sur la minimap, libellés de leur destination,
  // et un clic y guide.
  if (from.navi != nullptr) {
    const ro::formats::NaviMapIndex here = from.navi->FindMap(map_name_);
    for (const std::uint32_t rank : from.navi->LinksLeaving(here)) {
      const ro::formats::NaviLink& link = from.navi->links[rank];
      if (link.sprite_class != ro::formats::kNaviWarpPortalSprite) continue;
      std::string label(ro::core::Tr("Vers"));
      label += ' ';
      label += link.dest_map < from.navi->maps.size() ? from.navi->maps[link.dest_map].display_name
                                                      : std::string();
      labels_.push_back(std::move(label));
      pending.push_back({{link.cell_x, link.cell_y}, kPortalIcon, /*portal=*/true});
    }
  }
  // Les deux listes se partagent les libellés : les commodités d'abord, puis
  // les portails, dans l'ordre où ils ont été rangés.
  for (std::size_t rank = 0; rank < pending.size(); ++rank) {
    std::vector<ro::ui::MinimapTownIcon>& into = pending[rank].portal ? portals_ : town_;
    into.push_back({.cell = pending[rank].cell,
                    .image = from.image_at(pending[rank].icon),
                    .label = labels_[rank]});
  }
  view_.town = town_;
  view_.portals = portals_;
}

void MinimapComposer::ComposeMates(const MinimapSources& from) {
  party_.clear();
  guild_.clear();
  const std::uint32_t self = from.world->player_id();
  if (from.party != nullptr) {
    std::size_t rank = 0;
    for (const ro::sim::PartyMember& member : from.party->members()) {
      const std::size_t colour = rank++;
      if (member.account_id == self || !member.position.has_value()) continue;
      if (WithoutExtension(member.map) != map_name_) continue;
      party_.push_back(
          {.cell = {member.position->x, member.position->y}, .color = MinimapMateColor(colour)});
    }
  }
  if (from.guild != nullptr) {
    std::size_t rank = 0;
    for (const auto& [account_id, cell] : from.guild->member_cells()) {
      const std::size_t colour = rank++;
      if (account_id == self) continue;
      guild_.push_back({.cell = {cell.x, cell.y}, .color = MinimapMateColor(colour)});
    }
  }
  view_.party = party_;
  view_.guild = guild_;
}

void MinimapComposer::ComposeBattleMates(const MinimapSources& from) {
  battle_mate_names_.clear();
  battle_mates_.clear();
  if (!from.battle_mates_shown) {
    view_.battle_mates = battle_mates_;
    return;
  }
  // Les noms d'abord, tous : la vue garde des `string_view` dessus, et une
  // réallocation au milieu les rendrait caducs.
  const std::uint32_t self = from.world->player_id();
  const auto& mates = from.world->battlefield().mates();
  for (const auto& [account_id, mate] : mates) {
    if (account_id != self) battle_mate_names_.push_back(mate.name);
  }
  std::size_t rank = 0;
  for (const auto& [account_id, mate] : mates) {
    if (account_id == self) continue;
    battle_mates_.push_back({.cell = {mate.cell.x, mate.cell.y},
                             .color = from.battle_mate_color,
                             .name = battle_mate_names_[rank++]});
  }
  view_.battle_mates = battle_mates_;
}

void MinimapComposer::ComposeRoute(const MinimapSources& from) {
  route_.clear();
  if (from.guide == nullptr || !from.guide->active()) {
    view_.route_path = route_;
    return;
  }
  for (const ro::sim::CellPosition& corner : from.guide->path_corners())
    route_.push_back({corner.x, corner.y});
  view_.route_path = route_;
  if (route_.size() < 2) {
    if (const auto target = from.guide->local_target())
      view_.route_exit = ro::ui::MinimapCell{target->x, target->y};
  }
}

}  // namespace moonclient
