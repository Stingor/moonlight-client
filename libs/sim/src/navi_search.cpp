#include "ro/sim/navi_search.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "ro/core/text.h"

namespace ro::sim {

namespace {

using formats::NaviData;
using formats::NaviMapIndex;

// Le supplément Latin-1, de U+00C0 à U+00FF, ramené à sa lettre de base. Les
// deux signes qui n'en sont pas — × et ÷ — deviennent leurs équivalents ASCII.
constexpr char32_t kLatinSupplementFirst = 0x00C0;
constexpr std::string_view kLatinSupplementBase =
    "aaaaaaaceeeeiiii"
    "dnooooo*ouuuuyps"
    "aaaaaaaceeeeiiii"
    "dnooooo/ouuuuypy";
constexpr char32_t kAsciiLast = 0x7F;

// La qualité d'une correspondance, de la meilleure à la plus faible.
enum class Match : std::uint8_t {
  kNone = 0,
  kViaMap,
  kContains,
  kPrefix,
  kExact,
};

[[nodiscard]] std::vector<std::string> Words(std::string_view folded) {
  std::vector<std::string> words;
  std::size_t at = 0;
  while (at < folded.size()) {
    while (at < folded.size() && folded[at] == ' ') ++at;
    const std::size_t start = at;
    while (at < folded.size() && folded[at] != ' ') ++at;
    if (at > start) words.emplace_back(folded.substr(start, at - start));
  }
  return words;
}

// Chaque mot du terme doit se trouver dans le nom ou, à défaut, dans ce qui
// l'accompagne — le nom de sa carte.
[[nodiscard]] Match Judge(std::string_view folded_term, const std::vector<std::string>& words,
                          std::string_view folded_name, std::string_view folded_context) {
  if (words.empty()) return Match::kNone;
  if (folded_name == folded_term) return Match::kExact;
  bool all_in_name = true;
  for (const std::string& word : words) {
    if (folded_name.find(word) != std::string_view::npos) continue;
    all_in_name = false;
    if (folded_context.find(word) == std::string_view::npos) return Match::kNone;
  }
  if (!all_in_name) return Match::kViaMap;
  return folded_name.starts_with(folded_term) ? Match::kPrefix : Match::kContains;
}

struct GroupKey {
  NaviHitKind kind;
  std::string folded_name;
  auto operator<=>(const GroupKey&) const = default;
};

struct Collected {
  NaviHitGroup group;
  Match best = Match::kNone;
};

[[nodiscard]] std::string MapContext(const NaviData& data, NaviMapIndex map) {
  if (map >= data.maps.size()) return {};
  return FoldForSearch(data.maps[map].name) + " " + FoldForSearch(data.maps[map].display_name);
}

[[nodiscard]] NaviMapIndex MapOf(const NaviData& data, const NaviHit& hit) {
  switch (hit.kind) {
    case NaviHitKind::kMap:
      return static_cast<NaviMapIndex>(hit.rank);
    case NaviHitKind::kNpc:
      return data.npcs[hit.rank].map;
    case NaviHitKind::kSpawn:
      return data.spawns[hit.rank].map;
  }
  return formats::kNoNaviMap;
}

}  // namespace

std::string_view VisibleNpcName(std::string_view name) noexcept {
  const std::size_t hash = name.find('#');
  return hash == std::string_view::npos ? name : name.substr(0, hash);
}

std::string FoldForSearch(std::string_view utf8) {
  std::string folded;
  folded.reserve(utf8.size());
  std::size_t at = 0;
  while (at < utf8.size()) {
    const char32_t code = core::NextCodePoint(utf8, at);
    if (code <= kAsciiLast) {
      const char c = static_cast<char>(code);
      folded.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    } else if (code >= kLatinSupplementFirst &&
               code < kLatinSupplementFirst + kLatinSupplementBase.size()) {
      folded.push_back(kLatinSupplementBase[code - kLatinSupplementFirst]);
    } else {
      core::AppendUtf8(folded, code);
    }
  }
  return folded;
}

std::vector<NaviHitGroup> SearchNavi(const NaviData& data, const NaviSearchQuery& query) {
  const std::string term = FoldForSearch(VisibleNpcName(query.term));
  const std::vector<std::string> words = Words(term);
  // Le terme replié, ses espaces multiples réduits : c'est lui que compare
  // l'égalité exacte.
  std::string joined;
  for (const std::string& word : words) joined += (joined.empty() ? "" : " ") + word;
  if (words.empty()) return {};

  // 🔴 TOUT SE RECUEILLE, LA BORNE NE TOMBE QU'APRÈS LE CLASSEMENT. Bornée
  // pendant la collecte — cartes, puis PNJ, puis monstres —, elle laissait
  // quatre cents correspondances faibles évincer un nom de monstre exact.
  std::map<GroupKey, Collected> groups;
  const auto keep = [&](NaviHitKind kind, std::uint32_t rank, const std::string& name,
                        const std::string& folded_name, Match match) {
    if (match == Match::kNone) return;
    Collected& slot = groups[GroupKey{kind, folded_name}];
    if (slot.group.hits.empty()) {
      slot.group.kind = kind;
      slot.group.name = name;
    }
    slot.group.hits.push_back({kind, rank});
    slot.best = std::max(slot.best, match);
  };

  // Le contexte d'une carte se replie une fois, pas une fois par PNJ.
  std::vector<std::string> contexts(data.maps.size());
  for (std::size_t map = 0; map < data.maps.size(); ++map)
    contexts[map] = MapContext(data, static_cast<NaviMapIndex>(map));

  const auto in_scope = [&](NaviMapIndex map) {
    return query.only_map == formats::kNoNaviMap || map == query.only_map;
  };

  for (std::size_t rank = 0; rank < data.maps.size(); ++rank) {
    if (!in_scope(static_cast<NaviMapIndex>(rank))) continue;
    const formats::NaviMap& map = data.maps[rank];
    // Une carte se reconnaît sous ses DEUX noms : l'interne, qu'on tape en
    // commande, et l'affiché, qu'on lit partout.
    const std::string folded_display = FoldForSearch(map.display_name);
    const std::string folded_internal = FoldForSearch(map.name);
    const Match match = std::max(Judge(joined, words, folded_display, {}),
                                 Judge(joined, words, folded_internal, {}));
    keep(NaviHitKind::kMap, static_cast<std::uint32_t>(rank), map.display_name, folded_internal,
         match);
  }
  for (std::size_t rank = 0; rank < data.npcs.size(); ++rank) {
    const formats::NaviNpc& npc = data.npcs[rank];
    if (!in_scope(npc.map) || npc.name.empty()) continue;
    const std::string folded = FoldForSearch(npc.name);
    keep(NaviHitKind::kNpc, static_cast<std::uint32_t>(rank), npc.name, folded,
         Judge(joined, words, folded, npc.map < contexts.size() ? contexts[npc.map] : ""));
  }
  for (std::size_t rank = 0; rank < data.spawns.size(); ++rank) {
    const formats::NaviSpawn& spawn = data.spawns[rank];
    if (!in_scope(spawn.map) || spawn.name.empty()) continue;
    const std::string folded = FoldForSearch(spawn.name);
    keep(NaviHitKind::kSpawn, static_cast<std::uint32_t>(rank), spawn.name, folded,
         Judge(joined, words, folded, spawn.map < contexts.size() ? contexts[spawn.map] : ""));
  }

  std::vector<std::pair<Match, NaviHitGroup>> ordered;
  ordered.reserve(groups.size());
  for (auto& [key, collected] : groups) {
    // Dans un groupe, la carte du joueur d'abord, puis l'ordre des noms de carte.
    std::stable_sort(collected.group.hits.begin(), collected.group.hits.end(),
                     [&](const NaviHit& left, const NaviHit& right) {
                       const NaviMapIndex left_map = MapOf(data, left);
                       const NaviMapIndex right_map = MapOf(data, right);
                       const bool left_here = left_map == query.prefer_map;
                       const bool right_here = right_map == query.prefer_map;
                       if (left_here != right_here) return left_here;
                       return left_map < right_map;
                     });
    ordered.emplace_back(collected.best, std::move(collected.group));
  }
  std::stable_sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
    if (left.first != right.first) return left.first > right.first;
    return std::tie(left.second.kind, left.second.name) <
           std::tie(right.second.kind, right.second.name);
  });

  // La borne, dans l'ordre du classement : un groupe qui la franchit garde ses
  // premiers résultats, ceux de la carte du joueur en tête.
  std::vector<NaviHitGroup> result;
  result.reserve(ordered.size());
  std::size_t left = query.max_hits;
  for (auto& [match, group] : ordered) {
    if (left == 0) break;
    if (group.hits.size() > left) group.hits.resize(left);
    left -= group.hits.size();
    result.push_back(std::move(group));
  }
  return result;
}

std::vector<NaviHitGroup> ContentsOfMap(const NaviData& data, NaviMapIndex map) {
  std::vector<NaviHitGroup> result;
  NaviHitGroup npcs{NaviHitKind::kNpc, {}, {}};
  for (const std::uint32_t rank : data.NpcsOn(map))
    if (!data.npcs[rank].name.empty()) npcs.hits.push_back({NaviHitKind::kNpc, rank});
  NaviHitGroup spawns{NaviHitKind::kSpawn, {}, {}};
  for (const std::uint32_t rank : data.SpawnsOn(map))
    spawns.hits.push_back({NaviHitKind::kSpawn, rank});
  if (!npcs.hits.empty()) result.push_back(std::move(npcs));
  if (!spawns.hits.empty()) result.push_back(std::move(spawns));
  return result;
}

}  // namespace ro::sim
