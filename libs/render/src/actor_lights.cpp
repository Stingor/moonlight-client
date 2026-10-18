#include "ro/render/actor_lights.h"

#include <algorithm>
#include <cctype>
#include <cstddef>

#include "ro/render/effect_lights.h"
#include "ro/render/map_point_lights.h"

namespace ro::render {
namespace {

[[nodiscard]] bool SameSpriteName(std::string_view left, std::string_view right) {
  return std::ranges::equal(left, right, [](char a, char b) {
    return std::toupper(static_cast<unsigned char>(a)) ==
           std::toupper(static_cast<unsigned char>(b));
  });
}

// LA LUMIÈRE DE `spec` portée par `bearer`, son vacillement décalé par `slot`.
[[nodiscard]] PointLight LightOf(const ActorLightSpec& spec, const ActorLightBearer& bearer,
                                 std::size_t slot, double time_ms) {
  const float flicker = spec.flicker ? FlickerFactor(slot, time_ms) : 1.0F;
  PointLight light;
  light.world_position =
      bearer.feet_world + glm::vec3(0.0F, spec.height_world * bearer.scale, 0.0F);
  light.color = spec.color * (bearer.opacity * flicker);
  light.range_world = spec.range_world;
  light.casts_shadow = spec.casts_shadow;
  if (spec.casts_shadow) {
    light.shadow_depth = kEffectShadowDepth;
    light.shadow_spread = kEffectShadowSpread;
  }
  return light;
}

}  // namespace

ActorLightCatalogue ActorLightCatalogue::FromClassSprites(
    const std::unordered_map<std::uint32_t, std::string>& class_sprites) {
  ActorLightCatalogue catalogue;
  for (const auto& [job, sprite] : class_sprites) {
    for (const SpriteLight& entry : SpriteLightCatalogue()) {
      if (SameSpriteName(sprite, entry.sprite)) {
        catalogue.by_job_.emplace(job, &entry.light);
        break;
      }
    }
  }
  return catalogue;
}

const ActorLightSpec* ActorLightCatalogue::OfJob(std::uint32_t job) const {
  const auto found = by_job_.find(job);
  return found != by_job_.end() ? found->second : nullptr;
}

void ActorLightsOf(std::span<const ActorLightBearer> bearers, const ActorLightCatalogue& catalogue,
                   double time_ms, std::vector<PointLight>& out) {
  const std::span<const OptionLight> states = OptionLightCatalogue();
  for (const ActorLightBearer& bearer : bearers) {
    if (bearer.opacity < kActorLightMinOpacity) {
      continue;
    }
    // Chaque lumière d'un même porteur a son rang : la lampe et les feux de
    // Sight ne battent pas ensemble.
    std::size_t slot = static_cast<std::size_t>(bearer.seed) * (states.size() + 1);
    if (const ActorLightSpec* spec = catalogue.OfJob(bearer.job); spec != nullptr) {
      out.push_back(LightOf(*spec, bearer, slot, time_ms));
    }
    for (const OptionLight& state : states) {
      ++slot;
      if ((bearer.option & state.option_mask) != 0) {
        out.push_back(LightOf(state.light, bearer, slot, time_ms));
      }
    }
  }
}

PointLightSet WithActorLights(const PointLightSet& set, std::span<const PointLight> actors,
                              glm::vec3 eye) {
  const PointLightSet nearest = PointLightsByDistance(actors, eye);
  const std::size_t count = std::min(nearest.lights.size(), kMaxActorLights);
  PointLightSet merged = set;
  merged.lights.insert(merged.lights.end(), nearest.lights.begin(),
                       nearest.lights.begin() + static_cast<std::ptrdiff_t>(count));
  return merged;
}

}  // namespace ro::render
