// SIGHT TRASHER (effet 62) : huit boules et huit ombres en rosace, quatre vagues
// qui naissent chacune plus loin, plus bas, plus petites et plus pâles, filent
// sur leur voie radiale, ralentissent au même tic de l'effet et glissent d'un
// bloc ; puis l'onde jaune du tic dix, une couronne tournée vers l'extérieur.

#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_bearer_follow.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_sprite_seed.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/skill_suite_table.h"

using Catch::Approx;
using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;
using ro::render::EffectNodeSeed;
using ro::render::EffectNodeSeedFromPattern;
using ro::render::EffectNodeShape;

namespace {

constexpr std::uint32_t kSightTrasher = 62;
constexpr std::int32_t kParametre = 1;
// Les quatre tics de semis des boules et des ombres.
constexpr std::int32_t kFirstWave = 0;
constexpr std::int32_t kDeuxiemeVague = 5;
constexpr std::int32_t kTroisiemeVague = 10;
constexpr std::int32_t kQuatriemeVague = 15;
constexpr std::int32_t kWaveTick = 10;

struct Patterns {
  const EffectNodePattern* wave = nullptr;
  const EffectNodePattern* ball = nullptr;
  const EffectNodePattern* ombre = nullptr;
};

Patterns SightTrasherPatterns() {
  Patterns trouves;
  for (const EffectNodePattern& pattern : EffectNodePatternsFor(kSightTrasher)) {
    if (pattern.shape == EffectNodeShape::kRingCylinder) {
      trouves.wave = &pattern;
    } else if (trouves.ball == nullptr) {
      trouves.ball = &pattern;
    } else {
      trouves.ombre = &pattern;
    }
  }
  REQUIRE(trouves.wave != nullptr);
  REQUIRE(trouves.ball != nullptr);
  REQUIRE(trouves.ombre != nullptr);
  return trouves;
}

float Radians(float degrees) { return degrees * std::numbers::pi_v<float> / 180.0F; }

}  // namespace

TEST_CASE("Sight Trasher : quatre vagues de huit, en rosace", "[render][table]") {
  const Patterns p = SightTrasherPatterns();
  for (const EffectNodePattern* pattern : {p.ball, p.ombre}) {
    CHECK(pattern->copies == 8);
    CHECK(pattern->copy_heading_step_degrees == Approx(-45.0F));
    for (const std::int32_t tick : {kFirstWave, kDeuxiemeVague, kTroisiemeVague, kQuatriemeVague}) {
      CHECK(ro::render::EffectNodePatternSowsAt(*pattern, tick));
    }
    CHECK_FALSE(ro::render::EffectNodePatternSowsAt(*pattern, 20));
    // LE RAYON DE LA ROSACE GAGNE UN PAR VAGUE : `tic / 5 + 5`, en entier.
    CHECK(ro::render::EffectNodePatternScatterAt(*pattern, kFirstWave) == Approx(5.0F));
    CHECK(ro::render::EffectNodePatternScatterAt(*pattern, kQuatriemeVague) == Approx(8.0F));
  }
  // L'ÉCHELLE DÉCROÎT PAR VAGUE, autrement pour la boule et pour l'ombre.
  CHECK(ro::render::EffectNodePatternSpriteScaleAt(*p.ball, kTroisiemeVague) == Approx(1.5F));
  CHECK(ro::render::EffectNodePatternSpriteScaleAt(*p.ombre, kQuatriemeVague) == Approx(0.5F));
}

TEST_CASE("Sight Trasher : la vague dit sa hauteur, sa vie, sa sortie et sa bascule",
          "[render][table]") {
  const Patterns p = SightTrasherPatterns();
  const std::int32_t remainder = ro::render::EffectNodePatternTicksLeft(*p.ball, kDeuxiemeVague);
  const EffectNodeSeed seed =
      EffectNodeSeedFromPattern(*p.ball, remainder, kDeuxiemeVague, kParametre);
  // `tic / 3 − 20`, en entier : moins dix-neuf au tic cinq, et non −18,33.
  CHECK(seed.launch_drop == Approx(-19.0F));
  // LES TROIS RENDEZ-VOUS SONT DES TICS DE L'EFFET : mort à 85, sortie à 65,
  // bascule à 35 — la vague du tic cinq les atteint cinq tics plus tôt dans sa
  // propre vie.
  CHECK(seed.life_ticks == 80);
  CHECK(seed.radial_ticks_to_switch == 30);
  std::uint32_t generator = 1;
  const ro::render::EffectNode node = ro::render::SowEffectNode(seed, generator);
  CHECK(node.fade_out_tick == 60);
  // L'ALPHA DE LA DEUXIÈME VAGUE, `2 × (75 − 5)`.
  CHECK(node.alpha == Approx(140.0F));
  // La cadence de la boule se tire entre trois et quatre tics.
  CHECK(node.frame_hold_ticks >= 3);
  CHECK(node.frame_hold_ticks <= 4);

  // L'OMBRE RESTE AU SOL, à toutes les vagues.
  const EffectNodeSeed ombre =
      EffectNodeSeedFromPattern(*p.ombre, remainder, kQuatriemeVague, kParametre);
  CHECK(ombre.launch_drop == Approx(0.0F));
}

TEST_CASE("Sight Trasher : la voie radiale file, bascule, puis dérive", "[render][motion]") {
  const Patterns p = SightTrasherPatterns();
  EffectNodeSeed seed = EffectNodeSeedFromPattern(*p.ball, 1, kFirstWave, kParametre);
  // Une bascule rapprochée, pour la voir sans attendre trente-cinq tics.
  seed.radial_ticks_to_switch = 1;
  constexpr float kCapDuDeuxiemeExemplaire = -45.0F;
  ro::render::PooledEffectNode node;
  node.radial = ro::render::StartEffectRadialLane(seed, kCapDuDeuxiemeExemplaire);
  // LA MÊME DIRECTION QUE LE BRAS DE DISPERSION tiré au même cap.
  const glm::vec3 arm{std::cos(Radians(kCapDuDeuxiemeExemplaire)), 0.0F,
                      std::sin(Radians(kCapDuDeuxiemeExemplaire))};
  CHECK(node.radial.velocity.x == Approx(arm.x));
  CHECK(node.radial.velocity.y == Approx(0.0F));
  CHECK(node.radial.velocity.z == Approx(arm.z));

  // Premier tic : une unité le long du cap.
  ro::render::AdvanceRadialNode(node);
  CHECK(node.ground_position.x == Approx(arm.x));
  CHECK(node.ground_position.z == Approx(arm.z));
  // Deuxième tic : la bascule — moitié de la vitesse, et la dérive du client
  // `(−0,004 ; 0 ; 0,004)`, dont la profondeur se retourne chez nous.
  ro::render::AdvanceRadialNode(node);
  CHECK(node.ground_position.x == Approx(arm.x * 1.5F - 0.004F));
  CHECK(node.ground_position.z == Approx(arm.z * 1.5F - 0.004F));
  // Troisième tic : la dérive s'est ajoutée deux fois.
  ro::render::AdvanceRadialNode(node);
  CHECK(node.ground_position.x == Approx(arm.x * 2.0F - 0.004F * 3.0F));
  CHECK(node.ground_position.z == Approx(arm.z * 2.0F - 0.004F * 3.0F));
  CHECK(node.radial.ticks_to_switch == -1);
}

TEST_CASE("Sight Trasher : un nœud sans voie ne bouge pas", "[render][motion]") {
  ro::render::PooledEffectNode node;
  ro::render::AdvanceRadialNode(node);
  CHECK(node.ground_position == glm::vec3{0.0F, 0.0F, 0.0F});
}

TEST_CASE("Sight Trasher : l'onde jaune du tic dix s'ouvre hors de son rayon", "[render][table]") {
  const Patterns p = SightTrasherPatterns();
  CHECK(ro::render::EffectNodePatternSowsAt(*p.wave, kWaveTick));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAt(*p.wave, 0));
  CHECK(p.wave->life_ticks == 30);
  CHECK(p.wave->fade_out_tick == 20);
  CHECK(p.wave->ring.radius_at_anchor == Approx(-10.0F));
  CHECK(p.wave->ring.ring_radius_growth == Approx(2.5F));
  CHECK(p.wave->ring.ring_radius_acceleration == Approx(-(2.5F / 30.0F) / 1.5F));

  ro::render::EffectNode wave = ro::render::EffectNodeFromPattern(*p.wave);
  for (int tick = 0; tick < 4; ++tick) {
    static_cast<void>(ro::render::AdvanceEffectNode(wave));
  }
  const float radius = wave.ring_radius.half_size;
  REQUIRE(radius > 0.0F);
  const glm::vec3 anchor{0.0F, 0.0F, 0.0F};
  const std::vector<ro::render::EffectWorldQuad> quads = ro::render::ComposeEffectNodeRingCylinder(
      wave, anchor, ro::render::EffectNodeOriented(0.0F, 0.0F));
  REQUIRE_FALSE(quads.empty());
  // LE BORD « INTÉRIEUR » EST DIX UNITÉS AU-DELÀ DU RAYON : une bande tournée
  // vers l'extérieur, et non une couronne retournée donc invisible.
  CHECK(glm::length(quads.front().corners[0] - anchor) == Approx(radius + 10.0F));
  CHECK(glm::length(quads.front().corners[3] - anchor) == Approx(radius));
}

TEST_CASE("Sight Trasher : la rosace se lit dans les tables du client", "[render][motion]") {
  // UN ANGLE NÉGATIF SE LIT UN DEGRÉ À CÔTÉ : −45 comme 316.
  CHECK(ro::render::EffectClientTableDegrees(0) == 0);
  CHECK(ro::render::EffectClientTableDegrees(-45) == 316);
  CHECK(ro::render::EffectClientTableDegrees(-315) == 46);
  CHECK(ro::render::EffectClientTableDegrees(45) == 45);
  // La direction du client `(sin a, 0, −cos a)` : le premier exemplaire part
  // droit vers les profondeurs du client, donc vers les nôtres qui croissent.
  const float first = ro::render::EffectClientSinCosHeadingInWorld(0);
  CHECK(std::cos(Radians(first)) == Approx(0.0F).margin(1e-6));
  CHECK(std::sin(Radians(first)) == Approx(1.0F));
  // Le deuxième, à −45 lu 316 : `(sin 316, cos 316)` chez nous.
  const float deuxieme = ro::render::EffectClientSinCosHeadingInWorld(-45);
  CHECK(std::cos(Radians(deuxieme)) == Approx(std::sin(Radians(316.0F))));
  CHECK(std::sin(Radians(deuxieme)) == Approx(std::cos(Radians(316.0F))));
}

TEST_CASE("Sight Trasher : retire le Sight du lanceur, et reste où il est lancé",
          "[render][table]") {
  // LE RETRAIT PRÉCÈDE L'EFFET dans la suite de l'annonce, sur le même porteur.
  const std::span<const ro::render::SkillSuiteStep> suite =
      ro::render::SkillSuiteOf(81, ro::render::SkillSuiteMoment::kAnnounced);
  REQUIRE(suite.size() >= 2);
  CHECK(suite[0].kind == ro::render::SkillSuiteKind::kRemoveEffect);
  CHECK(suite[0].effect_id == 22);
  CHECK(suite[0].carrier == ro::render::SkillSuiteCarrier::kCaster);
  CHECK(suite[1].kind == ro::render::SkillSuiteKind::kEffect);
  CHECK(suite[1].effect_id == kSightTrasher);
  // L'INSTANCE NE REPREND JAMAIS LA PLACE DE SON LANCEUR : la rosace reste où
  // elle est née.
  CHECK(ro::render::EffectBearerFollowFor(kSightTrasher) == nullptr);
}
