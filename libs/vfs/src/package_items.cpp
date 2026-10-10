#include "ro/vfs/package_items.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "ro/formats/lua_table.h"
#include "ro/vfs/encoding.h"

namespace ro::vfs {
namespace {

// Le nom de la table globale, et ceux des champs d'une entrée.
constexpr std::string_view kTableGlobal = "tbl";
constexpr std::string_view kIdField = "id";
constexpr std::string_view kWeightField = "prob";
constexpr std::string_view kGroupField = "group";
constexpr std::string_view kNameField = "name";

// Les échelles du calcul d'affichage : le client porte la fraction sur sept
// chiffres, en arrondit un, et la rend en pour-cent au dix-millième.
constexpr double kWeightedScale = 10000000.0;
constexpr std::int64_t kRoundingHalf = 5;
constexpr std::int64_t kRoundingStep = 10;
constexpr double kPercentScale = 10000.0;
constexpr double kWholePercent = 100.0;

constexpr std::uint32_t kLargestWeight = std::numeric_limits<std::int32_t>::max();

[[nodiscard]] std::optional<std::uint32_t> WholeField(const formats::LuaTable& table,
                                                      std::string_view name) {
  const formats::LuaValue* value = table.Find(name);
  if (value == nullptr || value->kind != formats::LuaValue::Kind::kNumber) {
    return std::nullopt;
  }
  return formats::LuaWholeNumber(value->number, kLargestWeight);
}

}  // namespace

const PackageTable* PackageItems::Find(std::uint32_t package_id) const {
  const auto found = by_package.find(package_id);
  return found == by_package.end() ? nullptr : &found->second;
}

double PackagePercent(int group_id, int weight, int total) {
  if (group_id == kGuaranteedPackageGroup || total <= 0) {
    return kWholePercent;
  }
  const auto scaled = static_cast<std::int64_t>(kWeightedScale * static_cast<double>(weight) /
                                                static_cast<double>(total));
  return static_cast<double>((scaled + kRoundingHalf) / kRoundingStep) / kPercentScale;
}

core::Result<PackageItems> ReadPackageItems(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kPackageItemsPath));
  if (!bytes) {
    if (bytes.error().code == core::ErrorCode::kNotFound) {
      return PackageItems{};
    }
    return std::unexpected(core::Error{
        bytes.error().code, std::string(kPackageItemsPath) + " : " + bytes.error().detail});
  }
  auto items = ParsePackageItems(*bytes);
  if (!items) {
    return std::unexpected(core::Error{
        items.error().code, std::string(kPackageItemsPath) + " : " + items.error().detail});
  }
  return items;
}

core::Result<PackageItems> ParsePackageItems(std::span<const std::uint8_t> bytes) {
  PackageItems items;
  auto globals = formats::ReadLuaFile(bytes);
  if (!globals) {
    return std::unexpected(globals.error());
  }
  const formats::LuaValue* tbl = globals->Find(kTableGlobal);
  if (tbl == nullptr || tbl->kind != formats::LuaValue::Kind::kTable || !tbl->table) {
    return items;
  }
  for (const auto& [package_key, package_value] : tbl->table->by_index) {
    const std::optional<std::uint32_t> package_id =
        formats::LuaWholeNumber(package_key, std::numeric_limits<std::uint32_t>::max());
    if (!package_id.has_value() || package_value.kind != formats::LuaValue::Kind::kTable ||
        !package_value.table) {
      continue;
    }
    PackageTable& table = items.by_package[*package_id];
    for (const auto& [rank, entry_value] : package_value.table->by_index) {
      (void)rank;
      if (entry_value.kind != formats::LuaValue::Kind::kTable || !entry_value.table) {
        continue;
      }
      const std::optional<std::uint32_t> id = WholeField(*entry_value.table, kIdField);
      const std::optional<std::uint32_t> weight = WholeField(*entry_value.table, kWeightField);
      const std::optional<std::uint32_t> group = WholeField(*entry_value.table, kGroupField);
      if (!id.has_value() || !weight.has_value() || !group.has_value()) {
        continue;
      }
      const bool monster = *group == static_cast<std::uint32_t>(kMonsterPackageMarker);
      const int group_id = monster ? kMonsterPackageGroup : static_cast<int>(*group);
      auto found = std::find_if(table.groups.begin(), table.groups.end(),
                                [group_id](const PackageGroup& g) { return g.id == group_id; });
      if (found == table.groups.end()) {
        table.groups.push_back(PackageGroup{.id = group_id});
        found = std::prev(table.groups.end());
      }
      // Le libellé est dans la page de codes du client, comme toute table.
      std::string name;
      if (const formats::LuaValue* named = entry_value.table->Find(kNameField);
          named != nullptr && named->kind == formats::LuaValue::Kind::kString) {
        name = Cp949ToUtf8Lossy(named->text);
      }
      found->entries.push_back(PackageEntry{.id = *id,
                                            .is_monster = monster,
                                            .weight = static_cast<int>(*weight),
                                            .name = std::move(name)});
      found->total += static_cast<int>(*weight);
    }
    // Les groupes dans l'ordre de leur numéro, comme la base du jeu les range.
    std::sort(table.groups.begin(), table.groups.end(),
              [](const PackageGroup& a, const PackageGroup& b) { return a.id < b.id; });
  }
  return items;
}

}  // namespace ro::vfs
