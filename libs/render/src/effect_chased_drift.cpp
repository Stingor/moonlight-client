// La dérive poursuivie d'un pétale. Voir l'en-tête pour l'ordre des tirages et
// le repère.

#include "ro/render/effect_chased_drift.h"

#include <algorithm>
#include <array>

#include "ro/render/effect_track_spread.h"

#include "effect_draw_below.h"
#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES PÉTALES DE CERISIER : vingt sprites semés un par tic, nés à huit unités
// de l'effet, qui montent de 0,30, 0,45 ou 0,60 par tic et dérivent de quinze
// centièmes ; échelle de 22/70 à 27/70, l'une des trois actions, éteints sous
// un alpha de trois.
//
// LA MÉTÉO QUI TOMBE, semée tous les deux tics dans un carré de trois cents
// unités, deux cents au-dessus du porteur :
//
//   · @sakura (163) : cent cinquante pétales qui tombent de 0,2 ou 0,3 par tic
//     et dérivent de 0,24 en abscisse, 0,30 en profondeur ; échelle de 22/100
//     à 27/100, l'une des trois actions ;
//   · @leaves (333) : cent feuilles qui tombent de 0,06 à 0,15 par tic et
//     dérivent deux fois moins ; échelle de 35/100 à 40/100, action zéro. Leur
//     bâtisseur tire l'abscisse AVANT les angles.
constexpr std::array<EffectChasedDriftPattern, 3> kChasedDrifts = {{
    {.effect_id = 163,
     .client_function = "Effect_Build_ActorSpriteScatter28b",
     .rise_spread = 2,
     .rise_floor = 2,
     .rise_unit = -0.1F,
     .drift_step = 0.24F,
     .drift_step_depth = 0.30F,
     .scale_spread = 6,
     .scale_floor = 22,
     .scale_divisor = 100.0F,
     .action_spread = 3,
     .birth_square_side = 300.0F,
     .birth_lift = 200.0F},
    {.effect_id = 333,
     .client_function = "Effect_Build_ActorSpriteScatter28",
     .rise_spread = 4,
     .rise_floor = 2,
     .rise_unit = -0.03F,
     .drift_step = 0.12F,
     .drift_step_depth = 0.15F,
     .scale_spread = 6,
     .scale_floor = 35,
     .scale_divisor = 100.0F,
     .birth_square_side = 300.0F,
     .birth_lift = 200.0F,
     .square_x_before_angles = true},
    {.effect_id = 618,
     .client_function = "Effect_Build_FireArrowScatter",
     .birth_ring_radius = 8.0F,
     .rise_spread = 3,
     .rise_floor = 2,
     .rise_unit = 0.15F,
     .drift_step = 0.15F,
     .drift_step_depth = 0.15F,
     .scale_spread = 6,
     .scale_floor = 22,
     .scale_divisor = 70.0F,
     .action_spread = 3,
     .alpha_zero_below = 3.0F},
}};

// Les angles se tirent sur un tour, en degrés entiers.
constexpr std::int32_t kAngleSpread = kEffectTrackFullTurnDegrees;

// UN POINT DU CARRÉ, sur un côté : `tirage(côté) − côté / 2`, en entiers.
[[nodiscard]] float DrawAcrossSquare(std::uint32_t& generator, float side) {
  const auto whole = static_cast<std::int32_t>(side);
  return static_cast<float>(DrawBelowIfPositive(generator, whole) - whole / 2);
}

}  // namespace

std::span<const EffectChasedDriftPattern> EffectChasedDriftPatternTable() { return kChasedDrifts; }

const EffectChasedDriftPattern* EffectChasedDriftPatternFor(std::uint32_t effect_id) {
  const auto found =
      std::ranges::find(kChasedDrifts, effect_id, &EffectChasedDriftPattern::effect_id);
  return found == kChasedDrifts.end() ? nullptr : &*found;
}

EffectChasedDriftSowing SowEffectChasedDrift(const EffectChasedDriftPattern& pattern,
                                             EffectChasedDrift& drift, std::uint32_t& generator) {
  drift = EffectChasedDrift();
  drift.active = true;
  drift.drift_step = pattern.drift_step;
  drift.drift_step_depth = pattern.drift_step_depth;
  drift.birth_square_side = pattern.birth_square_side;
  drift.birth_lift = pattern.birth_lift;
  EffectChasedDriftSowing sowing;
  // ⚠ UNE MONTÉE ENTIÈRE, puis multipliée : trois valeurs possibles, pas une
  // plage.
  drift.rise_per_tick =
      static_cast<float>(DrawBelowIfPositive(generator, pattern.rise_spread) + pattern.rise_floor) *
      pattern.rise_unit;
  // L'ÉCHELLE, entière avant la division.
  sowing.sprite_scale = static_cast<float>(DrawBelowIfPositive(generator, pattern.scale_spread) +
                                           pattern.scale_floor) /
                        pattern.scale_divisor;
  const bool square = pattern.birth_square_side > 0.0F;
  float across = 0.0F;
  std::int32_t circle = 0;
  if (!square) {
    circle = DrawBelowIfPositive(generator, kAngleSpread);
  } else if (pattern.square_x_before_angles) {
    across = DrawAcrossSquare(generator, pattern.birth_square_side);
  }
  for (std::size_t case_ = 0; case_ < 2 * kChasedDriftAnglePairs; ++case_) {
    drift.angles.value[case_] = static_cast<float>(DrawBelowIfPositive(generator, kAngleSpread));
  }
  if (square) {
    if (!pattern.square_x_before_angles) {
      across = DrawAcrossSquare(generator, pattern.birth_square_side);
    }
    // ⚠ LA PROFONDEUR DU CLIENT EST L'OPPOSÉE DE LA NÔTRE, d'où le signe.
    const float depth = DrawAcrossSquare(generator, pattern.birth_square_side);
    sowing.birth_offset = {across, pattern.birth_lift, -depth};
  }
  sowing.action = DrawBelowIfPositive(generator, pattern.action_spread);
  if (square) {
    return sowing;
  }
  // ⚠ LA PROFONDEUR DU CLIENT EST L'OPPOSÉE DE LA NÔTRE : le sinus change de
  // signe. Le cercle reste un cercle, mais un même tirage ne le parcourt pas
  // dans le même sens.
  sowing.birth_offset = {CosOfDegrees(circle) * pattern.birth_ring_radius, 0.0F,
                         -SinOfDegrees(circle) * pattern.birth_ring_radius};
  return sowing;
}

glm::vec3 AdvanceEffectChasedDrift(EffectChasedDrift& drift, std::uint32_t& generator) {
  if (!drift.active) {
    return {0.0F, 0.0F, 0.0F};
  }
  // LES ANGLES D'ABORD : la dérive de ce tic lit les courants déjà avancés.
  ChaseEffectTrackAngles(drift.angles, kChasedDriftAnglePairs, generator);
  const float de_cote = SinOfDegrees(static_cast<std::int32_t>(drift.angles.value[0]));
  const float in_depth = SinOfDegrees(static_cast<std::int32_t>(drift.angles.value[2]));
  return {de_cote * drift.drift_step, drift.rise_per_tick, -in_depth * drift.drift_step_depth};
}

bool RebirthEffectChasedDrift(const EffectChasedDrift& drift, glm::vec3& position, glm::vec3 owner,
                              std::uint32_t& generator) {
  if (!EffectChasedDriftIsReborn(drift) || position.y >= owner.y) {
    return false;
  }
  const float across = DrawAcrossSquare(generator, drift.birth_square_side);
  const float depth = DrawAcrossSquare(generator, drift.birth_square_side);
  position = owner + glm::vec3{across, drift.birth_lift, -depth};
  return true;
}

}  // namespace ro::render
