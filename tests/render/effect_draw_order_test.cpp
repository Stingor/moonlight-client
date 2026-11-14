// L'ORDRE DES TIRAGES D'UN BÂTISSEUR, et la flèche de feu dont les étincelles
// partent du dernier projectile.

#include <cmath>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_launch.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_node_sprite_seed.h"

using Catch::Approx;
using ro::render::EffectNodePattern;
using ro::render::EffectNodeRange;
using ro::render::EffectNodeSeed;
using ro::render::EffectNodeShape;
using ro::render::EffectSpriteDraw;

namespace {

constexpr std::uint32_t kBlueSpray = 0;
constexpr std::uint32_t kIceWall = 74;
constexpr std::uint32_t kFireArrow = 31;
constexpr std::uint32_t kSeed = 12345;

const EffectNodePattern& DeForme(std::uint32_t effect_id, EffectNodeShape forme,
                                 std::size_t rang = 0) {
  std::size_t seen = 0;
  const EffectNodePattern* found = nullptr;
  for (const EffectNodePattern& pattern : ro::render::EffectNodePatternsFor(effect_id)) {
    if (pattern.shape == forme && seen++ == rang) {
      found = &pattern;
      break;
    }
  }
  REQUIRE(found != nullptr);
  return *found;
}

}  // namespace

TEST_CASE("un ordre de bâtisseur tire tout d'avance, dans son ordre", "[render][draw_order]") {
  EffectNodeSeed seed;
  seed.life_ticks = 6;
  seed.life_ticks_high = 29;
  seed.launch_yaw_degrees = EffectNodeRange{-40.0F, 39.0F, 80};
  seed.step_length = EffectNodeRange{0.6F, 1.49F, 90};
  seed.draw_order.draws[0] = EffectSpriteDraw::kLaunchYaw;
  seed.draw_order.draws[1] = EffectSpriteDraw::kLife;
  seed.draw_order.count = 2;
  std::uint32_t draw = kSeed;
  ro::render::PredrawEffectSprite(seed, EffectNodeRange{1.0F, 0.0F}, draw);

  std::uint32_t expected = kSeed;
  const float cap = ro::render::DrawEffectNodeRange(seed.launch_yaw_degrees, expected);
  const std::int32_t life = ro::render::DrawEffectNodeInteger(6, 29, expected);
  CHECK(ro::render::EffectSpritePredrawnValue(seed.predrawn, EffectSpriteDraw::kLaunchYaw) ==
        Approx(cap));
  CHECK(ro::render::EffectSpritePredrawnValue(seed.predrawn, EffectSpriteDraw::kLife) ==
        Approx(static_cast<float>(life)));
  // Ce que l'ordre ne nomme pas prend sa borne basse, sans tirer.
  CHECK(ro::render::EffectSpritePredrawnValue(seed.predrawn, EffectSpriteDraw::kStep) ==
        Approx(0.6F));
  CHECK(draw == expected);
  // Et le nœud semé lit ce qui a été tiré.
  const ro::render::EffectNode node = ro::render::SowEffectNode(seed, draw);
  CHECK(node.life_ticks == life);
  CHECK(node.step_length == Approx(0.6F));
}

TEST_CASE("la gerbe bleue tire son lancer avant son pas et sa vie", "[render][draw_order]") {
  const EffectNodePattern& ecarte = DeForme(kBlueSpray, EffectNodeShape::kSprite, 0);
  const EffectNodePattern& retombe = DeForme(kBlueSpray, EffectNodeShape::kSprite, 1);
  REQUIRE(ecarte.supplement.sprite_draw_order.count == 6);
  CHECK(ecarte.supplement.sprite_draw_order.draws[0] == EffectSpriteDraw::kLaunchYaw);
  CHECK(ecarte.supplement.sprite_draw_order.draws[3] == EffectSpriteDraw::kStep);
  CHECK(ecarte.supplement.sprite_draw_order.draws[4] == EffectSpriteDraw::kLife);
  REQUIRE(retombe.supplement.sprite_draw_order.count == 7);
  CHECK(retombe.supplement.sprite_draw_order.draws[3] == EffectSpriteDraw::kLife);
  CHECK(retombe.supplement.sprite_draw_order.draws[5] == EffectSpriteDraw::kFallSpeed);
}

TEST_CASE("le mur de glace jette l'échelle et tire son lacet avant son penchant",
          "[render][draw_order]") {
  const EffectNodePattern& shard = DeForme(kIceWall, EffectNodeShape::kCrossSpikes);
  REQUIRE(shard.supplement.draws_discarded_before_scale == 1);
  REQUIRE(shard.supplement.yaw_drawn_before_lean);
  std::uint32_t draw = kSeed;
  const ro::render::EffectNode node = ro::render::EffectNodeFromPattern(shard, draw);

  std::uint32_t expected = kSeed;
  static_cast<void>(ro::render::NextEffectNodeDraw(expected));
  const float height = ro::render::DrawEffectNodeRange(
      ro::render::EffectNodePatternRange(shard, ro::render::EffectDrawnRange::kHalfHeight,
                                         shard.ring.height, shard.ring.height_high),
      expected);
  constexpr std::uint32_t kTour = 360;
  const auto lacet = static_cast<float>(ro::render::NextEffectNodeDraw(expected) % kTour);
  CHECK(node.half_height.half_size == Approx(height));
  CHECK(node.yaw_degrees == Approx(lacet));
}

TEST_CASE("les angles d'une flèche : cap vers le point, inclinaison de sa naissance",
          "[render][draw_order]") {
  const ro::render::EffectProjectileAngles angles =
      ro::render::EffectClientArrowAngles(30.0F, 20.0F, 60.0F, 0.0F, 0.0F);
  // La face tournée vers le point : `atan2(−30, 20)`, ramenée sur le tour.
  const float face = std::atan2(-30.0F, 20.0F) / 3.141592F * 180.0F + 360.0F;
  CHECK(angles.yaw_degrees == Approx(-face).epsilon(0.0001));
  CHECK(angles.pitch_degrees ==
        Approx(std::atan2(60.0F, std::sqrt(30.0F * 30.0F + 20.0F * 20.0F)) / 3.141592F * 180.0F)
            .epsilon(0.0001));
  // La ligne des angles nuls part vers la profondeur du client, la nôtre
  // négative ; un quart de tour d'inclinaison la dresse vers le haut.
  const glm::vec3 straight = ro::render::EffectClientPitchedHeading(0.0F, 0.0F);
  CHECK(straight.z == Approx(-1.0F));
  CHECK(ro::render::EffectClientPitchedHeading(0.0F, 90.0F).y == Approx(1.0F).margin(0.0001F));
}

TEST_CASE("les étincelles de la flèche de feu partent de ses angles", "[render][draw_order]") {
  const EffectNodePattern& arrow = DeForme(kFireArrow, EffectNodeShape::kCrossedPlanes);
  CHECK(arrow.supplement.records_projectile);
  const EffectNodePattern& spark = DeForme(kFireArrow, EffectNodeShape::kSprite);
  CHECK(spark.supplement.launches_from_last_projectile);
  CHECK(spark.launch_yaw_is_absolute);
  CHECK(spark.launch_yaw_low == Approx(-20.0F));
  CHECK(spark.launch_pitch_high == Approx(9.0F));

  EffectNodeSeed seed = ro::render::EffectNodeSeedFromPattern(spark, 0, 4, 1);
  seed.last_projectile = ro::render::EffectProjectileAngles{10.0F, 45.0F};
  std::uint32_t draw = kSeed;
  ro::render::PredrawEffectSprite(seed, EffectNodeRange{0.2F, 0.49F, 30}, draw);
  const float cap =
      *ro::render::EffectSpritePredrawnValue(seed.predrawn, EffectSpriteDraw::kLaunchYaw);
  const float pitch =
      *ro::render::EffectSpritePredrawnValue(seed.predrawn, EffectSpriteDraw::kLaunchPitch);
  const ro::render::EffectLaunchDraw lance =
      ro::render::DrawEffectLaunch(seed, 0.0F, 20, 1.0F, draw);
  const glm::vec3 attendue = ro::render::EffectClientPitchedHeading(10.0F + cap, 45.0F + pitch);
  CHECK(lance.direction.x == Approx(attendue.x));
  CHECK(lance.direction.y == Approx(attendue.y));
  CHECK(lance.direction.z == Approx(attendue.z));
  // Et la flèche tire sa profondeur avant son abscisse.
  CHECK(arrow.supplement.offsets_drawn_depth_first);
}

TEST_CASE("l'angle du bras se tire en degrés entiers, ou s'écrit fixe", "[render][draw_order]") {
  constexpr std::uint32_t kAspersio = 86;
  constexpr std::uint32_t kSpirale = 72;
  constexpr std::int32_t kFullTurnDegrees = 360;
  const EffectNodeSeed goutte =
      ro::render::EffectNodeSeedFromPattern(DeForme(kAspersio, EffectNodeShape::kSprite), 0, 61, 1);
  CHECK(goutte.orbit_degrees.values == kFullTurnDegrees);
  // La spirale écrit deux degrés : le cap du bras chez nous est 2 − 90.
  const EffectNodePattern& spirale = DeForme(kSpirale, EffectNodeShape::kSprite);
  const EffectNodeSeed spark = ro::render::EffectNodeSeedFromPattern(spirale, 0, 0, 1);
  CHECK(spark.orbit_degrees.low == Approx(-88.0F));
  CHECK(spark.orbit_degrees.high == Approx(-88.0F));
  CHECK(spark.orbit_radius_step_switch_tick == 95);
  CHECK(spark.orbit_radius_step_after_switch == Approx(-0.15F));
  CHECK(spirale.orbit_degrees_per_tick == Approx(8.0F));
  CHECK(spirale.scatter_counts_twice);
}

TEST_CASE("le bras se resserre à l'âge dit, et passe de l'autre côté", "[render][draw_order]") {
  ro::render::EffectNodePool pool;
  ro::render::PooledEffectNode node;
  node.node.life_ticks = 1000;
  node.orbit_radius = 0.1F;
  node.orbit_degrees_per_tick = 8.0F;
  node.orbit_radius_step_per_tick = 0.15F;
  node.orbit_radius_step_switch_tick = 2;
  node.orbit_radius_step_after_switch = -0.15F;
  pool.Add(node);
  // Trois tics montants (âges 0, 1, 2), puis le pas rentre.
  for (int tick = 0; tick < 3; ++tick) {
    pool.Advance(ro::render::kEffectTickMs);
  }
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes()[0].orbit_radius == Approx(0.55F));
  CHECK(pool.nodes()[0].orbit_radius_step_per_tick == Approx(-0.15F));
  for (int tick = 0; tick < 5; ++tick) {
    pool.Advance(ro::render::kEffectTickMs);
  }
  CHECK(pool.nodes()[0].orbit_radius == Approx(-0.2F));
}
