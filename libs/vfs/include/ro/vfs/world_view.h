#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace ro::vfs {

class Vfs;

// ── LA CARTE DU MONDE ───────────────────────────────────────────────────────
//
// Lue dans `data\yaml_files\world_map.yml`, que `scripts/yaml_tables/world_map.py`
// tire des tables de la carte du monde que livre l'installation :
//
//     version: 1
//     worlds:
//       - title: "Midgard"
//         image: "worldmap.jpg"
//         areas:      # [groupe, carte, gauche, haut, droite, bas, nom, niveau]
//           - [108, "prontera", 812, 587, 870, 643, "Prontera", ""]
//         dungeons:   # [groupe, gauche, haut, droite, bas, nom, niveaux]
//           - [1, 725, 17, 745, 37, "Thanatos Tower", "110~130"]
//     colors:
//       dungeon_box: {red: 255, green: 0, blue: 0, alpha: 180}
//       dungeon_name: {letter: {red: 200}, shadow: {red: 40}, sizes: [9, 10, 11, 12]}
//       search_monsters: [{alpha: 110}, …]
//
// Un monde est une image — 1280 × 1024 dans l'installation —, et chaque carte
// y occupe un rectangle EN PIXELS DE CETTE IMAGE. Les étages d'un donjon
// partagent le numéro de groupe de sa case. Une composante de teinte absente,
// ou une taille `null`, garde le défaut de `WorldViewColors`.
inline constexpr std::string_view kWorldMapPath = "data\\yaml_files\\world_map.yml";

// Un rectangle de l'image d'un monde, en pixels de l'image.
struct WorldRect {
  std::int32_t left = 0;
  std::int32_t top = 0;
  std::int32_t right = 0;
  std::int32_t bottom = 0;
};

// UNE CARTE DU MONDE : son nom interne, sans « .rsw », sa place, son nom lisible
// et le niveau conseillé — vide pour une ville.
struct WorldArea {
  std::int32_t group = 0;
  std::string map_name;
  WorldRect rect;
  std::string title;
  std::string level;
};

// LA CASE D'UN DONJON : ses étages sont les cartes du même groupe.
struct WorldDungeon {
  std::int32_t group = 0;
  WorldRect rect;
  std::string title;
  std::string level;
};

struct WorldView {
  std::string title;
  // Le nom de l'image, sous la racine d'interface.
  std::string image;
  std::vector<WorldArea> areas;
  std::vector<WorldDungeon> dungeons;
};

// Une couleur de `worldview_info`, composante par composante.
struct WorldViewColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t alpha = 0;
};

// UN TEXTE DE LA CARTE DU MONDE : sa lettre, son ombre, et sa taille en pixels
// d'écran à chacun des quatre paliers — voir `WorldMapSizeTier`.
struct WorldViewText {
  WorldViewColor letter;
  WorldViewColor shadow;
  std::array<std::uint8_t, 4> sizes{};
};

// Les teintes que le client tire de `worldview_info`.
struct WorldViewColors {
  // La case d'un donjon : le trait vers son étiquette, et l'opacité de ses
  // images.
  WorldViewColor dungeon_box{.red = 255, .green = 0, .blue = 0, .alpha = 180};
  // Ce que la souris désigne : son filet, et le trait d'un donjon survolé.
  WorldViewColor selected{.red = 230, .green = 100, .blue = 0, .alpha = 255};
  // La carte survolée, voilée.
  WorldViewColor select_box{.red = 149, .green = 15, .blue = 15, .alpha = 70};
  // LE NOM D'UN DONJON sur son étiquette, et son niveau dessous quand les noms
  // des cartes se montrent.
  WorldViewText dungeon_name{.letter = {.red = 200, .green = 200, .blue = 200, .alpha = 255},
                             .shadow = {.red = 40, .green = 40, .blue = 40, .alpha = 255},
                             .sizes = {9, 10, 11, 12}};
  // LES NOMS DES CARTES, quand ils se montrent : le filet de chaque carte, son
  // nom, et son niveau conseillé.
  WorldViewColor field_edge{.red = 80, .green = 80, .blue = 80, .alpha = 255};
  WorldViewText field_name{.letter = {.red = 128, .green = 255, .blue = 128, .alpha = 255},
                           .shadow = {.red = 40, .green = 40, .blue = 40, .alpha = 255},
                           .sizes = {9, 10, 11, 12}};
  WorldViewText field_level{.letter = {.red = 255, .green = 255, .blue = 255, .alpha = 255},
                            .shadow = {.red = 0, .green = 0, .blue = 0, .alpha = 255},
                            .sizes = {12, 14, 20, 25}};
  // L'itinéraire de la navigation.
  WorldViewColor route{.red = 85, .green = 85, .blue = 255, .alpha = 102};
  // LA RECHERCHE DE LA NAVIGATION, surlignée sur les cartes qu'elle trouve : la
  // carte elle-même, un PNJ seul, et cinq paliers de monstres selon leur
  // nombre — voir `WorldSearchShade`.
  WorldViewColor search_map{.red = 156, .green = 190, .blue = 231, .alpha = 30};
  WorldViewColor search_npc{.red = 173, .green = 166, .blue = 198, .alpha = 80};
  std::array<WorldViewColor, 5> search_monsters = {
      WorldViewColor{.red = 239, .green = 247, .blue = 165, .alpha = 110},
      WorldViewColor{.red = 140, .green = 203, .blue = 156, .alpha = 110},
      WorldViewColor{.red = 255, .green = 231, .blue = 8, .alpha = 110},
      WorldViewColor{.red = 255, .green = 150, .blue = 8, .alpha = 110},
      WorldViewColor{.red = 255, .green = 73, .blue = 0, .alpha = 110},
  };
};

class WorldViewTables {
 public:
  [[nodiscard]] static core::Result<WorldViewTables> Load(Vfs& vfs);
  // Le texte de `world_map.yml`. Refuse un texte qui n'est pas du YAML, dont
  // `version` n'est pas 1, sans monde, dont un monde n'a pas d'image, ou dont
  // une ligne de carte ou de donjon, une teinte ou une taille n'a pas sa forme.
  [[nodiscard]] static core::Result<WorldViewTables> Parse(std::string_view yaml_text);

  [[nodiscard]] const std::vector<WorldView>& worlds() const noexcept { return worlds_; }
  [[nodiscard]] const WorldViewColors& colors() const noexcept { return colors_; }

  // LE MONDE OÙ SE TROUVE UNE CARTE, le premier qui la nomme ; rien sinon.
  [[nodiscard]] std::optional<std::size_t> WorldOf(std::string_view map_name) const;

 private:
  std::vector<WorldView> worlds_;
  WorldViewColors colors_;
};

}  // namespace ro::vfs
