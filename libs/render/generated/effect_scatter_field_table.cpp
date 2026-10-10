// CE QU'UN STYLE DE CHAMP DISPERSÉ SÈME, et quel effet emploie quel style.
//
// Une ligne par valeur de style.
//
// 14 styles pour 15 nœuds répartis sur 12 effets. Les textures viennent du
// BÂTISSEUR et non du site d'appel : il les choisit par branche.
//
// ⚠ LE COMPTE DE NŒUDS VIENT DE LA BOUCLE qui entoure l'appel, pas de ses
// arguments : il est transcrit dans le générateur, entrée par entrée.
//
// ENGENDRÉ par scripts/gen_client_effect_scatter_fields.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>
#include <string_view>

#include "ro/render/effect_scatter_seed.h"

namespace ro::render {
namespace {

constexpr std::array<std::string_view, 12> kScatterTextures = {{
    "data\\texture\\effect\\cloud4.tga",
    "data\\texture\\effect\\cloud1.tga",
    "data\\texture\\effect\\cloud2.tga",
    "data\\texture\\effect\\fog1.tga",
    "data\\texture\\effect\\fog2.tga",
    "data\\texture\\effect\\fog3.tga",
    "data\\texture\\effect\\star01.bmp",
    "data\\texture\\effect\\star02.bmp",
    "data\\texture\\effect\\star03.bmp",
    "data\\texture\\effect\\star04.bmp",
    "data\\texture\\effect\\star05.bmp",
    "data\\texture\\effect\\star06.bmp",
}};

constexpr std::array<EffectScatterStyleSeed, 14> kScatterStyles = {{
    {0, 0, 0, 3},
    {1, 0, 0, 3},
    {2, 0, 0, 3},
    {3, 12, 3, 3},
    {4, 0, 0, 3},
    {5, 14, 0, 3},
    {7, 16, 0, 3},
    {8, 17, 0, 3},
    {9, 23, 6, 6},
    {11, 14, 0, 3},
    {12, 16, 0, 3},
    {13, 17, 0, 3},
    {14, 24, 0, 3},
    {15, 25, 6, 6},
}};

constexpr std::array<EffectScatterNodePattern, 15> kScatterNodes = {{
    {229, "Effect_Build_CloudFogStarField", 0, 40},
    {230, "Effect_Build_CloudFogStarField", 1, 60},
    {233, "Effect_Build_CloudFogStarField", 2, 40},
    {515, "Effect_Build_CloudFogStarField", 3, 80},
    {516, "Effect_Build_CloudFogStarField", 4, 80},
    {592, "Effect_Build_CloudFogStarField", 5, 80},
    {697, "Effect_Build_CloudFogStarField", 7, 80},
    {698, "Effect_Build_CloudFogStarField", 8, 80},
    {945, "Effect_Build_CloudFogStarField", 9, 65},
    {1045, "Effect_Build_CloudFogStarField", 5, 30},
    {1115, "Effect_Build_CloudFogStarField", 11, 40},
    {1115, "Effect_Build_CloudFogStarField", 12, 40},
    {1115, "Effect_Build_CloudFogStarField", 13, 40},
    {1115, "Effect_Build_CloudFogStarField", 14, 40},
    {1145, "Effect_Build_CloudFogStarField", 15, 65},
}};

}  // namespace

std::span<const std::string_view> EffectScatterStyleTextures(const EffectScatterStyleSeed& seed) {
  return std::span<const std::string_view>(kScatterTextures)
      .subspan(seed.first_texture, seed.texture_count);
}

std::span<const EffectScatterStyleSeed> EffectScatterStyleSeedTable() { return kScatterStyles; }

std::span<const EffectScatterNodePattern> EffectScatterNodePatternTable() { return kScatterNodes; }

std::span<const EffectScatterNodePattern> EffectScatterNodePatternsFor(std::uint32_t effect_id) {
  const auto [first, last] =
      std::ranges::equal_range(kScatterNodes, effect_id, {}, &EffectScatterNodePattern::effect_id);
  return {first, last};
}

}  // namespace ro::render
