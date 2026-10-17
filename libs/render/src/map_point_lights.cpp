#include "ro/render/map_point_lights.h"

#include <cmath>

#include "ro/render/edited_map_effects.h"

namespace ro::render {
namespace {

// Les deux ondes du vacillement, en radians par milliseconde, leurs poids, et
// le décalage d'une lumière à la suivante, en radians.
constexpr double kFastWave = 0.0131;
constexpr double kSlowWave = 0.0047;
constexpr float kFastWeight = 0.6F;
constexpr float kSlowWeight = 0.4F;
constexpr double kSlotPhase = 2.399963;

}  // namespace

float FlickerFactor(std::size_t slot, double time_ms) {
  const double phase = static_cast<double>(slot) * kSlotPhase;
  const float wave = kFastWeight * static_cast<float>(std::sin(kFastWave * time_ms + phase)) +
                     kSlowWeight * static_cast<float>(std::sin(kSlowWave * time_ms + 2.0 * phase));
  // L'onde va de −1 à 1 : ramenée de 0 à 1, elle retire jusqu'à la profondeur.
  return 1.0F - kFlickerDepth * (0.5F + 0.5F * wave);
}

std::vector<ro::mapedit::AddedLight> DocumentLights(const ro::mapedit::MapEdit& edit) {
  std::vector<ro::mapedit::AddedLight> lights = edit.lights();
  for (const ro::mapedit::AddedEffect& effect : edit.effects()) {
    if (effect.glow.has_value()) {
      lights.push_back(ro::mapedit::AddedLight{.position = effect.position,
                                               .color = effect.glow->color,
                                               .range = effect.glow->range,
                                               .night_only = effect.night_only,
                                               .flicker = effect.glow->flicker,
                                               .why = effect.why});
    }
  }
  return lights;
}

PointLightSet MapPointLights(const std::vector<ro::mapedit::AddedLight>& lights,
                             const MapEffectFrame& frame, const SceneLight& sky, bool always_night,
                             glm::vec3 eye, double time_ms) {
  const float night = NightStrength(sky, always_night);
  std::vector<PointLight> placed;
  placed.reserve(lights.size());
  for (std::size_t slot = 0; slot < lights.size(); ++slot) {
    const ro::mapedit::AddedLight& light = lights[slot];
    const float strength =
        (light.night_only ? night : 1.0F) * (light.flicker ? FlickerFactor(slot, time_ms) : 1.0F);
    placed.push_back(PointLight{
        .world_position =
            frame.ToWorld(glm::vec3(light.position[0], light.position[1], light.position[2])),
        .color = glm::vec3(light.color[0], light.color[1], light.color[2]) * strength,
        .range_world = light.range});
  }
  return NearestPointLights(placed, eye);
}

}  // namespace ro::render
