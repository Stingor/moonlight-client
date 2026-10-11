#include "ro/formats/navi_tables.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/text.h"

namespace ro::formats {

namespace {

// Le décalage et les masques des statistiques empaquetées d'une apparition.
constexpr std::uint32_t kElementShift = 16;
constexpr std::uint32_t kElementMask = 0xFFFF;
constexpr std::uint32_t kElementsPerLevel = 20;
constexpr std::uint32_t kSizeShift = 8;
constexpr std::uint32_t kByteMask = 0xFF;
// Le quatrième champ d'une apparition : quantité en haut, sprite en bas.
constexpr std::uint32_t kAmountShift = 16;
constexpr std::uint32_t kSpriteMask = 0xFFFF;

// Les marqueurs de fin d'une liste de distances : `{"NULL", 0, 0}` entre
// liaisons, `{"", 0, 0}` vers les PNJ.
constexpr std::string_view kLinkPassEnd = "NULL";
constexpr std::string_view kLinkPassTag = "E";

// ── Le curseur ───────────────────────────────────────────────────────────────
//
// Un lecteur de la grammaire commune aux six tables : des accolades, des
// virgules, des chaînes entre guillemets doubles, des entiers, et des
// commentaires `--` jusqu'à la fin de la ligne.
class Cursor {
 public:
  explicit Cursor(std::span<const std::uint8_t> bytes)
      : text_(reinterpret_cast<const char*>(bytes.data()), bytes.size()) {}

  void SkipBlanks() {
    while (at_ < text_.size()) {
      const char c = text_[at_];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        ++at_;
      } else if (c == '-' && at_ + 1 < text_.size() && text_[at_ + 1] == '-') {
        while (at_ < text_.size() && text_[at_] != '\n') ++at_;
      } else {
        return;
      }
    }
  }

  [[nodiscard]] bool AtEnd() {
    SkipBlanks();
    return at_ >= text_.size();
  }

  [[nodiscard]] bool Peek(char wanted) {
    SkipBlanks();
    return at_ < text_.size() && text_[at_] == wanted;
  }

  [[nodiscard]] bool Take(char wanted) {
    if (!Peek(wanted)) return false;
    ++at_;
    return true;
  }

  // Une virgule entre deux éléments est facultative avant une accolade fermante :
  // Lua l'accepte en fin de liste, et le serveur l'écrit.
  void TakeOptionalComma() { (void)Take(','); }

  [[nodiscard]] std::optional<std::string_view> Name() {
    SkipBlanks();
    const std::size_t start = at_;
    while (at_ < text_.size()) {
      const char c = text_[at_];
      const bool word =
          (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
      if (!word) break;
      ++at_;
    }
    if (at_ == start) return std::nullopt;
    return text_.substr(start, at_ - start);
  }

  // Une chaîne entre guillemets doubles. Les échappements usuels sont défaits ;
  // le serveur n'en écrit pas, mais un nom de PNJ peut porter un guillemet.
  [[nodiscard]] std::optional<std::string> String() {
    if (!Take('"')) return std::nullopt;
    std::string out;
    while (at_ < text_.size()) {
      const char c = text_[at_++];
      if (c == '"') return out;
      if (c == '\\' && at_ < text_.size()) {
        const char escaped = text_[at_++];
        out.push_back(escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped);
        continue;
      }
      if (c == '\n') return std::nullopt;
      out.push_back(c);
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<std::int64_t> Integer() {
    SkipBlanks();
    bool negative = false;
    if (at_ < text_.size() && text_[at_] == '-') {
      negative = true;
      ++at_;
    }
    const std::size_t start = at_;
    std::int64_t value = 0;
    constexpr std::int64_t kCeiling = std::numeric_limits<std::int32_t>::max();
    while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') {
      value = value * 10 + (text_[at_] - '0');
      if (value > kCeiling) return std::nullopt;
      ++at_;
    }
    if (at_ == start) return std::nullopt;
    return negative ? -value : value;
  }

  [[nodiscard]] std::size_t offset() const noexcept { return at_; }

 private:
  std::string_view text_;
  std::size_t at_ = 0;
};

[[nodiscard]] std::unexpected<core::Error> Broken(std::string_view table, const Cursor& cursor,
                                                  std::string_view what) {
  return core::Fail(core::ErrorCode::kCorruptData, std::string(table) + " : " + std::string(what) +
                                                       " à l'octet " +
                                                       std::to_string(cursor.offset()));
}

// `Nom = {` — l'en-tête de chaque fichier.
[[nodiscard]] bool OpenTable(Cursor& cursor, std::string_view expected_name) {
  const auto name = cursor.Name();
  return name && *name == expected_name && cursor.Take('=') && cursor.Take('{');
}

// Une valeur bornée à un type étroit. Hors bornes, la ligne est illisible.
template <typename T>
[[nodiscard]] std::optional<T> Narrow(std::optional<std::int64_t> value) {
  if (!value || *value < std::numeric_limits<T>::min() || *value > std::numeric_limits<T>::max())
    return std::nullopt;
  return static_cast<T>(*value);
}

// Lit `, <entier>` : la virgule qui SÉPARE un champ du précédent.
[[nodiscard]] std::optional<std::int64_t> NextInteger(Cursor& cursor) {
  if (!cursor.Take(',')) return std::nullopt;
  return cursor.Integer();
}

[[nodiscard]] std::optional<std::string> NextString(Cursor& cursor) {
  if (!cursor.Take(',')) return std::nullopt;
  return cursor.String();
}

// Les libellés sont écrits dans l'encodage des scripts du serveur. Le fil sait
// déjà reconnaître l'UTF-8 du 1252 ; on s'en remet à lui.
[[nodiscard]] std::string Readable(const std::string& raw) { return core::Utf8FromWire(raw); }

// Parcourt une liste `{ {…}, {…}, … }` et confie chaque entrée à `read_entry`,
// qui lit ce qu'il y a ENTRE ses accolades. Rend faux au premier accroc.
template <typename ReadEntry>
[[nodiscard]] bool ForEachEntry(Cursor& cursor, ReadEntry&& read_entry) {
  while (!cursor.Peek('}')) {
    if (!cursor.Take('{')) return false;
    if (!read_entry()) return false;
    if (!cursor.Take('}')) return false;
    cursor.TakeOptionalComma();
  }
  return cursor.Take('}');
}

// Un fichier lu entièrement : la table fermée, et rien derrière.
[[nodiscard]] bool ClosedAtEnd(Cursor& cursor) { return cursor.AtEnd(); }

// Un lecteur qui écrit dans une COPIE, rendue à `data` seulement s'il réussit :
// un fichier refusé ne laisse pas une table à moitié remplie.
template <typename T, typename Read>
[[nodiscard]] core::Result<void> Replacing(std::vector<T>& target, Read&& read) {
  std::vector<T> fresh;
  auto result = read(fresh);
  if (!result) return result;
  target = std::move(fresh);
  return {};
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

// ── Lecteurs ─────────────────────────────────────────────────────────────────

core::Result<void> ReadNaviMaps(std::span<const std::uint8_t> bytes, NaviData& data) {
  constexpr std::string_view kTable = "Navi_Map";
  auto result = Replacing(data.maps, [&](std::vector<NaviMap>& maps) -> core::Result<void> {
    Cursor cursor(bytes);
    if (!OpenTable(cursor, kTable)) return Broken(kTable, cursor, "en-tête attendu");
    const bool read = ForEachEntry(cursor, [&] {
      NaviMap map;
      auto name = cursor.String();
      auto shown = NextString(cursor);
      const auto kind = Narrow<std::uint16_t>(NextInteger(cursor));
      const auto width = Narrow<std::uint16_t>(NextInteger(cursor));
      const auto height = Narrow<std::uint16_t>(NextInteger(cursor));
      if (!name || !shown || !kind || !width || !height) return false;
      map.name = std::move(*name);
      map.display_name = shown->empty() ? map.name : Readable(*shown);
      map.map_kind = *kind;
      map.width_cells = *width;
      map.height_cells = *height;
      maps.push_back(std::move(map));
      return maps.size() < kNoNaviMap;
    });
    if (!read || !ClosedAtEnd(cursor)) return Broken(kTable, cursor, "ligne illisible");
    return {};
  });
  // Les tables suivantes résolvent leurs noms de carte contre celle-ci.
  if (result) data.BuildIndexes();
  return result;
}

core::Result<void> ReadNaviNpcs(std::span<const std::uint8_t> bytes, NaviData& data) {
  constexpr std::string_view kTable = "Navi_Npc";
  std::size_t orphans = 0;
  auto result = Replacing(data.npcs, [&](std::vector<NaviNpc>& npcs) -> core::Result<void> {
    Cursor cursor(bytes);
    if (!OpenTable(cursor, kTable)) return Broken(kTable, cursor, "en-tête attendu");
    const bool read = ForEachEntry(cursor, [&] {
      const auto map = cursor.String();
      const auto id = Narrow<std::uint32_t>(NextInteger(cursor));
      const auto subtype = Narrow<std::uint16_t>(NextInteger(cursor));
      const auto sprite = Narrow<std::int32_t>(NextInteger(cursor));
      const auto name = NextString(cursor);
      const auto unique = NextString(cursor);
      const auto x = Narrow<std::int16_t>(NextInteger(cursor));
      const auto y = Narrow<std::int16_t>(NextInteger(cursor));
      if (!map || !id || !subtype || !sprite || !name || !unique || !x || !y) return false;
      const NaviMapIndex where = data.FindMap(*map);
      if (where == kNoNaviMap) {
        ++orphans;
        return true;
      }
      NaviNpc npc;
      npc.map = where;
      npc.npc_id = *id;
      npc.subtype = *subtype;
      npc.sprite_class = *sprite;
      npc.name = Readable(*name);
      npc.cell_x = *x;
      npc.cell_y = *y;
      npcs.push_back(std::move(npc));
      return true;
    });
    if (!read || !ClosedAtEnd(cursor)) return Broken(kTable, cursor, "ligne illisible");
    return {};
  });
  if (result) {
    data.skipped.npcs_on_unknown_map = orphans;
    data.BuildIndexes();
  }
  return result;
}

core::Result<void> ReadNaviSpawns(std::span<const std::uint8_t> bytes, NaviData& data) {
  constexpr std::string_view kTable = "Navi_Mob";
  std::size_t orphans = 0;
  auto result = Replacing(data.spawns, [&](std::vector<NaviSpawn>& spawns) -> core::Result<void> {
    Cursor cursor(bytes);
    if (!OpenTable(cursor, kTable)) return Broken(kTable, cursor, "en-tête attendu");
    const bool read = ForEachEntry(cursor, [&] {
      const auto map = cursor.String();
      const auto id = Narrow<std::uint32_t>(NextInteger(cursor));
      const auto subtype = Narrow<std::uint16_t>(NextInteger(cursor));
      const auto packed = Narrow<std::uint32_t>(NextInteger(cursor));
      const auto name = NextString(cursor);
      const auto sprite_name = NextString(cursor);
      const auto level = Narrow<std::int32_t>(NextInteger(cursor));
      const auto stats = Narrow<std::uint32_t>(NextInteger(cursor));
      if (!map || !id || !subtype || !packed || !name || !sprite_name || !level || !stats)
        return false;
      const NaviMapIndex where = data.FindMap(*map);
      if (where == kNoNaviMap) {
        ++orphans;
        return true;
      }
      NaviSpawn spawn;
      spawn.map = where;
      spawn.spawn_id = *id;
      spawn.subtype = *subtype;
      spawn.amount = static_cast<std::uint16_t>(*packed >> kAmountShift);
      spawn.sprite_class = static_cast<std::uint16_t>(*packed & kSpriteMask);
      spawn.name = Readable(*name);
      spawn.sprite_name = std::move(*sprite_name);
      spawn.level = *level;
      spawn.packed_stats = *stats;
      spawns.push_back(std::move(spawn));
      return true;
    });
    if (!read || !ClosedAtEnd(cursor)) return Broken(kTable, cursor, "ligne illisible");
    return {};
  });
  if (result) {
    data.skipped.spawns_on_unknown_map = orphans;
    data.BuildIndexes();
  }
  return result;
}

core::Result<void> ReadNaviLinks(std::span<const std::uint8_t> bytes, NaviData& data) {
  constexpr std::string_view kTable = "Navi_Link";
  std::size_t orphans = 0;
  auto result = Replacing(data.links, [&](std::vector<NaviLink>& links) -> core::Result<void> {
    Cursor cursor(bytes);
    if (!OpenTable(cursor, kTable)) return Broken(kTable, cursor, "en-tête attendu");
    const bool read = ForEachEntry(cursor, [&] {
      const auto map = cursor.String();
      const auto id = Narrow<std::uint32_t>(NextInteger(cursor));
      const auto kind = Narrow<std::uint16_t>(NextInteger(cursor));
      const auto sprite = Narrow<std::int32_t>(NextInteger(cursor));
      const auto name = NextString(cursor);
      const auto unique = NextString(cursor);
      const auto x = Narrow<std::int16_t>(NextInteger(cursor));
      const auto y = Narrow<std::int16_t>(NextInteger(cursor));
      const auto dest = NextString(cursor);
      const auto dest_x = Narrow<std::int16_t>(NextInteger(cursor));
      const auto dest_y = Narrow<std::int16_t>(NextInteger(cursor));
      if (!map || !id || !kind || !sprite || !name || !unique || !x || !y || !dest || !dest_x ||
          !dest_y)
        return false;
      const NaviMapIndex from = data.FindMap(*map);
      const NaviMapIndex to = data.FindMap(*dest);
      if (from == kNoNaviMap || to == kNoNaviMap) {
        ++orphans;
        return true;
      }
      NaviLink link;
      link.map = from;
      link.link_id = *id;
      link.kind = *kind;
      link.sprite_class = *sprite;
      link.name = Readable(*name);
      link.cell_x = *x;
      link.cell_y = *y;
      link.dest_map = to;
      link.dest_cell_x = *dest_x;
      link.dest_cell_y = *dest_y;
      links.push_back(std::move(link));
      return true;
    });
    if (!read || !ClosedAtEnd(cursor)) return Broken(kTable, cursor, "ligne illisible");
    return {};
  });
  if (result) {
    data.skipped.links_on_unknown_map = orphans;
    data.BuildIndexes();
  }
  return result;
}

// ── Les deux tables de distances ─────────────────────────────────────────────
//
// Même charpente pour les deux : par carte, `"<carte>", <compte>, { … }`, et dans
// les accolades une entrée par propriétaire — `{ <id>, {…}, {…}, <fin> }`.
// Seules la forme d'une ligne et son marqueur de fin diffèrent.
//
// La ligne de format, quand elle est là, est la PREMIÈRE du fichier. C'est un
// commentaire Lua : le curseur la saute comme les autres.

namespace {

constexpr std::string_view kFormatLine = "-- navi_format:";
constexpr std::int32_t kStepsWrap = 256;

[[nodiscard]] NaviSteps StepsOf(std::span<const std::uint8_t> bytes) {
  const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  if (!text.starts_with(kFormatLine)) return NaviSteps::kModulo256;
  Cursor cursor(bytes.subspan(kFormatLine.size()));
  const std::optional<std::int64_t> format = cursor.Integer();
  return format && *format >= kNaviExactStepsFormat ? NaviSteps::kExact : NaviSteps::kModulo256;
}

}  // namespace

std::int32_t NaviExactSteps(NaviSteps encoding, std::int32_t steps, std::int32_t from_x,
                            std::int32_t from_y, std::int32_t to_x, std::int32_t to_y) noexcept {
  if (encoding == NaviSteps::kExact || steps < 0) return steps;
  const std::int32_t least = std::max(std::abs(to_x - from_x), std::abs(to_y - from_y));
  while (steps < least) steps += kStepsWrap;
  return steps;
}

core::Result<void> ReadNaviLinkDistances(std::span<const std::uint8_t> bytes, NaviData& data) {
  constexpr std::string_view kTable = "Navi_Distance";
  Cursor cursor(bytes);
  if (!OpenTable(cursor, kTable)) return Broken(kTable, cursor, "en-tête attendu");

  std::vector<NaviPassRun> runs;
  std::unordered_map<std::uint32_t, NaviPassSpan> spans;
  while (!cursor.Peek('}')) {
    const auto map = cursor.String();
    const auto declared = NextInteger(cursor);
    if (!map || !declared || !cursor.Take(',') || !cursor.Take('{'))
      return Broken(kTable, cursor, "carte illisible");
    const bool read = ForEachEntry(cursor, [&] {
      const auto owner = Narrow<std::uint32_t>(cursor.Integer());
      if (!owner) return false;
      NaviPassSpan span{static_cast<std::uint32_t>(runs.size()), 0};
      while (true) {
        if (!cursor.Take(',') || !cursor.Take('{')) return false;
        const auto tag = cursor.String();
        const auto first = Narrow<std::uint32_t>(NextInteger(cursor));
        const auto distance = Narrow<std::int32_t>(NextInteger(cursor));
        if (!tag || !first || !distance) return false;
        std::uint32_t count = 1;
        if (cursor.Peek(',')) {
          const auto length = Narrow<std::uint32_t>(NextInteger(cursor));
          if (!length || *length == 0) return false;
          count = *length;
        }
        if (!cursor.Take('}')) return false;
        if (*tag == kLinkPassEnd) break;
        // Seules les passes « E » (depuis la sortie de la liaison) existent dans
        // les tables qu'on lit ; une autre étiquette n'a pas de sens ici.
        if (*tag != kLinkPassTag) return false;
        runs.push_back({*first, count, *distance});
        ++span.count;
      }
      cursor.TakeOptionalComma();
      spans[*owner] = span;
      return true;
    });
    if (!read) return Broken(kTable, cursor, "entrée illisible");
    cursor.TakeOptionalComma();
  }
  if (!cursor.Take('}') || !ClosedAtEnd(cursor)) return Broken(kTable, cursor, "fin attendue");

  std::size_t orphans = 0;
  for (const auto& [owner, span] : spans)
    if (data.FindLink(owner) == nullptr) ++orphans;
  data.link_passes = std::move(runs);
  data.link_pass_spans = std::move(spans);
  data.link_steps = StepsOf(bytes);
  data.skipped.distances_from_unknown_link = orphans;
  return {};
}

core::Result<void> ReadNaviNpcDistances(std::span<const std::uint8_t> bytes, NaviData& data) {
  constexpr std::string_view kTable = "Navi_NpcDistance";
  Cursor cursor(bytes);
  if (!OpenTable(cursor, kTable)) return Broken(kTable, cursor, "en-tête attendu");

  std::vector<NaviNpcPassRun> runs;
  std::unordered_map<std::uint32_t, NaviPassSpan> spans;
  while (!cursor.Peek('}')) {
    const auto map = cursor.String();
    const auto declared = NextInteger(cursor);
    if (!map || !declared || !cursor.Take(',') || !cursor.Take('{'))
      return Broken(kTable, cursor, "carte illisible");
    const bool read = ForEachEntry(cursor, [&] {
      const auto owner = Narrow<std::uint32_t>(cursor.Integer());
      if (!owner) return false;
      NaviPassSpan span{static_cast<std::uint32_t>(runs.size()), 0};
      while (true) {
        if (!cursor.Take(',') || !cursor.Take('{')) return false;
        const auto from = cursor.String();
        const auto first = Narrow<std::uint32_t>(NextInteger(cursor));
        const auto distance = Narrow<std::int32_t>(NextInteger(cursor));
        if (!from || !first || !distance) return false;
        std::uint32_t count = 1;
        if (cursor.Peek(',')) {
          const auto length = Narrow<std::uint32_t>(NextInteger(cursor));
          if (!length || *length == 0) return false;
          count = *length;
        }
        if (!cursor.Take('}')) return false;
        if (from->empty()) break;
        runs.push_back({data.FindMap(*from), *first, count, *distance});
        ++span.count;
      }
      cursor.TakeOptionalComma();
      spans[*owner] = span;
      return true;
    });
    if (!read) return Broken(kTable, cursor, "entrée illisible");
    cursor.TakeOptionalComma();
  }
  if (!cursor.Take('}') || !ClosedAtEnd(cursor)) return Broken(kTable, cursor, "fin attendue");

  data.npc_passes = std::move(runs);
  data.npc_pass_spans = std::move(spans);
  data.npc_steps = StepsOf(bytes);
  return {};
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

std::span<const NaviNpcPassRun> NaviData::PassesToNpc(std::uint32_t npc_id) const {
  const auto found = npc_pass_spans.find(npc_id);
  if (found == npc_pass_spans.end()) return {};
  return std::span<const NaviNpcPassRun>(npc_passes)
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
