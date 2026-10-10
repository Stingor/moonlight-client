#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/formats/image.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/map_effects.h"

namespace ro::render {

// UN LOT D'EFFET DE CARTE, prêt pour le renderer d'effets : il reçoit son
// image, ou celle que la scène a chargée pour sa table. Rien pour un lot sans
// quad, ou dont la texture manque — le client, lui, peindrait le sprite par
// défaut de son cache.
//
// ⚠ LES POINTEURS RENDUS VISENT `lot` ET `textures` : les deux doivent vivre
// jusqu'au dessin.
[[nodiscard]] inline std::optional<DrawnWorldQuads> DrawnMapEffectLot(
    const MapEffectLot& lot, const std::map<std::string, formats::Image>& textures) {
  // `zenable` à zéro : le lot passe par-dessus tout, chaque quad gardant son
  // mélange.
  constexpr std::uint32_t kOverEverything =
      kEffectRenderWordOverEverything | kEffectRenderWordOwnBlend;
  if (lot.quads.empty() || lot.texture == nullptr) {
    return std::nullopt;
  }
  const std::string* path = lot.texture;
  const formats::Image* image = lot.image;
  // Un lot sans image propre prend celle que la scène a chargée pour sa table.
  if (image == nullptr) {
    const auto texture = textures.find(MapEffectTexturePath(*lot.texture));
    if (texture == textures.end()) {
      return std::nullopt;
    }
    path = &texture->first;
    image = &texture->second;
  }
  return DrawnWorldQuads{
      .quads = lot.quads,
      .texture_path = path,
      .texture = image,
      .ground_position = lot.ground_position,
      .render_word = lot.depth_tested ? lot.render_word : kOverEverything,
  };
}

// Tous les lots d'une image, sauf ceux que `DrawnMapEffectLot` laisse de côté.
[[nodiscard]] inline std::vector<DrawnWorldQuads> DrawnMapEffectLots(
    std::span<const MapEffectLot> lots, const std::map<std::string, formats::Image>& textures) {
  std::vector<DrawnWorldQuads> drawn;
  drawn.reserve(lots.size());
  for (const MapEffectLot& lot : lots) {
    if (std::optional<DrawnWorldQuads> one = DrawnMapEffectLot(lot, textures)) {
      drawn.push_back(*one);
    }
  }
  return drawn;
}

}  // namespace ro::render
