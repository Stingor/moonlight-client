// La carte du monde. Voir l'en-tête.

#include "ro/vfs/world_view.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/lua_chain.h"

namespace ro::vfs {
namespace {

using formats::LuaTable;
using formats::LuaValue;

constexpr std::string_view kWorldListName = "World_List";
constexpr std::string_view kInfoName = "worldview_info";
// L'extension que la table écrit après le nom de la carte.
constexpr std::string_view kWorldExtension = ".rsw";

// Les colonnes d'une ligne de monde.
enum WorldColumn : int { kWorldTitle = 1, kWorldAreas, kWorldDungeons, kWorldImage };
// Les colonnes d'une carte : groupe, fichier, rectangle, nom, niveau.
enum AreaColumn : int {
  kAreaGroup = 1,
  kAreaMap,
  kAreaLeft,
  kAreaTop,
  kAreaRight,
  kAreaBottom,
  kAreaTitle,
  kAreaLevel
};
// Les colonnes d'un donjon : groupe, rectangle, nom, niveau.
enum DungeonColumn : int {
  kDungeonGroup = 1,
  kDungeonLeft,
  kDungeonTop,
  kDungeonRight,
  kDungeonBottom,
  kDungeonTitle,
  kDungeonLevel
};

[[nodiscard]] const LuaTable* TableOf(const LuaValue* value) {
  return value != nullptr && value->kind == LuaValue::Kind::kTable ? value->table.get() : nullptr;
}

[[nodiscard]] std::string TextAt(const LuaTable& row, int column) {
  const LuaValue* value = row.Find(static_cast<double>(column));
  if (value == nullptr || value->kind != LuaValue::Kind::kString) {
    return {};
  }
  return Cp949ToUtf8Lossy(value->text);
}

[[nodiscard]] std::optional<std::int32_t> WholeAt(const LuaTable& row, int column) {
  const LuaValue* value = row.Find(static_cast<double>(column));
  if (value == nullptr || value->kind != LuaValue::Kind::kNumber) {
    return std::nullopt;
  }
  return static_cast<std::int32_t>(std::lround(value->number));
}

[[nodiscard]] std::optional<WorldRect> RectAt(const LuaTable& row, int first_column) {
  const auto left = WholeAt(row, first_column);
  const auto top = WholeAt(row, first_column + 1);
  const auto right = WholeAt(row, first_column + 2);
  const auto bottom = WholeAt(row, first_column + 3);
  if (!left || !top || !right || !bottom) {
    return std::nullopt;
  }
  return WorldRect{.left = *left, .top = *top, .right = *right, .bottom = *bottom};
}

// Les lignes d'une table, dans l'ordre du fichier ; vide si elle manque.
template <typename Read>
void ForEachRow(const LuaTable& globals, std::string_view name, Read read) {
  const LuaTable* rows = TableOf(globals.Find(name));
  if (rows == nullptr) {
    return;
  }
  for (const auto& [index, value] : rows->by_index) {
    if (const LuaTable* row = TableOf(&value); row != nullptr) {
      read(*row);
    }
  }
}

[[nodiscard]] std::uint8_t ChannelOf(const LuaTable& info, const std::string& field,
                                     std::uint8_t fallback) {
  const LuaValue* value = info.Find(field);
  if (value == nullptr || value->kind != LuaValue::Kind::kNumber) {
    return fallback;
  }
  constexpr double kMostChannel = 255.0;
  return static_cast<std::uint8_t>(
      std::clamp(std::lround(value->number), 0L, static_cast<long>(kMostChannel)));
}

// Un texte : sa lettre `<lettre>R/G/B`, son ombre `<lettre>RS/GS/BS`, et ses
// quatre tailles `<taille>0` à `<taille>3`.
void ReadText(const LuaTable& info, std::string_view letter, std::string_view size,
              WorldViewText& into) {
  const std::string color(letter);
  into.letter.red = ChannelOf(info, color + "R", into.letter.red);
  into.letter.green = ChannelOf(info, color + "G", into.letter.green);
  into.letter.blue = ChannelOf(info, color + "B", into.letter.blue);
  into.shadow.red = ChannelOf(info, color + "RS", into.shadow.red);
  into.shadow.green = ChannelOf(info, color + "GS", into.shadow.green);
  into.shadow.blue = ChannelOf(info, color + "BS", into.shadow.blue);
  for (std::size_t tier = 0; tier < into.sizes.size(); ++tier) {
    into.sizes[tier] = ChannelOf(info, std::string(size) + std::to_string(tier), into.sizes[tier]);
  }
}

// Une couleur `<préfixe>ColorR/G/B` et `<préfixe>Alpha`, champ par champ.
void ReadColor(const LuaTable& info, std::string_view prefix, WorldViewColor& into) {
  const std::string name(prefix);
  into.red = ChannelOf(info, name + "ColorR", into.red);
  into.green = ChannelOf(info, name + "ColorG", into.green);
  into.blue = ChannelOf(info, name + "ColorB", into.blue);
  into.alpha = ChannelOf(info, name + "Alpha", into.alpha);
}

}  // namespace

core::Result<WorldViewTables> WorldViewTables::Load(Vfs& vfs) {
  auto globals = ReadLuaChain(vfs, kWorldViewPaths);
  if (!globals) {
    return std::unexpected(globals.error());
  }
  return FromGlobals(*globals);
}

core::Result<WorldViewTables> WorldViewTables::FromGlobals(const LuaTable& globals) {
  WorldViewTables tables;
  ForEachRow(globals, kWorldListName, [&globals, &tables](const LuaTable& line) {
    WorldView world;
    world.title = TextAt(line, kWorldTitle);
    world.image = TextAt(line, kWorldImage);
    ForEachRow(globals, TextAt(line, kWorldAreas), [&world](const LuaTable& row) {
      const auto group = WholeAt(row, kAreaGroup);
      const auto rect = RectAt(row, kAreaLeft);
      std::string map = core::AsciiLower(TextAt(row, kAreaMap));
      if (map.ends_with(kWorldExtension)) {
        map.resize(map.size() - kWorldExtension.size());
      }
      if (!group || !rect || map.empty()) {
        return;
      }
      world.areas.push_back(WorldArea{.group = *group,
                                      .map_name = std::move(map),
                                      .rect = *rect,
                                      .title = TextAt(row, kAreaTitle),
                                      .level = TextAt(row, kAreaLevel)});
    });
    ForEachRow(globals, TextAt(line, kWorldDungeons), [&world](const LuaTable& row) {
      const auto group = WholeAt(row, kDungeonGroup);
      const auto rect = RectAt(row, kDungeonLeft);
      if (!group || !rect) {
        return;
      }
      world.dungeons.push_back(WorldDungeon{.group = *group,
                                            .rect = *rect,
                                            .title = TextAt(row, kDungeonTitle),
                                            .level = TextAt(row, kDungeonLevel)});
    });
    if (!world.image.empty()) {
      tables.worlds_.push_back(std::move(world));
    }
  });
  if (tables.worlds_.empty()) {
    return core::Fail(core::ErrorCode::kNotFound, "World_List : aucun monde");
  }
  if (const LuaTable* info = TableOf(globals.Find(kInfoName)); info != nullptr) {
    ReadColor(*info, "DungeonBox_", tables.colors_.dungeon_box);
    ReadColor(*info, "Dungeon_Select", tables.colors_.selected);
    ReadColor(*info, "SelectBox_", tables.colors_.select_box);
    ReadColor(*info, "MobBoxLine_", tables.colors_.field_edge);
    ReadText(*info, "Dungeon_FontColor", "Dungeon_fontSize", tables.colors_.dungeon_name);
    ReadText(*info, "MobName_fontColor", "MobName_fontSize", tables.colors_.field_name);
    ReadText(*info, "MobLv_fontColor", "MobLv_fontSize", tables.colors_.field_level);
    ReadColor(*info, "Run_", tables.colors_.route);
    ReadColor(*info, "SreachMap_", tables.colors_.search_map);
    ReadColor(*info, "SreachNpc_", tables.colors_.search_npc);
    for (std::size_t tier = 0; tier < tables.colors_.search_monsters.size(); ++tier) {
      ReadColor(*info, "SreachMob" + std::to_string(tier + 1) + "_",
                tables.colors_.search_monsters[tier]);
    }
  }
  return tables;
}

std::optional<std::size_t> WorldViewTables::WorldOf(std::string_view map_name) const {
  const std::string wanted = core::AsciiLower(std::string(map_name));
  for (std::size_t rank = 0; rank < worlds_.size(); ++rank) {
    if (std::ranges::any_of(worlds_[rank].areas,
                            [&wanted](const WorldArea& area) { return area.map_name == wanted; })) {
      return rank;
    }
  }
  return std::nullopt;
}

}  // namespace ro::vfs
