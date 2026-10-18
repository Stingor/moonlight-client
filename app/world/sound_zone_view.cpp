#include "world/sound_zone_view.h"

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

#include "ro/audio/ambient_sound.h"
#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/render/actor_pick.h"
#include "ro/render/terrain_mesh.h"

namespace moonclient {

namespace {

// Au-delà de cet écart entre l'auditeur et l'emprise entière de la source,
// portée comprise, elle ne se montre pas.
constexpr float kShownMargin = 300.0F;
// Combien de segments pour le cercle d'écoute.
constexpr std::size_t kHearingSegments = 48;
// Combien de temps le tracé reste allumé après un bruit.
constexpr std::uint32_t kSoundingFlashMs = 300;

class ZoneProjector {
 public:
  ZoneProjector(const MapStage& stage, const ro::render::MapEffectFrame& frame,
                const ro::render::Camera& camera, float aspect_ratio, float width, float height)
      : stage_(stage),
        frame_(frame),
        camera_(camera),
        aspect_ratio_(aspect_ratio),
        width_(width),
        height_(height) {}

  // Un point du sol, donné dans le repère du client, projeté à l'écran.
  [[nodiscard]] std::optional<glm::vec2> Project(std::array<float, 2> native_ground) const {
    glm::vec3 world = frame_.ToWorld(glm::vec3{native_ground[0], 0.0F, native_ground[1]});
    world.y = ro::render::GroundHeightAt(stage_.scene.gnd, world.x, world.z);
    return ro::render::ProjectToScreen(camera_, aspect_ratio_, world, width_, height_);
  }

  // Un contour fermé ; vide dès qu'un de ses points passe derrière l'œil.
  [[nodiscard]] std::vector<glm::vec2> Outline(
      const std::vector<std::array<float, 2>>& native_points) const {
    std::vector<glm::vec2> outline;
    outline.reserve(native_points.size());
    for (const std::array<float, 2>& point : native_points) {
      const std::optional<glm::vec2> screen = Project(point);
      if (!screen.has_value()) {
        return {};
      }
      outline.push_back(*screen);
    }
    return outline;
  }

 private:
  const MapStage& stage_;
  const ro::render::MapEffectFrame& frame_;
  const ro::render::Camera& camera_;
  float aspect_ratio_;
  float width_;
  float height_;
};

[[nodiscard]] std::string LabelOf(const MapAmbientSource& source) {
  const ro::audio::AmbientSoundSource& data = source.emitter.source();
  std::string text;
  if (data.width == 0) {
    text = ro::core::Printed("%s  (%s)", source.name.c_str(), ro::core::Tr("muette"));
  } else {
    text = ro::core::Printed("%s  ·  %s %.0f  ·  %s %.1f s", source.name.c_str(),
                             ro::core::Tr("portée"), static_cast<double>(data.range),
                             ro::core::Tr("cycle"), static_cast<double>(data.cycle_seconds));
  }
  return text;
}

}  // namespace

std::vector<ro::ui::SoundZoneView> SoundZoneViews(const ActorSounds& sounds, const MapStage& stage,
                                                  const ro::render::Camera& camera,
                                                  float aspect_ratio, float viewport_width,
                                                  float viewport_height, std::uint32_t now_ms) {
  std::vector<ro::ui::SoundZoneView> zones;
  const ro::render::MapEffectFrame& frame = sounds.ambience_frame();
  const std::array<float, 3> ear = sounds.ambience_listener();
  const glm::vec3 native_ear = frame.ToNative(glm::vec3{ear[0], ear[1], ear[2]});
  const std::array<float, 2> listener = {native_ear.x, native_ear.z};
  const ZoneProjector projector(stage, frame, camera, aspect_ratio, viewport_width,
                                viewport_height);
  for (const MapAmbientSource& source : sounds.ambience()) {
    const ro::audio::AmbientSoundSource& data = source.emitter.source();
    const float diagonal =
        std::hypot(static_cast<float>(data.width), static_cast<float>(data.height));
    const float distance =
        std::hypot(listener[0] - data.native_ground[0], listener[1] - data.native_ground[1]);
    if (distance - (diagonal + data.range) > kShownMargin) {
      continue;
    }
    ro::ui::SoundZoneView zone;
    zone.footprint = projector.Outline(ro::audio::AmbientSoundFootprint(data));
    zone.label_at = projector.Project(data.native_ground);
    zone.label = LabelOf(source);
    if (data.width != 0) {
      // LE POINT D'OÙ LE BRUIT PART, calculé comme la source le calcule : il
      // bouge avec le joueur, et peut quitter l'emprise.
      const std::array<float, 2> point = ro::audio::AmbientSoundPoint(data, listener);
      zone.emitter = projector.Project(point);
      zone.listener = projector.Project(listener);
      zone.hearing = projector.Outline(
          ro::audio::AmbientSoundHearingCircle(point, data.range, kHearingSegments));
      zone.just_played = now_ms - source.emitter.last_played_ms() < kSoundingFlashMs;
    }
    zones.push_back(std::move(zone));
  }
  return zones;
}

}  // namespace moonclient
