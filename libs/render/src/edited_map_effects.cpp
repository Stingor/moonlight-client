#include "ro/render/edited_map_effects.h"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>

#include "ro/mapedit/model_light.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"

namespace ro::render {
namespace {

// Le nom d'un objet que le document ajoute : il ne vient d'aucun RSW.
constexpr std::string_view kAddedEffectName = "map_edit";

// Ajoute à `objects` les effets `added` qui se lèvent à cette nuit.
void RaiseAdded(std::span<const mapedit::AddedEffect> added, float night, std::optional<float> hour,
                std::vector<formats::RswEffect>& objects) {
  for (const mapedit::AddedEffect& effect : added) {
    if (effect.night_only && !LitAtNight(effect.hours, night, hour)) {
      continue;
    }
    objects.push_back(formats::RswEffect{.name = std::string(kAddedEffectName),
                                         .position = effect.position,
                                         .number = effect.number,
                                         .emit_speed = effect.emit_speed,
                                         .param = effect.param});
  }
}

}  // namespace

float NightStrength(const SceneLight& sky, bool always_night) {
  if (always_night) {
    return 1.0F;
  }
  const glm::vec3 day = sky.daylight * sky.tint;
  return 1.0F - std::clamp(std::max({day.r, day.g, day.b}), 0.0F, 1.0F);
}

bool LitAtNight(const std::optional<mapedit::LitHours>& hours, float night,
                std::optional<float> hour) {
  if (hours.has_value() && hour.has_value()) {
    return mapedit::LitHoursCover(*hours, *hour);
  }
  return night >= kEffectNightThreshold;
}

float NightGlow(const std::optional<mapedit::LitHours>& hours, float night,
                std::optional<float> hour) {
  if (hours.has_value() && hour.has_value()) {
    return mapedit::LitHoursStrength(*hours, *hour);
  }
  return night;
}

std::vector<formats::RswEffect> ActiveMapEffects(std::span<const formats::RswEffect> map_objects,
                                                 const mapedit::MapEdit& document, float night,
                                                 std::optional<float> hour) {
  std::vector<formats::RswEffect> objects;
  objects.reserve(map_objects.size() + document.effects().size());
  for (std::size_t rank = 0; rank < map_objects.size(); ++rank) {
    formats::RswEffect object = map_objects[rank];
    const mapedit::EffectEdit* edit = document.EffectEditAt(rank);
    if (edit != nullptr && mapedit::EffectEditTargets(*edit, object)) {
      if (edit->hidden || (edit->night_only && !LitAtNight(edit->hours, night, hour))) {
        continue;
      }
      object.position = edit->position.value_or(object.position);
    }
    objects.push_back(std::move(object));
  }
  RaiseAdded(document.effects(), night, hour, objects);
  return objects;
}

std::vector<formats::RswEffect> SceneMapEffects(const MapScene& scene,
                                                const mapedit::MapEdit& document,
                                                const mapedit::MapEdit& model_book, float night,
                                                std::optional<float> hour) {
  std::vector<formats::RswEffect> objects =
      ActiveMapEffects(scene.rsw.effects(), document, night, hour);
  RaiseAdded(mapedit::ModelEffectsOn(model_book, scene.rsw.models(), scene.model_keys,
                                     scene.hidden_models, scene.unlit_models),
             night, hour, objects);
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
