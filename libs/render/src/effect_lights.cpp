#include "ro/render/effect_lights.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include "ro/render/effect_laid.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_scene.h"

namespace ro::render {
namespace {

// Les poids de la luminance, ceux de la Rec. 709.
constexpr glm::vec3 kLuminanceWeights{0.2126F, 0.7152F, 0.0722F};
constexpr float kChannelMax = 255.0F;
constexpr std::size_t kRgbaChannels = 4;
constexpr std::size_t kAlphaChannel = 3;

// Les facteurs de Direct3D que la règle d'addition lit, dans l'ordre de
// D3DBLEND.
constexpr std::int32_t kBlendZero = 1;
constexpr std::int32_t kBlendDestinationAlpha = 7;
constexpr std::int32_t kBlendSourceAlphaSaturate = 11;

// UNE COUCHE AJOUTE SA LUMIÈRE quand sa destination garde le fond tel quel et
// que sa source n'est pas nulle. La destination vaut alors un, l'alpha de la
// destination — la scène est peinte sur une cible opaque : c'est le couple
// dominant des fichiers —, ou un facteur que Direct3D ne connaît pas, que le
// renderer peint comme un.
bool AddsLight(std::int32_t source, std::int32_t destination) {
  const bool keeps_background = destination == kEffectRenderBlendOne ||
                                destination == kBlendDestinationAlpha || destination < kBlendZero ||
                                destination > kBlendSourceAlphaSaturate;
  return keeps_background && source != kBlendZero;
}

// CE QU'UNE COUCHE AJOUTE à la lumière de son effet, avant qu'on les réunisse.
struct LayerGlow {
  glm::vec3 emission{0.0F};
  glm::vec3 centre{0.0F};
  float half_size_world = 0.0F;
};

// LA SOMME des couches d'un effet : la position et la taille se pèsent par
// l'intensité de chacune.
class GlowSum {
 public:
  void Add(const LayerGlow& layer) {
    const float weight = std::max({layer.emission.r, layer.emission.g, layer.emission.b});
    if (weight <= 0.0F) {
      return;
    }
    emission_ += layer.emission;
    weighted_centre_ += layer.centre * weight;
    weighted_size_ += layer.half_size_world * weight;
    weight_ += weight;
  }

  void Emit(std::vector<PointLight>& out) const {
    glm::vec3 color = emission_ * kEffectLightGain;
    const float brightest = std::max({color.r, color.g, color.b});
    if (brightest < kEffectLightMinStrength) {
      return;
    }
    if (brightest > kEffectLightPeak) {
      color *= kEffectLightPeak / brightest;
    }
    out.push_back(PointLight{
        .world_position = weighted_centre_ / weight_,
        .color = color,
        .range_world = std::clamp(weighted_size_ / weight_ * kEffectLightRangePerSize,
                                  kEffectLightMinRangeWorld, kEffectLightMaxRangeWorld)});
  }

 private:
  glm::vec3 emission_{0.0F};
  glm::vec3 weighted_centre_{0.0F};
  float weighted_size_ = 0.0F;
  float weight_ = 0.0F;
};

// La lumière d'une couche additive : la lueur de sa texture, teintée de sa
// couleur, pesée par son alpha et par sa taille jusqu'à la pleine lueur.
LayerGlow LayerOf(const TextureGlow& glow, const std::array<std::uint8_t, 4>& tint, float alpha,
                  const std::array<glm::vec3, 4>& corners) {
  const glm::vec3 tint_color = glm::vec3(static_cast<float>(tint[0]), static_cast<float>(tint[1]),
                                         static_cast<float>(tint[2])) /
                               kChannelMax;
  const float tint_alpha = alpha * static_cast<float>(tint[kAlphaChannel]) / kChannelMax;
  // Les coins vont dans l'ordre d'un quad : le premier et le troisième se font
  // face.
  const float half_size = glm::length(corners[2] - corners[0]) * 0.5F;
  const float size_share = std::min(half_size / kEffectLightFullSizeWorld, 1.0F);
  return LayerGlow{.emission = glow.color * tint_color * (glow.strength * tint_alpha * size_share),
                   .centre = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25F,
                   .half_size_world = half_size};
}

}  // namespace

TextureGlow GlowOfImage(const formats::Image& image) {
  const std::size_t texels = image.pixels.size() / kRgbaChannels;
  if (texels == 0) {
    return {};
  }
  glm::vec3 weighted_color{0.0F};
  float coverage = 0.0F;
  for (std::size_t texel = 0; texel < texels; ++texel) {
    const std::uint8_t* rgba = &image.pixels[texel * kRgbaChannels];
    const glm::vec3 color = glm::vec3(static_cast<float>(rgba[0]), static_cast<float>(rgba[1]),
                                      static_cast<float>(rgba[2])) /
                            kChannelMax;
    const float luminance = glm::dot(color, kLuminanceWeights);
    if (luminance < kGlowTexelLuminance) {
      continue;
    }
    const float weight = luminance * static_cast<float>(rgba[kAlphaChannel]) / kChannelMax;
    weighted_color += color * weight;
    coverage += weight;
  }
  coverage /= static_cast<float>(texels);
  if (coverage < kGlowMinCoverage) {
    return {};
  }
  return TextureGlow{.color = weighted_color / (coverage * static_cast<float>(texels)),
                     .strength = std::min(coverage / kGlowFullCoverage, 1.0F)};
}

const TextureGlow& TextureGlowCache::Of(const std::string& path, const formats::Image& image) {
  auto found = glows_.find(path);
  if (found == glows_.end()) {
    found = glows_.emplace(path, GlowOfImage(image)).first;
  }
  return found->second;
}

void EffectLightsOf(std::span<const DrawnEffect> effects,
                    std::span<const DrawnWorldQuads> world_lots, const BillboardBasis& basis,
                    TextureGlowCache& glows, std::vector<PointLight>& out) {
  std::vector<EffectQuad> quads;
  for (const DrawnEffect& drawn : effects) {
    if (drawn.effect == nullptr) {
      continue;
    }
    const LoadedEffect& loaded = *drawn.effect;
    ComposeEffect(loaded.prepared, drawn.frame, quads);
    // L'ancre se relève le long de l'axe du panneau, comme au dessin.
    const glm::vec3 anchor =
        drawn.ground_position + basis.up * (drawn.lift_pixels / kSpritePixelsPerWorldUnit);
    GlowSum sum;
    for (const EffectQuad& quad : quads) {
      if (!AddsLight(quad.blend_source, quad.blend_destination) ||
          quad.texture >= loaded.textures.size() || loaded.textures[quad.texture].empty()) {
        continue;
      }
      const TextureGlow& glow =
          glows.Of(loaded.texture_paths[quad.texture], loaded.textures[quad.texture]);
      if (glow.strength <= 0.0F) {
        continue;
      }
      std::array<glm::vec2, 4> pixels = quad.corners;
      ShapeStandingEffectCorners(pixels, drawn.lift_pixels, drawn.spin_degrees, drawn.scale,
                                 drawn.flip_x, drawn.flip_y);
      sum.Add(LayerOf(glow, quad.tint, drawn.alpha, BillboardCorners(pixels, anchor, basis)));
    }
    sum.Emit(out);
  }
  for (const DrawnWorldQuads& lot : world_lots) {
    if (lot.texture == nullptr || lot.texture_path == nullptr || lot.texture->empty()) {
      continue;
    }
    const TextureGlow& glow = glows.Of(*lot.texture_path, *lot.texture);
    if (glow.strength <= 0.0F) {
      continue;
    }
    GlowSum sum;
    for (const EffectWorldQuad& quad : lot.quads) {
      if (const EffectRenderBlend blend =
              EffectWorldQuadBlend(quad.blend_source, quad.blend_destination, lot.render_word);
          !AddsLight(blend.source, blend.destination)) {
        continue;
      }
      sum.Add(LayerOf(glow, quad.tint, 1.0F, quad.corners));
    }
    sum.Emit(out);
  }
}

PointLightSet WithEffectLights(const PointLightSet& document, std::span<const PointLight> effects,
                               glm::vec3 eye) {
  const PointLightSet nearest_effects = NearestPointLights(effects, eye);
  const std::size_t effect_count = std::min(nearest_effects.count, kMaxEffectLights);
  PointLightSet merged = document;
  merged.count = std::min(document.count, kMaxPointLights - effect_count);
  merged.shadowed_count = std::min(merged.shadowed_count, merged.count);
  for (std::size_t rank = 0; rank < effect_count; ++rank) {
    merged.lights[merged.count++] = nearest_effects.lights[rank];
  }
  return merged;
}

}  // namespace ro::render
