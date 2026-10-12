#include "ro/formats/navi_npc_distances.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/ascii.h"

#include "navi_cursor.h"

namespace ro::formats {

namespace {

using navi_internal::Cursor;
using navi_internal::ForEachEntry;
using navi_internal::Narrow;
using navi_internal::NextInteger;
using navi_internal::OpenTable;
using navi_internal::StepsOf;

constexpr std::string_view kTable = "Navi_NpcDistance";

// La profondeur d'accolades où se tiennent les noms de carte : dans la table,
// hors de tout bloc.
constexpr int kMapDepth = 1;

[[nodiscard]] std::unexpected<core::Error> Broken(std::string_view where, std::size_t offset,
                                                  std::string_view what) {
  return core::Fail(core::ErrorCode::kCorruptData, std::string(kTable) + " (" + std::string(where) +
                                                       ") : " + std::string(what) + " à l'octet " +
                                                       std::to_string(offset));
}

// Juste après la chaîne ouverte en `at`, ou rien si elle ne se ferme pas sur
// sa ligne. Le repérage des blocs saute les chaînes ainsi, pour ne pas compter
// les accolades qu'un nom contiendrait.
[[nodiscard]] std::optional<std::size_t> EndOfString(std::string_view text, std::size_t at) {
  for (std::size_t cursor = at + 1; cursor < text.size(); ++cursor) {
    const char c = text[cursor];
    if (c == '\\') {
      ++cursor;
      continue;
    }
    if (c == '\n') return std::nullopt;
    if (c == '"') return cursor + 1;
  }
  return std::nullopt;
}

}  // namespace

std::span<const NaviNpcPassRun> NaviNpcDistances::PassesTo(std::uint32_t npc_id) const {
  const auto found = spans.find(npc_id);
  if (found == spans.end()) return {};
  return std::span<const NaviNpcPassRun>(runs).subspan(found->second.first, found->second.count);
}

core::Result<NaviNpcDistanceFile> NaviNpcDistanceFile::Index(std::vector<std::uint8_t> bytes) {
  NaviNpcDistanceFile file;
  file.bytes_ = std::move(bytes);
  file.steps_ = StepsOf(file.bytes_);

  Cursor header(file.bytes_);
  if (!OpenTable(header, kTable)) return Broken("en-tête", header.offset(), "en-tête attendu");

  const std::string_view text(reinterpret_cast<const char*>(file.bytes_.data()),
                              file.bytes_.size());
  int depth = kMapDepth;
  std::string pending_map;
  std::size_t pending_begin = 0;
  std::size_t at = header.offset();
  while (at < text.size() && depth > 0) {
    const char c = text[at];
    if (c == '-' && at + 1 < text.size() && text[at + 1] == '-') {
      while (at < text.size() && text[at] != '\n') ++at;
      continue;
    }
    if (c == '"') {
      const std::optional<std::size_t> end = EndOfString(text, at);
      if (!end) return Broken("structure", at, "chaîne non fermée");
      if (depth == kMapDepth) {
        if (!pending_map.empty()) return Broken(pending_map, at, "bloc de carte attendu");
        pending_map = core::AsciiLower(text.substr(at + 1, *end - at - 2));
        pending_begin = at;
      }
      at = *end;
      continue;
    }
    if (c == '{') {
      if (depth == kMapDepth && pending_map.empty())
        return Broken("structure", at, "nom de carte attendu");
      ++depth;
    }
    if (c == '}') {
      --depth;
      if (depth == kMapDepth && !pending_map.empty()) {
        file.blocks_.emplace(std::move(pending_map), Block{pending_begin, at + 1});
        pending_map.clear();
      }
    }
    ++at;
  }
  if (depth != 0) return Broken("structure", at, "fin attendue");
  if (!pending_map.empty()) return Broken(pending_map, at, "bloc de carte attendu");
  Cursor rest(std::span<const std::uint8_t>(file.bytes_).subspan(at));
  if (!rest.AtEnd()) return Broken("structure", at + rest.offset(), "rien n'est attendu après");
  return file;
}

core::Result<NaviNpcDistances> NaviNpcDistanceFile::ReadMap(std::string_view map_name,
                                                            const NaviData& data) const {
  NaviNpcDistances distances;
  distances.steps = steps_;
  const auto found = blocks_.find(core::AsciiLower(map_name));
  if (found == blocks_.end()) return distances;
  const Block block = found->second;
  Cursor cursor(
      std::span<const std::uint8_t>(bytes_).subspan(block.begin, block.end - block.begin));

  const auto map = cursor.String();
  const auto declared = NextInteger(cursor);
  if (!map || !declared || !cursor.Take(',') || !cursor.Take('{'))
    return Broken(map_name, block.begin + cursor.offset(), "carte illisible");
  const bool read = ForEachEntry(cursor, [&] {
    const auto owner = Narrow<std::uint32_t>(cursor.Integer());
    if (!owner) return false;
    NaviPassSpan span{static_cast<std::uint32_t>(distances.runs.size()), 0};
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
      // `{"", 0, 0}` clôt la liste d'un PNJ.
      if (from->empty()) break;
      distances.runs.push_back({data.FindMap(*from), *first, count, *distance});
      ++span.count;
    }
    cursor.TakeOptionalComma();
    distances.spans[*owner] = span;
    return true;
  });
  if (!read || !cursor.AtEnd())
    return Broken(map_name, block.begin + cursor.offset(), "entrée illisible");
  return distances;
}

}  // namespace ro::formats
