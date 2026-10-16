#include "ro/render/ground_light.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace ro::render {
namespace {

// Deux cases de marche par carreau de sol, dans chaque sens.
constexpr std::int32_t kCellsPerCube = 2;

// Une case de marche peut dépasser le sol de cette hauteur et prendre encore sa
// lumière. Les hauteurs croissent vers le BAS : « au-dessus » est un nombre plus
// petit.
constexpr float kPerchedMargin = 2.5F;

// Le texel qu'une case lit dans la lightmap de son carreau : le premier du
// rebord intérieur pour la case basse, quatre plus loin pour la haute. Dans une
// lightmap de huit texels de côté, ce sont les colonnes 1 et 5.
constexpr std::int32_t kFirstTexel = 1;
constexpr std::int32_t kTexelStepPerCell = 4;

// Les cinq cases lues pour une : elle-même, puis ses voisines en croix.
constexpr std::array<std::array<std::int32_t, 2>, 5> kCrossOffsets = {
    {{0, 0}, {0, -1}, {0, 1}, {1, 0}, {-1, 0}}};

// Ce qu'un texel absent vaut : ombre nulle, couleur pleine, sur les quatre
// canaux. C'est ce qu'une case hors carte ou sans lightmap apporte à la moyenne.
constexpr std::uint8_t kMissingTexel = 0xFF;

// Les deux échelles de la somme : l'ombre sur 255, la couleur posée sur 128.
constexpr float kShadowScale = 255.0F;
constexpr float kColorScale = 128.0F;

constexpr std::array<float, 3> kWhite = {1.0F, 1.0F, 1.0F};

// L'ombre, puis le rouge, le vert et le bleu d'un texel.
struct Texel {
  std::uint8_t shadow = kMissingTexel;
  std::array<std::uint8_t, 3> rgb = {kMissingTexel, kMissingTexel, kMissingTexel};
};

// Le texel que la case lit sous elle, ou le texel absent.
Texel TexelUnder(const formats::GndMap& gnd, std::int32_t cell_x, std::int32_t cell_y) {
  if (cell_x < 0 || cell_y < 0) {
    return {};
  }
  const std::int32_t cube_x = cell_x / kCellsPerCube;
  const std::int32_t cube_y = cell_y / kCellsPerCube;
  if (cube_x >= gnd.width() || cube_y >= gnd.height()) {
    return {};
  }
  const std::int32_t tile_index = gnd.at(cube_x, cube_y).tile_up;
  if (tile_index < 0 || static_cast<std::size_t>(tile_index) >= gnd.tiles().size()) {
    return {};
  }
  const std::int32_t lightmap_width = gnd.lightmap_width();
  const std::int32_t lightmap_height = gnd.lightmap_height();
  const std::int32_t texel_x = kFirstTexel + kTexelStepPerCell * (cell_x % kCellsPerCube);
  const std::int32_t texel_y = kFirstTexel + kTexelStepPerCell * (cell_y % kCellsPerCube);
  const std::uint16_t lightmap_index =
      gnd.tiles()[static_cast<std::size_t>(tile_index)].lightmap_index;
  if (texel_x >= lightmap_width || texel_y >= lightmap_height ||
      lightmap_index >= gnd.lightmap_count()) {
    return {};
  }
  // Un plan d'ombre, puis la couleur en R, G, B par texel.
  const std::size_t texels =
      static_cast<std::size_t>(lightmap_width) * static_cast<std::size_t>(lightmap_height);
  constexpr std::size_t kBytesPerTexel = 4;
  const std::size_t base = static_cast<std::size_t>(lightmap_index) * texels * kBytesPerTexel;
  const std::size_t texel =
      static_cast<std::size_t>(texel_y) * static_cast<std::size_t>(lightmap_width) +
      static_cast<std::size_t>(texel_x);
  const std::vector<std::uint8_t>& data = gnd.lightmap_data();
  constexpr std::size_t kColorBytes = 3;
  if (base + texels * kBytesPerTexel > data.size()) {
    return {};
  }
  const std::size_t color = base + texels + texel * kColorBytes;
  return Texel{data[base + texel], {data[color], data[color + 1], data[color + 2]}};
}

// Vrai quand la case de marche passe au-dessus du sol plus haut que la marge :
// elle n'est pas éclairée par la lightmap qui est sous elle.
bool IsPerched(const formats::GndMap& gnd, const formats::GatMap& gat, std::int32_t cell_x,
               std::int32_t cell_y) {
  const formats::GndCube& cube = gnd.at(cell_x / kCellsPerCube, cell_y / kCellsPerCube);
  // Sans face du dessus, il n'y a pas de sol à qui prendre la lumière.
  if (cube.tile_up < 0) {
    return true;
  }
  // Le point le plus HAUT du carreau, donc la plus petite hauteur.
  const float ground_top = *std::min_element(cube.corner_height.begin(), cube.corner_height.end());
  float walk_height = 0.0F;
  if (gat.InBounds(cell_x, cell_y)) {
    const std::array<float, 4>& corners = gat.at(cell_x, cell_y).corner_height;
    constexpr float kCorners = 4.0F;
    walk_height = (corners[0] + corners[1] + corners[2] + corners[3]) / kCorners;
  }
  return walk_height < ground_top - kPerchedMargin;
}

std::array<float, 3> LightOf(const formats::GndMap& gnd, std::int32_t cell_x, std::int32_t cell_y,
                             GroundLightTerms terms) {
  std::uint32_t shadow_sum = 0;
  std::array<std::uint32_t, 3> color_sum = {0, 0, 0};
  for (const std::array<std::int32_t, 2>& offset : kCrossOffsets) {
    const Texel texel = TexelUnder(gnd, cell_x + offset[0], cell_y + offset[1]);
    shadow_sum += texel.shadow;
    for (std::size_t channel = 0; channel < color_sum.size(); ++channel) {
      color_sum[channel] += texel.rgb[channel];
    }
  }
  const auto samples = static_cast<float>(kCrossOffsets.size());
  // SANS L'OMBRE CUITE, la case est en pleine lumière : l'ombre du soleil la
  // remplace.
  const float shadow = terms == GroundLightTerms::kLaidLightOnly
                           ? 1.0F
                           : static_cast<float>(shadow_sum) / samples / kShadowScale;
  std::array<float, 3> factors{};
  for (std::size_t channel = 0; channel < factors.size(); ++channel) {
    factors[channel] = static_cast<float>(color_sum[channel]) / samples / kColorScale + shadow;
  }
  const float strongest = *std::max_element(factors.begin(), factors.end());
  if (strongest > 1.0F) {
    for (float& factor : factors) {
      factor /= strongest;
    }
  }
  return factors;
}

}  // namespace

std::array<float, 3> GroundLight::At(std::int32_t cell_x, std::int32_t cell_y) const {
  if (cell_x < 0 || cell_y < 0 || cell_x >= width || cell_y >= height) {
    return kWhite;
  }
  return factors[static_cast<std::size_t>(cell_y) * static_cast<std::size_t>(width) +
                 static_cast<std::size_t>(cell_x)];
}

std::array<float, 3> GroundLight::Under(float cell_x, float cell_y) const {
  constexpr float kHalfCell = 0.5F;
  return At(static_cast<std::int32_t>(std::floor(cell_x + kHalfCell)),
            static_cast<std::int32_t>(std::floor(cell_y + kHalfCell)));
}

GroundLight BuildGroundLight(const formats::GndMap& gnd, const formats::GatMap& gat,
                             GroundLightTerms terms) {
  GroundLight light;
  if (gnd.lightmap_count() <= 0) {
    return light;
  }
  light.width = gnd.width() * kCellsPerCube;
  light.height = gnd.height() * kCellsPerCube;
  light.factors.assign(
      static_cast<std::size_t>(light.width) * static_cast<std::size_t>(light.height), kWhite);
  for (std::int32_t cell_y = 0; cell_y < light.height; ++cell_y) {
    for (std::int32_t cell_x = 0; cell_x < light.width; ++cell_x) {
      if (IsPerched(gnd, gat, cell_x, cell_y)) {
        continue;
      }
      light.factors[static_cast<std::size_t>(cell_y) * static_cast<std::size_t>(light.width) +
                    static_cast<std::size_t>(cell_x)] = LightOf(gnd, cell_x, cell_y, terms);
    }
  }
  return light;
}

std::array<std::uint8_t, 4> LitTint(std::array<std::uint8_t, 4> tint,
                                    const std::array<float, 3>& factors, bool cold) {
  constexpr std::size_t kRed = 0;
  constexpr std::size_t kBlue = 2;
  std::array<float, 3> light = factors;
  if (cold) {
    std::swap(light[kRed], light[kBlue]);
  }
  for (std::size_t channel = 0; channel < light.size(); ++channel) {
    tint[channel] = static_cast<std::uint8_t>(static_cast<float>(tint[channel]) * light[channel]);
  }
  return tint;
}

}  // namespace ro::render
