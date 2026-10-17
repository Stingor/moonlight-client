#include "ro/render/edited_map_effects.h"

#include <algorithm>
#include <string_view>

#include "ro/render/scene_light.h"

namespace ro::render {
namespace {

// Le nom d'un objet que le document ajoute : il ne vient d'aucun RSW.
constexpr std::string_view kAddedEffectName = "map_edit";

}  // namespace

float NightStrength(const SceneLight& sky, bool always_night) {
  if (always_night) {
    return 1.0F;
  }
  const glm::vec3 day = sky.daylight * sky.tint;
  return 1.0F - std::clamp(std::max({day.r, day.g, day.b}), 0.0F, 1.0F);
}

std::vector<formats::RswEffect> ActiveMapEffects(std::span<const formats::RswEffect> map_objects,
                                                 const std::vector<mapedit::AddedEffect>& added,
                                                 float night) {
  std::vector<formats::RswEffect> objects(map_objects.begin(), map_objects.end());
  for (const mapedit::AddedEffect& effect : added) {
    if (effect.night_only && night < kEffectNightThreshold) {
      continue;
    }
    objects.push_back(formats::RswEffect{.name = std::string(kAddedEffectName),
                                         .position = effect.position,
                                         .number = effect.number,
                                         .emit_speed = effect.emit_speed,
                                         .param = effect.param});
  }
  return objects;
}

bool SameMapEffects(std::span<const formats::RswEffect> left,
                    std::span<const formats::RswEffect> right) {
  return std::ranges::equal(left, right,
                            [](const formats::RswEffect& one, const formats::RswEffect& other) {
                              return one.name == other.name && one.position == other.position &&
                                     one.number == other.number &&
                                     one.emit_speed == other.emit_speed && one.param == other.param;
                            });
}

}  // namespace ro::render
