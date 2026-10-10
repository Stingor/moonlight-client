#include "ro/formats/gnd.h"

#include <string_view>

#include "ro/core/byte_reader.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::ErrorCode;
using core::Fail;

constexpr std::string_view kGndSignature{"GRGN", 4};

// Seuils de version, en 0xMMmm.
constexpr std::int32_t kVersionLightmapsAndTiles = 0x0101;  // strictement au-dessus de 1.0
constexpr std::int32_t kVersionWideTileIndices = 0x0106;    // strictement au-dessus de 1.5
constexpr std::int32_t kVersionWater = 0x0108;
constexpr std::int32_t kVersionFullWaterZones = 0x0109;

// Bornes de cohérence, confrontées aux valeurs annoncées AVANT toute
// allocation. La plus grande carte officielle a une grille de cubes bien en
// dessous de 4096 de côté.
constexpr std::int32_t kMaxDimension = 4096;
constexpr std::int32_t kMaxTextureCount = 65536;
constexpr std::int32_t kMaxTexturePathSize = 1024;
constexpr std::int32_t kMaxLightmapCount = 1 << 20;
constexpr std::int32_t kMaxLightmapSide = 256;
constexpr std::int32_t kMaxTileCount = 1 << 22;
constexpr std::int32_t kMaxWaterSplit = 256;

GndWaterZone ReadWaterZone(ByteReader& reader) {
  GndWaterZone zone;
  zone.level = reader.F32();
  zone.type = reader.I32();
  zone.wave_height = reader.F32();
  zone.wave_speed = reader.F32();
  zone.wave_pitch = reader.F32();
  zone.texture_cycling = reader.I32();
  return zone;
}

}  // namespace

core::Result<GndMap> GndMap::Parse(std::span<const std::uint8_t> data) {
  ByteReader reader(data);

  const std::span<const std::uint8_t> signature_bytes = reader.Bytes(kGndSignature.size());
  const std::string_view signature(reinterpret_cast<const char*>(signature_bytes.data()),
                                   signature_bytes.size());
  if (signature != kGndSignature) {
    // GRFEditor sait encore lire un « GND alpha » sans signature, une forme
    // antérieure à la 1.0 qu'on ne rencontre plus. La refuser explicitement
    // vaut mieux que de mal l'interpréter.
    return Fail(ErrorCode::kBadSignature, "en-tête GND attendu « GRGN »");
  }

  GndMap map;
  map.major_version_ = reader.U8();
  map.minor_version_ = reader.U8();
  map.width_ = reader.I32();
  map.height_ = reader.I32();
  map.proportion_ratio_ = reader.F32();
  const std::int32_t texture_count = reader.I32();
  const std::int32_t texture_path_size = reader.I32();
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "en-tête GND tronqué");
  }

  if (map.width_ <= 0 || map.height_ <= 0 || map.width_ > kMaxDimension ||
      map.height_ > kMaxDimension) {
    return Fail(ErrorCode::kCorruptData,
                "dimensions GND " + std::to_string(map.width_) + "x" + std::to_string(map.height_));
  }
  if (texture_count < 0 || texture_count > kMaxTextureCount || texture_path_size <= 0 ||
      texture_path_size > kMaxTexturePathSize) {
    return Fail(ErrorCode::kCorruptData,
                "table de textures GND : " + std::to_string(texture_count) + " entrées de " +
                    std::to_string(texture_path_size) + " octets");
  }

  map.texture_paths_.reserve(static_cast<std::size_t>(texture_count));
  for (std::int32_t index = 0; index < texture_count; ++index) {
    map.texture_paths_.emplace_back(
        reader.FixedString(static_cast<std::size_t>(texture_path_size)));
  }
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "chemins de textures GND tronqués");
  }

  const std::int32_t version = map.version();

  if (version >= kVersionLightmapsAndTiles) {
    map.lightmap_count_ = reader.I32();
    map.lightmap_width_ = reader.I32();
    map.lightmap_height_ = reader.I32();
    // Le champ « cellules par lightmap » est lu puis IGNORÉ : GRFEditor le
    // force à 1 juste après l'avoir lu, et la taille des données s'en déduit
    // avec 1, pas avec la valeur du fichier. Le lire quand même est
    // indispensable — il occupe quatre octets.
    static_cast<void>(reader.I32());
    if (!reader.ok()) {
      return Fail(ErrorCode::kUnexpectedEnd, "en-tête de lightmaps GND tronqué");
    }

    if (map.lightmap_count_ < 0 || map.lightmap_count_ > kMaxLightmapCount ||
        map.lightmap_width_ <= 0 || map.lightmap_width_ > kMaxLightmapSide ||
        map.lightmap_height_ <= 0 || map.lightmap_height_ > kMaxLightmapSide) {
      return Fail(ErrorCode::kCorruptData,
                  "lightmaps GND : " + std::to_string(map.lightmap_count_) + " de " +
                      std::to_string(map.lightmap_width_) + "x" +
                      std::to_string(map.lightmap_height_));
    }

    const std::size_t lightmap_bytes = static_cast<std::size_t>(map.lightmap_count_) *
                                       static_cast<std::size_t>(map.lightmap_width_) *
                                       static_cast<std::size_t>(map.lightmap_height_) * 4;
    const std::span<const std::uint8_t> lightmaps = reader.Bytes(lightmap_bytes);
    if (!reader.ok()) {
      return Fail(ErrorCode::kUnexpectedEnd, "données de lightmaps GND tronquées");
    }
    map.lightmap_data_.assign(lightmaps.begin(), lightmaps.end());

    const std::int32_t tile_count = reader.I32();
    if (!reader.ok() || tile_count < 0 || tile_count > kMaxTileCount) {
      return Fail(ErrorCode::kCorruptData, "GND : " + std::to_string(tile_count) + " tuiles");
    }
    if (reader.remaining() < static_cast<std::size_t>(tile_count) * kGndTileSize) {
      return Fail(ErrorCode::kUnexpectedEnd, "tuiles GND tronquées");
    }

    map.tiles_.resize(static_cast<std::size_t>(tile_count));
    for (GndTile& tile : map.tiles_) {
      // Les quatre U d'abord, puis les quatre V — pas quatre paires.
      for (float& coordinate : tile.u) {
        coordinate = reader.F32();
      }
      for (float& coordinate : tile.v) {
        coordinate = reader.F32();
      }
      tile.texture_index = reader.I16();
      tile.lightmap_index = reader.U16();
      for (std::uint8_t& channel : tile.color) {
        channel = reader.U8();
      }
    }
    if (!reader.ok()) {
      return Fail(ErrorCode::kUnexpectedEnd, "tuiles GND tronquées");
    }
  }

  const std::size_t cube_count =
      static_cast<std::size_t>(map.width_) * static_cast<std::size_t>(map.height_);
  const bool wide_tile_indices = version >= kVersionWideTileIndices;
  const std::size_t cube_size = 4 * sizeof(float) + (wide_tile_indices ? 12 : 8);
  if (reader.remaining() < cube_count * cube_size) {
    return Fail(ErrorCode::kUnexpectedEnd, "GND annonce " + std::to_string(cube_count) +
                                               " cubes, le fichier n'en porte que " +
                                               std::to_string(reader.remaining() / cube_size));
  }

  map.cubes_.resize(cube_count);
  for (GndCube& cube : map.cubes_) {
    for (float& corner : cube.corner_height) {
      corner = reader.F32();
    }
    if (wide_tile_indices) {
      cube.tile_up = reader.I32();
      cube.tile_front = reader.I32();
      cube.tile_side = reader.I32();
    } else {
      cube.tile_up = reader.I16();
      cube.tile_front = reader.I16();
      cube.tile_side = reader.I16();
      reader.Skip(2);  // bourrage
    }
  }
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "cubes GND tronqués");
  }

  if (version >= kVersionWater) {
    // Un jeu de valeurs par défaut précède la découpe. En version 1.8 les zones
    // n'en portent QUE le niveau et héritent du reste ; à partir de 1.9 elles
    // sont complètes et ce jeu par défaut ne sert plus.
    const GndWaterZone defaults = ReadWaterZone(reader);
    map.water_split_width_ = reader.I32();
    map.water_split_height_ = reader.I32();
    if (!reader.ok()) {
      return Fail(ErrorCode::kUnexpectedEnd, "en-tête d'eau GND tronqué");
    }
    if (map.water_split_width_ <= 0 || map.water_split_height_ <= 0 ||
        map.water_split_width_ > kMaxWaterSplit || map.water_split_height_ > kMaxWaterSplit) {
      return Fail(ErrorCode::kCorruptData, "découpe d'eau GND " +
                                               std::to_string(map.water_split_width_) + "x" +
                                               std::to_string(map.water_split_height_));
    }

    const auto zone_count = static_cast<std::size_t>(map.water_split_width_) *
                            static_cast<std::size_t>(map.water_split_height_);
    map.water_zones_.reserve(zone_count);
    for (std::size_t index = 0; index < zone_count; ++index) {
      if (version >= kVersionFullWaterZones) {
        map.water_zones_.push_back(ReadWaterZone(reader));
      } else {
        GndWaterZone zone = defaults;
        zone.level = reader.F32();
        map.water_zones_.push_back(zone);
      }
    }
    if (!reader.ok()) {
      return Fail(ErrorCode::kUnexpectedEnd, "zones d'eau GND tronquées");
    }
  }

  return map;
}

core::Result<WaterField> GndMap::MakeWaterField() const {
  if (water_zones_.empty()) {
    return WaterField::None();
  }
  std::vector<float> levels;
  levels.reserve(water_zones_.size());
  for (const GndWaterZone& zone : water_zones_) {
    levels.push_back(zone.level);
  }
  return WaterField::Zoned(std::move(levels), water_split_width_, water_split_height_, width_,
                           height_);
}

void GndMap::LevelCornerHeights(float height) noexcept {
  for (GndCube& cube : cubes_) {
    cube.corner_height.fill(height);
  }
}

}  // namespace ro::formats
