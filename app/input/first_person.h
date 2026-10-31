#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/walk_map.h"

// LA VUE À LA PREMIÈRE PERSONNE : l'œil du joueur, à hauteur de tête, et la
// marche au clavier dans la direction du regard.
//
// 🔴 ELLE NE DÉPLACE PERSONNE PAR ELLE-MÊME. Le serveur reste maître de la
// position : les touches de marche ne font que DEMANDER une case quelques pas
// devant, comme un clic tenu sur le sol. Un mur arrête donc le personnage
// exactement comme il l'arrête à la souris.
//
// La caméra du jeu orbite autour d'un point visé ; celle-ci se pose sur l'œil.
// Les deux partagent la même structure : la vue écrit le point visé devant
// l'œil, à la distance qui laisse l'œil exactement où il doit être.
namespace moonclient {

// La hauteur de l'œil au-dessus des pieds, en unités du monde. Un corps d'une
// soixantaine de pixels de sprite en mesure à peu près neuf.
inline constexpr float kFirstPersonEyeUnits = 8.0F;
// Le champ vertical, en degrés : celui d'un jeu de tir, et non le champ étroit
// qui donne à la vue du jeu son aspect presque orthographique.
inline constexpr float kFirstPersonFovDegrees = 75.0F;
// Le regard ne descend ni ne monte au-delà, en degrés depuis l'horizon.
inline constexpr float kFirstPersonMaxPitchDegrees = 80.0F;
// Ce qu'un pixel de glisser tourne le regard, en degrés.
inline constexpr float kFirstPersonDegreesPerPixel = 0.25F;
// LA CASE DEMANDÉE est à ce nombre de pas devant le personnage : assez loin
// pour que la marche ne s'arrête pas entre deux demandes, assez près pour qu'un
// relâchement l'arrête vite.
inline constexpr std::int32_t kFirstPersonStrideCells = 3;
// En deçà, deux touches opposées se sont annulées : aucune direction.
inline constexpr float kNoDirection = 1e-3F;

// TOURNE UN REGARD d'un glisser de la souris, en pixels.
//
// ⚠ LE LACET DE LA CAMÉRA CROÎT VERS LA GAUCHE : tirer vers la droite le fait
// donc décroître. Tirer vers le bas incline vers le sol, comme la caméra du jeu.
void TurnGaze(float& yaw_degrees, float& pitch_degrees, float dx_px, float dy_px);

// LA DISTANCE DU POINT VISÉ DEVANT L'ŒIL, en unités du monde.
//
// ⚠ ELLE NE CHANGE RIEN À L'IMAGE DU MONDE, mais tout à ce qui se peint par-
// dessus : jauges, noms et chiffres de dégâts se mesurent à l'échelle
// apparente de la caméra, qui se déduit de son recul et de son champ. Ce recul
// rend la même échelle que la caméra du jeu : rien ne grossit ni ne rapetisse
// en passant d'une vue à l'autre.
[[nodiscard]] float FirstPersonGazeUnits();

// Les quatre directions de marche. Leurs touches sont celles de la table des
// raccourcis : voir `GameAction::kStepForward`.
enum class StrideKey : std::uint8_t { kForward, kBack, kLeft, kRight };
inline constexpr std::size_t kStrideKeyCount = 4;
using StrideKeys = std::array<bool, kStrideKeyCount>;

// LA CASE VERS LAQUELLE MARCHER depuis `from_x`, `from_y`, regard tourné de
// `yaw_degrees`, avec ces touches tenues. Rien quand aucune direction ne
// ressort — aucune touche, ou deux opposées.
//
// ⚠ LA DERNIÈRE CASE PRATICABLE AVANT LE PREMIER OBSTACLE, et non la case au
// bout de la foulée : face à un mur, le personnage s'en approche au lieu de
// demander la case de l'autre côté, que le serveur atteindrait en le
// contournant — ou pas du tout.
[[nodiscard]] std::optional<ro::sim::CellPosition> FirstPersonStride(const ro::sim::WalkMap& map,
                                                                     std::int32_t from_x,
                                                                     std::int32_t from_y,
                                                                     float yaw_degrees,
                                                                     const StrideKeys& held);

// Pose `camera` sur l'œil placé à `eye`, regard tourné de `yaw_degrees` et
// incliné de `pitch_degrees` vers le BAS — l'inclinaison de la caméra du jeu.
void AimFirstPersonCamera(ro::render::Camera& camera, const glm::vec3& eye, float yaw_degrees,
                          float pitch_degrees);

class FirstPersonView {
 public:
  [[nodiscard]] bool active() const noexcept { return active_; }

  // Entre dans la vue ou en sort. En entrant, le regard part du lacet de la
  // caméra, à l'horizontale ; en sortant, la caméra retrouve exactement le
  // recul, l'inclinaison, le lacet et le champ qu'elle avait.
  void Toggle(ro::render::Camera& camera);

  // Le joueur tourne le regard : un glisser de la souris, en pixels.
  void Look(float dx_px, float dy_px);

  void Press(StrideKey key) noexcept { held_[static_cast<std::size_t>(key)] = true; }
  void Release(StrideKey key) noexcept { held_[static_cast<std::size_t>(key)] = false; }
  // ⚠ UNE TOUCHE RELÂCHÉE HORS DE LA FENÊTRE n'arrive jamais : l'appelant
  // recoupe donc chaque image avec l'état réel du clavier.
  [[nodiscard]] const StrideKeys& held() const noexcept { return held_; }

  // Pose la caméra sur l'œil du personnage dont les pieds sont à `feet`.
  void Aim(ro::render::Camera& camera, const glm::vec3& feet) const;

  [[nodiscard]] float yaw_degrees() const noexcept { return yaw_degrees_; }

  // UNE CASE DOIT-ELLE REPARTIR À CET INSTANT ? Vrai à la première image d'une
  // marche, puis une fois passé `repeat_ms` depuis la précédente : la caméra
  // avance avec le personnage, et la case visée avec elle. Note l'envoi.
  [[nodiscard]] bool StrideDue(std::uint32_t now_ms, std::uint32_t repeat_ms);
  // Vrai, une seule fois, à la première image sans marche après une marche :
  // c'est là que le personnage doit s'arrêter.
  [[nodiscard]] bool TakeStrideEnd();

 private:
  struct Kept {
    float distance = 0.0F;
    float pitch_degrees = 0.0F;
    float yaw_degrees = 0.0F;
    float fov_degrees = 0.0F;
  };

  bool active_ = false;
  float yaw_degrees_ = 0.0F;
  float pitch_degrees_ = 0.0F;
  StrideKeys held_{};
  std::optional<std::uint32_t> stride_sent_ms_;
  Kept kept_;
};

}  // namespace moonclient
