#include "world/map_editor_sound_marks.h"

#include <array>
#include <cstddef>
#include <optional>

#include "ro/audio/ambient_sound.h"
#include "ro/render/actor_pick.h"
#include "ro/render/map_effect_math.h"

#include "world/map_editor_overlay.h"

namespace moonclient {

std::vector<MapEditorSession::SoundDot> SoundDots(const ro::ui::MapEditorView& view,
                                                  const ro::render::MapScene& scene,
                                                  const ro::render::Camera& camera,
                                                  float aspect_ratio, float viewport_width,
                                                  float viewport_height) {
  const ro::render::MapEffectFrame frame{scene.gnd.width(), scene.gnd.height()};
  std::vector<MapEditorSession::SoundDot> dots;
  dots.reserve(view.sounds.size());
  for (const ro::ui::MapEditorSound& sound : view.sounds) {
    const std::array<float, 3>& native = sound.position;
    if (const std::optional<glm::vec2> at = ro::render::ProjectToScreen(
            camera, aspect_ratio, frame.ToWorld(glm::vec3(native[0], native[1], native[2])),
            viewport_width, viewport_height)) {
      dots.push_back(MapEditorSession::SoundDot{.at = *at, .key = sound.key});
    }
  }
  return dots;
}

std::vector<ro::ui::MapEditorLine> SoundReach(const ro::ui::MapEditorSound& sound,
                                              const ro::render::MapScene& scene,
                                              const ro::render::Camera& camera, float aspect_ratio,
                                              float viewport_width, float viewport_height) {
  // LA PORTÉE se trace comme celle d'une lumière : un cercle autour de sa place.
  std::vector<ro::ui::MapEditorLine> lines =
      LightRangeRing(ro::ui::MapEditorLight{.position = sound.position, .range = sound.range},
                     scene, camera, aspect_ratio, viewport_width, viewport_height);
  // L'EMPRISE, dans le repère du client comme la source la calcule, à la hauteur
  // de sa place.
  ro::audio::AmbientSoundSource source;
  source.native_ground = {sound.position[0], sound.position[2]};
  source.width = sound.width;
  source.height = sound.height;
  const std::vector<std::array<float, 2>> corners = ro::audio::AmbientSoundFootprint(source);
  const ro::render::MapEffectFrame frame{scene.gnd.width(), scene.gnd.height()};
  const auto project = [&](const std::array<float, 2>& corner) {
    return ro::render::ProjectToScreen(
        camera, aspect_ratio, frame.ToWorld(glm::vec3(corner[0], sound.position[1], corner[1])),
        viewport_width, viewport_height);
  };
  for (std::size_t side = 0; side < corners.size(); ++side) {
    const std::optional<glm::vec2> from = project(corners[side]);
    const std::optional<glm::vec2> to = project(corners[(side + 1) % corners.size()]);
    if (from.has_value() && to.has_value()) {
      lines.push_back(ro::ui::MapEditorLine{
          .from = *from, .to = *to, .kind = ro::ui::MapEditorLineKind::kSelection});
    }
  }
  return lines;
}

}  // namespace moonclient
