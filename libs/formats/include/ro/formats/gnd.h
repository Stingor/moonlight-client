#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/water_field.h"

namespace ro::formats {

// Fichier .gnd : le sol d'une carte — le maillage du terrain, ses textures, ses
// lightmaps, et (à partir de la version 1.8) son eau.
//
// La grille de CUBES du GND fait la moitié de la grille de CELLULES du GAT :
// un cube couvre 2 × 2 cellules.
//
// Ordre de lecture, qui est aussi celui du fichier : en-tête, chemins de
// textures, lightmaps, tuiles, cubes, eau. Chaque section est de longueur
// variable et dépend de la précédente — sauter une section, c'est décaler tout
// ce qui suit.
//
// Référence : GRF/FileFormats/GndFormat/ de GRFEditor.

inline constexpr std::size_t kGndHeaderSize = 26;

// Une tuile : 8 flottants d'UV, l'index de texture, celui de la lightmap, et
// quatre octets de couleur.
inline constexpr std::size_t kGndTileSize = 8 * 4 + 2 + 2 + 4;

struct GndTile {
  // Coordonnées de texture des quatre coins. Le fichier range les quatre U
  // d'abord, puis les quatre V — pas quatre paires.
  std::array<float, 4> u{};
  std::array<float, 4> v{};
  std::int16_t texture_index = 0;
  std::uint16_t lightmap_index = 0;
  // Dans l'ordre du fichier : bleu, vert, rouge, alpha. C'est l'ordre d'un
  // pixel Windows, pas celui d'un nom de couleur.
  std::array<std::uint8_t, 4> color{};
};

struct GndCube {
  // Hauteurs des quatre coins, dans l'ordre du fichier :
  // bas-gauche, bas-droit, haut-gauche, haut-droit.
  std::array<float, 4> corner_height{};
  // Index dans le tableau des tuiles ; -1 signifie « pas de face ».
  std::int32_t tile_up = -1;
  std::int32_t tile_front = -1;
  std::int32_t tile_side = -1;
};

struct GndWaterZone {
  float level = 0.0F;
  std::int32_t type = 0;
  float wave_height = 1.0F;
  float wave_speed = 2.0F;
  float wave_pitch = 50.0F;
  std::int32_t texture_cycling = 3;
};

// LES PIÈCES D'UN SOL BÂTI EN MÉMOIRE, dans l'unité, l'ordre et le sens du
// fichier : ce qu'un sol généré donne à `GndMap::Assemble` au lieu d'octets.
struct GndParts {
  std::uint8_t major_version = 1;
  std::uint8_t minor_version = 7;
  std::int32_t width = 0;
  std::int32_t height = 0;
  float proportion_ratio = 10.0F;
  std::vector<std::string> texture_paths;
  std::int32_t lightmap_width = 8;
  std::int32_t lightmap_height = 8;
  // Les lightmaps l'une après l'autre, chacune son plan d'ombre puis ses
  // couleurs RGB : lightmap_width × lightmap_height × 4 octets chacune.
  std::vector<std::uint8_t> lightmap_data;
  std::vector<GndTile> tiles;
  // width × height cubes, ligne par ligne.
  std::vector<GndCube> cubes;
  // L'eau, seulement à partir de la version 1.8 : une zone par case de la
  // découpe.
  std::vector<GndWaterZone> water_zones;
  std::int32_t water_split_width = 1;
  std::int32_t water_split_height = 1;
};

class GndMap {
 public:
  [[nodiscard]] static core::Result<GndMap> Parse(std::span<const std::uint8_t> data);

  // UN SOL ASSEMBLÉ EN MÉMOIRE, refusé là où `Parse` refuserait le même
  // fichier : dimensions, nombre et longueur des chemins de textures,
  // lightmaps, tuiles, eau. Les tailles doivent s'accorder entre elles (cubes,
  // octets des lightmaps, zones d'eau).
  [[nodiscard]] static core::Result<GndMap> Assemble(GndParts parts);

  [[nodiscard]] std::int32_t width() const noexcept { return width_; }
  [[nodiscard]] std::int32_t height() const noexcept { return height_; }
  [[nodiscard]] std::uint8_t major_version() const noexcept { return major_version_; }
  [[nodiscard]] std::uint8_t minor_version() const noexcept { return minor_version_; }
  [[nodiscard]] std::int32_t version() const noexcept {
    return (static_cast<std::int32_t>(major_version_) << 8) |
           static_cast<std::int32_t>(minor_version_);
  }

  // Facteur d'échelle de la grille, 10 en pratique : un cube fait 10 unités.
  [[nodiscard]] float proportion_ratio() const noexcept { return proportion_ratio_; }

  [[nodiscard]] const std::vector<std::string>& texture_paths() const noexcept {
    return texture_paths_;
  }
  [[nodiscard]] const std::vector<GndTile>& tiles() const noexcept { return tiles_; }
  [[nodiscard]] const std::vector<GndCube>& cubes() const noexcept { return cubes_; }

  [[nodiscard]] const GndCube& at(std::int32_t x, std::int32_t y) const {
    return cubes_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                  static_cast<std::size_t>(x)];
  }

  // LE RELIEF POSÉ À UNE SEULE ALTITUDE : les quatre coins de chaque cube
  // prennent `height`, dans l'unité et le sens du fichier. Les faces, les tuiles,
  // les lightmaps et l'eau ne bougent pas.
  void LevelCornerHeights(float height) noexcept;

  // LES QUATRE HAUTEURS DU CUBE (x, y), dans l'ordre et le sens du fichier :
  // c'est par là que l'éditeur de carte retouche le relief. Un cube hors de la
  // carte n'est pas touché. Les faces, les tuiles et l'eau ne bougent pas.
  void SetCornerHeights(std::int32_t x, std::int32_t y,
                        const std::array<float, 4>& heights) noexcept;

  // Lightmaps conservées brutes : chacune fait
  // lightmap_width × lightmap_height × 4 octets. Leur découpage en textures
  // relève du rendu, pas du parseur.
  [[nodiscard]] const std::vector<std::uint8_t>& lightmap_data() const noexcept {
    return lightmap_data_;
  }
  [[nodiscard]] std::int32_t lightmap_count() const noexcept { return lightmap_count_; }
  [[nodiscard]] std::int32_t lightmap_width() const noexcept { return lightmap_width_; }
  [[nodiscard]] std::int32_t lightmap_height() const noexcept { return lightmap_height_; }

  [[nodiscard]] const std::vector<GndWaterZone>& water_zones() const noexcept {
    return water_zones_;
  }
  [[nodiscard]] std::int32_t water_split_width() const noexcept { return water_split_width_; }
  [[nodiscard]] std::int32_t water_split_height() const noexcept { return water_split_height_; }

  // Le champ d'eau prêt à être confronté aux cellules GAT. Vide si le GND ne
  // porte pas d'eau (version antérieure à 1.8).
  [[nodiscard]] core::Result<WaterField> MakeWaterField() const;

 private:
  GndMap() = default;

  std::vector<std::string> texture_paths_;
  std::vector<GndTile> tiles_;
  std::vector<GndCube> cubes_;
  std::vector<std::uint8_t> lightmap_data_;
  std::vector<GndWaterZone> water_zones_;

  std::int32_t width_ = 0;
  std::int32_t height_ = 0;
  float proportion_ratio_ = 0.0F;
  std::int32_t lightmap_count_ = 0;
  std::int32_t lightmap_width_ = 0;
  std::int32_t lightmap_height_ = 0;
  std::int32_t water_split_width_ = 1;
  std::int32_t water_split_height_ = 1;
  std::uint8_t major_version_ = 0;
  std::uint8_t minor_version_ = 0;
};

}  // namespace ro::formats
