// LA NEIGE (162) : deux flocons à chaque tic autour du joueur, nés dans un
// disque cent unités au-dessus du sol, qui tombent tout droit et restent où ils
// sont nés quand le joueur marche.

#include <cstdint>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_bearer_follow.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_sprite_seed.h"

#include "support/effect_bearers.h"

using Catch::Approx;
using ro::render::EffectNodePattern;
using ro::render::EffectNodePool;

namespace {

constexpr std::uint32_t kSnowEffect = 162;
constexpr std::uint32_t kBearer = 4000;
const glm::vec3 kBirthPlace{10.0F, 0.0F, 20.0F};
// Une marche du porteur, en unités.
constexpr float kWalkedDistance = 40.0F;

const EffectNodePattern& SnowPattern() {
  const std::span<const EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(kSnowEffect);
  REQUIRE(patterns.size() == 1);
  return patterns.front();
}

}  // namespace

TEST_CASE("la neige sème deux flocons à chaque tic, sans fin", "[render][effect_snowfall]") {
  const EffectNodePattern& snow = SnowPattern();
  CHECK(snow.copies == 2);
  CHECK(snow.period_ticks == 1);
  CHECK(snow.last_tick > ro::render::kEffectNodeDefaultLifeTicks * 100);
  CHECK(snow.life_ticks == 320);
  CHECK(snow.fade_out_tick == 256);
}

TEST_CASE("un flocon naît dans un disque, cent unités au-dessus du sol, et tombe",
          "[render][effect_snowfall]") {
  const EffectNodePattern& snow = SnowPattern();
  CHECK(snow.launch_yaw_high - snow.launch_yaw_low == Approx(359.0F));
  CHECK(snow.launch_distance_low == Approx(1.0F));
  CHECK(snow.launch_distance_high == Approx(300.0F));
  // Moins, dans le repère du client : cent unités AU-DESSUS du sol.
  CHECK(snow.launch_drop == Approx(-100.0F));
  // Une demi-unité par tic vers le bas, sans accélération ni pas au sol.
  CHECK(snow.fall_speed_low == Approx(0.5F));
  CHECK(snow.fall_speed_high == Approx(0.5F));
  CHECK(snow.step_high == Approx(0.0F));
  // Il paraît en dix tics : un dixième de son plafond par tic, depuis zéro.
  CHECK(snow.alpha_start == Approx(0.0F));
  CHECK(snow.alpha_step == Approx(snow.alpha_max / 10.0F));
}

TEST_CASE("un flocon né ne suit pas le joueur qui marche", "[render][effect_snowfall]") {
  CHECK(SnowPattern().nodes_stay_where_sown);
  // ET LA GRAINE LE PORTE jusqu'au semis, qui le pose sur le nœud.
  CHECK(ro::render::EffectNodeSeedFromPattern(SnowPattern(), 0, 0, 0).nodes_stay_where_sown);
  // L'EFFET, lui, suit son porteur : c'est ce qui fait naître les flocons
  // suivants autour du joueur.
  CHECK(ro::render::EffectBearerFollowOf(kSnowEffect).last_tick ==
        ro::render::kEffectFollowsToTheEnd);

  EffectNodePool pool;
  {
    const EffectNodePool::AnchoredSowing anchor{pool, kBearer, kBirthPlace, kSnowEffect};
    ro::render::PooledEffectNode flocon;
    flocon.ground_position = kBirthPlace;
    flocon.stays_where_sown = true;
    pool.Add(flocon);
  }
  ro::test_support::MoveBearer(pool, kBearer, kBirthPlace + glm::vec3{kWalkedDistance, 0.0F, 0.0F});
  REQUIRE(pool.nodes().size() == 1);
  CHECK(pool.nodes()[0].ground_position.x == Approx(kBirthPlace.x));
  // Il appartient pourtant toujours au porteur : il part avec lui.
  CHECK(ro::test_support::IsAnchoredTo(pool, kBearer));
}
