#pragma once

#include <cstdint>
#include <limits>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/world_fwd.h"

#include "input/camera_aim.h"
#include "survivors/survivors_stage.h"

namespace moonclient {

// ── LA CAMÉRA DE L'ARÈNE (U9) ───────────────────────────────────────────────
//
// Celle de RO, champ 15° et élévation 50°, au recul d'entrée de 400. La
// molette rapproche jusqu'à 300 et recule jusqu'où le cadre d'apparition le
// permet pour le format de l'image (`ArenaFarthestDistance`). 🔴 LE LACET EST
// FIGÉ AU NORD : le cadre d'apparition de la simulation est aligné sur lui, et
// « haut » reste « haut » pour les touches. Ni la molette, ni la secousse, ni
// la descente n'y touchent.

inline constexpr float kArenaFovDegrees = ro::render::kGameFovDegrees;
inline constexpr float kArenaElevationDegrees = ro::render::kGameElevationDegrees;
inline constexpr float kArenaYawDegrees = 0.0F;
// Les reculs de la molette, en unités du monde : celui d'entrée, le plus
// proche, et la borne du plus lointain, une fois et demie celui d'entrée.
inline constexpr float kArenaEntryDistance = 400.0F;
inline constexpr float kArenaNearestDistance = 300.0F;
inline constexpr float kArenaFarthestReachDistance = 600.0F;
// Ce qu'un cran de molette rapproche.
inline constexpr float kArenaWheelStepDistance = 25.0F;

// LA DESCENTE D'ENTRÉE : la caméra part de ce multiple du recul et s'y pose en
// ralentissant, pendant le bandeau du nom de la carte. 🔴 ELLE DURE L'ARRIVÉE
// DE L'ÉTAGE et suit son horloge (`SurvivorsStage::arrival_ms`) : la horde ne
// part qu'une fois la caméra posée, quand l'image tient dans le cadre
// d'apparition.
inline constexpr std::uint32_t kArenaDescentMs = kArrivalMs;
inline constexpr float kArenaDescentFromFactor = 3.0F;
// Une descente que rien ne borne : une carte entière, dont le vide autour se
// montre comme dans le jeu.
inline constexpr float kArenaDescentUnbounded = std::numeric_limits<float>::max();

// Le recul de la descente `arrival_ms` après l'entrée, vers `rest_distance` :
// multiplié par `kArenaDescentFromFactor` à l'entrée — sans dépasser
// `most_from_distance`, ni descendre sous `rest_distance` —, égal à lui dès
// `kArenaDescentMs`.
[[nodiscard]] float ArenaDescentDistance(float rest_distance, std::uint32_t arrival_ms,
                                         float most_from_distance = kArenaDescentUnbounded);

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

// LE RECUL LE PLUS LOINTAIN pour une image de ce rapport : le plus grand, borné
// à `kArenaFarthestReachDistance`, dont l'emprise au sol tient encore dans le
// cadre d'apparition `frame` — la horde naît donc les pieds hors de l'image.
// Un grand monstre né au sud peut y montrer la tête : il paraît en fondu
// (`kHordeBirthFadeMs`), sans surgir.
// Jamais moins que le recul d'entrée : le contenu est réglé pour lui.
[[nodiscard]] float ArenaFarthestDistance(float aspect_ratio, const ro::sim::SpawnFrame& frame);

// LE PLUS HAUT DÉPART DE LA DESCENTE dont l'emprise au sol reste à
// `reach_cells` au plus du point visé, de chaque côté, pour une image de ce
// rapport : un sol qui a un bord proche — la fenêtre du sol généré — ne le
// montre pas pendant la descente. Jamais moins que `kArenaNearestDistance`.
[[nodiscard]] float ArenaDescentCeiling(float aspect_ratio, float reach_cells);

// LA CAMÉRA DE L'ARÈNE : suivi rigide du héros, molette, secousse et descente.
//
// ⚠ LA SECOUSSE SE RETIRE AVANT CHAQUE POSE (`ScreenShake::Undo`) : posée par
// dessus la précédente, elle dériverait.
class ArenaCamera {
 public:
  // `frame` : le cadre d'apparition des règles, qui borne le recul.
  explicit ArenaCamera(const ro::sim::SpawnFrame& frame);

  // L'ENTRÉE DANS L'ARÈNE : le suivi repart du héros.
  void Enter() noexcept;
  // Des crans de molette : positifs, la caméra se rapproche ; le recul reste
  // entre `kArenaNearestDistance` et le plus lointain du format de la
  // dernière pose (`farthest_distance`).
  void Wheel(float notches) noexcept;
  // Le recul de la molette, borné de même.
  void SetDistance(float distance) noexcept;
  // Une secousse demandée jusqu'à `until_ms` (météore, séisme, coup fort).
  void Shake(std::uint32_t until_ms) noexcept { aim_.shake.Request(until_ms); }
  // `/quake` : éteinte, aucune secousse ne se pose.
  void set_quake_enabled(bool enabled) noexcept { aim_.shake.enabled = enabled; }
  // LE SOL SE RECENTRE : la caméra et son suivi se déplacent de `world_delta`,
  // et l'image suivante voit la même chose que la précédente.
  void Shift(glm::vec3 world_delta) noexcept;
  // LA DESCENTE BORNÉE : son emprise reste à `reach_cells` du héros de chaque
  // côté (`ArenaDescentCeiling`) ; rien, elle part de `kArenaDescentFromFactor`
  // fois le recul.
  void BoundDescent(std::optional<float> reach_cells) noexcept;

  // POSE LA CAMÉRA POUR CETTE IMAGE sur le héros, au sol `hero_ground`, à
  // `arrival_ms` de l'arrivée de l'étage, pour une image de `aspect_ratio`.
  // Les effets en cours de `effects_world` peuvent demander la secousse.
  const ro::render::Camera& Pose(const glm::vec3& hero_ground, std::uint32_t arrival_ms,
                                 float aspect_ratio, std::uint32_t now_ms,
                                 const ro::sim::World& effects_world);

  [[nodiscard]] const ro::render::Camera& camera() const noexcept { return camera_; }
  [[nodiscard]] float wheel_distance() const noexcept { return wheel_distance_; }
  // Le recul le plus lointain du format de la dernière pose ; la borne entière
  // avant la première.
  [[nodiscard]] float farthest_distance() const noexcept { return farthest_distance_; }

 private:
  ro::render::Camera camera_;
  CameraAim aim_;
  ro::sim::SpawnFrame frame_;
  float wheel_distance_ = kArenaEntryDistance;
  float farthest_distance_ = kArenaFarthestReachDistance;
  // Le rapport pour lequel `farthest_distance_` a été cherché.
  std::optional<float> farthest_aspect_;
  // La borne de la descente, et son plus haut départ pour `farthest_aspect_`.
  std::optional<float> descent_reach_cells_;
  float descent_ceiling_ = kArenaDescentUnbounded;
};

}  // namespace moonclient
