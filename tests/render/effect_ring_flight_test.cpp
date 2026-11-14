// LES ANNEAUX, CARRÉS ET PLANS CROISÉS de Blitz Beat (115), de l'anneau jaune
// (81), de la boule qui vole (93) et des traînées ac_center2 (813 à 818, 847,
// 848, 864) : ce que la table en dit, et ce que le mouvement en fait.

#include "ro/render/effect_ring_flight.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_launch.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_sprite_seed.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/effect_seeded_ring.h"
#include "ro/render/effect_track_motion.h"
#include "ro/render/skill_suite_table.h"

using Catch::Approx;
using ro::render::AdvanceEffectNode;
using ro::render::EffectNode;
using ro::render::EffectNodeFromPattern;
using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;
using ro::render::EffectNodePatternTextures;
using ro::render::EffectNodeShape;

namespace {

constexpr std::uint32_t kBlitzBeat = 115;
constexpr std::uint32_t kYellowRing = 81;
constexpr std::uint32_t kFlyingBall = 93;
constexpr std::uint32_t kTraineesPastel = 813;
constexpr std::uint32_t kTraineesRouges = 847;
constexpr std::uint32_t kTraineesBleutees = 848;
constexpr std::uint32_t kBothTrails = 864;
constexpr std::uint32_t kBlueFan = 488;
constexpr std::uint8_t kBlueVariant = 4;
constexpr std::int16_t kArcOuvert = 315;
constexpr float kSmallestKey = 27.0F;
constexpr std::size_t kRed = 0;
constexpr std::size_t kVert = 1;
constexpr std::size_t kBlue = 2;
constexpr float kFull = 255.0F;
constexpr float kPastelLow = 140.0F;
// Assez de tirages pour que chaque valeur d'une teinte à deux valeurs sorte.
constexpr std::uint32_t kTirages = 300;

const EffectNodePattern& FirstOfShape(std::uint32_t effect_id, EffectNodeShape forme,
                                      std::size_t rang = 0) {
  std::size_t seen_count = 0;
  const EffectNodePattern* found = nullptr;
  for (const EffectNodePattern& pattern : EffectNodePatternsFor(effect_id)) {
    if (pattern.shape == forme && seen_count++ == rang) {
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

TEST_CASE("blitz beat pose dix plans devant le porteur, qui reculent", "[render][table]") {
  const EffectNodePattern& plans = FirstOfShape(kBlitzBeat, EffectNodeShape::kCrossedPlanes);
  CHECK(plans.ring.instance_count == 10);
  CHECK(plans.ring.advances_along_depth_axis);
  CHECK(plans.ring.yaw_is_owner_body_angle);
  CHECK(plans.launch_distance_low == Approx(3.5F));
  CHECK(plans.launch_distance_high == Approx(3.5F));

  EffectNode node = EffectNodeFromPattern(plans);
  CHECK(node.translates_along_axis);
  CHECK(node.axial_translation == Approx(15.0F));
  // Dix tics : le pas va de −1,3 à −2,2, soit −17,5 en tout.
  Advance(node, 10);
  CHECK(node.axial_translation == Approx(-2.5F));
  // Au dixième tic, la seconde image-clé : −0,3 plus un millième.
  Advance(node, 1);
  CHECK(node.axial_translation == Approx(-2.5F - 0.299F));
}

TEST_CASE("blitz beat seme un anneau vert par coup, des le trente-sixieme tic", "[render][table]") {
  const EffectNodePattern& ring = FirstOfShape(kBlitzBeat, EffectNodeShape::kRingBand);
  CHECK(ring.first_tick == 36);
  CHECK(ring.period_ticks == 3);
  CHECK(ring.sowings_follow_param);
  CHECK(ring.fade_out_tick == 6);
  CHECK(ring.ring.yaw_is_owner_body_angle);
  CHECK(ring.ring.axial_translation_start == Approx(2.0F));
  // L'ENFOUISSEMENT ET L'ABSCISSE SONT DES TIRAGES ENTIERS : une unité de base
  // sous `(rand() % 20 + 1) / 2 − 12`, et `(rand() % 10 + 1) / 5`.
  CHECK(ring.ring.burial_depth == Approx(1.0F));
  CHECK(ring.ring.burial_depth_draw.modulus == 20);
  CHECK(ring.ring.burial_depth_draw.divisor == 2);
  CHECK(ring.ring.burial_depth_draw.bias == -12);
  CHECK(ring.ring.offset_x_draw.modulus == 10);
  CHECK(ring.ring.offset_x_draw.divisor == 5);
  CHECK(ring.ring.draws_discarded_after_burial == 1);
  // Deux coups, deux anneaux : aux tics 36 et 39, et pas au 42.
  CHECK(ro::render::EffectNodePatternSowsAtParam(ring, 36, 2));
  CHECK(ro::render::EffectNodePatternSowsAtParam(ring, 39, 2));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAtParam(ring, 42, 2));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAtParam(ring, 35, 2));
}

TEST_CASE("l'anneau jaune nait une fois, aux rayons six et onze", "[render][table]") {
  const EffectNodePattern& ring = FirstOfShape(kYellowRing, EffectNodeShape::kRingBand);
  CHECK(ring.life_ticks == 15);
  CHECK(ring.fade_out_tick == 8);
  CHECK(ring.first_tick == 0);
  CHECK(ring.period_ticks == 0);
  CHECK(ring.ring.radius_at_anchor == Approx(6.0F));
  CHECK(ring.ring.radius_at_height == Approx(11.0F));
  CHECK(ring.ring.burial_depth == Approx(-10.0F));
  CHECK(ring.node_yaw_follows_owner);
}

TEST_CASE("la boule vole vers la cible, sous ses propres images", "[render][table]") {
  const EffectNodePattern& ball = FirstOfShape(kFlyingBall, EffectNodeShape::kOrientedQuad);
  const EffectNodePattern& halo = FirstOfShape(kFlyingBall, EffectNodeShape::kOrientedQuad, 1);
  CHECK(ball.ring.flies_to_target);
  CHECK(halo.ring.flies_to_target);
  CHECK((ball.ring.flags & ro::render::kEffectNodeDiesAtTarget) != 0U);
  const std::span<const std::string_view> ball_images = EffectNodePatternTextures(ball);
  REQUIRE(ball_images.size() == 1);
  CHECK(ball_images.front().ends_with("thunder_center.bmp"));
  const std::span<const std::string_view> images_du_halo = EffectNodePatternTextures(halo);
  REQUIRE(images_du_halo.size() == 6);
  CHECK(images_du_halo.front().ends_with("thunder_ball_a.bmp"));
  CHECK(images_du_halo.back().ends_with("thunder_ball_f.bmp"));
}

TEST_CASE("un noeud porteur du bit d'arrivee meurt a trois unites de la cible", "[render]") {
  ro::render::PooledEffectNode node;
  node.shape = ro::render::PooledNodeShape::kOrientedQuad;
  node.node.flags = ro::render::kEffectNodeDiesAtTarget;
  node.ground_position = glm::vec3{8.0F, 5.0F, 0.0F};
  CHECK_FALSE(ro::render::EffectNodeArrivedAtTarget(node));
  node.target_position = glm::vec3{10.0F, 0.0F, 0.0F};
  // L'ordonnée ne compte pas : deux unités au sol.
  CHECK(ro::render::EffectNodeArrivedAtTarget(node));
  node.ground_position = glm::vec3{6.0F, 0.0F, 0.0F};
  CHECK_FALSE(ro::render::EffectNodeArrivedAtTarget(node));
  // Une autre figure ne lit pas ce bit.
  node.ground_position = glm::vec3{9.0F, 0.0F, 0.0F};
  node.shape = ro::render::PooledNodeShape::kRingBand;
  CHECK_FALSE(ro::render::EffectNodeArrivedAtTarget(node));
  // UN SPRITE VISÉ porte le sien, sans le bit des sous-types quinze et seize.
  node.node.flags = 0;
  node.dies_near_target = true;
  CHECK(ro::render::EffectNodeArrivedAtTarget(node));
}

TEST_CASE("l'axe du pas d'un plan dresse descend, celui d'un plan couche recule", "[render]") {
  const glm::vec3 dresse = ro::render::EffectNodeDepthAxis(0.0F, -90.0F, 0.0F);
  CHECK(dresse.x == Approx(0.0F).margin(1e-6));
  CHECK(dresse.y == Approx(-1.0F));
  CHECK(dresse.z == Approx(0.0F).margin(1e-6));
  const glm::vec3 layer = ro::render::EffectNodeDepthAxis(0.0F, 0.0F, 0.0F);
  CHECK(layer.z == Approx(-1.0F));

  ro::render::PooledEffectNode node;
  node.node_pitch_degrees = -90.0F;
  node.node.axial_translation = 10.0F;
  CHECK(glm::length(ro::render::EffectNodeDepthAdvance(node)) == Approx(0.0F));
  node.node.translates_along_axis = true;
  // ⚠ L'INCLINAISON NÉGATIVE SE LIT UN DEGRÉ À CÔTÉ dans les tables du client :
  // −90° y vaut −89°, et le pas descend de dix fois le sinus de 89°.
  constexpr float kUnDegre = 3.141592F / 180.0F;
  CHECK(ro::render::EffectNodeDepthAdvance(node).y == Approx(-10.0F * std::cos(kUnDegre)));
}

TEST_CASE("les trainees pastel tombent, dressees, sous une teinte a deux valeurs",
          "[render][table]") {
  const EffectNodePattern& trail = FirstOfShape(kTraineesPastel, EffectNodeShape::kCrossedPlanes);
  CHECK(trail.period_ticks == 2);
  CHECK(trail.last_tick == 99);
  CHECK(trail.launch_pitch_low == Approx(-90.0F));
  CHECK(trail.launch_distance_low == Approx(3.0F));
  CHECK(trail.launch_distance_high == Approx(9.0F));
  CHECK(trail.ring.burial_depth == Approx(-20.0F));
  CHECK(trail.ring.advances_along_depth_axis);

  std::array<std::uint32_t, 3> pleins{};
  std::uint32_t generator = 1;
  for (std::uint32_t draw = 0; draw < kTirages; ++draw) {
    const EffectNode node = EffectNodeFromPattern(trail, generator);
    for (std::size_t canal = 0; canal < node.color.size(); ++canal) {
      const float value = node.color[canal];
      REQUIRE((value == kPastelLow || value == kFull));
      pleins[canal] += value == kFull ? 1U : 0U;
    }
  }
  // Une fois sur trois, à la louche : ni jamais, ni toujours.
  for (const std::uint32_t count : pleins) {
    CHECK(count > kTirages / 6);
    CHECK(count < kTirages / 2);
  }
  // Et le pas : il part de zéro et gagne 0,015 par tic — il TOMBE.
  EffectNode node = EffectNodeFromPattern(trail);
  Advance(node, 2);
  CHECK(node.axial_translation == Approx(0.015F + 0.03F));
}

TEST_CASE("les trainees teintees tirent deux canaux ensemble", "[render][table]") {
  std::uint32_t generator = 7;
  const EffectNodePattern& bleutee =
      FirstOfShape(kTraineesBleutees, EffectNodeShape::kCrossedPlanes);
  const EffectNodePattern& red = FirstOfShape(kTraineesRouges, EffectNodeShape::kCrossedPlanes);
  bool shade_seen = false;
  for (std::uint32_t draw = 0; draw < kTirages; ++draw) {
    const EffectNode b = EffectNodeFromPattern(bleutee, generator);
    CHECK(b.color[kRed] == b.color[kVert]);
    CHECK(b.color[kBlue] == kFull);
    const EffectNode r = EffectNodeFromPattern(red, generator);
    CHECK(r.color[kVert] == r.color[kBlue]);
    CHECK(r.color[kRed] == kFull);
    shade_seen = shade_seen || (b.color[kRed] > 0.0F && b.color[kRed] < kFull);
  }
  CHECK(shade_seen);
  // Et elles MONTENT : un pas qui perd 0,015 par tic.
  EffectNode node = EffectNodeFromPattern(bleutee);
  Advance(node, 1);
  CHECK(node.axial_translation == Approx(-0.015F));
}

TEST_CASE("l'eventail bleu de l'effet 488 seme deux noeuds, cles recopiees en queue",
          "[render][table]") {
  const std::span<const ro::render::EffectSeededRingPattern> nodes =
      ro::render::EffectSeededRingPatternsFor(kBlueFan);
  REQUIRE(nodes.size() == 2);
  for (const ro::render::EffectSeededRingPattern& node : nodes) {
    CHECK(node.shape == ro::render::PooledNodeShape::kRadialSpreadParticles);
    CHECK(node.trail_variant == kBlueVariant);
    std::uint32_t generator = 1;
    const auto tracks = ro::render::SeedEffectSeededRingTracks(node, generator);
    for (const ro::render::EffectTrack& track : tracks) {
      CHECK(track.active);
      CHECK(track.degrees == kArcOuvert);
      CHECK(track.alpha == 0);
      CHECK(track.lift_key >= kSmallestKey);
      CHECK(track.tail[ro::render::kRadialSpreadKeyBaseTailSlot] == track.lift_key);
    }
  }
}

TEST_CASE("l'effet 864 seme ses deux trainees, la bleutee d'abord", "[render][table]") {
  const EffectNodePattern& first = FirstOfShape(kBothTrails, EffectNodeShape::kCrossedPlanes);
  const EffectNodePattern& second = FirstOfShape(kBothTrails, EffectNodeShape::kCrossedPlanes, 1);
  CHECK(first.client_function == "Effect_InitAcCenter3DColored");
  CHECK(second.client_function == "Effect_InitAcCenter3D");
  CHECK(first.ring.burial_depth == Approx(-5.0F));
  CHECK(second.ring.burial_depth == Approx(-20.0F));
}

TEST_CASE("un tirage entier du client ne rend que ses valeurs, avec leurs fréquences",
          "[render][ring_flight]") {
  // `(rand() % 10 + 1) / 5` : zéro quatre fois sur dix, un cinq fois, deux une.
  const ro::render::EffectClientIntegerDraw abscisse{10, 1, 5, 0};
  std::array<int, 3> count{};
  std::uint32_t generator = 17;
  constexpr int kTiragesEntiers = 10000;
  for (int attempt = 0; attempt < kTiragesEntiers; ++attempt) {
    const std::int32_t value = ro::render::DrawEffectClientInteger(abscisse, generator);
    REQUIRE(value >= 0);
    REQUIRE(value <= 2);
    ++count[static_cast<std::size_t>(value)];
  }
  CHECK(count[0] == Approx(4000).epsilon(0.1));
  CHECK(count[1] == Approx(5000).epsilon(0.1));
  CHECK(count[2] == Approx(1000).epsilon(0.2));
  // Un modulo nul ne tire rien et rend le biais.
  std::uint32_t intact = 5;
  CHECK(ro::render::DrawEffectClientInteger({0, 0, 1, -3}, intact) == -3);
  CHECK(intact == 5U);
}

TEST_CASE("les traînées et les rayons d'agilité naissent sur un cercle ENTIER",
          "[render][ring_flight]") {
  for (const std::uint32_t effect_id : {kTraineesPastel, kTraineesRouges, kTraineesBleutees}) {
    const EffectNodePattern& trail = FirstOfShape(effect_id, EffectNodeShape::kCrossedPlanes);
    CAPTURE(effect_id);
    CHECK(trail.ring.birth_circle_is_integer);
    CHECK(trail.ring.birth_radius_draw.modulus == 7);
    CHECK(trail.ring.birth_radius_draw.addend == 3);
  }
  const EffectNodePattern& plan = FirstOfShape(kBlitzBeat, EffectNodeShape::kCrossedPlanes);
  CHECK(plan.ring.birth_circle_is_integer);
  CHECK(plan.ring.birth_radius_draw.modulus == 0);
  CHECK(plan.launch_distance_low == Approx(3.5F));
}

TEST_CASE("Blitz Beat compte ses anneaux sur un compteur partagé", "[render][ring_flight]") {
  CHECK(ro::render::EffectParamCounterIsShared(kBlitzBeat));
  CHECK_FALSE(ro::render::EffectParamCounterIsShared(kYellowRing));
  const EffectNodePattern& ring = FirstOfShape(kBlitzBeat, EffectNodeShape::kRingBand);
  // Jusqu'à la fin de la vie de l'instance, et non trente-six anneaux au plus.
  CHECK(ring.last_tick == 249);
}

TEST_CASE("le cap sur la grille 7×7 rend quarante-neuf directions à plat",
          "[render][ring_flight]") {
  std::uint32_t generator = 3;
  std::array<glm::vec3, 64> seen{};
  std::size_t distinctes = 0;
  for (int attempt = 0; attempt < 4000; ++attempt) {
    const glm::vec3 direction = ro::render::DrawEffectClientGridHeading(generator);
    REQUIRE(direction.y == Approx(0.0F));
    REQUIRE(glm::length(direction) == Approx(1.0F));
    bool already = false;
    for (std::size_t rang = 0; rang < distinctes; ++rang) {
      already = already || glm::length(seen[rang] - direction) < 1e-5F;
    }
    if (!already) {
      REQUIRE(distinctes < seen.size());
      seen[distinctes++] = direction;
    }
  }
  // Quarante-huit cases et le centre ; plusieurs cases partagent une direction
  // (la même diagonale à un et deux pas), si bien qu'on en voit moins.
  CHECK(distinctes <= 49);
  // LE CENTRE, SANS DIRECTION, garde la face nulle : cap zéro, la troisième
  // ligne `(0, 0, 1)` du client — la profondeur négative chez nous.
  bool centre = false;
  for (std::size_t rang = 0; rang < distinctes; ++rang) {
    centre = centre || glm::length(seen[rang] - glm::vec3{0.0F, 0.0F, -1.0F}) < 1e-5F;
  }
  CHECK(centre);
}

TEST_CASE("un vol visé par la table du client va vers le point, au degré près",
          "[render][ring_flight]") {
  constexpr float kUnDegre = 3.141592F / 180.0F;
  // Droit devant chez nous (la profondeur négative) : la face vaut cent
  // quatre-vingts, le cap −180 se lit 181 dans la table — un degré de travers.
  const glm::vec3 devant = ro::render::EffectClientAimedHeading({0.0F, 0.0F, -10.0F}, 0.0F);
  CHECK(devant.z == Approx(-std::cos(kUnDegre)));
  CHECK(devant.x == Approx(std::sin(kUnDegre)).margin(1e-5));
  CHECK(devant.y == Approx(0.0F).margin(1e-6));
  // À droite : face quatre-vingt-dix, cap −90 lu 271.
  const glm::vec3 right = ro::render::EffectClientAimedHeading({10.0F, 0.0F, 0.0F}, 0.0F);
  CHECK(right.x == Approx(std::cos(kUnDegre)));
  CHECK(right.z == Approx(std::sin(kUnDegre)).margin(1e-5));
  // L'ÉCART DE LA FACE tourne le vol : moins cinq degrés de face, cap −85 lu
  // 276, soit six degrés vers notre profondeur positive.
  const glm::vec3 ecartee = ro::render::EffectClientAimedHeading({10.0F, 0.0F, 0.0F}, -5.0F);
  CHECK(ecartee.x == Approx(std::cos(6.0F * kUnDegre)));
  CHECK(ecartee.z == Approx(std::sin(6.0F * kUnDegre)).margin(1e-5));
}

TEST_CASE("la flèche verte montre par son action la direction du point visé",
          "[render][ring_flight]") {
  // Le point visé droit devant nous (notre profondeur négative) : l'instance
  // est, vue de lui, à 270 degrés dans le repère du client. Caméra nulle :
  // quart 270..359, action deux.
  const glm::vec3 instance{0.0F, 0.0F, 0.0F};
  const glm::vec3 aimed{0.0F, 0.0F, -10.0F};
  CHECK(ro::render::EffectAimQuadrantAction(instance, aimed, 0) == 2);
  // La caméra tournée : l'angle de caméra est le complément du tour de
  // billboard. Quatre-vingt-dix de tour → 270 de caméra → 180 : action quatre.
  CHECK(ro::render::EffectAimQuadrantAction(instance, aimed, 90) == 4);
  CHECK(ro::render::EffectAimQuadrantAction(instance, aimed, 180) == 6);
  CHECK(ro::render::EffectAimQuadrantAction(instance, aimed, 270) == 0);

  const auto fleches = EffectNodePatternsFor(935);
  REQUIRE(fleches.size() == 1);
  CHECK(fleches.front().aims_at_target);
  CHECK(fleches.front().action_from_aim_quadrant);
  CHECK(fleches.front().dies_near_target);
  CHECK_FALSE(fleches.front().turns_with_flight);

  // SA TÊTE SEULE GARDE LES COULEURS DE SES CALQUES ; les trois suivantes
  // prennent le vert du patron et s'effacent.
  const EffectNodePattern& arrow = fleches.front();
  CHECK((arrow.sprite_flags & ro::render::kEffectNodeKeepsLayerColor) == 0U);
  const ro::render::EffectNodeSeed head =
      ro::render::EffectNodeSeedFromPattern(arrow, arrow.life_ticks, arrow.first_tick, 0);
  CHECK((head.flags & ro::render::kEffectNodeKeepsLayerColor) != 0U);
  const std::int32_t second_tick = arrow.first_tick + arrow.period_ticks;
  const ro::render::EffectNodeSeed suivante =
      ro::render::EffectNodeSeedFromPattern(arrow, arrow.life_ticks, second_tick, 0);
  CHECK((suivante.flags & ro::render::kEffectNodeKeepsLayerColor) == 0U);
  CHECK(suivante.color[0] == Approx(105.0F));
  CHECK(suivante.color[1] == Approx(255.0F));
  CHECK(suivante.color[2] == Approx(105.0F));
  CHECK(suivante.alpha_max == Approx(120.0F));
}

TEST_CASE("la fabrique montre la direction visée par deux tables d'actions",
          "[render][ring_flight]") {
  // Le type 8 lit 0/3/2/1 par quart, le type 28 comme la flèche verte.
  const glm::vec3 instance{0.0F, 0.0F, 0.0F};
  const glm::vec3 aimed{0.0F, 0.0F, -10.0F};
  constexpr ro::render::EffectActionsByQuarter kTypeHuit = {0, 3, 2, 1};
  CHECK(ro::render::EffectAimQuadrantAction(instance, aimed, 0, kTypeHuit) == 1);
  CHECK(ro::render::EffectAimQuadrantAction(instance, aimed, 90, kTypeHuit) == 2);
  CHECK(ro::render::EffectAimQuadrantAction(instance, aimed, 180, kTypeHuit) == 3);
  CHECK(ro::render::EffectAimQuadrantAction(instance, aimed, 270, kTypeHuit) == 0);

  const auto huit = EffectNodePatternsFor(584);
  REQUIRE(huit.size() == 1);
  CHECK(huit.front().action_from_aim_quadrant);
  CHECK(huit.front().aim_quadrant_actions == kTypeHuit);
  CHECK_FALSE(huit.front().aims_at_target);
  const auto vingt_huit = EffectNodePatternsFor(645);
  REQUIRE(vingt_huit.size() == 1);
  CHECK(vingt_huit.front().action_from_aim_quadrant);
  CHECK(vingt_huit.front().aim_quadrant_actions == ro::render::kEffectArrowActionsByQuarter);

  // ET LEUR POINT VISÉ EST LA CIBLE : nés sur le lanceur, ils reçoivent le
  // message 14 avec la place de la cible, comme la boule de 80.
  struct SkillAndEffect {
    std::uint16_t skill_id;
    std::uint32_t effect_id;
  };
  constexpr std::array<SkillAndEffect, 2> kCompetences = {{{519, 584}, {520, 645}}};
  for (const auto& [skill_id, effect_id] : kCompetences) {
    CAPTURE(skill_id);
    bool found = false;
    for (const ro::render::SkillSuiteStep& etape :
         ro::render::SkillSuiteOf(skill_id, ro::render::SkillSuiteMoment::kImpact)) {
      if (etape.effect_id != effect_id) {
        continue;
      }
      found = true;
      CHECK(etape.carrier == ro::render::SkillSuiteCarrier::kCaster);
      CHECK(etape.aim == ro::render::SkillSuiteAim::kTarget);
      std::uint32_t generator = 1;
      const auto point =
          ro::render::SkillSuiteAimPoint(etape, std::nullopt, aimed, instance, generator);
      REQUIRE(point.has_value());
      CHECK(point->z == Approx(aimed.z));
    }
    CHECK(found);
  }

  // LES TYPES 15, 17 ET 60 TIRENT À PILE OU FACE, sans angle.
  constexpr std::array<std::uint32_t, 3> kPileOuFace = {630, 632, 797};
  constexpr std::array<std::int32_t, 3> kAutres = {4, 2, 7};
  for (std::size_t rang = 0; rang < kPileOuFace.size(); ++rang) {
    CAPTURE(kPileOuFace[rang]);
    const auto patterns = EffectNodePatternsFor(kPileOuFace[rang]);
    REQUIRE(patterns.size() == 1);
    CHECK_FALSE(patterns.front().action_from_aim_quadrant);
    CHECK(patterns.front().action == 0);
    CHECK(patterns.front().action_alternate == kAutres[rang]);
  }
}

TEST_CASE("les trois salves de l'effet 891 visent, écartées de cinq degrés",
          "[render][ring_flight]") {
  const auto patterns = EffectNodePatternsFor(891);
  REQUIRE(patterns.size() == 3);
  constexpr std::array<std::int32_t, 3> kPremiers = {0, 6, 12};
  constexpr std::array<float, 3> kEcarts = {0.0F, -5.0F, 5.0F};
  for (std::size_t rang = 0; rang < patterns.size(); ++rang) {
    CAPTURE(rang);
    const EffectNodePattern& salve = patterns[rang];
    CHECK(salve.first_tick == kPremiers[rang]);
    CHECK(salve.last_tick == kPremiers[rang] + 3);
    CHECK(salve.period_ticks == 1);
    CHECK(salve.aims_at_target);
    CHECK(salve.aim_heading_on_client_table);
    CHECK(salve.aim_face_offset_degrees == Approx(kEcarts[rang]));
    CHECK(salve.launch_drop == Approx(-15.0F));
    CHECK(salve.life_ticks == 20);
    CHECK(salve.dies_near_target);
  }
  // L'ALPHA DÉCROÎT PAR RANG DANS LA SALVE, la teinte reste la sienne.
  const ro::render::EffectTintByRank* quatrieme = ro::render::EffectTintByRankFor(891, 4);
  REQUIRE(quatrieme != nullptr);
  CHECK(quatrieme->alpha == Approx(80.0F));
  CHECK(quatrieme->keeps_color);
}

TEST_CASE("le poing de l'effet 806 vole vers le point vise et montre sa direction",
          "[render][ring_flight]") {
  const auto poings = EffectNodePatternsFor(806);
  REQUIRE(poings.size() == 1);
  const EffectNodePattern& poing = poings.front();
  // QUATRE SEMIS, un tous les deux tics de zéro à six.
  CHECK(poing.first_tick == 0);
  CHECK(poing.last_tick == 6);
  CHECK(poing.period_ticks == 2);
  CHECK(poing.aims_at_target);
  CHECK(poing.aim_heading_on_client_table);
  CHECK(poing.dies_near_target);
  CHECK(poing.launch_drop == Approx(-15.0F));
  CHECK(poing.action_from_aim_quadrant);
  CHECK(poing.aim_quadrant_actions == ro::render::kEffectArrowActionsByQuarter);
  // ⚠ SON QUAD NE SE COUCHE PAS SUR SON VOL : le bâtisseur écrit sa face sans
  // le bit qui la ferait lire.
  CHECK_FALSE(poing.turns_with_flight);
  // LA TÊTE SEULE GARDE LES COULEURS DE SES CALQUES ; les suivants restent
  // blancs et s'effacent de soixante à vingt.
  CHECK((poing.sprite_flags & ro::render::kEffectNodeKeepsLayerColor) == 0U);
  const ro::render::EffectTintByRank* head = ro::render::EffectTintByRankFor(806, 1);
  REQUIRE(head != nullptr);
  CHECK(head->keeps_layer_color);
  constexpr std::array<float, 3> kAlphas = {60.0F, 40.0F, 20.0F};
  for (std::size_t rang = 0; rang < kAlphas.size(); ++rang) {
    CAPTURE(rang);
    const ro::render::EffectTintByRank* next =
        ro::render::EffectTintByRankFor(806, static_cast<std::int32_t>(rang) + 2);
    REQUIRE(next != nullptr);
    CHECK(next->alpha == Approx(kAlphas[rang]));
    CHECK(next->keeps_color);
    CHECK_FALSE(next->keeps_layer_color);
  }
}

TEST_CASE("une matrice de nœud lit ses angles comme les tables du client",
          "[render][ring_flight]") {
  CHECK(ro::render::EffectClientMatrixDegrees(-90.0F) == Approx(271.0F).margin(1e-3));
  CHECK(ro::render::EffectClientMatrixDegrees(45.7F) == Approx(45.0F).margin(1e-3));
  CHECK(ro::render::EffectClientMatrixDegrees(-0.5F) == Approx(0.0F).margin(1e-3));
}

TEST_CASE("la boule qui vole tourne son corps vers la cible, en symétrique de sa face",
          "[render][ring_flight]") {
  // Face vers notre +z, c'est-à-dire vers le −z du client : zéro, et le corps
  // vaut le demi-tour.
  CHECK(ro::render::EffectNodeBodyTowardTarget(glm::vec3{0.0F, 0.0F, 10.0F}) ==
        Approx(180.0F).margin(1e-3));
  CHECK(ro::render::EffectNodeBodyTowardTarget(glm::vec3{10.0F, 0.0F, 0.0F}) ==
        Approx(90.0F).margin(1e-3));
  // Une face négative revient dans le tour avant le symétrique, et le corps
  // négatif y revient après.
  CHECK(ro::render::EffectNodeBodyTowardTarget(glm::vec3{-10.0F, 0.0F, 0.0F}) ==
        Approx(270.0F).margin(1e-3));
  // Le demi-tour exact, divisé par le π court du client, dépasse cent quatre-vingts
  // d'un rien : le corps passe sous zéro et revient juste sous le tour.
  CHECK(ro::render::EffectNodeBodyTowardTarget(glm::vec3{0.0F, 0.0F, -10.0F}) ==
        Approx(360.0F).margin(1e-3));
  // Un écart nul n'écrit pas la face : le corps vaut le demi-tour.
  CHECK(ro::render::EffectNodeBodyTowardTarget(glm::vec3{0.0F, 5.0F, 0.0F}) == 180.0F);
}
