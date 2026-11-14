// LE CŒUR QUI ERRE ET SES ÉCLATS DE GEL : le sous-type 145, l'effet 833 —
// son semis, son mouvement tourbillonnant et sa figure d'anneau bleu projeté.
#include "ro/render/effect_thunder_freeze.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_figures.h"
#include "ro/render/effect_thunder_freeze_seed.h"

#include "support/effect_bearers.h"

using Catch::Approx;
using ro::render::AdvanceThunderFreezeTracks;
using ro::render::ComposeThunderFreezeQuads;
using ro::render::EffectNodeLifeFor;
using ro::render::EffectNodePool;
using ro::render::EffectThunderFreezePattern;
using ro::render::EffectThunderFreezePatternsFor;
using ro::render::EffectThunderFreezePatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTickMs;
using ro::render::kEffectTracksPerNode;
using ro::render::KnowsProceduralEffect;
using ro::render::NextEffectNodeDraw;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SeedThunderFreezeTracks;
using ro::render::SowThunderFreezeNodes;
using ro::render::ThunderFreezeNodeCount;

namespace {

using Tracks = std::array<EffectTrack, kEffectTracksPerNode>;

constexpr std::uint32_t kThunderFreezeEffectId = 833;

// Les cases et les marques, aux rangs du client.
constexpr std::size_t kOrbite = ro::render::kThunderFreezeSlotOfOrbitDegrees;
constexpr std::size_t kHeight = ro::render::kThunderFreezeSlotOfHeight;
constexpr std::size_t kRespiration = ro::render::kThunderFreezeSlotOfPulseDegrees;
constexpr std::size_t kSize = ro::render::kThunderFreezeSlotOfShardSize;
constexpr std::size_t kHalfWander = ro::render::kThunderFreezeMarkOfHalvedWander;

// Ce que le client écrit en dur, relu ici pour que l'essai ne se recopie pas.
constexpr float kCircleRadius = 7.0F;
constexpr float kQuinzeAuDessus = 15.0F;
constexpr std::int16_t kCoreCeiling = 200;
constexpr std::int16_t kShardAlpha = 200;
constexpr float kBirthSize = 3.0F;
constexpr float kCroissance = 1.02F;
constexpr std::int32_t kDelayPerRank = 20;
constexpr float kTolerance = 0.001F;
// Une destination de mélange UN : l'addition de Direct3D.
constexpr std::int32_t kBlendOne = 2;

[[nodiscard]] float Radians(float degrees) { return degrees * std::numbers::pi_v<float> / 180.0F; }

// UN NŒUD AU REPOS : la première piste allumée, le tremblement figé à zéro
// (angle et cible nuls — la cible se retire à 359 sans tirage, l'angle reste à
// zéro, son sinus aussi), la respiration à zéro, aucun éclat en vie.
[[nodiscard]] Tracks RestingNode(float orbite) {
  Tracks tracks;
  tracks[0].active = true;
  tracks[0].value[kOrbite] = orbite;
  tracks[0].value[kHeight] = -kQuinzeAuDessus;
  tracks[0].radius_at_anchor = 1.0F;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    // Loin dans le retard : aucun éclat ne compte dans ces essais-là.
    tracks[rang].ticks = rang == 0 ? 0 : -1000;
  }
  return tracks;
}

[[nodiscard]] glm::vec3 CentreDe(const EffectWorldQuad& quad) {
  return (quad.corners[0] + quad.corners[1] + quad.corners[2] + quad.corners[3]) * 0.25F;
}

}  // namespace

TEST_CASE("Cœur de gel : un site, un bâtisseur, et la famille est connue", "[render][effectnode]") {
  const std::span<const EffectThunderFreezePattern> table = EffectThunderFreezePatternTable();
  REQUIRE(table.size() == 1);
  const std::span<const EffectThunderFreezePattern> patterns =
      EffectThunderFreezePatternsFor(kThunderFreezeEffectId);
  REQUIRE(patterns.size() == 1);
  CHECK(patterns[0].client_function == "Effect_InitThunderFreeze");
  CHECK(KnowsProceduralEffect(kThunderFreezeEffectId));
  CHECK(EffectThunderFreezePatternsFor(kThunderFreezeEffectId + 1).empty());
}

TEST_CASE("Cœur de gel : le nombre de nœuds est l'abscisse demandée, tronquée et plafonnée",
          "[render][effectnode]") {
  CHECK(ThunderFreezeNodeCount(ro::render::kThunderFreezeLaunchedCount) == 2);
  CHECK(ThunderFreezeNodeCount(2.9F) == 2);
  CHECK(ThunderFreezeNodeCount(7.0F) == 5);
  CHECK(ThunderFreezeNodeCount(0.5F) == 0);
  CHECK(ThunderFreezeNodeCount(0.0F) == 0);
  CHECK(ThunderFreezeNodeCount(-3.0F) == 0);

  std::uint32_t seed = 5U;
  CHECK(SowThunderFreezeNodes({}, 0.0F, 100, seed).empty());
  // ⚠ SEPT DEMANDÉS, CINQ NŒUDS, espacés d'un SEPTIÈME de tour : la division se
  // fait par le flottant demandé, pas par le nombre plafonné.
  const std::vector<PooledEffectNode> sept = SowThunderFreezeNodes({}, 7.0F, 100, seed);
  REQUIRE(sept.size() == 5);
  for (std::size_t rang = 0; rang < sept.size(); ++rang) {
    CHECK(sept[rang].tracks[0].value[kOrbite] == Approx(static_cast<float>(rang) * 360.0F / 7.0F));
  }
}

TEST_CASE("Cœur de gel : deux nœuds opposés, nés sur le porteur, à deux images",
          "[render][effectnode]") {
  const glm::vec3 bearer{30.0F, 2.0F, -40.0F};
  std::uint32_t seed = 99U;
  const std::vector<PooledEffectNode> nodes =
      SowThunderFreezeNodes(bearer, ro::render::kThunderFreezeLaunchedCount,
                            EffectNodeLifeFor(kThunderFreezeEffectId), seed);
  REQUIRE(nodes.size() == 2);
  CHECK(nodes[0].tracks[0].value[kOrbite] == Approx(0.0F));
  CHECK(nodes[1].tracks[0].value[kOrbite] == Approx(180.0F));
  for (const PooledEffectNode& node : nodes) {
    CHECK(node.shape == PooledNodeShape::kThunderFreeze);
    CHECK(node.ground_position == bearer);
    CHECK(node.owner_position == bearer);
    CHECK(node.node.render_word == ro::render::kThunderFreezeRenderWord);
    CHECK(node.node.life_ticks == EffectNodeLifeFor(kThunderFreezeEffectId));
    REQUIRE(node.resource_paths.size() == 2);
    CHECK(node.resource_paths[0] == ro::render::kThunderFreezeCoreImage);
    CHECK(node.resource_paths[1] == ro::render::kThunderFreezeShardImage);
  }
}

TEST_CASE("Cœur de gel : les pistes du semis, dans l'ordre des tirages du client",
          "[render][effectnode]") {
  std::uint32_t seed = 1234U;
  std::uint32_t witness = seed;
  const Tracks tracks = SeedThunderFreezeTracks(1, 2.0F, seed);
  const EffectTrack& leader = tracks[0];
  CHECK(leader.active);
  for (std::size_t rang = 0; rang < 6; ++rang) {
    CHECK(leader.value[rang] == static_cast<float>(NextEffectNodeDraw(witness) % 360U));
  }
  CHECK(leader.value[kRespiration] == static_cast<float>(NextEffectNodeDraw(witness) % 360U));
  CHECK(leader.value[kOrbite] == Approx(180.0F));
  CHECK(leader.value[kHeight] == Approx(-kQuinzeAuDessus));
  CHECK(leader.radius_at_anchor == Approx(1.0F));
  CHECK(leader.mark[kHalfWander] == 1);
  CHECK(leader.alpha == 0);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    const EffectTrack& shard = tracks[rang];
    CHECK(shard.active == (rang == 0));
    CHECK(shard.ticks == -kDelayPerRank * static_cast<std::int32_t>(rang));
    CHECK(shard.degrees == static_cast<std::int16_t>(NextEffectNodeDraw(witness) % 360U));
    CHECK(shard.lift_key == Approx(kBirthSize));
    CHECK(shard.azimuth_degrees == static_cast<std::int16_t>(NextEffectNodeDraw(witness) % 360U));
    CHECK(shard.tilt_degrees == kShardAlpha);
  }
  CHECK(seed == witness);
}

TEST_CASE("Cœur de gel : l'alpha monte d'un par tic jusqu'à deux cents, et les angles se rangent",
          "[render][effectnode]") {
  Tracks tracks = RestingNode(0.0F);
  std::uint32_t seed = 3U;
  constexpr std::int16_t kTour = 123;
  constexpr std::int16_t kTilt = 77;
  AdvanceThunderFreezeTracks(tracks, kTour, kTilt, seed);
  CHECK(tracks[0].alpha == 1);
  // ⚠ TELS QUELS : ce sont déjà les compléments du client.
  CHECK(tracks[0].style == kTour);
  CHECK(tracks[1].style == kTilt);
  for (int tick = 1; tick < 250; ++tick) {
    AdvanceThunderFreezeTracks(tracks, kTour, kTilt, seed);
  }
  CHECK(tracks[0].alpha == kCoreCeiling);

  // Une première piste éteinte ne bouge plus, mais les angles se rangent quand même.
  Tracks eteint = RestingNode(0.0F);
  eteint[0].active = false;
  AdvanceThunderFreezeTracks(eteint, kTour, kTilt, seed);
  CHECK(eteint[0].alpha == 0);
  CHECK(eteint[0].ticks == 0);
  CHECK(eteint[0].style == kTour);
  CHECK(eteint[1].style == kTilt);
}

TEST_CASE("Cœur de gel : le cercle de sept, quinze au-dessus, un degré tous les deux tics",
          "[render][effectnode]") {
  std::uint32_t seed = 11U;
  Tracks tracks = RestingNode(90.0F);
  AdvanceThunderFreezeTracks(tracks, 0, 0, seed);
  // Compteur impair : le cercle n'a pas tourné.
  CHECK(tracks[0].value[kOrbite] == Approx(90.0F));
  // Le client pose x = 7·sin, z = 7·cos, y = −15 ; notre repère retourne y et z.
  CHECK(tracks[0].anchor.x == Approx(kCircleRadius).margin(kTolerance));
  CHECK(tracks[0].anchor.y == Approx(kQuinzeAuDessus).margin(kTolerance));
  CHECK(tracks[0].anchor.z == Approx(0.0F).margin(kTolerance));
  AdvanceThunderFreezeTracks(tracks, 0, 0, seed);
  CHECK(tracks[0].value[kOrbite] == Approx(91.0F));

  Tracks au_nord = RestingNode(0.0F);
  AdvanceThunderFreezeTracks(au_nord, 0, 0, seed);
  CHECK(au_nord[0].anchor.x == Approx(0.0F).margin(kTolerance));
  CHECK(au_nord[0].anchor.z == Approx(-kCircleRadius).margin(kTolerance));

  // Le tour se replie à trois cent soixante.
  Tracks at_end = RestingNode(359.0F);
  at_end[0].ticks = 1;
  AdvanceThunderFreezeTracks(at_end, 0, 0, seed);
  CHECK(at_end[0].value[kOrbite] == Approx(0.0F));
}

TEST_CASE("Cœur de gel : le tremblement marche d'un degré, puis tire une cible de l'autre côté",
          "[render][effectnode]") {
  std::uint32_t seed = 17U;
  // MONTÉE : la cible est au-dessus ; l'angle l'atteint sans la passer, rien ne se tire.
  Tracks rise = RestingNode(0.0F);
  rise[0].value[0] = 10.0F;
  rise[0].value[1] = 11.0F;
  std::uint32_t witness = seed;
  AdvanceThunderFreezeTracks(rise, 0, 0, seed);
  CHECK(rise[0].value[0] == Approx(11.0F));
  CHECK(rise[0].value[1] == Approx(11.0F));
  // ÉGALE, donc la descente : l'angle recule d'un degré et tire AU-DESSUS de lui.
  AdvanceThunderFreezeTracks(rise, 0, 0, seed);
  CHECK(rise[0].value[0] == Approx(10.0F));
  CHECK(rise[0].value[1] >= 350.0F);
  CHECK(rise[0].value[1] <= 359.0F);

  // PASSÉE : une cible fractionnaire dépassée tire EN DESSOUS de l'angle.
  Tracks passee = RestingNode(0.0F);
  passee[0].value[0] = 10.0F;
  passee[0].value[1] = 10.5F;
  AdvanceThunderFreezeTracks(passee, 0, 0, seed);
  CHECK(passee[0].value[0] == Approx(11.0F));
  CHECK(passee[0].value[1] >= 0.0F);
  CHECK(passee[0].value[1] <= 10.0F);

  // PLAFOND : l'angle ne monte pas au-delà de 359.
  Tracks ceiling = RestingNode(0.0F);
  ceiling[0].value[2] = 359.0F;
  ceiling[0].value[3] = 400.0F;
  AdvanceThunderFreezeTracks(ceiling, 0, 0, witness);
  CHECK(ceiling[0].value[2] == Approx(359.0F));
}

TEST_CASE("Cœur de gel : la marque divise le tremblement par deux", "[render][effectnode]") {
  std::uint32_t seed = 23U;
  Tracks integer = RestingNode(0.0F);
  integer[0].value[0] = 89.0F;
  integer[0].value[1] = 200.0F;
  Tracks moitie = integer;
  moitie[0].mark[kHalfWander] = 1;
  AdvanceThunderFreezeTracks(integer, 0, 0, seed);
  AdvanceThunderFreezeTracks(moitie, 0, 0, seed);
  const float sinus = std::sin(Radians(90.0F));
  CHECK(integer[0].anchor.x == Approx(sinus).margin(kTolerance));
  CHECK(moitie[0].anchor.x == Approx(sinus * 0.5F).margin(kTolerance));
}

TEST_CASE("Cœur de gel : un éclat attend son tour, grandit, pâlit et renaît",
          "[render][effectnode]") {
  std::uint32_t seed = 29U;
  Tracks tracks = RestingNode(0.0F);
  tracks[1].ticks = -kDelayPerRank;
  tracks[1].lift_key = kBirthSize;
  tracks[1].tilt_degrees = kShardAlpha;
  tracks[1].degrees = 89;
  for (int tick = 0; tick < kDelayPerRank; ++tick) {
    AdvanceThunderFreezeTracks(tracks, 0, 0, seed);
  }
  // Compteur à zéro : rien n'a bougé, et la figure le tait.
  CHECK(tracks[1].ticks == 0);
  CHECK(tracks[1].value[kSize] == 0.0F);
  CHECK(tracks[1].degrees == 89);
  AdvanceThunderFreezeTracks(tracks, 0, 0, seed);
  CHECK(tracks[1].degrees == 90);
  const float base = kBirthSize * kCroissance;
  CHECK(tracks[1].lift_key == Approx(base));
  CHECK(tracks[1].value[kSize] == Approx(base * 0.1F + base * 0.9F));

  // Passé six, l'alpha perd trois ; à zéro, l'éclat renaît.
  tracks[1].lift_key = 6.5F;
  tracks[1].tilt_degrees = 3;
  tracks[1].degrees = 0;
  AdvanceThunderFreezeTracks(tracks, 0, 0, seed);
  CHECK(tracks[1].tilt_degrees == kShardAlpha);
  CHECK(tracks[1].lift_key == Approx(kBirthSize));
  CHECK(tracks[1].azimuth_degrees >= 0);
  CHECK(tracks[1].azimuth_degrees < 360);
  CHECK(tracks[1].value[kSize] ==
        Approx(kBirthSize * std::sin(Radians(1.0F)) * 0.1F + kBirthSize * 0.9F));

  tracks[1].lift_key = 6.5F;
  tracks[1].tilt_degrees = 100;
  AdvanceThunderFreezeTracks(tracks, 0, 0, seed);
  CHECK(tracks[1].tilt_degrees == 97);
}

TEST_CASE("Cœur de gel : deux carrés bleus puis un éclat par piste vivante, en addition",
          "[render][effectnode]") {
  std::uint32_t seed = 31U;
  Tracks tracks = RestingNode(0.0F);
  const glm::vec3 origin{100.0F, 5.0F, 200.0F};
  // Au semis, l'alpha est nul : rien ne se voit.
  CHECK(ComposeThunderFreezeQuads(tracks, origin).empty());
  tracks[0].lift_key = kBirthSize;
  tracks[0].tilt_degrees = kShardAlpha;
  AdvanceThunderFreezeTracks(tracks, 0, 0, seed);
  const std::vector<EffectWorldQuad> quads = ComposeThunderFreezeQuads(tracks, origin);
  REQUIRE(quads.size() == 3);
  CHECK(quads[0].texture == ro::render::kThunderFreezeCoreTexture);
  CHECK(quads[1].texture == ro::render::kThunderFreezeCoreTexture);
  CHECK(quads[2].texture == ro::render::kThunderFreezeShardTexture);
  CHECK(quads[0].tint == std::array<std::uint8_t, 4>{0x44, 0x2A, 0xFF, 1});
  CHECK(quads[1].tint == std::array<std::uint8_t, 4>{0x9B, 0x9B, 0xFF, 1});
  CHECK(quads[2].tint == std::array<std::uint8_t, 4>{105, 105, 0xFF, kShardAlpha});
  for (const EffectWorldQuad& quad : quads) {
    CHECK(quad.blend_destination == kBlendOne);
    // L'image retournée : un aux deux premiers coins, zéro aux deux derniers.
    CHECK(quad.texture_uv[0].y == 1.0F);
    CHECK(quad.texture_uv[2].y == 0.0F);
  }
  // Le cœur est posé sur l'ancre, et l'ancre se rapporte à la place du nœud.
  const glm::vec3 centre = origin + tracks[0].anchor;
  CHECK(CentreDe(quads[0]).x == Approx(centre.x).margin(kTolerance));
  CHECK(CentreDe(quads[0]).y == Approx(centre.y).margin(kTolerance));
  CHECK(CentreDe(quads[0]).z == Approx(centre.z).margin(kTolerance));
}

TEST_CASE("Cœur de gel : le second carré n'est pas la moitié du premier", "[render][effectnode]") {
  std::uint32_t seed = 37U;
  Tracks tracks = RestingNode(0.0F);
  AdvanceThunderFreezeTracks(tracks, 0, 0, seed);
  // Respiration au sommet : sin(90) = 1.
  tracks[0].value[kRespiration] = 90.0F;
  const std::vector<EffectWorldQuad> quads = ComposeThunderFreezeQuads(tracks, {});
  REQUIRE(quads.size() == 2);
  const float outer = glm::length(quads[0].corners[0] - CentreDe(quads[0]));
  const float inner = glm::length(quads[1].corners[0] - CentreDe(quads[1]));
  CHECK(outer == Approx(1.05F).margin(kTolerance));
  CHECK(inner == Approx(0.55F).margin(kTolerance));
}

TEST_CASE("Cœur de gel : le cœur fait face à la caméra", "[render][effectnode]") {
  for (const float cap : {0.0F, 37.0F, 90.0F, 200.0F, 315.0F}) {
    for (const float tilt : {30.0F, 45.0F, 60.0F}) {
      INFO("cap " << cap << ", inclinaison " << tilt);
      ro::render::Camera camera;
      camera.yaw_degrees = cap;
      camera.pitch_degrees = tilt;
      Tracks tracks = RestingNode(0.0F);
      std::uint32_t seed = 41U;
      AdvanceThunderFreezeTracks(tracks, ro::render::EffectBillboardTurnDegrees(camera),
                                 ro::render::EffectBillboardTiltDegrees(camera), seed);
      const std::vector<EffectWorldQuad> quads = ComposeThunderFreezeQuads(tracks, {});
      REQUIRE_FALSE(quads.empty());
      const EffectWorldQuad& world_quad = quads[0];
      const glm::vec3 normale =
          glm::normalize(glm::cross(world_quad.corners[1] - world_quad.corners[0],
                                    world_quad.corners[3] - world_quad.corners[0]));
      const glm::vec3 vers_l_oeil = glm::normalize(camera.Position() - CentreDe(world_quad));
      CHECK(std::abs(std::abs(glm::dot(normale, vers_l_oeil)) - 1.0F) < 0.02F);
    }
  }
}

TEST_CASE("Cœur de gel : un éclat est un carré DEBOUT, en travers de son cap",
          "[render][effectnode]") {
  Tracks tracks = RestingNode(0.0F);
  tracks[0].alpha = kShardAlpha;
  tracks[0].anchor = {1.0F, 15.0F, -7.0F};
  tracks[2].value[kSize] = 4.0F;
  tracks[2].azimuth_degrees = 0;
  tracks[2].tilt_degrees = kShardAlpha;
  const glm::vec3 origin{10.0F, 0.0F, 10.0F};
  const std::vector<EffectWorldQuad> quads = ComposeThunderFreezeQuads(tracks, origin);
  REQUIRE(quads.size() == 3);
  const EffectWorldQuad& shard = quads[2];
  const glm::vec3 centre = origin + tracks[0].anchor;
  // Cap zéro : les bords au cap et au cap opposé, sur l'abscisse, et une demi-taille
  // sous puis au-dessus du cœur.
  CHECK(shard.corners[0].x == Approx(centre.x + 2.0F));
  CHECK(shard.corners[0].y == Approx(centre.y - 2.0F));
  CHECK(shard.corners[1].x == Approx(centre.x - 2.0F));
  CHECK(shard.corners[1].y == Approx(centre.y - 2.0F));
  CHECK(shard.corners[2].x == Approx(centre.x - 2.0F));
  CHECK(shard.corners[2].y == Approx(centre.y + 2.0F));
  CHECK(shard.corners[3].x == Approx(centre.x + 2.0F));
  CHECK(shard.corners[3].y == Approx(centre.y + 2.0F));
  for (const glm::vec3& corner : shard.corners) {
    CHECK(corner.z == Approx(centre.z).margin(kTolerance));
  }
  // La teinte est celle du RANG : la troisième piste peint un gris de 75.
  CHECK(shard.tint[0] == 75);
  CHECK(shard.tint[2] == 0xFF);
}

TEST_CASE("Cœur de gel : le nœud suit son porteur", "[render][effectnode]") {
  EffectNodePool pool;
  const glm::vec3 bearer{0.0F, 0.0F, 0.0F};
  std::uint32_t seed = 43U;
  std::vector<PooledEffectNode> nodes =
      SowThunderFreezeNodes(bearer, ro::render::kThunderFreezeLaunchedCount,
                            EffectNodeLifeFor(kThunderFreezeEffectId), seed);
  REQUIRE(nodes.size() == 2);
  pool.AnchorNextTo(17U, bearer);
  for (PooledEffectNode& node : nodes) {
    pool.Add(std::move(node));
  }
  pool.AnchorNextTo(0U);

  const glm::vec3 farther{-20.0F, 0.0F, 5.0F};
  ro::test_support::MoveBearer(pool, 17U, farther);
  pool.Advance(kEffectTickMs);
  REQUIRE(pool.size() == 2);
  for (const PooledEffectNode& alive : pool.nodes()) {
    CHECK(alive.ground_position.x == Approx(farther.x).margin(kTolerance));
    CHECK(alive.ground_position.z == Approx(farther.z).margin(kTolerance));
    CHECK(alive.tracks[0].alpha == 1);
  }
}
