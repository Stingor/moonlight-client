// La carte du monde. Voir l'en-tête.

#include "ro/vfs/world_view.h"

#include <algorithm>
#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/ascii.h"
#include "ro/vfs/vfs.h"

#include "yaml_required.h"

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;

// Le rectangle tient quatre colonnes : gauche, haut, droite, bas.
constexpr std::size_t kRectColumns = 4;
// Les colonnes d'une carte : groupe, carte, rectangle, nom, niveau.
constexpr std::size_t kAreaGroup = 0;
constexpr std::size_t kAreaMap = 1;
constexpr std::size_t kAreaLeft = 2;
constexpr std::size_t kAreaTitle = kAreaLeft + kRectColumns;
constexpr std::size_t kAreaLevel = kAreaTitle + 1;
constexpr std::size_t kAreaColumns = kAreaLevel + 1;
// Les colonnes d'un donjon : groupe, rectangle, nom, niveaux.
constexpr std::size_t kDungeonGroup = 0;
constexpr std::size_t kDungeonLeft = 1;
constexpr std::size_t kDungeonTitle = kDungeonLeft + kRectColumns;
constexpr std::size_t kDungeonLevel = kDungeonTitle + 1;
constexpr std::size_t kDungeonColumns = kDungeonLevel + 1;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "carte du monde : " + std::move(detail)};
}

// Une forme inattendue : une exception, comme yaml-cpp signale un champ qui
// n'entre pas dans son type.
[[noreturn]] void Malformed(const YAML::Node& node, const std::string& what) {
  throw YAML::RepresentationException(node.Mark(), what);
}

[[nodiscard]] YAML::Node RowOf(const YAML::Node& row, std::size_t columns) {
  if (!row.IsSequence() || row.size() != columns) {
    Malformed(row, "une ligne n'a pas ses " + std::to_string(columns) + " colonnes");
  }
  return row;
}

[[nodiscard]] WorldRect RectAt(const YAML::Node& row, std::size_t first) {
  // Les quatre coins, dans l'ordre des colonnes.
  enum Corner : std::size_t { kLeft, kTop, kRight, kBottom };
  return WorldRect{.left = row[first + kLeft].as<std::int32_t>(),
                   .top = row[first + kTop].as<std::int32_t>(),
                   .right = row[first + kRight].as<std::int32_t>(),
                   .bottom = row[first + kBottom].as<std::int32_t>()};
}

// Une liste facultative : absente, rien ; présente, une suite.
[[nodiscard]] YAML::Node ListAt(const YAML::Node& map, const char* key) {
  YAML::Node list = map[key];
  if (list && !list.IsSequence()) {
    Malformed(list, std::string("« ") + key + " » n'est pas une liste");
  }
  return list;
}

// Une composante écrite remplace le défaut ; absente, il reste.
void ReadChannel(const YAML::Node& color, const char* key, std::uint8_t& into) {
  if (const YAML::Node value = color[key]; value) {
    into = value.as<std::uint8_t>();
  }
}

void ReadColor(const YAML::Node& color, WorldViewColor& into) {
  if (!color) {
    return;
  }
  if (!color.IsMap()) {
    Malformed(color, "une teinte n'est pas une table");
  }
  ReadChannel(color, "red", into.red);
  ReadChannel(color, "green", into.green);
  ReadChannel(color, "blue", into.blue);
  ReadChannel(color, "alpha", into.alpha);
}

void ReadText(const YAML::Node& text, WorldViewText& into) {
  if (!text) {
    return;
  }
  if (!text.IsMap()) {
    Malformed(text, "un texte n'est pas une table");
  }
  ReadColor(text["letter"], into.letter);
  ReadColor(text["shadow"], into.shadow);
  const YAML::Node sizes = text["sizes"];
  if (!sizes) {
    return;
  }
  if (!sizes.IsSequence() || sizes.size() != into.sizes.size()) {
    Malformed(sizes, "« sizes » n'a pas ses quatre paliers");
  }
  for (std::size_t tier = 0; tier < into.sizes.size(); ++tier) {
    if (!sizes[tier].IsNull()) {
      into.sizes[tier] = sizes[tier].as<std::uint8_t>();
    }
  }
}

void ReadColors(const YAML::Node& colors, WorldViewColors& into) {
  if (!colors) {
    return;
  }
  if (!colors.IsMap()) {
    Malformed(colors, "« colors » n'est pas une table");
  }
  ReadColor(colors["dungeon_box"], into.dungeon_box);
  ReadColor(colors["selected"], into.selected);
  ReadColor(colors["select_box"], into.select_box);
  ReadColor(colors["field_edge"], into.field_edge);
  ReadText(colors["dungeon_name"], into.dungeon_name);
  ReadText(colors["field_name"], into.field_name);
  ReadText(colors["field_level"], into.field_level);
  ReadColor(colors["route"], into.route);
  ReadColor(colors["search_map"], into.search_map);
  ReadColor(colors["search_npc"], into.search_npc);
  const YAML::Node monsters = colors["search_monsters"];
  if (!monsters) {
    return;
  }
  if (!monsters.IsSequence() || monsters.size() != into.search_monsters.size()) {
    Malformed(monsters, "« search_monsters » n'a pas ses cinq paliers");
  }
  for (std::size_t tier = 0; tier < into.search_monsters.size(); ++tier) {
    ReadColor(monsters[tier], into.search_monsters[tier]);
  }
}

[[nodiscard]] WorldView ReadWorld(const YAML::Node& node) {
  if (!node.IsMap()) {
    Malformed(node, "un monde n'est pas une table");
  }
  WorldView world;
  world.title = node["title"].as<std::string>(std::string());
  world.image = node["image"].as<std::string>(std::string());
  if (world.image.empty()) {
    Malformed(node, "un monde sans image");
  }
  for (const YAML::Node& line : ListAt(node, "areas")) {
    const YAML::Node row = RowOf(line, kAreaColumns);
    world.areas.push_back(WorldArea{.group = row[kAreaGroup].as<std::int32_t>(),
                                    .map_name = row[kAreaMap].as<std::string>(),
                                    .rect = RectAt(row, kAreaLeft),
                                    .title = row[kAreaTitle].as<std::string>(),
                                    .level = row[kAreaLevel].as<std::string>()});
    if (world.areas.back().map_name.empty()) {
      Malformed(line, "une carte sans nom");
    }
  }
  for (const YAML::Node& line : ListAt(node, "dungeons")) {
    const YAML::Node row = RowOf(line, kDungeonColumns);
    world.dungeons.push_back(WorldDungeon{.group = row[kDungeonGroup].as<std::int32_t>(),
                                          .rect = RectAt(row, kDungeonLeft),
                                          .title = row[kDungeonTitle].as<std::string>(),
                                          .level = row[kDungeonLevel].as<std::string>()});
  }
  return world;
}

}  // namespace

core::Result<WorldViewTables> WorldViewTables::Load(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kWorldMapPath));
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  return Parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

core::Result<WorldViewTables> WorldViewTables::Parse(std::string_view yaml_text) {
  WorldViewTables tables;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const auto root = VersionedRoot(yaml_text, kSupportedVersion);
    if (!root) {
      return std::unexpected(Refused(root.error()));
    }
    const YAML::Node& whole = *root;
    const YAML::Node worlds = whole["worlds"];
    if (!worlds || !worlds.IsSequence() || worlds.size() == 0) {
      return std::unexpected(Refused("aucun monde"));
    }
    for (const YAML::Node& world : worlds) {
      tables.worlds_.push_back(ReadWorld(world));
    }
    ReadColors(whole["colors"], tables.colors_);
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
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
