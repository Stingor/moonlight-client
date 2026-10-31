#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/sim/packed_position.h"

// LE VOL LIBRE DE L'ADMINISTRATEUR : un œil qui traverse murs, décors et sol,
// et se déplace dans les trois dimensions.
//
// 🔴 LE CORPS SUIT, POSÉ PAR LE SERVEUR. Le serveur n'envoie monstres et PNJ
// qu'autour du corps : une caméra qui s'en éloignerait ne verrait plus que la
// carte. Le client demande donc la case sous l'œil (`ro/net/free_flight.h`), et
// c'est le serveur qui juge le rang. En sortant, la caméra retrouve sa pose
// d'avant, autour du corps là où il a été posé.
namespace moonclient {

// La vitesse de l'œil, en unités du monde par seconde : douze cases.
inline constexpr float kFreeFlightUnitsPerSecond = 60.0F;
// Maj tenue multiplie la vitesse d'autant, pour traverser une carte.
inline constexpr float kFreeFlightFastFactor = 4.0F;

// Les six directions du vol : les quatre de la marche, plus monter et descendre.
enum class FlightKey : std::uint8_t { kForward, kBack, kLeft, kRight, kUp, kDown };
inline constexpr std::size_t kFlightKeyCount = 6;
using FlightKeys = std::array<bool, kFlightKeyCount>;

// LE DÉPLACEMENT DE L'ŒIL pendant `elapsed_ms`, regard tourné de `yaw_degrees`
// et incliné de `pitch_degrees` vers le bas.
//
// ⚠ AVANCER SUIT LE REGARD, inclinaison comprise : regarder le sol et avancer
// fait descendre. Les pas de côté restent à l'horizontale, monter et descendre
// suivent la verticale du monde.
[[nodiscard]] glm::vec3 FreeFlightStep(float yaw_degrees, float pitch_degrees,
                                       const FlightKeys& held, bool fast, std::uint32_t elapsed_ms);

// LE CORPS SUIT LA CAMÉRA quand elle s'en éloigne d'au moins ce nombre de cases,
// sur un axe ou l'autre : assez peu pour que la zone envoyée par le serveur,
// une quarantaine de cases autour du corps, couvre toujours ce que l'œil voit.
inline constexpr std::int32_t kFreeFlightLeashCells = 5;
// Et pas plus souvent : chaque case demandée fait recalculer sa zone au serveur.
inline constexpr std::uint32_t kFreeFlightLeashRepeatMs = 250;

// LA CASE OÙ POSER LE CORPS, celle que survole l'œil ramenée dans la carte de
// `map_width` sur `map_height` cases. Rien quand le corps en est assez près.
[[nodiscard]] std::optional<ro::sim::CellPosition> FreeFlightLeashCell(
    std::int32_t eye_cell_x, std::int32_t eye_cell_y, std::int32_t body_cell_x,
    std::int32_t body_cell_y, std::int32_t map_width, std::int32_t map_height);

class FreeFlight {
 public:
  [[nodiscard]] bool active() const noexcept { return active_; }

  // Prend son envol là où se trouve l'œil de `camera`, regard inchangé, et
  // retient la pose à rendre.
  void Enter(const ro::render::Camera& camera);
  // Rend à `camera` la pose retenue à l'envol.
  void Leave(ro::render::Camera& camera);

  // Le joueur tourne le regard : un glisser de la souris, en pixels.
  void Look(float dx_px, float dy_px);

  void Press(FlightKey key) noexcept { held_[static_cast<std::size_t>(key)] = true; }
  void Release(FlightKey key) noexcept { held_[static_cast<std::size_t>(key)] = false; }

  // Avance l'œil jusqu'à `now_ms`. La première image après l'envol ne bouge
  // pas : elle ne fait que noter l'heure.
  void Advance(std::uint32_t now_ms, bool fast);

  // Pose `camera` sur l'œil.
  void Aim(ro::render::Camera& camera) const;

  [[nodiscard]] const glm::vec3& eye() const noexcept { return eye_; }

  // LE CORPS PEUT-IL ÊTRE DEMANDÉ À CET INSTANT ? Vrai au plus une fois par
  // `kFreeFlightLeashRepeatMs`, et note la demande.
  [[nodiscard]] bool LeashDue(std::uint32_t now_ms);

 private:
  bool active_ = false;
  glm::vec3 eye_{0.0F};
  float yaw_degrees_ = 0.0F;
  float pitch_degrees_ = 0.0F;
  FlightKeys held_{};
  std::optional<std::uint32_t> last_ms_;
  std::optional<std::uint32_t> leash_sent_ms_;
  ro::render::Camera kept_;
};

}  // namespace moonclient
