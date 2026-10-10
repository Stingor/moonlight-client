#include "ro/render/lightmap_atlas.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ro::render {
namespace {

// Nombre d'octets par texel dans une lightmap : un d'alpha, puis trois de
// couleur. Le plan alpha précède la couleur, il n'est pas entrelacé avec elle.
constexpr std::size_t kBytesPerLightmapTexel = 4;

// Où commence et jusqu'où va le parcours d'une lightmap, en texels.
//
// Une lightmap fait huit texels de côté, mais un carreau n'en occupe que SIX :
// le tour est un BORD, qui reprend les texels des lightmaps voisines. Mesuré
// sur gon_fild01, sur quinze cents paires de carreaux voisins dont les deux
// ombres varient : la colonne 6 de l'un vaut EXACTEMENT la colonne 0 de son
// voisin de droite, et la colonne 7 sa colonne 1 — écart moyen nul, contre
// vingt-trois niveaux pour tout autre appariement.
//
// C'est ce bord qui rend l'ombre continue d'un carreau à l'autre : au ras de
// la frontière, le filtrage bilinéaire mélange les colonnes 6 et 7 d'un côté,
// 0 et 1 de l'autre, et les deux mélanges portent les mêmes valeurs.
//
// Les deux réglages se posent donc SÉPARÉMENT, et pas comme un retrait qu'on
// partagerait entre les bords : une marge d'un texel entier, une étendue de
// six. Rogner moins — commencer à un demi texel et en parcourir sept — fait
// apparaître un quadrillage fin sur tout le sol, celui des frontières de
// carreaux ; ne rien rogner en fait un plus marqué encore, pris aux cases
// voisines de l'atlas. Les trois ont été rendus et comparés.
constexpr float kMarginTexels = 1.0F;
constexpr float kSpanTexels = 6.0F;

}  // namespace

std::array<float, 2> LightmapAtlas::MinimumOf(std::size_t index) const {
  return index < minimum.size() ? minimum[index] : std::array<float, 2>{0.0F, 0.0F};
}

std::array<float, 2> LightmapAtlas::MaximumOf(std::size_t index) const {
  return index < maximum.size() ? maximum[index] : std::array<float, 2>{0.0F, 0.0F};
}

LightmapAtlas BuildLightmapAtlas(const formats::GndMap& gnd) {
  LightmapAtlas atlas;

  const std::int32_t count = gnd.lightmap_count();
  const std::int32_t cell_width = gnd.lightmap_width();
  const std::int32_t cell_height = gnd.lightmap_height();
  if (count <= 0 || cell_width <= 0 || cell_height <= 0 || gnd.lightmap_data().empty()) {
    return atlas;
  }

  // CÔTÉ DE L'ATLAS : la plus petite grille carrée qui range TOUTES les
  // lightmaps, et rien de plus. Le mesurer sur les dimensions de la carte
  // réservait plus de quarante mégaoctets à une carte de 200 × 200 qui en
  // occupe moins de dix — en mémoire, puis autant en mémoire graphique.
  auto per_side = static_cast<std::int32_t>(std::sqrt(static_cast<double>(count)));
  while (static_cast<std::int64_t>(per_side) * per_side < count) {
    ++per_side;
  }
  const std::int32_t side = per_side * std::max(cell_width, cell_height);
  if (side <= 0) {
    return atlas;
  }

  atlas.image.width = side;
  atlas.image.height = side;
  atlas.image.pixels.assign(static_cast<std::size_t>(side) * static_cast<std::size_t>(side) * 4, 0);

  const std::size_t texels_per_lightmap =
      static_cast<std::size_t>(cell_width) * static_cast<std::size_t>(cell_height);
  const std::size_t bytes_per_lightmap = texels_per_lightmap * kBytesPerLightmapTexel;
  const std::vector<std::uint8_t>& source = gnd.lightmap_data();

  std::int32_t origin_x = 0;
  std::int32_t origin_y = 0;
  for (std::int32_t index = 0; index < count; ++index) {
    const std::size_t base = static_cast<std::size_t>(index) * bytes_per_lightmap;
    if (base + bytes_per_lightmap <= source.size() && origin_y + cell_height <= side) {
      const std::uint8_t* alpha = source.data() + base;
      const std::uint8_t* color = alpha + texels_per_lightmap;

      for (std::int32_t y = 0; y < cell_height; ++y) {
        for (std::int32_t x = 0; x < cell_width; ++x) {
          const std::size_t destination =
              (static_cast<std::size_t>(origin_x + x) +
               static_cast<std::size_t>(side) * static_cast<std::size_t>(origin_y + y)) *
              4;
          // La couleur source est rangée en RGB, comme l'atlas. ⚠ LA LIRE EN BGR
          // échange le rouge et le bleu : la lumière chaude d'une lanterne sort
          // cyan, et toute la carte vire au froid.
          atlas.image.pixels[destination] = color[0];
          atlas.image.pixels[destination + 1] = color[1];
          atlas.image.pixels[destination + 2] = color[2];
          atlas.image.pixels[destination + 3] = *alpha;
          color += 3;
          ++alpha;
        }
      }
    }

    origin_x += cell_width;
    if (origin_x + cell_width > side) {
      origin_x = 0;
      origin_y += cell_height;
    }
  }

  // Coordonnées de texture : la marge dit où le carreau commence, l'étendue
  // combien il parcourt.
  const auto side_float = static_cast<float>(side);
  const float unit = 1.0F / side_float;
  const float margin = kMarginTexels * unit;
  const float step_x = static_cast<float>(cell_width) / side_float;
  const float step_y = static_cast<float>(cell_height) / side_float;
  const float span_x = kSpanTexels * unit;
  const float span_y = kSpanTexels * unit;
  const std::int32_t per_row = side / cell_width;

  atlas.minimum.reserve(static_cast<std::size_t>(count));
  atlas.maximum.reserve(static_cast<std::size_t>(count));
  for (std::int32_t index = 0; index < count; ++index) {
    const float x = static_cast<float>(index % per_row) * step_x + margin;
    const float y = static_cast<float>(index / per_row) * step_y + margin;
    atlas.minimum.push_back({x, y});
    atlas.maximum.push_back({x + span_x, y + span_y});
  }

  return atlas;
}

}  // namespace ro::render
