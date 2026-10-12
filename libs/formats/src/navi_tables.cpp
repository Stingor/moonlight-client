#include "ro/formats/navi_tables.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "ro/core/ascii.h"

#include "navi_cursor.h"

namespace ro::formats {

namespace {

using navi_internal::Cursor;
using navi_internal::FieldKey;
using navi_internal::ForEachField;
using navi_internal::ForEachItem;
using navi_internal::Narrow;

constexpr std::int64_t kSchemaVersion = 1;

// Le décalage et les masques des statistiques empaquetées d'une apparition.
constexpr std::uint32_t kElementShift = 16;
constexpr std::uint32_t kElementMask = 0xFFFF;
constexpr std::uint32_t kElementsPerLevel = 20;
constexpr std::uint32_t kSizeShift = 8;
constexpr std::uint32_t kByteMask = 0xFF;

// Une suite : [première liaison, pas] ou [première liaison, pas, nombre].
constexpr std::size_t kPassFields = 2;
constexpr std::size_t kRunFields = 3;

constexpr std::string_view kTrue = "true";
constexpr std::string_view kFalse = "false";

struct NamedKind {
  std::string_view word;
  std::uint16_t value;
};

constexpr std::array kMapKinds{
    NamedKind{"plain", navi_map_kind::kPlain},
    NamedKind{"airport", navi_map_kind::kAirport},
    NamedKind{"segmented", navi_map_kind::kSegmented},
    NamedKind{"segmented_with_monsters", navi_map_kind::kSegmentedWithMonsters},
};

constexpr std::array kLinkKinds{
    NamedKind{"portal", navi_link_kind::kWarpPortal},
    NamedKind{"npc", navi_link_kind::kScriptedNpc},
    NamedKind{"kafra_dungeon", navi_link_kind::kKafraDungeonWarp},
    NamedKind{"event_dungeon", navi_link_kind::kEventDungeonWarp},
    NamedKind{"service", navi_link_kind::kTransportService},
    NamedKind{"airport", navi_link_kind::kAirport},
};

// Un champ ne se lit qu'une fois, et seulement s'il se lit.
template <typename T>
[[nodiscard]] bool Once(std::optional<T>& slot, std::optional<T> value) {
  if (slot.has_value() || !value.has_value()) return false;
  slot = std::move(value);
  return true;
}

template <std::size_t N>
[[nodiscard]] std::optional<std::uint16_t> KindOf(Cursor& cursor,
                                                  const std::array<NamedKind, N>& kinds) {
  const auto word = cursor.Word();
  if (!word) return std::nullopt;
  for (const NamedKind& kind : kinds)
    if (kind.word == *word) return kind.value;
  return std::nullopt;
}

[[nodiscard]] std::optional<bool> Flag(Cursor& cursor) {
  const auto word = cursor.Word();
  if (word == kTrue) return true;
  if (word == kFalse) return false;
  return std::nullopt;
}

// Une carte nommée par la section des cartes ; rien sinon.
[[nodiscard]] std::optional<NaviMapIndex> MapOf(Cursor& cursor, const NaviData& data) {
  const auto name = cursor.Quoted();
  if (!name) return std::nullopt;
  const NaviMapIndex found = data.FindMap(*name);
  if (found == kNoNaviMap) return std::nullopt;
  return found;
}

// `<section>: { <clé>: { … }, … }`, chaque entrée confiée à `read_entry`.
template <typename ReadEntry>
[[nodiscard]] core::Result<void> ReadSection(Cursor& cursor, std::string_view section,
                                             ReadEntry&& read_entry) {
  const auto key = FieldKey(cursor);
  if (key != section)
    return std::unexpected(cursor.Refused("section « " + std::string(section) + " » attendue"));
  if (!ForEachItem(cursor, '{', '}', read_entry))
    return std::unexpected(
        cursor.Refused("entrée illisible dans « " + std::string(section) + " »"));
  return {};
}

// Un identifiant suivi de ses deux-points, jamais deux fois dans une section.
[[nodiscard]] std::optional<std::uint32_t> EntryId(Cursor& cursor,
                                                   std::unordered_set<std::uint32_t>& seen) {
  const auto id = Narrow<std::uint32_t>(cursor.Integer());
  if (!id || !cursor.Take(':') || !seen.insert(*id).second) return std::nullopt;
  return id;
}

// Une carte, jamais deux fois sous le même nom, à la casse près.
[[nodiscard]] bool ReadMap(Cursor& cursor, NaviData& data,
                           std::unordered_set<std::string>& seen_names) {
  auto internal = cursor.Quoted();
  if (!internal || internal->empty() || !cursor.Take(':') ||
      !seen_names.insert(core::AsciiLower(*internal)).second)
    return false;
  std::optional<std::string> shown;
  std::optional<std::uint16_t> kind;
  std::optional<std::uint16_t> width;
  std::optional<std::uint16_t> height;
  const bool read = ForEachField(cursor, [&](std::string_view key) {
    if (key == "name") return Once(shown, cursor.Quoted());
    if (key == "kind") return Once(kind, KindOf(cursor, kMapKinds));
    if (key == "width") return Once(width, Narrow<std::uint16_t>(cursor.Integer()));
    if (key == "height") return Once(height, Narrow<std::uint16_t>(cursor.Integer()));
    return false;
  });
  if (!read || !shown || !kind || !width || !height || data.maps.size() >= kNoNaviMap) return false;
  if (shown->empty()) shown = *internal;
  data.maps.push_back({.name = std::move(*internal),
                       .display_name = std::move(*shown),
                       .map_kind = *kind,
                       .width_cells = *width,
                       .height_cells = *height});
  return true;
}

[[nodiscard]] bool ReadNpc(Cursor& cursor, NaviData& data,
                           std::unordered_set<std::uint32_t>& seen) {
  const auto id = EntryId(cursor, seen);
  if (!id) return false;
  std::optional<NaviMapIndex> map;
  std::optional<bool> shop;
  std::optional<std::int32_t> sprite;
  std::optional<std::string> name;
  std::optional<std::int16_t> x;
  std::optional<std::int16_t> y;
  const bool read = ForEachField(cursor, [&](std::string_view key) {
    if (key == "map") return Once(map, MapOf(cursor, data));
    if (key == "shop") return Once(shop, Flag(cursor));
    if (key == "sprite") return Once(sprite, Narrow<std::int32_t>(cursor.Integer()));
    if (key == "name") return Once(name, cursor.Quoted());
    if (key == "x") return Once(x, Narrow<std::int16_t>(cursor.Integer()));
    if (key == "y") return Once(y, Narrow<std::int16_t>(cursor.Integer()));
    return false;
  });
  if (!read || !map || !sprite || !name || !x || !y) return false;
  data.npcs.push_back({.map = *map,
                       .npc_id = *id,
                       .subtype = shop.value_or(false) ? navi_subtype::kShop : navi_subtype::kNpc,
                       .sprite_class = *sprite,
                       .name = std::move(*name),
                       .cell_x = *x,
                       .cell_y = *y});
  return true;
}

[[nodiscard]] bool ReadSpawn(Cursor& cursor, NaviData& data,
                             std::unordered_set<std::uint32_t>& seen) {
  const auto id = EntryId(cursor, seen);
  if (!id) return false;
  std::optional<NaviMapIndex> map;
  std::optional<bool> boss;
  std::optional<std::uint16_t> amount;
  std::optional<std::uint16_t> sprite;
  std::optional<std::string> name;
  std::optional<std::string> sprite_name;
  std::optional<std::int32_t> level;
  std::optional<std::uint32_t> element;
  std::optional<std::uint32_t> element_level;
  std::optional<std::uint32_t> size;
  std::optional<std::uint32_t> race;
  const bool read = ForEachField(cursor, [&](std::string_view key) {
    if (key == "map") return Once(map, MapOf(cursor, data));
    if (key == "boss") return Once(boss, Flag(cursor));
    if (key == "amount") return Once(amount, Narrow<std::uint16_t>(cursor.Integer()));
    if (key == "sprite") return Once(sprite, Narrow<std::uint16_t>(cursor.Integer()));
    if (key == "name") return Once(name, cursor.Quoted());
    if (key == "sprite_name") return Once(sprite_name, cursor.Quoted());
    if (key == "level") return Once(level, Narrow<std::int32_t>(cursor.Integer()));
    if (key == "element") return Once(element, Narrow<std::uint32_t>(cursor.Integer()));
    if (key == "element_level") return Once(element_level, Narrow<std::uint32_t>(cursor.Integer()));
    if (key == "size") return Once(size, Narrow<std::uint32_t>(cursor.Integer()));
    if (key == "race") return Once(race, Narrow<std::uint32_t>(cursor.Integer()));
    return false;
  });
  if (!read || !map || !amount || !sprite || !name || !sprite_name || !level || !element ||
      !element_level || !size || !race)
    return false;
  const std::uint64_t element_code =
      std::uint64_t{*element_level} * kElementsPerLevel + std::uint64_t{*element};
  if (*element >= kElementsPerLevel || element_code > kElementMask || *size > kByteMask ||
      *race > kByteMask)
    return false;
  data.spawns.push_back(
      {.map = *map,
       .spawn_id = *id,
       .subtype = boss.value_or(false) ? navi_subtype::kBossSpawn : navi_subtype::kSpawn,
       .amount = *amount,
       .sprite_class = *sprite,
       .name = std::move(*name),
       .sprite_name = std::move(*sprite_name),
       .level = *level,
       .packed_stats = static_cast<std::uint32_t>(element_code << kElementShift) |
                       (*size << kSizeShift) | *race});
  return true;
}

[[nodiscard]] bool ReadLink(Cursor& cursor, NaviData& data,
                            std::unordered_set<std::uint32_t>& seen) {
  const auto id = EntryId(cursor, seen);
  if (!id) return false;
  std::optional<NaviMapIndex> map;
  std::optional<std::uint16_t> kind;
  std::optional<std::int32_t> sprite;
  std::optional<std::string> name;
  std::optional<std::int16_t> x;
  std::optional<std::int16_t> y;
  std::optional<NaviMapIndex> to;
  std::optional<std::int16_t> to_x;
  std::optional<std::int16_t> to_y;
  const bool read = ForEachField(cursor, [&](std::string_view key) {
    if (key == "map") return Once(map, MapOf(cursor, data));
    if (key == "kind") return Once(kind, KindOf(cursor, kLinkKinds));
    if (key == "sprite") return Once(sprite, Narrow<std::int32_t>(cursor.Integer()));
    if (key == "name") return Once(name, cursor.Quoted());
    if (key == "x") return Once(x, Narrow<std::int16_t>(cursor.Integer()));
    if (key == "y") return Once(y, Narrow<std::int16_t>(cursor.Integer()));
    if (key == "to") return Once(to, MapOf(cursor, data));
    if (key == "to_x") return Once(to_x, Narrow<std::int16_t>(cursor.Integer()));
    if (key == "to_y") return Once(to_y, Narrow<std::int16_t>(cursor.Integer()));
    return false;
  });
  if (!read || !map || !kind || !sprite || !name || !x || !y || !to || !to_x || !to_y) return false;
  data.links.push_back({.map = *map,
                        .link_id = *id,
                        .kind = *kind,
                        .sprite_class = *sprite,
                        .name = std::move(*name),
                        .cell_x = *x,
                        .cell_y = *y,
                        .dest_map = *to,
                        .dest_cell_x = *to_x,
                        .dest_cell_y = *to_y});
  return true;
}

// `<liaison>: [[première, pas], [première, pas, nombre], …]`. La liaison et
// chacune des liaisons d'une suite doivent être connues.
[[nodiscard]] bool ReadPasses(Cursor& cursor, NaviData& data,
                              std::unordered_set<std::uint32_t>& seen) {
  const auto owner = EntryId(cursor, seen);
  if (!owner || data.FindLink(*owner) == nullptr) return false;
  NaviPassSpan span{static_cast<std::uint32_t>(data.link_passes.size()), 0};
  const bool read = ForEachItem(cursor, '[', ']', [&] {
    std::array<std::int64_t, kRunFields> fields{};
    std::size_t count = 0;
    const bool listed = ForEachItem(cursor, '[', ']', [&] {
      const auto value = cursor.Integer();
      if (!value || count >= kRunFields) return false;
      fields[count++] = *value;
      return true;
    });
    if (!listed || count < kPassFields) return false;
    const auto first = Narrow<std::uint32_t>(fields[0]);
    const auto steps = Narrow<std::int32_t>(fields[1]);
    const std::optional<std::uint32_t> length =
        count == kRunFields ? Narrow<std::uint32_t>(fields[2]) : std::optional<std::uint32_t>(1);
    if (!first || !steps || !length || *length == 0) return false;
    for (std::uint32_t offset = 0; offset < *length; ++offset)
      if (data.FindLink(*first + offset) == nullptr) return false;
    data.link_passes.push_back({*first, *length, *steps});
    ++span.count;
    return true;
  });
  if (!read || span.count == 0) return false;
  data.link_pass_spans.emplace(*owner, span);
  return true;
}

}  // namespace

// ── Statistiques empaquetées ─────────────────────────────────────────────────

std::uint32_t NaviSpawn::element() const noexcept {
  return ((packed_stats >> kElementShift) & kElementMask) % kElementsPerLevel;
}

std::uint32_t NaviSpawn::element_level() const noexcept {
  return ((packed_stats >> kElementShift) & kElementMask) / kElementsPerLevel;
}

std::uint32_t NaviSpawn::size() const noexcept { return (packed_stats >> kSizeShift) & kByteMask; }

std::uint32_t NaviSpawn::race() const noexcept { return packed_stats & kByteMask; }

// ── Le lecteur ───────────────────────────────────────────────────────────────

core::Result<NaviData> ReadNaviTable(std::span<const std::uint8_t> bytes) {
  Cursor cursor(bytes);
  NaviData data;

  const auto version_key = FieldKey(cursor);
  const auto version = cursor.Integer();
  if (version_key != "version" || version != kSchemaVersion)
    return std::unexpected(
        cursor.Refused("version " + std::to_string(kSchemaVersion) + " attendue"));
  const auto exact_key = FieldKey(cursor);
  const auto exact = Flag(cursor);
  if (exact_key != "exact_steps" || !exact)
    return std::unexpected(cursor.Refused("exact_steps attendu"));
  data.exact_steps = *exact;

  // Les cartes d'abord : les autres sections les nomment.
  std::unordered_set<std::string> map_names;
  if (auto read = ReadSection(cursor, "maps", [&] { return ReadMap(cursor, data, map_names); });
      !read)
    return std::unexpected(read.error());
  data.BuildIndexes();

  std::unordered_set<std::uint32_t> seen;
  if (auto read = ReadSection(cursor, "npcs", [&] { return ReadNpc(cursor, data, seen); }); !read)
    return std::unexpected(read.error());
  seen.clear();
  if (auto read = ReadSection(cursor, "spawns", [&] { return ReadSpawn(cursor, data, seen); });
      !read)
    return std::unexpected(read.error());
  seen.clear();
  if (auto read = ReadSection(cursor, "links", [&] { return ReadLink(cursor, data, seen); }); !read)
    return std::unexpected(read.error());
  // Les suites désignent les liaisons par leur identifiant.
  data.BuildIndexes();
  seen.clear();
  if (auto read = ReadSection(cursor, "passes", [&] { return ReadPasses(cursor, data, seen); });
      !read)
    return std::unexpected(read.error());
  if (!cursor.AtEnd()) return std::unexpected(cursor.Refused("texte après la dernière section"));
  data.BuildIndexes();
  return data;
}

// ── Index ────────────────────────────────────────────────────────────────────

NaviMapIndex NaviData::FindMap(std::string_view internal_name) const {
  const auto found = map_by_lower_name_.find(core::AsciiLower(internal_name));
  return found == map_by_lower_name_.end() ? kNoNaviMap : found->second;
}

std::optional<std::uint32_t> NaviData::LinkSlot(std::uint32_t link_id) const {
  if (link_id >= first_link_id_ && link_id - first_link_id_ < link_slot_dense_.size()) {
    const std::uint32_t slot = link_slot_dense_[link_id - first_link_id_];
    if (slot != kNoSlot) return slot;
    return std::nullopt;
  }
  const auto found = link_slot_by_id_.find(link_id);
  if (found == link_slot_by_id_.end()) return std::nullopt;
  return found->second;
}

const NaviLink* NaviData::FindLink(std::uint32_t link_id) const {
  const auto slot = LinkSlot(link_id);
  return slot ? &links[*slot] : nullptr;
}

const NaviNpc* NaviData::FindNpc(std::uint32_t npc_id) const {
  const auto found = npc_slot_by_id_.find(npc_id);
  return found == npc_slot_by_id_.end() ? nullptr : &npcs[found->second];
}

namespace {

[[nodiscard]] std::span<const std::uint32_t> RanksOf(
    const std::vector<std::vector<std::uint32_t>>& by_map, NaviMapIndex map) {
  if (map >= by_map.size()) return {};
  return by_map[map];
}

}  // namespace

std::span<const std::uint32_t> NaviData::LinksLeaving(NaviMapIndex map) const {
  return RanksOf(links_leaving_, map);
}

std::span<const std::uint32_t> NaviData::NpcsOn(NaviMapIndex map) const {
  return RanksOf(npcs_on_, map);
}

std::span<const std::uint32_t> NaviData::SpawnsOn(NaviMapIndex map) const {
  return RanksOf(spawns_on_, map);
}

std::span<const NaviPassRun> NaviData::PassesAfter(std::uint32_t link_id) const {
  const auto found = link_pass_spans.find(link_id);
  if (found == link_pass_spans.end()) return {};
  return std::span<const NaviPassRun>(link_passes)
      .subspan(found->second.first, found->second.count);
}

void NaviData::BuildIndexes() {
  map_by_lower_name_.clear();
  for (std::size_t rank = 0; rank < maps.size(); ++rank)
    map_by_lower_name_.emplace(core::AsciiLower(maps[rank].name), static_cast<NaviMapIndex>(rank));

  link_slot_by_id_.clear();
  npc_slot_by_id_.clear();
  links_leaving_.assign(maps.size(), {});
  npcs_on_.assign(maps.size(), {});
  spawns_on_.assign(maps.size(), {});

  // Le serveur numérote ses liaisons d'affilée : un tableau direct, indexé par
  // l'écart au plus petit identifiant, sert la question que le pathfinder pose
  // des centaines de milliers de fois. Une numérotation trop éparse retombe sur
  // la table de hachage.
  link_slot_dense_.clear();
  first_link_id_ = 0;
  if (!links.empty()) {
    const auto [lowest, highest] = std::minmax_element(
        links.begin(), links.end(),
        [](const NaviLink& left, const NaviLink& right) { return left.link_id < right.link_id; });
    const std::uint64_t span = std::uint64_t{highest->link_id} - lowest->link_id + 1;
    if (span <= kMaxDenseLinkSpan) {
      first_link_id_ = lowest->link_id;
      link_slot_dense_.assign(static_cast<std::size_t>(span), kNoSlot);
    }
  }
  for (std::size_t slot = 0; slot < links.size(); ++slot) {
    const std::uint32_t id = links[slot].link_id;
    if (!link_slot_dense_.empty())
      link_slot_dense_[id - first_link_id_] = static_cast<std::uint32_t>(slot);
    else
      link_slot_by_id_[id] = static_cast<std::uint32_t>(slot);
    if (links[slot].map < links_leaving_.size())
      links_leaving_[links[slot].map].push_back(static_cast<std::uint32_t>(slot));
  }
  for (std::size_t slot = 0; slot < npcs.size(); ++slot) {
    npc_slot_by_id_[npcs[slot].npc_id] = static_cast<std::uint32_t>(slot);
    if (npcs[slot].map < npcs_on_.size())
      npcs_on_[npcs[slot].map].push_back(static_cast<std::uint32_t>(slot));
  }
  for (std::size_t slot = 0; slot < spawns.size(); ++slot)
    if (spawns[slot].map < spawns_on_.size())
      spawns_on_[spawns[slot].map].push_back(static_cast<std::uint32_t>(slot));
}

}  // namespace ro::formats
