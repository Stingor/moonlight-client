// Les commodités de chaque ville. Voir l'en-tête.

#include "ro/vfs/town_info.h"

#include <optional>
#include <string>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/text.h"
#include "ro/vfs/vfs.h"

#include "lua_source_cut.h"

namespace ro::vfs {
namespace {

constexpr std::string_view kNameField = "name";
constexpr std::string_view kCellXField = "X";
constexpr std::string_view kCellYField = "Y";
constexpr std::string_view kFacilityField = "TYPE";
// La plus grande coordonnée qu'une carte puisse porter.
constexpr std::uint32_t kLargestCell = 1023;

[[nodiscard]] std::string_view AsText(const std::vector<std::uint8_t>& bytes) {
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

[[nodiscard]] std::optional<std::uint32_t> WholeField(const formats::LuaTable& entry,
                                                      std::string_view field, std::uint32_t most) {
  const formats::LuaValue* value = entry.Find(field);
  if (value == nullptr || value->kind != formats::LuaValue::Kind::kNumber) return std::nullopt;
  return formats::LuaWholeNumber(value->number, most);
}

// Une entrée lisible, ou rien : une commodité sans position ni nature connue
// n'a pas d'icône à poser.
[[nodiscard]] std::optional<TownFacilityEntry> EntryOf(const formats::LuaTable& entry) {
  const auto x = WholeField(entry, kCellXField, kLargestCell);
  const auto y = WholeField(entry, kCellYField, kLargestCell);
  const auto facility = WholeField(entry, kFacilityField, kTownFacilityCount - 1);
  if (!x || !y || !facility) return std::nullopt;
  TownFacilityEntry out;
  out.cell_x = static_cast<std::int32_t>(*x);
  out.cell_y = static_cast<std::int32_t>(*y);
  out.facility = static_cast<TownFacility>(*facility);
  if (const formats::LuaValue* name = entry.Find(kNameField);
      name != nullptr && name->kind == formats::LuaValue::Kind::kString)
    out.name = core::Utf8FromWire(name->text);
  return out;
}

}  // namespace

void TownInfo::Merge(const formats::LuaTable& globals, std::string_view table_name) {
  const formats::LuaValue* table = globals.Find(table_name);
  if (table == nullptr || table->kind != formats::LuaValue::Kind::kTable || !table->table) return;
  for (const auto& [map_name, list] : table->table->by_name) {
    if (list.kind != formats::LuaValue::Kind::kTable || !list.table) continue;
    std::vector<TownFacilityEntry> entries;
    for (const auto& [rank, value] : list.table->by_index) {
      if (value.kind != formats::LuaValue::Kind::kTable || !value.table) continue;
      if (auto entry = EntryOf(*value.table)) entries.push_back(std::move(*entry));
    }
    // Une carte déclarée REMPLACE ce qu'on avait pour elle.
    by_map_[core::AsciiLower(map_name)] = std::move(entries);
  }
}

core::Result<TownInfo> TownInfo::ParseSources(std::string_view main, std::string_view custom) {
  TownInfo info;
  // La table se termine par le chargement conditionnel de son complément : on
  // le lit nous-mêmes, juste après.
  auto globals = formats::ReadLuaSource(WithoutTrailingConditional(main), formats::LuaTable{});
  if (!globals) return std::unexpected(globals.error());
  info.Merge(*globals, kTownInfoTable);
  if (!custom.empty()) {
    // Le complément appelle une fonction de fusion : l'appel est gardé tel quel
    // par le lecteur, et c'est `Merge` qui fait la fusion. Il lit ses globales
    // dans l'état de la table principale, comme le client l'enchaîne.
    if (auto extra = formats::ReadLuaSource(custom, *globals))
      info.Merge(*extra, kTownInfoCustomTable);
  }
  return info;
}

core::Result<TownInfo> TownInfo::Load(Vfs& vfs) {
  auto main = vfs.Read(kTownInfoPath);
  if (!main) return std::unexpected(main.error());
  std::vector<std::uint8_t> custom;
  if (auto extra = vfs.Read(kTownInfoCustomPath)) custom = std::move(*extra);
  return ParseSources(AsText(*main), AsText(custom));
}

std::span<const TownFacilityEntry> TownInfo::On(std::string_view map_name) const {
  const auto found = by_map_.find(core::AsciiLower(map_name));
  if (found == by_map_.end()) return {};
  return found->second;
}

}  // namespace ro::vfs
