// LA RONDE DES APPARITIONS : le fantôme (358) et les deux chauves-souris (359,
// 360) que les semeurs de niflheim lancent. Un sprite qui tourne autour de sa
// naissance, flotte, paraît presque transparent et choisit sa pose — et non un
// quad figé, opaque, mort à cent tics.

#include "ro/render/effect_haunt.h"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"

using Catch::Approx;
using ro::render::AdvanceEffectHaunt;
using ro::render::EffectHaunt;
using ro::render::EffectHauntFlight;
using ro::render::StartEffectHaunt;

namespace {

constexpr std::uint32_t kGhostEffect = 358;
constexpr std::uint32_t kBatEffect = 359;
constexpr std::uint32_t kLoopingBatEffect = 360;

// La durée que le client déclare pour les trois.
constexpr std::int32_t kHauntEffectLife = 4000;
// Les plafonds d'alpha : trente pour les deux vols lents, deux cent cinquante
// pour la chauve-souris qui dessine un huit.
constexpr float kQuietCeiling = 30.0F;
constexpr float kLoopingCeiling = 250.0F;
// Un cap de caméra quelconque, qui ne fait tomber le fantôme d'aucun côté
// particulier.
constexpr std::int32_t kCameraYaw = 0;
constexpr float kTolerance = 1.0e-3F;
constexpr float kRadiansPerDegree = std::numbers::pi_v<float> / 180.0F;
// Le pas de la troupe, et combien de tics une seconde d'avance lui fait battre.
constexpr std::uint32_t kPoolStepMs = 16;
constexpr std::int32_t kPoolTicks = 60;

// Avance d'autant de tics, et rend le dernier écart.
glm::vec3 AdvanceTicks(EffectHaunt& haunt, std::int32_t ticks, std::int32_t camera_yaw,
                       std::uint32_t& generator) {
  glm::vec3 offset{0.0F, 0.0F, 0.0F};
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    offset = AdvanceEffectHaunt(haunt, camera_yaw, generator);
  }
  return offset;
}

const ro::render::EffectNodePattern& OnlyPattern(std::uint32_t effect_id) {
  const std::span<const ro::render::EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(effect_id);
  REQUIRE(patterns.size() == 1);
  return patterns.front();
}

}  // namespace

TEST_CASE("les trois apparitions ont leur vol et la vie de leur effet", "[render][effect_haunt]") {
  CHECK(OnlyPattern(kGhostEffect).haunt_flight == EffectHauntFlight::kGhost);
  CHECK(OnlyPattern(kBatEffect).haunt_flight == EffectHauntFlight::kBat);
  CHECK(OnlyPattern(kLoopingBatEffect).haunt_flight == EffectHauntFlight::kLoopingBat);
  for (const std::uint32_t each_effect_id : {kGhostEffect, kBatEffect, kLoopingBatEffect}) {
    INFO("effet " << each_effect_id);
    CHECK(OnlyPattern(each_effect_id).life_from_effect);
    // ⚠ LE SEMIS D'UN SPRITE LIT LA VIE DU PATRON : c'est elle qui doit valoir
    // la durée de l'effet, et non le défaut de cent.
    CHECK(OnlyPattern(each_effect_id).life_ticks == kHauntEffectLife);
    CHECK(ro::render::EffectNodeLifeFor(each_effect_id) == kHauntEffectLife);
  }
}

TEST_CASE("le fantôme paraît d'un cran par tic et reste presque transparent",
          "[render][effect_haunt]") {
  EffectHaunt ghost = StartEffectHaunt(EffectHauntFlight::kGhost);
  std::uint32_t draw = 1;
  CHECK(ghost.alpha == 0.0F);
  (void)AdvanceTicks(ghost, 10, kCameraYaw, draw);
  CHECK(ghost.alpha == Approx(10.0F));
  (void)AdvanceTicks(ghost, 200, kCameraYaw, draw);
  CHECK(ghost.alpha == Approx(kQuietCeiling));
}

TEST_CASE("il tourne à seize unités de sa naissance et flotte de quatre",
          "[render][effect_haunt]") {
  EffectHaunt ghost = StartEffectHaunt(EffectHauntFlight::kGhost);
  std::uint32_t draw = 1;
  // Trente tics : l'orbite est à trente degrés, le flottement à quatre-vingt-dix.
  const glm::vec3 offset = AdvanceTicks(ghost, 30, kCameraYaw, draw);
  CHECK(std::hypot(offset.x, offset.z) == Approx(ro::render::kEffectHauntRadius));
  CHECK(offset.x == Approx(ro::render::kEffectHauntRadius * std::cos(30.0F * kRadiansPerDegree)));
  CHECK(offset.y == Approx(ro::render::kEffectHauntBobHeight));
  // Le fantôme ne tire rien : sa pose vient de la caméra.
  CHECK(draw == 1U);
}

TEST_CASE("le fantôme montre sa face ou son dos selon la caméra", "[render][effect_haunt]") {
  EffectHaunt face = StartEffectHaunt(EffectHauntFlight::kGhost);
  EffectHaunt dos = StartEffectHaunt(EffectHauntFlight::kGhost);
  std::uint32_t draw = 1;
  (void)AdvanceEffectHaunt(face, 0, draw);
  (void)AdvanceEffectHaunt(dos, 200, draw);
  CHECK(face.action == ro::render::kEffectHauntGhostFacingAction);
  CHECK(dos.action == ro::render::kEffectHauntGhostTurnedAction);
}

TEST_CASE("la chauve-souris en huit va deux fois plus vite en profondeur",
          "[render][effect_haunt]") {
  EffectHaunt huit = StartEffectHaunt(EffectHauntFlight::kLoopingBat);
  std::uint32_t draw = 1;
  // Vingt tics à deux degrés : l'orbite est à quarante, la profondeur à quatre-vingts.
  const glm::vec3 offset = AdvanceTicks(huit, 20, kCameraYaw, draw);
  CHECK(huit.orbit_degrees == 40);
  CHECK(offset.z == Approx(ro::render::kEffectHauntRadius * std::sin(80.0F * kRadiansPerDegree)));
  (void)AdvanceTicks(huit, 300, kCameraYaw, draw);
  CHECK(huit.alpha == Approx(kLoopingCeiling));
}

TEST_CASE("passé son âge, l'apparition s'éloigne et s'efface", "[render][effect_haunt]") {
  EffectHaunt ghost = StartEffectHaunt(EffectHauntFlight::kGhost);
  EffectHaunt huit = StartEffectHaunt(EffectHauntFlight::kLoopingBat);
  std::uint32_t draw = 1;
  (void)AdvanceTicks(ghost, ro::render::kEffectHauntLeaveTick - 1, kCameraYaw, draw);
  (void)AdvanceTicks(huit, ro::render::kEffectHauntLeaveTick - 1, kCameraYaw, draw);
  CHECK(ghost.radius == Approx(ro::render::kEffectHauntRadius));
  // Soixante tics de départ : quatre crans d'effacement.
  (void)AdvanceTicks(ghost, 60, kCameraYaw, draw);
  (void)AdvanceTicks(huit, 60, kCameraYaw, draw);
  CHECK(ghost.radius > ro::render::kEffectHauntRadius);
  CHECK(ghost.alpha == Approx(kQuietCeiling - 4.0F));
  // La chauve-souris en huit ne s'éloigne pas, mais s'efface cinq fois plus vite.
  CHECK(huit.radius == Approx(ro::render::kEffectHauntRadius));
  CHECK(huit.alpha == Approx(kLoopingCeiling - 20.0F));
}

TEST_CASE("la troupe recopie la ronde dans le nœud, et dans NOTRE repère",
          "[render][effect_haunt]") {
  ro::render::EffectNodePool pool;
  ro::render::PooledEffectNode node;
  node.node.life_ticks = kHauntEffectLife;
  node.haunt = StartEffectHaunt(EffectHauntFlight::kGhost);
  const glm::vec3 centre{100.0F, 5.0F, 200.0F};
  node.haunt_center = centre;
  node.ground_position = centre;
  pool.Add(node);
  pool.Advance(kPoolStepMs * static_cast<std::uint32_t>(kPoolTicks));
  REQUIRE(pool.nodes().size() == 1);
  const ro::render::PooledEffectNode& vu = pool.nodes().front();
  // Soixante degrés d'orbite : le client avance en profondeur POSITIVE, ce qui
  // est NÉGATIF chez nous.
  const glm::vec3 offset = vu.ground_position - centre;
  CHECK(offset.x == Approx(8.0F).margin(kTolerance));
  CHECK(offset.z == Approx(-ro::render::kEffectHauntRadius * std::sin(60.0F * kRadiansPerDegree))
                        .margin(kTolerance));
  CHECK(vu.node.alpha == Approx(kQuietCeiling));
}
