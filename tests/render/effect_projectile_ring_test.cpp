// LES PROJECTILES ET LES ANNEAUX des sous-types 5, 6, 7, 9, 12, 13 et 15 : ce
// que la table en dit, et ce que le mouvement et le semis en font.

#include <cmath>
#include <cstdint>
#include <set>
#include <span>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_cross_spikes.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_marching_sower.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_sprite_seed.h"
#include "ro/render/effect_node_world.h"

using Catch::Approx;
using ro::render::AdvanceEffectNode;
using ro::render::EffectNode;
using ro::render::EffectNodeFromPattern;
using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;
using ro::render::EffectNodePatternTextures;
using ro::render::EffectNodeShape;

namespace {

constexpr std::uint32_t kSonicBlow = 122;
constexpr std::uint32_t kBlueYellowRing = 73;
constexpr std::uint32_t kFlasher = 99;
constexpr std::uint32_t kPlasma = 94;
constexpr std::uint32_t kPlasmaEnTourbillons = 452;
constexpr std::uint32_t kGroundBash = 82;
constexpr std::uint32_t kEarthSpike = 79;
constexpr std::uint32_t kIceWall = 74;
constexpr std::uint32_t kTrepied = 132;
constexpr std::uint32_t kProjectileDePierre = 123;
constexpr std::uint32_t kTraitDApparat = 801;
constexpr std::uint32_t kPluieDePieces = 10;
constexpr std::uint32_t kDetoxication = 21;
constexpr std::uint32_t kRemoveTrap = 100;
constexpr std::uint32_t kAdrenalineRush = 98;
constexpr std::uint32_t kSteal = 18;
constexpr std::uint32_t kRisingRing = 9;
// Assez de tirages pour que chaque valeur d'un petit tirage sorte.
constexpr std::uint32_t kTirages = 400;

const EffectNodePattern& DeForme(std::uint32_t effect_id, EffectNodeShape forme,
                                 std::size_t rang = 0) {
  std::size_t seen = 0;
  const EffectNodePattern* found = nullptr;
  for (const EffectNodePattern& pattern : EffectNodePatternsFor(effect_id)) {
    if (pattern.shape == forme && seen++ == rang) {
      found = &pattern;
      break;
    }
  }
  REQUIRE(found != nullptr);
  return *found;
}

void Advance(EffectNode& node, std::int32_t ticks) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    static_cast<void>(AdvanceEffectNode(node));
  }
}

}  // namespace

TEST_CASE("sonic blow : l'anneau part deux unités devant, levé et décalé en entiers",
          "[render][table]") {
  const EffectNodePattern& ring = DeForme(kSonicBlow, EffectNodeShape::kRingBand);
  // Le cap est l'angle de base de l'effet : le patron le suit tel quel.
  CHECK(ring.node_yaw_follows_owner);
  CHECK_FALSE(ring.ring.yaw_is_owner_body_angle);
  CHECK(ring.ring.axial_translation_start == Approx(2.0F));
  CHECK(ring.fade_out_tick == 6);
  // `(rand() % 10 + 1) / 4` puis `(rand() % 20 + 1) / 2 − 11`, sur une base nulle.
  CHECK(ring.ring.offset_x_draw.modulus == 10);
  CHECK(ring.ring.offset_x_draw.divisor == 4);
  CHECK(ring.ring.burial_depth == Approx(0.0F));
  CHECK(ring.ring.burial_depth_draw.modulus == 20);
  CHECK(ring.ring.burial_depth_draw.divisor == 2);
  CHECK(ring.ring.burial_depth_draw.bias == -11);
  CHECK(ring.ring.draws_discarded_after_burial == 1);
  EffectNode node = EffectNodeFromPattern(ring);
  CHECK(node.axial_translation == Approx(2.0F));
}

TEST_CASE("l'anneau bleu naît deux fois, et la couronne jaune relance son rayon",
          "[render][table]") {
  const EffectNodePattern& first = DeForme(kBlueYellowRing, EffectNodeShape::kRingBand, 0);
  const EffectNodePattern& second = DeForme(kBlueYellowRing, EffectNodeShape::kRingBand, 1);
  CHECK(first.first_tick == 0);
  CHECK(second.first_tick == 5);
  CHECK(second.period_ticks == 0);
  CHECK_FALSE(first.life_from_effect);
  CHECK(first.life_ticks == 20);
  CHECK(second.life_ticks == 15);
  CHECK(first.alpha_start == Approx(240.0F));
  CHECK(second.alpha_start == Approx(205.0F));
  CHECK(first.fade_out_tick == 10);
  CHECK(second.fade_out_tick == 8);
  CHECK(first.launch_yaw_low == Approx(0.0F));
  CHECK(second.launch_yaw_low == Approx(100.0F));

  const EffectNodePattern& crown = DeForme(kBlueYellowRing, EffectNodeShape::kRingCylinder);
  CHECK(crown.ring.ring_radius_acceleration == Approx(-0.7F / 30.0F));
  EffectNode node = EffectNodeFromPattern(crown);
  // Vingt tics de croissance freinée : `8 + Σ (0,7 − k × 0,7/30)`, k = 1..20.
  Advance(node, 20);
  const float before = node.ring_radius.half_size;
  CHECK(before == Approx(8.0F + 20.0F * 0.7F - 0.7F / 30.0F * 210.0F).epsilon(0.001));
  // Au vingtième tic la bascule relance le RAYON, pas le pas : 0,08 par tic.
  Advance(node, 1);
  CHECK(node.ring_radius.half_size == Approx(before + 0.08F));
  CHECK(node.step_length == Approx(0.0F));
}

TEST_CASE("l'anneau jaune de l'effet 9 freine sa montée", "[render][table]") {
  const EffectNodePattern& ring = DeForme(kRisingRing, EffectNodeShape::kRingBand);
  CHECK(ring.ring.height_acceleration == Approx(-(2.0F / 80.0F) / 1.5F));
}

TEST_CASE("la rune au sol prend la teinte et la bascule de son paramètre", "[render][table]") {
  struct Rune {
    std::uint32_t effect_id;
    std::array<std::uint8_t, 3> tint;
    std::int32_t bascule;
    std::int32_t sortie;
  };
  const std::array<Rune, 8> runes = {{
      {119, {255, 255, 255}, 17, 40},
      {722, {250, 210, 140}, 17, 40},
      {724, {140, 255, 140}, 17, 40},
      {725, {255, 255, 255}, 17, 40},
      {778, {255, 255, 255}, 10, 30},
      {780, {255, 140, 140}, 17, 40},
      {786, {210, 250, 210}, 17, 40},
      {812, {10, 10, 10}, 10, 30},
  }};
  for (const Rune& rune : runes) {
    INFO("effet " << rune.effect_id);
    const EffectNodePattern& quad = DeForme(rune.effect_id, EffectNodeShape::kOrientedQuad);
    CHECK(quad.color == rune.tint);
    CHECK(quad.ring.second_keyframe_tick == rune.bascule);
    CHECK(quad.fade_out_tick == rune.sortie);
    // La seconde écriture de l'ordonnée est celle qui vaut.
    CHECK(quad.ring.burial_depth == Approx(-5.0F));
  }
}

TEST_CASE("thunder plasma blast : l'éclair tous les vingt tics, la boule au dixième",
          "[render][table]") {
  const EffectNodePattern& eclair = DeForme(kPlasma, EffectNodeShape::kOrientedQuad, 0);
  CHECK(eclair.first_tick == 0);
  CHECK(eclair.period_ticks == 20);
  REQUIRE(EffectNodePatternTextures(eclair).size() == 1);
  CHECK(EffectNodePatternTextures(eclair).front().ends_with("thunder_pang.bmp"));
  const EffectNodePattern& ball = DeForme(kPlasma, EffectNodeShape::kOrientedQuad, 1);
  CHECK(ball.first_tick == 10);
  CHECK(ball.period_ticks == 0);
  REQUIRE(EffectNodePatternTextures(ball).size() == 5);
  CHECK(EffectNodePatternTextures(ball).back().ends_with("thunder_ball_f.bmp"));
  const EffectNodePattern& tourbillons =
      DeForme(kPlasmaEnTourbillons, EffectNodeShape::kOrientedQuad, 1);
  REQUIRE(EffectNodePatternTextures(tourbillons).size() == 5);
  CHECK(EffectNodePatternTextures(tourbillons).front().ends_with("twirl_soft.bmp"));
  CHECK(EffectNodePatternTextures(DeForme(kPlasmaEnTourbillons, EffectNodeShape::kOrientedQuad, 0))
            .front()
            .ends_with("pokjuk_d.bmp"));
}

TEST_CASE("flasher : au quarantième tic l'alpha retombe à cent vingt", "[render][table]") {
  const EffectNodePattern& quad = DeForme(kFlasher, EffectNodeShape::kOrientedQuad);
  EffectNode node = EffectNodeFromPattern(quad);
  Advance(node, 40);
  CHECK(node.alpha == Approx(200.0F));
  // L'image-clé d'abord, puis le pas du tic : 120 + 200/6.
  Advance(node, 1);
  CHECK(node.alpha == Approx(120.0F + 200.0F / 6.0F));
  CHECK(node.alpha_max == Approx(200.0F));
}

TEST_CASE("ground bash : la rune ne naît qu'au cinquantième tic", "[render][table]") {
  const EffectNodePattern& rune = DeForme(kGroundBash, EffectNodeShape::kOrientedQuad);
  CHECK(rune.first_tick == 50);
  CHECK(rune.last_tick == 50);
  CHECK(rune.period_ticks == 0);
}

TEST_CASE("une pointe qui plonge se peint en blanc opaque", "[render][table]") {
  const EffectNodePattern& centre = DeForme(kEarthSpike, EffectNodeShape::kCrossSpikes, 0);
  EffectNode node = EffectNodeFromPattern(centre);
  REQUIRE(ro::render::EffectNodeDives(node));
  CHECK(node.alpha == Approx(255.0F));
  Advance(node, 100);
  CHECK(node.alpha == Approx(255.0F));
}

TEST_CASE("la couronne de la pointe de terre tire son cap par secteur", "[render][table]") {
  const EffectNodePattern& crown = DeForme(kEarthSpike, EffectNodeShape::kCrossSpikes, 1);
  CHECK(crown.supplement.copy_yaw_sector_degrees == Approx(60.0F));
  CHECK(crown.supplement.copy_yaw_ceiling_degrees == Approx(360.0F));
  CHECK_FALSE(crown.ring.draws_its_yaw);
}

TEST_CASE("le mur de glace pose trois éclats sur la profondeur", "[render][table]") {
  const EffectNodePattern& eclats = DeForme(kIceWall, EffectNodeShape::kCrossSpikes);
  CHECK(eclats.ring.instance_count == 3);
  CHECK(eclats.ring.instance_placement == ro::render::EffectNodeInstancePlacement::kDrawnDepthLine);
  CHECK(eclats.sprite_scale == Approx(2.5F));
  CHECK(eclats.ring.draws_its_yaw);
  CHECK(eclats.ring.lean_degrees == Approx(-3.0F));
  CHECK(eclats.ring.lean_degrees_high == Approx(2.0F));
  // Celui du milieu reste au centre ; les deux autres s'écartent de moins de deux,
  // à l'opposé l'un de l'autre pour un même angle.
  std::uint32_t draw = 7;
  for (std::uint32_t attempt = 0; attempt < kTirages; ++attempt) {
    const glm::vec3 middle = ro::render::EffectNodeInstanceOffset(
        ro::render::EffectNodeInstancePlacement::kDrawnDepthLine, 3, 1, 2.0F, 0.0F, 0.0F, draw);
    CHECK(glm::length(middle) == Approx(0.0F));
    const glm::vec3 edge = ro::render::EffectNodeInstanceOffset(
        ro::render::EffectNodeInstancePlacement::kDrawnDepthLine, 3, 2, 2.0F, 0.0F, 0.0F, draw);
    CHECK(edge.x == Approx(0.0F));
    CHECK(std::abs(edge.z) <= 2.0F);
  }
}

TEST_CASE("le trépied de pierre pose trois pointes fixes", "[render][table]") {
  const EffectNodePattern& pointes = DeForme(kTrepied, EffectNodeShape::kCrossSpikes);
  CHECK(pointes.ring.instance_count == 3);
  REQUIRE(pointes.supplement.copy_placement_count == 3);
  CHECK(pointes.supplement.copy_placements[1].offset_x == Approx(12.0F));
  CHECK(pointes.supplement.copy_placements[1].offset_z == Approx(6.0F));
  CHECK(pointes.supplement.copy_placements[2].yaw_degrees == Approx(120.0F));
  CHECK(pointes.ring.lean_degrees == Approx(-15.0F));
  CHECK_FALSE(pointes.ring.draws_its_yaw);
}

TEST_CASE("le projectile de pierre marche tous les trois tics, jugé depuis la source",
          "[render][effect]") {
  const ro::render::EffectMarchingSower* march =
      ro::render::EffectMarchingSowerFor(kProjectileDePierre);
  REQUIRE(march != nullptr);
  CHECK(march->first_distance == Approx(2.0F));
  CHECK(march->distance_per_tick == Approx(2.0F));
  // Une source loin de sa cible ne s'arrête jamais : un semis tous les trois tics.
  CHECK(ro::render::EffectMarchingSowerSowsAt(*march, 0, 40.0F));
  CHECK_FALSE(ro::render::EffectMarchingSowerSowsAt(*march, 1, 40.0F));
  CHECK(ro::render::EffectMarchingSowerSowsAt(*march, 3, 40.0F));
  CHECK(ro::render::EffectMarchingSowerSowsAt(*march, 60, 40.0F));
  // Une source déjà au but ne sème qu'une fois.
  CHECK(ro::render::EffectMarchingSowerSowsAt(*march, 0, 2.0F));
  CHECK_FALSE(ro::render::EffectMarchingSowerSowsAt(*march, 3, 2.0F));
  const EffectNodePattern& tip = DeForme(kProjectileDePierre, EffectNodeShape::kCrossSpikes);
  CHECK(tip.ring.draws_its_yaw);
  CHECK(tip.ring.lean_degrees == Approx(-15.0F));
  CHECK(tip.ring.lean_degrees_high == Approx(14.0F));
}

TEST_CASE("le trait d'apparat peint ses pointes d'un bleu tiré", "[render][effect]") {
  CHECK(DeForme(kTraitDApparat, EffectNodeShape::kCrossSpikes).supplement.cross_spike_mark ==
        ro::render::kCrossSpikeDrawnColor);
  std::set<std::uint8_t> rouges;
  std::uint32_t draw = 3;
  for (std::uint32_t attempt = 0; attempt < kTirages; ++attempt) {
    const std::array<std::uint8_t, 4> tint = ro::render::DrawCrossSpikeOwnColor(draw);
    CHECK(tint[0] >= 40);
    CHECK(tint[0] <= 50);
    CHECK(tint[1] >= 40);
    CHECK(tint[1] <= 50);
    CHECK(tint[2] == 255);
    CHECK(tint[3] == 255);
    rouges.insert(tint[0]);
  }
  CHECK(rouges.size() == 11);
}

TEST_CASE("sous /mineffect, trois bâtisseurs changent de cadence", "[render][table]") {
  CHECK(DeForme(kPluieDePieces, EffectNodeShape::kSprite)
            .supplement.period_ticks_with_minimal_effects ==
        ro::render::kEffectNodeNotSownWithMinimalEffects);
  CHECK(DeForme(kDetoxication, EffectNodeShape::kSprite)
            .supplement.period_ticks_with_minimal_effects == 15);
  CHECK(DeForme(27, EffectNodeShape::kCrossSpikes).supplement.period_ticks_with_minimal_effects ==
        ro::render::kEffectNodeNotSownWithMinimalEffects);
}

TEST_CASE("les pièces fondent sur le dernier tiers de leur vie tirée", "[render][effect]") {
  const EffectNodePattern& pieces = DeForme(kPluieDePieces, EffectNodeShape::kSprite);
  const ro::render::EffectNodeSeed seed = ro::render::EffectNodeSeedFromPattern(pieces, 0, 20, 1);
  std::uint32_t draw = 11;
  for (std::uint32_t attempt = 0; attempt < kTirages; ++attempt) {
    const EffectNode node = ro::render::SowEffectNode(seed, draw);
    CHECK(node.fade_out_tick == node.life_ticks - node.life_ticks / 3);
  }
}

TEST_CASE("steal : l'inclinaison place le départ, le vol reste à plat", "[render][effect]") {
  const ro::render::EffectNodeSeed seed =
      ro::render::EffectNodeSeedFromPattern(DeForme(kSteal, EffectNodeShape::kSprite), 0, 0, 1);
  REQUIRE(seed.launch_pitch_places_only);
  std::uint32_t draw = 5;
  bool has_tilted_start = false;
  for (std::uint32_t attempt = 0; attempt < kTirages; ++attempt) {
    const ro::render::EffectLaunchDraw lance =
        ro::render::DrawEffectLaunch(seed, 0.0F, 50, 0.7F, draw);
    CHECK(lance.direction.y == Approx(0.0F));
    has_tilted_start = has_tilted_start || std::abs(lance.offset.y - 10.0F) > 0.5F;
  }
  CHECK(has_tilted_start);
}

TEST_CASE("remove trap : un second jet, un bras qui s'allonge, une montée freinée tirée",
          "[render][effect]") {
  const EffectNodePattern& jet = DeForme(kRemoveTrap, EffectNodeShape::kSprite);
  CHECK(jet.first_tick == 0);
  CHECK(jet.last_tick == 7);
  CHECK(jet.period_ticks == 7);
  CHECK(jet.step_low == Approx(0.2F));
  CHECK(jet.launch_yaw_is_absolute);
  const ro::render::EffectNodeSeed seed = ro::render::EffectNodeSeedFromPattern(jet, 0, 0, 1);
  std::uint32_t draw = 9;
  std::set<float> accelerations;
  for (std::uint32_t attempt = 0; attempt < kTirages; ++attempt) {
    const ro::render::EffectLaunchDraw lance =
        ro::render::DrawEffectLaunch(seed, 0.0F, 50, 0.2F, draw);
    CHECK(lance.fall_speed == Approx(2.0F));
    CHECK(lance.fall_acceleration <= -0.0799F);
    CHECK(lance.fall_acceleration >= -0.0891F);
    accelerations.insert(lance.fall_acceleration);
  }
  CHECK(accelerations.size() == 10);
}

TEST_CASE("adrenaline rush : quatre étincelles en croix, une orbite qui accélère",
          "[render][effect]") {
  const EffectNodePattern& sparks = DeForme(kAdrenalineRush, EffectNodeShape::kSprite);
  CHECK(sparks.orbit_degrees_per_tick == Approx(0.3F));
  CHECK(sparks.supplement.orbit_acceleration_per_tick == Approx(0.2F));
  CHECK(sparks.supplement.copy_orbit_step_degrees == Approx(90.0F));
  CHECK(sparks.fall_speed_low == Approx(-0.1F));

  ro::render::EffectNodePool pool;
  ro::render::PooledEffectNode node;
  node.node.life_ticks = 1000;
  node.orbit_anchor = glm::vec3{0.0F, 0.0F, 0.0F};
  node.orbit_radius = 7.0F;
  node.orbit_degrees_per_tick = 0.3F;
  node.orbit_acceleration_per_tick = 0.2F;
  node.ground_position = glm::vec3{7.0F, 0.0F, 0.0F};
  pool.Add(node);
  // Premier tic : 0,5 degré, lu en degrés entiers — la place ne bouge pas.
  pool.Advance(ro::render::kEffectTickMs);
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes()[0].ground_position.z == Approx(0.0F).margin(0.0001F));
  // Deuxième : 0,5 + 0,7 = 1,2, soit un degré.
  pool.Advance(ro::render::kEffectTickMs);
  CHECK(pool.nodes()[0].ground_position.z ==
        Approx(7.0F * std::sin(glm::radians(1.0F))).margin(0.0001F));
}
