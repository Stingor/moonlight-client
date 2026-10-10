#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace ro::render {

// LA RONDE D'UNE APPARITION : les fantômes et les chauves-souris des cartes
// hantées.
//
// Ce n'est ni un lancer ni l'orbite ordinaire de la troupe. Le sprite tourne
// autour de son point de naissance à distance fixe, flotte de haut en bas,
// paraît en fondu jusqu'à un plafond qui lui est propre — un fantôme reste
// presque transparent —, puis, passé un âge, s'éloigne en spirale en
// s'effaçant. Et il CHOISIT SON ACTION : le fantôme se tourne selon la caméra,
// la chauve-souris change de pose au hasard.
//
// Description PURE : l'état avance d'un tic et rend l'écart au centre dans le
// repère du CLIENT. C'est l'appelant qui le passe dans le nôtre.

// LES TROIS VOLS, tels que le bâtisseur les distingue.
enum class EffectHauntFlight : std::uint8_t {
  kNone,
  // Un cercle à plat, un degré par tic, un alpha plafonné à trente. L'action
  // suit l'angle sous lequel la caméra le voit.
  kGhost,
  // Le même cercle, mais l'action se tire.
  kBat,
  // Deux degrés par tic, et la profondeur tourne DEUX FOIS plus vite que la
  // largeur : la chauve-souris dessine un huit au lieu d'un cercle. Son plafond
  // d'alpha est presque plein, et elle s'efface cinq fois plus vite.
  kLoopingBat,
};

// Le rayon du cercle, en unités du monde.
inline constexpr float kEffectHauntRadius = 16.0F;
// L'amplitude du flottement vertical, et son pas en degrés par tic.
inline constexpr float kEffectHauntBobHeight = 4.0F;
inline constexpr std::int32_t kEffectHauntBobDegreesPerTick = 3;
// L'alpha ne monte que pendant ces premiers tics, d'un cran par tic.
inline constexpr std::int32_t kEffectHauntRiseTicks = 250;
// L'âge où l'apparition commence à partir : elle accélère, s'éloigne et
// s'efface.
inline constexpr std::int32_t kEffectHauntLeaveTick = 2600;
// Tous les combien de tics l'alpha perd un cran, une fois l'apparition partie.
inline constexpr std::int32_t kEffectHauntFadePeriodTicks = 15;
// Ce que le bras gagne à chaque tic du départ, en facteur.
inline constexpr float kEffectHauntSpiralFactor = 1.002F;

// Les deux actions du fantôme : face à la caméra, et de dos.
inline constexpr std::int32_t kEffectHauntGhostFacingAction = 0;
inline constexpr std::int32_t kEffectHauntGhostTurnedAction = 6;
// Les deux poses de la chauve-souris, et la chance de passer à chacune par tic.
inline constexpr std::int32_t kEffectHauntBatRestAction = 0;
inline constexpr std::int32_t kEffectHauntBatOtherAction = 1;

struct EffectHaunt {
  EffectHauntFlight flight = EffectHauntFlight::kNone;
  std::int32_t age_ticks = 0;
  // En degrés entiers, dans [0, 360) : le client les lit dans une table.
  std::int32_t orbit_degrees = 0;
  std::int32_t bob_degrees = 0;
  std::int32_t orbit_step_degrees = 1;
  float radius = kEffectHauntRadius;
  float alpha = 0.0F;
  float alpha_ceiling = 0.0F;
  std::int32_t action = 0;
};

// L'apparition à sa naissance. Rien ne se tire : le bâtisseur n'en tire rien.
[[nodiscard]] EffectHaunt StartEffectHaunt(EffectHauntFlight flight);

// Avance d'un tic et rend l'écart au centre, dans le repère du CLIENT.
//
// `camera_yaw_degrees` est le cap de la caméra tel que le client le range — le
// nôtre, tronqué : le fantôme en tire son action. Le générateur sert aux poses
// de la chauve-souris, deux tirages par tic ; le fantôme n'y touche pas.
[[nodiscard]] glm::vec3 AdvanceEffectHaunt(EffectHaunt& haunt, std::int32_t camera_yaw_degrees,
                                           std::uint32_t& generator);

}  // namespace ro::render
