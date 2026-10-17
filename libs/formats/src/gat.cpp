#include "ro/formats/gat.h"

#include <string>

#include "ro/core/byte_reader.h"
#include "ro/formats/rsw.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::ErrorCode;
using core::Fail;

constexpr std::string_view kGatSignature{"GRAT", 4};

// Les cartes officielles les plus grandes tiennent largement en dessous ; le
// plafond ne sert qu'à refuser une dimension corrompue avant qu'elle ne devienne
// une allocation de plusieurs gibioctets.
constexpr std::int32_t kMaxDimension = 8192;

}  // namespace

core::Result<GatMap> GatMap::Parse(std::span<const std::uint8_t> data) {
  ByteReader reader(data);

  const std::span<const std::uint8_t> signature_bytes = reader.Bytes(kGatSignature.size());
  const std::string_view signature(reinterpret_cast<const char*>(signature_bytes.data()),
                                   signature_bytes.size());
  if (signature != kGatSignature) {
    return Fail(ErrorCode::kBadSignature, "en-tête GAT attendu « GRAT »");
  }

  GatMap map;
  map.major_version_ = reader.U8();
  map.minor_version_ = reader.U8();
  map.width_ = reader.I32();
  map.height_ = reader.I32();
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "en-tête GAT tronqué");
  }

  if (map.width_ <= 0 || map.height_ <= 0 || map.width_ > kMaxDimension ||
      map.height_ > kMaxDimension) {
    return Fail(ErrorCode::kCorruptData,
                "dimensions GAT " + std::to_string(map.width_) + "x" + std::to_string(map.height_));
  }

  const std::size_t cell_count =
      static_cast<std::size_t>(map.width_) * static_cast<std::size_t>(map.height_);
  if (reader.remaining() < cell_count * kGatCellSize) {
    return Fail(ErrorCode::kUnexpectedEnd, "GAT annonce " + std::to_string(cell_count) +
                                               " cellules, le fichier n'en porte que " +
                                               std::to_string(reader.remaining() / kGatCellSize));
  }

  map.cells_.resize(cell_count);
  for (GatCell& cell : map.cells_) {
    cell.corner_height[0] = reader.F32();
    cell.corner_height[1] = reader.F32();
    cell.corner_height[2] = reader.F32();
    cell.corner_height[3] = reader.F32();
    cell.type = reader.I32();
  }
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "cellules GAT tronquées");
  }
  return map;
}

std::vector<std::uint8_t> DeriveServerCells(const GatMap& gat, const WaterField& water,
                                            WaterLevelRule rule) {
  std::vector<std::uint8_t> derived;
  derived.reserve(gat.cells().size());

  for (std::int32_t y = 0; y < gat.height(); ++y) {
    for (std::int32_t x = 0; x < gat.width(); ++x) {
      const GatCell& cell = gat.at(x, y);
      const auto raw = static_cast<std::uint32_t>(cell.type);
      const bool flagged_as_water = (raw & kGatWaterFlag) != 0;
      std::uint32_t type = raw & ~kGatWaterFlag;

      if (type == kGatWalkable) {
        if (flagged_as_water) {
          // L'éditeur de carte a marqué la cellule comme eau : pas de calcul,
          // pas de niveau à consulter.
          type = kGatWalkableWater;
        } else if (const std::optional<float> level = water.LevelAt(x, y); level.has_value()) {
          const float water_height = rule == WaterLevelRule::kTruncatedLevel
                                         ? static_cast<float>(static_cast<std::int32_t>(*level))
                                         : *level;
          // Cellule marchable mais sous le niveau de l'eau : elle est reclassée
          // en 3 (eau marchable). Seul le coin BAS-GAUCHE compte, et la
          // comparaison est STRICTE — une cellule pile au niveau reste sèche.
          if (cell.corner_height[0] > water_height) {
            type = kGatWalkableWater;
          }
        }
      }

      // Troncature à un octet, comme le `(unsigned char)type` du serveur :
      // c'est elle qui ramène un type à bit de poids fort (0x80000003) sur sa
      // valeur utile (3).
      derived.push_back(static_cast<std::uint8_t>(type));
    }
  }
  return derived;
}

void GatMap::LevelCornerHeights(float height) noexcept {
  for (GatCell& cell : cells_) {
    cell.corner_height.fill(height);
  }
}

void GatMap::SetCornerHeights(std::int32_t x, std::int32_t y,
                              const std::array<float, 4>& heights) noexcept {
  if (!InBounds(x, y)) {
    return;
  }
  cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
         static_cast<std::size_t>(x)]
      .corner_height = heights;
}

}  // namespace ro::formats
