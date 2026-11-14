// LES ANGLES DE PANNEAU QUE LA TROUPE RANGE DANS LES PISTES.
//
// `EffectNodePool::FaceCamera` reçoit `EffectBillboardTurnDegrees` et
// `EffectBillboardTiltDegrees` — déjà le complément que le client calcule sur
// SA caméra (`360 − lacet`, `450 − altitude`) —, et chaque mouvement les range tels quels
// dans ses deux premières pistes, où la figure les relit. Un mouvement qui
// recalcule le complément sur ces angles-là rend à la figure l'angle BRUT de la
// caméra, et son carré ne lui fait plus face.
#include <array>
#include <cmath>
#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_particle_figures.h"
#include "ro/render/effect_tinted_quad.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

namespace {

constexpr float kFaceTolerance = 0.02F;

ro::render::EffectTrack LiveTrack() {
  ro::render::EffectTrack track;
  track.active = true;
  track.alpha = 200;
  track.radius_at_anchor = 10.0F;
  return track;
}

// Vrai quand le carré est vu de face depuis la caméra.
[[nodiscard]] bool FaitFace(const ro::render::EffectWorldQuad& quad,
                            const ro::render::Camera& camera) {
  const glm::vec3 normale = glm::normalize(
      glm::cross(quad.corners[1] - quad.corners[0], quad.corners[3] - quad.corners[0]));
  const glm::vec3 centre = (quad.corners[0] + quad.corners[2]) * 0.5F;
  const glm::vec3 vers_l_oeil = glm::normalize(camera.Position() - centre);
  return std::abs(std::abs(glm::dot(normale, vers_l_oeil)) - 1.0F) < kFaceTolerance;
}

}  // namespace

TEST_CASE("Panneau : les angles de la troupe font face à la caméra", "[render][billboard]") {
  for (const float cap : {0.0F, 37.0F, 90.0F, 200.0F, 315.0F}) {
    for (const float tilt : {30.0F, 45.0F, 60.0F}) {
      INFO("cap " << cap << ", inclinaison " << tilt);
      ro::render::Camera camera;
      camera.yaw_degrees = cap;
      camera.pitch_degrees = tilt;
      const std::optional<ro::render::EffectWorldQuad> world_quad =
          ro::render::ComposeTintedRotatedQuad(LiveTrack(), 0, 0,
                                               ro::render::EffectBillboardTurnDegrees(camera),
                                               ro::render::EffectBillboardTiltDegrees(camera));
      REQUIRE(world_quad.has_value());
      CHECK(FaitFace(*world_quad, camera));
    }
  }
}

TEST_CASE("Panneau : un mouvement range les angles de la troupe tels quels",
          "[render][billboard]") {
  // Le guidage du sous-type 42 et l'objet lancé les recomplétaient.
  ro::render::Camera camera;
  camera.yaw_degrees = 37.0F;
  camera.pitch_degrees = 45.0F;
  const std::int16_t tour = ro::render::EffectBillboardTurnDegrees(camera);
  const std::int16_t tilt = ro::render::EffectBillboardTiltDegrees(camera);
  std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> tracks;
  std::uint32_t seed = 1U;
  ro::render::AdvanceHomingTrailTracks(tracks, glm::vec3{0.0F}, glm::vec3{10.0F, 0.0F, 0.0F}, tour,
                                       tilt, seed);
  CHECK(tracks[0].style == tour);
  CHECK(tracks[1].style == tilt);
  std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> objet;
  ro::render::AdvanceThrownItemTracks(objet, glm::vec3{10.0F, 0.0F, 0.0F}, tour, tilt);
  CHECK(objet[0].style == tour);
  CHECK(objet[1].style == tilt);
}
