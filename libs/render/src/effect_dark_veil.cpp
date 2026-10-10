#include "ro/render/effect_dark_veil.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "ro/render/camera.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES QUATRE QUADRANTS DU CŒUR, par le signe de leur coin extérieur à l'écran :
// l'abscisse vers la droite, l'ordonnée vers le bas.
constexpr std::array<std::array<float, 2>, kDarkVeilCoreQuads> kCoreQuadrants = {{
    {{1.0F, 1.0F}},
    {{1.0F, -1.0F}},
    {{-1.0F, 1.0F}},
    {{-1.0F, -1.0F}},
}};

// Pour passer du champ du jeu à son demi-angle, en radians : c'est lui, avec
// le recul, qui fixe la taille apparente d'un objet au centre de l'écran.
constexpr float kHalf = 0.5F;
constexpr float kDegreesPerHalfTurn = 180.0F;

[[nodiscard]] std::uint8_t ChannelOrOwner(std::uint8_t channel, std::int32_t owner) {
  return owner == kEffectOwnerTintAbsent ? channel : static_cast<std::uint8_t>(owner);
}

// LA PLACE D'IMAGE D'UN COIN DU CŒUR. Le coin transparent de l'image — en bas à
// gauche — se pose au centre ; l'abscisse de l'image court le long de la
// VERTICALE de l'écran, son ordonnée le long de l'horizontale. C'est la
// disposition du jeu, qui pose chaque quadrant couché avant de le redresser.
[[nodiscard]] glm::vec2 CoreTextureAt(glm::vec2 corner, glm::vec2 centre, float radius) {
  const float across = std::abs(corner.x - centre.x) / radius;
  const float along = std::abs(corner.y - centre.y) / radius;
  return {along, 1.0F - across};
}

// Un rectangle aligné, en cycle haut-gauche, bas-gauche, bas-droit, haut-droit,
// et l'image entière dessus.
[[nodiscard]] EffectScreenQuad FrameBand(glm::vec2 top_left, glm::vec2 bottom_right,
                                         const std::array<std::uint8_t, 4>& tint) {
  EffectScreenQuad quad;
  quad.corners = {top_left, glm::vec2{top_left.x, bottom_right.y}, bottom_right,
                  glm::vec2{bottom_right.x, top_left.y}};
  quad.texture_uv = {glm::vec2{0.0F, 0.0F}, glm::vec2{0.0F, 1.0F}, glm::vec2{1.0F, 1.0F},
                     glm::vec2{1.0F, 0.0F}};
  quad.tint = tint;
  quad.blend_mode = kDarkVeilOrdinaryBlend;
  quad.texture = kDarkVeilFrameTexture;
  return quad;
}

}  // namespace

formats::Image DarkVeilCoreJoinedToFrame(const formats::Image& core) {
  constexpr std::size_t kChannels = 4;
  constexpr std::size_t kAlpha = 3;
  constexpr std::uint32_t kOpaque = 255;
  formats::Image joined = core;
  if (core.width <= 0 || core.height <= 0 ||
      core.pixels.size() != static_cast<std::size_t>(core.width) *
                                static_cast<std::size_t>(core.height) * kChannels) {
    return joined;
  }
  const auto width = static_cast<std::size_t>(core.width);
  const auto height = static_cast<std::size_t>(core.height);
  const auto alpha_at = [&core, width](std::size_t column, std::size_t row) {
    return static_cast<std::uint32_t>(core.pixels[(row * width + column) * kChannels + kAlpha]);
  };
  // LE BORD LE PLUS CLAIR, sur les deux côtés qui touchent le cadre.
  std::uint32_t clearest_edge = kOpaque;
  for (std::size_t row = 0; row < height; ++row) {
    clearest_edge = std::min(clearest_edge, alpha_at(width - 1, row));
  }
  for (std::size_t column = 0; column < width; ++column) {
    clearest_edge = std::min(clearest_edge, alpha_at(column, 0));
  }
  if (clearest_edge == 0 || clearest_edge == kOpaque) {
    return joined;
  }
  for (std::size_t texel = 0; texel < width * height; ++texel) {
    std::uint8_t& alpha = joined.pixels[texel * kChannels + kAlpha];
    const std::uint32_t stretched = (alpha * kOpaque + clearest_edge / 2) / clearest_edge;
    alpha = static_cast<std::uint8_t>(std::min(stretched, kOpaque));
  }
  return joined;
}

float DarkVeilScreenRadiusPixels(float radius_at_anchor, float viewport_height) {
  const float half_field =
      std::tan(kGameFovDegrees * kHalf * std::numbers::pi_v<float> / kDegreesPerHalfTurn);
  const float visible_height = 2.0F * kGameViewDistance * half_field;
  return radius_at_anchor * viewport_height / visible_height;
}

std::vector<EffectScreenQuad> ComposeScreenDarkVeilQuads(const EffectTrack& track,
                                                         std::optional<glm::vec2> bearer_pixel,
                                                         float viewport_width,
                                                         float viewport_height,
                                                         const EffectOwnerTint& owner) {
  std::vector<EffectScreenQuad> quads;
  if (!track.active || track.alpha <= 0 || viewport_width <= 0.0F || viewport_height <= 0.0F) {
    return quads;
  }
  const std::uint8_t channel =
      track.mark[kMarkOfVeilShade] == kDarkVeilPaler ? kDarkVeilPaleChannel : kDarkVeilDeepChannel;
  const std::array<std::uint8_t, 4> tint = {
      ChannelOrOwner(channel, owner.red), ChannelOrOwner(channel, owner.green),
      ChannelOrOwner(channel, owner.blue),
      static_cast<std::uint8_t>(std::min<std::int16_t>(track.alpha, kEffectTrackAlphaMax))};

  // SANS PLACE D'ÉCRAN, PAS DE CŒUR : l'écran entier est au cadre.
  if (!bearer_pixel.has_value()) {
    quads.push_back(FrameBand({0.0F, 0.0F}, {viewport_width, viewport_height}, tint));
    return quads;
  }
  const glm::vec2 centre = *bearer_pixel;
  const float radius = DarkVeilScreenRadiusPixels(track.radius_at_anchor, viewport_height);
  if (radius <= 0.0F) {
    quads.push_back(FrameBand({0.0F, 0.0F}, {viewport_width, viewport_height}, tint));
    return quads;
  }

  quads.reserve(kDarkVeilQuads);
  for (const std::array<float, 2>& signs : kCoreQuadrants) {
    const glm::vec2 outer = centre + glm::vec2{signs[0] * radius, signs[1] * radius};
    const glm::vec2 top_left{std::min(centre.x, outer.x), std::min(centre.y, outer.y)};
    const glm::vec2 bottom_right{std::max(centre.x, outer.x), std::max(centre.y, outer.y)};
    EffectScreenQuad quad;
    quad.corners = {top_left, glm::vec2{top_left.x, bottom_right.y}, bottom_right,
                    glm::vec2{bottom_right.x, top_left.y}};
    for (std::size_t corner = 0; corner < quad.corners.size(); ++corner) {
      quad.texture_uv[corner] = CoreTextureAt(quad.corners[corner], centre, radius);
    }
    quad.tint = tint;
    quad.blend_mode = kDarkVeilOrdinaryBlend;
    quad.texture = kDarkVeilCoreTexture;
    quads.push_back(quad);
  }

  // LE CADRE : deux bandes pleine largeur au-dessus et au-dessous du cœur, deux
  // bandes à sa hauteur de part et d'autre. Chacune est bornée à l'écran.
  const float core_top = std::clamp(centre.y - radius, 0.0F, viewport_height);
  const float core_bottom = std::clamp(centre.y + radius, 0.0F, viewport_height);
  const float core_left = std::clamp(centre.x - radius, 0.0F, viewport_width);
  const float core_right = std::clamp(centre.x + radius, 0.0F, viewport_width);
  const std::array<std::array<glm::vec2, 2>, kDarkVeilFrameQuads> bands = {{
      {{glm::vec2{0.0F, 0.0F}, glm::vec2{viewport_width, core_top}}},
      {{glm::vec2{0.0F, core_bottom}, glm::vec2{viewport_width, viewport_height}}},
      {{glm::vec2{0.0F, core_top}, glm::vec2{core_left, core_bottom}}},
      {{glm::vec2{core_right, core_top}, glm::vec2{viewport_width, core_bottom}}},
  }};
  for (const std::array<glm::vec2, 2>& band : bands) {
    if (band[1].x <= band[0].x || band[1].y <= band[0].y) {
      continue;
    }
    quads.push_back(FrameBand(band[0], band[1], tint));
  }
  return quads;
}

}  // namespace ro::render
