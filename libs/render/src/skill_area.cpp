#include "ro/render/skill_area.h"

#include <algorithm>
#include <cmath>

namespace ro::render {
namespace {

// Le marqueur de destination du client : un carré transparent sauf QUATRE
// ÉQUERRES dans ses coins. C'est la même image que le curseur au sol pose.
constexpr std::string_view kCornersTexture = "data\\texture\\grid.tga";
// Le cadre carré dont le client entoure ses propres zones de sort.
constexpr std::string_view kFrameTexture = "data\\texture\\effect\\SquareRange.tga";

// ⚠ CE N'EST PAS UN CHEMIN D'ARCHIVE, et il ne doit ressembler à aucun : le
// carreau plein est un aplat que le rendu bâtit. Le nom ne sert qu'à indexer son
// téléversement, comme celui d'une vraie texture.
constexpr std::string_view kSolidTexture = "<aplat>\\zone_de_sort";

constexpr float kPercentFull = 100.0F;
// Quatre canaux par texel, et l'opacité pleine.
constexpr std::size_t kPixelChannels = 4;
constexpr std::uint8_t kOpaque = 255;

}  // namespace

std::array<std::uint8_t, 4> SkillAreaColorBytes(std::uint32_t color) noexcept {
  return {static_cast<std::uint8_t>(color & 0xFFU), static_cast<std::uint8_t>((color >> 8) & 0xFFU),
          static_cast<std::uint8_t>((color >> 16) & 0xFFU),
          static_cast<std::uint8_t>((color >> 24) & 0xFFU)};
}

float SkillAreaFill(const SkillAreaLook& look) noexcept {
  if (look.pattern != SkillAreaPattern::kSolid) {
    return 1.0F;
  }
  const float gap =
      std::clamp(look.gap_percent, kSmallestSkillAreaGapPercent, kLargestSkillAreaGapPercent);
  return 1.0F - gap / kPercentFull;
}

std::string_view SkillAreaTexturePath(SkillAreaPattern pattern) noexcept {
  switch (pattern) {
    case SkillAreaPattern::kCorners:
      return kCornersTexture;
    case SkillAreaPattern::kFrame:
      return kFrameTexture;
    case SkillAreaPattern::kSolid:
      break;
  }
  return kSolidTexture;
}

bool SkillAreaIsPainted(std::string_view texture_path) noexcept {
  return texture_path == kSolidTexture;
}

namespace {

// LA DISTANCE SIGNÉE À UN CARRÉ AUX COINS ARRONDIS : négative dedans, nulle sur
// le bord, positive dehors.
//
// 🔴 ELLE DONNE LES DEUX D'UN COUP — les coins ronds et le fondu — là où un
// découpage en cas demanderait de traiter à part les quatre angles, et ferait
// apparaître une couture là où deux cas se rejoignent.
[[nodiscard]] float RoundedSquareDistance(float from_center_x, float from_center_y, float half,
                                          float radius) {
  // Le coin du carré RÉDUIT du rayon : au-delà, la forme est un quart de cercle.
  const float toward_x = std::abs(from_center_x) - (half - radius);
  const float toward_y = std::abs(from_center_y) - (half - radius);
  const float outside = std::hypot(std::max(toward_x, 0.0F), std::max(toward_y, 0.0F));
  const float inside = std::min(std::max(toward_x, toward_y), 0.0F);
  return outside + inside - radius;
}

}  // namespace

formats::Image ComposeSkillAreaTile() {
  formats::Image tile;
  tile.width = kSkillAreaTilePixels;
  tile.height = kSkillAreaTilePixels;
  tile.pixels.assign(
      static_cast<std::size_t>(kSkillAreaTilePixels) * kSkillAreaTilePixels * kPixelChannels, 0);
  constexpr float kHalf = static_cast<float>(kSkillAreaTilePixels) / 2.0F;
  // ⚠ LE CENTRE DU TEXEL, ET NON SON COIN : mesurer depuis le coin décale la
  // forme d'un demi-texel, ce qui se voit sur le fondu comme une asymétrie
  // entre les deux bords opposés.
  constexpr float kTexelCenter = 0.5F;
  for (std::int32_t y = 0; y < kSkillAreaTilePixels; ++y) {
    for (std::int32_t x = 0; x < kSkillAreaTilePixels; ++x) {
      const float distance = RoundedSquareDistance(static_cast<float>(x) + kTexelCenter - kHalf,
                                                   static_cast<float>(y) + kTexelCenter - kHalf,
                                                   kHalf, kSkillAreaTileCornerRadius);
      // LE FONDU DU BORD : la part du texel que la forme couvre. Un demi sur le
      // bord exact, un bien dedans, zéro bien dehors.
      const float covered = std::clamp(0.5F - distance / kSkillAreaTileEdgeSoftness, 0.0F, 1.0F);
      // LE LISERÉ : plein contre le bord, il retombe sur l'intérieur au bout de
      // son épaisseur.
      const float depth = std::max(-distance, 0.0F);
      const float rim = std::clamp(1.0F - depth / kSkillAreaTileEdgeWidth, 0.0F, 1.0F);
      const float inner = static_cast<float>(kSkillAreaTileInnerAlpha);
      const float alpha = (inner + (static_cast<float>(kOpaque) - inner) * rim) * covered;
      const auto at =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(kSkillAreaTilePixels) +
           static_cast<std::size_t>(x)) *
          kPixelChannels;
      tile.pixels[at] = kOpaque;
      tile.pixels[at + 1] = kOpaque;
      tile.pixels[at + 2] = kOpaque;
      tile.pixels[at + 3] = static_cast<std::uint8_t>(std::lround(alpha));
    }
  }
  return tile;
}

std::vector<EffectGroundCellPaint> ComposeSkillAreaCells(const SkillAreaLook& look,
                                                         std::uint16_t width_cells,
                                                         std::uint16_t height_cells) {
  std::vector<EffectGroundCellPaint> cells;
  if (width_cells == 0 || height_cells == 0) {
    return cells;
  }
  cells.reserve(static_cast<std::size_t>(width_cells) * height_cells);
  // ⚠ DIVISION ENTIÈRE, comme le client : `cx + i - largeur / 2`. Une largeur
  // paire n'a pas de milieu, et arrondir autrement décalerait la zone d'une
  // case par rapport à la sienne.
  const auto half_width = static_cast<std::int32_t>(width_cells / 2);
  const auto half_height = static_cast<std::int32_t>(height_cells / 2);
  for (std::uint16_t along_x = 0; along_x < width_cells; ++along_x) {
    for (std::uint16_t along_y = 0; along_y < height_cells; ++along_y) {
      EffectGroundCellPaint paint;
      paint.cell_offset_x = static_cast<std::int32_t>(along_x) - half_width;
      paint.cell_offset_y = static_cast<std::int32_t>(along_y) - half_height;
      // L'IMAGE ENTIÈRE SUR CHAQUE CASE : le motif se répète case par case, il
      // ne s'étire pas sur la zone. C'est ce qui fait qu'une zone de neuf sur
      // neuf se lit comme neuf carreaux et non comme un grand carré.
      paint.texture_uv = {glm::vec2{0.0F, 0.0F}, glm::vec2{1.0F, 0.0F}, glm::vec2{0.0F, 1.0F},
                          glm::vec2{1.0F, 1.0F}};
      paint.color = SkillAreaColorBytes(look.color);
      cells.push_back(paint);
    }
  }
  return cells;
}

}  // namespace ro::render
