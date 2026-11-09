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
// permet pour le format de l'image (`ArenaFarthestDistance`).
//
// LE LACET TOURNE COMME EN JEU : clic droit glissé, double clic droit pour
// revenir face au nord (`DragCamera`, `ResetCameraYaw`). Il part du nord à
// chaque entrée et ne bouge pas pendant la descente ; « haut » reste le haut
// de l'écran pour les touches. 🔴 LE CADRE D'APPARITION DE LA SIMULATION RESTE
// ALIGNÉ SUR LE NORD : une caméra tournée recule moins loin, pour que son
// emprise y tienne encore (`ArenaFarthestDistance`).

inline constexpr float kArenaFovDegrees = ro::render::kGameFovDegrees;
inline constexpr float kArenaElevationDegrees = ro::render::kGameElevationDegrees;
// Le lacet de l'entrée et de la remise : face au nord.
inline constexpr float kArenaNorthYawDegrees = 0.0F;
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
// à sa hauteur — ou `ground_below_units` plus bas : un sol en creux sous le
// point visé se voit plus loin. De combien vers l'ouest, l'est, le nord et le
// sud. Rien si un coin de l'image regarde au-dessus de l'horizon.
struct GroundFootprint {
  float west_cells = 0.0F;
  float east_cells = 0.0F;
  float north_cells = 0.0F;
  float south_cells = 0.0F;
};
[[nodiscard]] std::optional<GroundFootprint> GroundFootprintOf(const ro::render::Camera& camera,
                                                               float aspect_ratio,
                                                               float ground_below_units = 0.0F);

// LA VISÉE LA PLUS RASE de la caméra de l'arène pour une image de ce rapport :
// la tangente de la plus faible plongée sous l'horizon parmi les rayons de
// l'image, celle d'un coin du haut. Elle ne dépend ni du recul ni de la
// descente, seulement de l'élévation et du champ ; un sol dont la pente reste
// en dessous ne cache jamais le sol plus loin (`kGroundSlopeCap`).
[[nodiscard]] float ArenaShallowestSightTangent(float aspect_ratio);

// LE RECUL LE PLUS LOINTAIN pour une image de ce rapport, sous ce lacet : le
// plus grand, borné à `kArenaFarthestReachDistance`, dont l'emprise au sol
// tient encore dans le cadre d'apparition `frame` — la horde naît donc les
// pieds hors de l'image. Un grand monstre né au sud peut y montrer la tête : il
// paraît en fondu (`kHordeBirthFadeMs`), sans surgir.
// Jamais moins que le recul d'entrée : le contenu est réglé pour lui. Tournée
// en biais, la caméra y voit donc un coin du cadre, où la horde naît en fondu.
[[nodiscard]] float ArenaFarthestDistance(float aspect_ratio, const ro::sim::SpawnFrame& frame,
                                          float yaw_degrees = kArenaNorthYawDegrees);

// LE POINT DU SOL SOUS UN POINT DE L'ÉCRAN, en points depuis le haut à gauche
// d'une image `viewport_width` × `viewport_height` : le rayon de la caméra
// coupe le plan qui passe par `origin`, `east` (une case à l'est) et `north`
// (une case au nord). Rend l'écart à `origin` en cases vers l'est et le nord,
// à tout lacet ; rien si le rayon longe le plan ou le coupe derrière l'œil.
[[nodiscard]] std::optional<glm::vec2> GroundCellsUnder(const ro::render::Camera& camera,
                                                        glm::vec2 screen, float viewport_width,
                                                        float viewport_height, glm::vec3 origin,
                                                        glm::vec3 east, glm::vec3 north);

// LE PLUS HAUT DÉPART DE LA DESCENTE dont l'emprise au sol reste à
// `reach_cells` au plus du point visé, de chaque côté, pour une image de ce
// rapport : un sol qui a un bord proche — la fenêtre du sol généré — ne le
// montre pas pendant la descente. Le sol se lit `ground_below_units` sous le
// point visé : un relief peut creuser le bord de l'image sous le héros.
// Jamais moins que `kArenaNearestDistance`.
[[nodiscard]] float ArenaDescentCeiling(float aspect_ratio, float reach_cells,
                                        float ground_below_units = 0.0F);

// LA CAMÉRA DE L'ARÈNE : suivi rigide du héros, molette, lacet, secousse et
// descente. Le recul posé est celui de la molette, ramené au plus lointain du
// lacet tenu : revenir au nord rend celui de la molette.
//
// ⚠ LA SECOUSSE SE RETIRE AVANT CHAQUE POSE (`ScreenShake::Undo`) : posée par
// dessus la précédente, elle dériverait.
class ArenaCamera {
 public:
  // `frame` : le cadre d'apparition des règles, qui borne le recul.
  explicit ArenaCamera(const ro::sim::SpawnFrame& frame);

  // L'ENTRÉE DANS L'ARÈNE : le suivi repart du héros, face au nord.
  void Enter() noexcept;
  // LE CLIC DROIT GLISSÉ de `dx_px` points vers la droite tourne la caméra
  // autour du héros, à la règle du jeu ; rien pendant la descente.
  void Orbit(float dx_px) noexcept;
  // LE DOUBLE CLIC DROIT : la caméra revient face au nord.
  void ResetYaw() noexcept { yaw_degrees_ = kArenaNorthYawDegrees; }
  // Le lacet posé d'emblée, ramené dans un tour : ce que le banc demande.
  void TurnTo(float yaw_degrees) noexcept;
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
  // côté (`ArenaDescentCeiling`), sur un sol jusqu'à `ground_below_units` sous
  // lui ; rien, elle part de `kArenaDescentFromFactor` fois le recul.
  void BoundDescent(std::optional<float> reach_cells, float ground_below_units = 0.0F) noexcept;

  // POSE LA CAMÉRA POUR CETTE IMAGE sur le héros, au sol `hero_ground`, à
  // `arrival_ms` de l'arrivée de l'étage, pour une image de `aspect_ratio`.
  // Les effets en cours de `effects_world` peuvent demander la secousse.
  const ro::render::Camera& Pose(const glm::vec3& hero_ground, std::uint32_t arrival_ms,
                                 float aspect_ratio, std::uint32_t now_ms,
                                 const ro::sim::World& effects_world);

  [[nodiscard]] const ro::render::Camera& camera() const noexcept { return camera_; }
  [[nodiscard]] float wheel_distance() const noexcept { return wheel_distance_; }
  // Le lacet que la prochaine pose prendra : les faces des sprites le suivent
  // dès cette image.
  [[nodiscard]] float yaw_degrees() const noexcept { return yaw_degrees_; }
  // Le recul le plus lointain du format de la dernière pose ; la borne entière
  // avant la première.
  [[nodiscard]] float farthest_distance() const noexcept { return farthest_distance_; }

 private:
  ro::render::Camera camera_;
  CameraAim aim_;
  ro::sim::SpawnFrame frame_;
  float wheel_distance_ = kArenaEntryDistance;
  float farthest_distance_ = kArenaFarthestReachDistance;
  float yaw_degrees_ = kArenaNorthYawDegrees;
  // La descente en cours à la dernière pose : le lacet n'y bouge pas.
  bool descending_ = false;
  // Le recul le plus lointain sous `turned_yaw_`, pour `farthest_aspect_`.
  std::optional<float> turned_yaw_;
  float turned_farthest_distance_ = kArenaFarthestReachDistance;
  // Le rapport pour lequel `farthest_distance_` a été cherché.
  std::optional<float> farthest_aspect_;
  // La borne de la descente, et son plus haut départ pour `farthest_aspect_`.
  std::optional<float> descent_reach_cells_;
  float descent_ground_below_units_ = 0.0F;
  float descent_ceiling_ = kArenaDescentUnbounded;
};

}  // namespace moonclient
