#pragma once

#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/sim/world_fwd.h"

#include "input/camera_aim.h"
#include "survivors/survivors_stage.h"

namespace moonclient {

// ── LA CAMÉRA DE L'ARÈNE (U9) ───────────────────────────────────────────────
//
// Celle de RO, champ 15° et élévation 50°, au recul de 400 que la molette
// rapproche jusqu'à 300. 🔴 LE LACET EST FIGÉ AU NORD : le cadre d'apparition
// de la simulation est aligné sur lui, et « haut » reste « haut » pour les
// touches. Ni la molette, ni la secousse, ni la descente n'y touchent.

inline constexpr float kArenaFovDegrees = ro::render::kGameFovDegrees;
inline constexpr float kArenaElevationDegrees = ro::render::kGameElevationDegrees;
inline constexpr float kArenaYawDegrees = 0.0F;
// Le recul de la molette, en unités du monde : le plus loin est celui d'entrée.
inline constexpr float kArenaFarthestDistance = 400.0F;
inline constexpr float kArenaNearestDistance = 300.0F;
// Ce qu'un cran de molette rapproche.
inline constexpr float kArenaWheelStepDistance = 25.0F;

// LA DESCENTE D'ENTRÉE : la caméra part de ce multiple du recul et s'y pose en
// ralentissant, pendant le bandeau du nom de la carte. 🔴 ELLE DURE L'ARRIVÉE
// DE L'ÉTAGE et suit son horloge (`SurvivorsStage::arrival_ms`) : la horde ne
// part qu'une fois la caméra posée, quand l'image tient dans le cadre
// d'apparition.
inline constexpr std::uint32_t kArenaDescentMs = kArrivalMs;
inline constexpr float kArenaDescentFromFactor = 3.0F;

// Le recul de la descente `arrival_ms` après l'entrée, vers `rest_distance` :
// multiplié par `kArenaDescentFromFactor` à l'entrée, égal à lui dès
// `kArenaDescentMs`.
[[nodiscard]] float ArenaDescentDistance(float rest_distance, std::uint32_t arrival_ms);

// LA LARGEUR VUE EST BORNÉE à celle du 21:9 (conception §4.2) : au-delà, le
// champ vertical se resserre pour que l'image ne voie pas plus large, sans
// quoi la horde naîtrait dans le champ, sur les côtés.
inline constexpr float kArenaWidestAspect = 21.0F / 9.0F;
// Le champ vertical de l'arène pour une image de ce rapport.
[[nodiscard]] float ArenaFovDegrees(float aspect_ratio);

// CE QUE LA CAMÉRA VOIT DU SOL, en cases autour du point visé, sur un sol plat
// à sa hauteur : de combien vers l'ouest, l'est, le nord et le sud. Rien si un
// coin de l'image regarde au-dessus de l'horizon.
struct GroundFootprint {
  float west_cells = 0.0F;
  float east_cells = 0.0F;
  float north_cells = 0.0F;
  float south_cells = 0.0F;
};
[[nodiscard]] std::optional<GroundFootprint> GroundFootprintOf(const ro::render::Camera& camera,
                                                               float aspect_ratio);

// LA CAMÉRA DE L'ARÈNE : suivi rigide du héros, molette, secousse et descente.
//
// ⚠ LA SECOUSSE SE RETIRE AVANT CHAQUE POSE (`ScreenShake::Undo`) : posée par
// dessus la précédente, elle dériverait.
class ArenaCamera {
 public:
  ArenaCamera();

  // L'ENTRÉE DANS L'ARÈNE : le suivi repart du héros.
  void Enter() noexcept;
  // Des crans de molette : positifs, la caméra se rapproche ; le recul reste
  // dans [`kArenaNearestDistance`, `kArenaFarthestDistance`].
  void Wheel(float notches) noexcept;
  // Le recul de la molette, borné de même.
  void SetDistance(float distance) noexcept;
  // Une secousse demandée jusqu'à `until_ms` (météore, séisme, coup fort).
  void Shake(std::uint32_t until_ms) noexcept { aim_.shake.Request(until_ms); }
  // `/quake` : éteinte, aucune secousse ne se pose.
  void set_quake_enabled(bool enabled) noexcept { aim_.shake.enabled = enabled; }

  // POSE LA CAMÉRA POUR CETTE IMAGE sur le héros, au sol `hero_ground`, à
  // `arrival_ms` de l'arrivée de l'étage, pour une image de `aspect_ratio`.
  // Les effets en cours de `effects_world` peuvent demander la secousse.
  const ro::render::Camera& Pose(const glm::vec3& hero_ground, std::uint32_t arrival_ms,
                                 float aspect_ratio, std::uint32_t now_ms,
                                 const ro::sim::World& effects_world);

  [[nodiscard]] const ro::render::Camera& camera() const noexcept { return camera_; }
  [[nodiscard]] float wheel_distance() const noexcept { return wheel_distance_; }

 private:
  ro::render::Camera camera_;
  CameraAim aim_;
  float wheel_distance_ = kArenaFarthestDistance;
};

}  // namespace moonclient
