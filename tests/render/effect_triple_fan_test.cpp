// LES ÉVENTAILS DE TROIS BOULES : le retourné (378) et le teinté (383, 1207,
// 1208). Même mouvement que les arcs visés — frein le long de l'axe et
// balancement roulé —, mais le premier vole de la CIBLE vers le lanceur, et le
// second ne vole nulle part : il recule sur un cap tiré et revient.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_launch.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_sprite_seed.h"

using Catch::Approx;
using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;
using ro::render::EffectNodeSeed;
using ro::render::EffectNodeSeedFromPattern;

namespace {

constexpr std::int32_t kBallLife = 60;
// Tout ce qui reste à l'effet quand on ne le borne pas.
constexpr std::int32_t kUnknownRemainder = 0;

const EffectNodePattern& SinglePattern(std::uint32_t effect_id) {
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(effect_id);
  REQUIRE(patterns.size() == 1);
  return patterns.front();
}

}  // namespace

TEST_CASE("Éventail retourné : trois boules nées sur la cible reviennent au lanceur",
          "[render][table]") {
  const EffectNodePattern& ball = SinglePattern(378);
  CHECK(ball.aims_at_target);
  CHECK(ball.flies_from_target);
  // Le pas double du client porte sur la MOITIÉ de la distance : une fois la
  // distance entière par la vie, et la boule arrive à sa mort.
  CHECK(ball.aim_step_factor == Approx(1.0F));
  CHECK(ball.copies == 3);
  CHECK(ball.copy_roll_start_degrees == Approx(-90.0F));
  CHECK(ball.copy_roll_step_degrees == Approx(90.0F));
  CHECK(ball.launch_drop == Approx(-10.0F));
  CHECK(ball.brakes_along_axis);
  CHECK(ball.fall_speed_low == Approx(2.0F));
  CHECK(ball.fall_acceleration_share == Approx(2.0F));
  CHECK(ball.swing_speed_low == Approx(-2.0F));
  CHECK(ball.swing_acceleration_share == Approx(2.0F));
  // Une seule fois, au tic zéro.
  CHECK(ball.period_ticks == 0);

  // LES BOUTS DU VOL S'ÉCHANGENT, et seulement quand il y a une cible.
  const EffectNodeSeed seed = EffectNodeSeedFromPattern(ball, kUnknownRemainder, 0, 1);
  const glm::vec3 caster{0.0F, 0.0F, 0.0F};
  const glm::vec3 target{50.0F, 0.0F, 0.0F};
  const ro::render::EffectFlightEnds retourne =
      ro::render::EffectNodeFlightEnds(seed, caster, target);
  CHECK(retourne.birth == target);
  REQUIRE(retourne.aim.has_value());
  CHECK(*retourne.aim == caster);
  const ro::render::EffectFlightEnds without_target =
      ro::render::EffectNodeFlightEnds(seed, caster, std::nullopt);
  CHECK(without_target.birth == caster);
  CHECK_FALSE(without_target.aim.has_value());
}

TEST_CASE("Un vol ordinaire garde ses bouts", "[render][launch]") {
  const EffectNodeSeed seed =
      EffectNodeSeedFromPattern(SinglePattern(216), kUnknownRemainder, 0, 1);
  const glm::vec3 caster{0.0F, 0.0F, 0.0F};
  const glm::vec3 target{50.0F, 0.0F, 0.0F};
  const ro::render::EffectFlightEnds bouts = ro::render::EffectNodeFlightEnds(seed, caster, target);
  CHECK(bouts.birth == caster);
  REQUIRE(bouts.aim.has_value());
  CHECK(*bouts.aim == target);
}

TEST_CASE("Éventail retourné : chaque boule tire sa teinte bleutée", "[render][table]") {
  const EffectNodeSeed seed =
      EffectNodeSeedFromPattern(SinglePattern(378), kUnknownRemainder, 0, 1);
  std::uint32_t generator = 7U;
  bool red_varies = false;
  float first_red = -1.0F;
  for (int rang = 0; rang < 20; ++rang) {
    const ro::render::EffectNode node = ro::render::SowEffectNode(seed, generator);
    CHECK(node.color[0] >= 160.0F);
    CHECK(node.color[0] <= 240.0F);
    CHECK(node.color[1] >= 160.0F);
    CHECK(node.color[1] <= 240.0F);
    CHECK(node.color[2] == Approx(255.0F));
    if (first_red < 0.0F) {
      first_red = node.color[0];
    } else if (node.color[0] != first_red) {
      red_varies = true;
    }
  }
  CHECK(red_varies);
}

TEST_CASE("Éventail teinté : quinze boules sur cinq tics, sans pas", "[render][table]") {
  for (const std::uint32_t each_effect_id : {383U, 1207U, 1208U}) {
    const EffectNodePattern& ball = SinglePattern(each_effect_id);
    CAPTURE(each_effect_id);
    for (std::int32_t tick = 0; tick <= 4; ++tick) {
      CHECK(ro::render::EffectNodePatternSowsAt(ball, tick));
    }
    CHECK_FALSE(ro::render::EffectNodePatternSowsAt(ball, 5));
    CHECK(ball.copies == 3);
    CHECK_FALSE(ball.aims_at_target);
    CHECK(ball.step_low == Approx(0.0F));
    CHECK(ball.step_high == Approx(0.0F));
    // LE CAP NE SE TIRE PAS SUR LE TOUR : il se tire sur la grille du client,
    // et l'échelle en six pas.
    CHECK(ball.launch_yaw_high == Approx(0.0F));
    CHECK(ball.launch_heading_on_client_grid);
    CHECK(ball.sprite_scale_steps == 6);
    CHECK(ball.copy_roll_start_degrees == Approx(-90.0F));
  }
}

TEST_CASE("Éventail teinté : la boule recule vingt-neuf unités puis revient", "[render][launch]") {
  const EffectNodeSeed seed =
      EffectNodeSeedFromPattern(SinglePattern(383), kUnknownRemainder, 0, 0);
  std::uint32_t generator = 3U;
  const ro::render::EffectLaunchDraw lance =
      ro::render::DrawEffectLaunch(seed, 0.0F, kBallLife, 0.0F, generator);
  ro::render::PooledEffectNode ball;
  ball.node.life_ticks = kBallLife;
  ball.launch_direction = lance.direction;
  ball.brakes_along_axis = seed.brakes_along_axis;
  ball.fall_speed = lance.fall_speed;
  ball.fall_acceleration = lance.fall_acceleration;
  REQUIRE(glm::length(ball.launch_direction) == Approx(1.0F));
  float farthest = 0.0F;
  for (std::int32_t tick = 1; tick <= kBallLife; ++tick) {
    ro::render::AdvanceLaunchedNode(ball, ro::render::EffectScatterWorld{});
    farthest = std::min(farthest, glm::dot(ball.ground_position, lance.direction));
  }
  // `748 = 2 − k × 4/60` retranché au pas nul : trente tics de recul, puis le
  // retour, deux unités au-delà du départ.
  CHECK(farthest == Approx(-29.0F).margin(0.01F));
  CHECK(glm::dot(ball.ground_position, lance.direction) == Approx(2.0F).margin(0.01F));
}

TEST_CASE("Éventail teinté : la teinte suit le paramètre du répartiteur", "[render][table]") {
  std::uint32_t generator = 11U;
  // Zéro : vert pâle, rouge et bleu tirés chacun.
  const EffectNodeSeed vert =
      EffectNodeSeedFromPattern(SinglePattern(383), kUnknownRemainder, 0, 0);
  // Un et deux : UN tirage pour les trois canaux, clair puis sombre.
  const EffectNodeSeed clair =
      EffectNodeSeedFromPattern(SinglePattern(1207), kUnknownRemainder, 0, 0);
  const EffectNodeSeed sombre =
      EffectNodeSeedFromPattern(SinglePattern(1208), kUnknownRemainder, 0, 0);
  for (int rang = 0; rang < 20; ++rang) {
    const ro::render::EffectNode v = ro::render::SowEffectNode(vert, generator);
    CHECK(v.color[1] == Approx(255.0F));
    CHECK(v.color[0] >= 160.0F);
    CHECK(v.color[2] <= 240.0F);
    const ro::render::EffectNode c = ro::render::SowEffectNode(clair, generator);
    CHECK(c.color[0] == c.color[1]);
    CHECK(c.color[1] == c.color[2]);
    CHECK(c.color[0] >= 200.0F);
    CHECK(c.color[0] <= 254.0F);
    const ro::render::EffectNode s = ro::render::SowEffectNode(sombre, generator);
    CHECK(s.color[0] == s.color[2]);
    CHECK(s.color[0] >= 20.0F);
    CHECK(s.color[0] <= 119.0F);
  }
}
