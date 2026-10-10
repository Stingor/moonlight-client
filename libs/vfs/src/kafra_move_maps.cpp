// Le service de téléportation des Kafra. Voir l'en-tête.

#include "ro/vfs/kafra_move_maps.h"

#include <array>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/vfs/lua_chain.h"

namespace ro::vfs {
namespace {

using formats::LuaTable;
using formats::LuaValue;

constexpr std::string_view kListName = "KafraMoveMapServiceList";
// L'extension que la table écrit après chaque nom de carte.
constexpr std::string_view kMapExtension = ".rsw";

[[nodiscard]] std::string MapNameOf(std::string_view written) {
  std::string name = core::AsciiLower(written);
  if (name.ends_with(kMapExtension)) {
    name.resize(name.size() - kMapExtension.size());
  }
  return name;
}

}  // namespace

core::Result<KafraMoveMaps> KafraMoveMaps::Load(Vfs& vfs) {
  constexpr std::array<std::string_view, 1> kPaths = {kKafraMoveMapsPath};
  auto globals = ReadLuaChain(vfs, kPaths);
  if (!globals) {
    return std::unexpected(globals.error());
  }
  return FromGlobals(*globals);
}

KafraMoveMaps KafraMoveMaps::FromGlobals(const LuaTable& globals) {
  KafraMoveMaps maps;
  const LuaValue* list = globals.Find(kListName);
  if (list == nullptr || list->kind != LuaValue::Kind::kTable) {
    return maps;
  }
  for (const auto& [town, value] : list->table->by_name) {
    if (value.kind != LuaValue::Kind::kTable) {
      continue;
    }
    std::vector<std::string> destinations;
    for (const auto& [index, destination] : value.table->by_index) {
      if (destination.kind == LuaValue::Kind::kString) {
        destinations.push_back(MapNameOf(destination.text));
      }
    }
    maps.destinations_.insert_or_assign(MapNameOf(town), std::move(destinations));
  }
  return maps;
}

std::span<const std::string> KafraMoveMaps::DestinationsOf(std::string_view town) const {
  const auto found = destinations_.find(town);
  return found == destinations_.end() ? std::span<const std::string>() : found->second;
}

}  // namespace ro::vfs
