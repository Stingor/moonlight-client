#include "ro/render/map_point_lights.h"

#include <array>
#include <cmath>

#include "ro/mapedit/model_light.h"
#include "ro/render/edited_map_effects.h"
#include "ro/render/map_scene.h"

namespace ro::render {
namespace {

// Les deux ondes du vacillement, en radians par milliseconde, leurs poids, et
// le décalage d'une lumière à la suivante, en radians.
constexpr double kFastWave = 0.0131;
constexpr double kSlowWave = 0.0047;
constexpr float kFastWeight = 0.6F;
constexpr float kSlowWeight = 0.4F;
constexpr double kSlotPhase = 2.399963;

// Une position du RSW soulevée à la hauteur d'une flamme : un y négatif monte.
std::array<float, 3> Lifted(std::array<float, 3> anchor) {
  anchor[1] -= kEffectGlowLiftWorld;
  return anchor;
}

}  // namespace

float FlickerFactor(std::size_t slot, double time_ms) {
  const double phase = static_cast<double>(slot) * kSlotPhase;
  const float wave = kFastWeight * static_cast<float>(std::sin(kFastWave * time_ms + phase)) +
                     kSlowWeight * static_cast<float>(std::sin(kSlowWave * time_ms + 2.0 * phase));
  // L'onde va de −1 à 1 : ramenée de 0 à 1, elle retire jusqu'à la profondeur.
  return 1.0F - kFlickerDepth * (0.5F + 0.5F * wave);
}

std::vector<ro::mapedit::AddedLight> SceneLights(const ro::mapedit::MapEdit& edit,
                                                 const ro::mapedit::MapEdit& model_book,
                                                 const MapScene& scene) {
  std::vector<ro::mapedit::AddedLight> lights = DocumentLights(edit, scene.rsw.effects());
  const std::vector<ro::mapedit::AddedLight> carried = ro::mapedit::ModelLightsOn(
      model_book, scene.rsw.models(), scene.model_keys, scene.hidden_models, scene.unlit_models);
  lights.insert(lights.end(), carried.begin(), carried.end());
  return lights;
}

std::vector<ro::mapedit::AddedLight> DocumentLights(
    const ro::mapedit::MapEdit& edit, std::span<const ro::formats::RswEffect> map_objects) {
  std::vector<ro::mapedit::AddedLight> lights = edit.lights();
  for (const ro::mapedit::EffectEdit& retouch : edit.effect_edits()) {
    if (!retouch.glow.has_value() || retouch.hidden || retouch.effect_index >= map_objects.size() ||
        !ro::mapedit::EffectEditTargets(retouch, map_objects[retouch.effect_index])) {
      continue;
    }
    lights.push_back(ro::mapedit::AddedLight{
        .position = Lifted(retouch.position.value_or(map_objects[retouch.effect_index].position)),
        .color = retouch.glow->color,
        .range = retouch.glow->range,
        .night_only = retouch.night_only,
        .hours = retouch.hours,
        .flicker = retouch.glow->flicker,
        .shadow = retouch.glow->shadow,
        .why = retouch.why});
  }
  for (const ro::mapedit::AddedEffect& effect : edit.effects()) {
    if (effect.glow.has_value()) {
      lights.push_back(ro::mapedit::AddedLight{.position = Lifted(effect.position),
                                               .color = effect.glow->color,
                                               .range = effect.glow->range,
                                               .night_only = effect.night_only,
                                               .hours = effect.hours,
                                               .flicker = effect.glow->flicker,
                                               .shadow = effect.glow->shadow,
                                               .why = effect.why});
    }
  }
  return lights;
}

PointLightSet MapPointLights(const std::vector<ro::mapedit::AddedLight>& lights,
                             const MapEffectFrame& frame, const SceneLight& sky, bool always_night,
                             glm::vec3 eye, double time_ms, std::optional<float> hour) {
  const float night = NightStrength(sky, always_night);
  std::vector<PointLight> placed;
  placed.reserve(lights.size());
  for (std::size_t slot = 0; slot < lights.size(); ++slot) {
    const ro::mapedit::AddedLight& light = lights[slot];
    const float dark = always_night ? night : NightGlow(light.hours, night, hour);
    const float strength =
        (light.night_only ? dark : 1.0F) * (light.flicker ? FlickerFactor(slot, time_ms) : 1.0F);
    placed.push_back(PointLight{
        .world_position =
            frame.ToWorld(glm::vec3(light.position[0], light.position[1], light.position[2])),
        .color = glm::vec3(light.color[0], light.color[1], light.color[2]) * strength,
        .range_world = light.range,
        .casts_shadow = light.shadow});
  }
  return PointLightsByDistance(placed, eye);
}

}  // namespace ro::render
