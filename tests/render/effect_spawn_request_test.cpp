// LES NŒUDS QUI FONT NAÎTRE UN AUTRE EFFET : la lueur en arc (109), l'émetteur
// en spirale (151) et le semeur qui marche (120).
//
// Aucun des trois ne peint ce qu'il montre : il DEMANDE un effet à chaque
// peinture, sur l'acteur qu'il désigne ou au point où il se trouve, et c'est
// l'application qui le fait naître.
#include "ro/render/effect_spawn_request.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_arcing_light.h"
#include "ro/render/effect_arcing_light_seed.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_spiral_emitter.h"
#include "ro/render/effect_spiral_emitter_seed.h"
#include "ro/render/effect_walking_sower.h"
#include "ro/render/effect_walking_sower_seed.h"

using Catch::Approx;
using ro::render::EffectSpawnPlace;
using ro::render::EffectSpawnRequest;
using ro::render::EffectTrack;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;

namespace {

constexpr std::uint32_t kPlayerId = 2'000'001;
// UN IDENTIFIANT DE MONSTRE, au-delà de ce qu'un flottant tient : il s'arrondit
// au multiple de huit le plus proche.
constexpr std::uint32_t kMonsterId = 110'000'005;
constexpr std::uint32_t kMonsterIdAsStored = 110'000'008;
constexpr float kTolerance = 1.0e-4F;
constexpr std::uint32_t kGeneratorSeed = 7U;
constexpr double kRadiansPerDegree = 3.14159265358979323846 / 180.0;

std::vector<EffectSpawnRequest> RequestsOf(const PooledEffectNode& node) {
  std::vector<EffectSpawnRequest> requests;
  ro::render::CollectEffectSpawnRequests(node, requests);
  return requests;
}

// Un nœud de spirale, octet donné, avancé de `tics` tics caméra à zéro.
PooledEffectNode Spirale(std::uint8_t octet, std::int32_t ticks) {
  const ro::render::EffectSpiralEmitterPattern pattern{849, 850, "", "", octet};
  std::uint32_t generator = kGeneratorSeed;
  PooledEffectNode node =
      ro::render::SowSpiralEmitterNode(pattern, glm::vec3(0.0F), 500, kPlayerId, generator);
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    ro::render::AdvanceSpiralEmitterTracks(node.tracks, 0, 0);
  }
  return node;
}

}  // namespace

TEST_CASE("Demande d'effet : l'acteur désigné se range en flottant, et s'arrondit au-delà de 2^24",
          "[effect_spawn_request]") {
  CHECK(ro::render::DesignatedActorId(ro::render::StoredDesignatedActor(kPlayerId)) == kPlayerId);
  // ⚠ LE CLIENT RANGE L'IDENTIFIANT EN FLOTTANT : celui d'un monstre ne revient
  // pas tel quel.
  CHECK(ro::render::DesignatedActorId(ro::render::StoredDesignatedActor(kMonsterId)) ==
        kMonsterIdAsStored);
  CHECK(ro::render::DesignatedActorId(-3.0F) == 0);
  CHECK(ro::render::DesignatedActorId(std::numeric_limits<float>::quiet_NaN()) == 0);
  CHECK(ro::render::DesignatedActorId(1.0e12F) == std::numeric_limits<std::uint32_t>::max());
  // Le joueur se reconnaît EN FLOTTANT : deux identifiants que le flottant
  // confond le désignent tous deux.
  CHECK(ro::render::DesignatesPlayer(ro::render::StoredDesignatedActor(kPlayerId), kPlayerId));
  CHECK_FALSE(
      ro::render::DesignatesPlayer(ro::render::StoredDesignatedActor(kPlayerId + 1U), kPlayerId));
  CHECK(ro::render::DesignatesPlayer(ro::render::StoredDesignatedActor(kMonsterId),
                                     kMonsterIdAsStored));
}

TEST_CASE("Demande d'effet : l'acteur désigné après rangement, joueur compris",
          "[effect_spawn_request]") {
  // UN MONSTRE QUI A PASSÉ 2^24 : le serveur numérote les siens à la suite, à
  // chaque apparition. L'impair s'arrondit au pair, et désigne son voisin.
  constexpr std::uint32_t kLateMonsterId = 16'777'219;
  constexpr std::uint32_t kLateMonsterIdAsStored = 16'777'220;
  CHECK(ro::render::DesignatedActorAfterStorage(kLateMonsterId, kPlayerId) ==
        kLateMonsterIdAsStored);
  CHECK(ro::render::DesignatedActorAfterStorage(kPlayerId, kPlayerId) == kPlayerId);
  // ⚠ LE JOUEUR SE RECONNAÎT EN FLOTTANT, avant la relecture : un identifiant que
  // le flottant confond avec le sien le désigne, LUI.
  CHECK(ro::render::DesignatedActorAfterStorage(kMonsterId, kMonsterIdAsStored) ==
        kMonsterIdAsStored);
  // Personne ne se désigne par zéro.
  CHECK(ro::render::DesignatedActorAfterStorage(0, kPlayerId) == 0);
}

TEST_CASE("Demande d'effet : la lueur en arc demande 502 sur l'acteur désigné, jusqu'au tic 180",
          "[effect_spawn_request]") {
  const ro::render::EffectArcingLightPattern pattern{503, "", ""};
  std::uint32_t generator = kGeneratorSeed;
  PooledEffectNode node =
      ro::render::SowArcingLightNode(pattern, glm::vec3(0.0F), 500, generator, kPlayerId);
  ro::render::AdvanceArcingLightTracks(node.tracks, 0, 0);
  const std::vector<EffectSpawnRequest> requests = RequestsOf(node);
  REQUIRE(requests.size() == 1);
  CHECK(requests[0].effect_id == ro::render::kArcingLightFollowUpEffectId);
  CHECK(requests[0].place == EffectSpawnPlace::kOnDesignatedActor);
  CHECK(ro::render::DesignatedActorId(requests[0].designated_actor) == kPlayerId);
  CHECK(requests[0].position == glm::vec3(0.0F));

  while (node.tracks[0].ticks < ro::render::kArcingLightFollowUpUntilTick) {
    ro::render::AdvanceArcingLightTracks(node.tracks, 0, 0);
  }
  CHECK(RequestsOf(node).empty());
}

TEST_CASE("Émetteur en spirale : 849 une fois, 931 deux fois, octets 2 puis 1",
          "[effect_spawn_request]") {
  const auto un = ro::render::EffectSpiralEmitterPatternsFor(849);
  REQUIRE(un.size() == 1);
  CHECK(un[0].byte == 0);
  const auto two = ro::render::EffectSpiralEmitterPatternsFor(931);
  REQUIRE(two.size() == 2);
  CHECK(two[0].byte == 2);
  CHECK(two[1].byte == 1);
  // L'EFFET EN TABLE EST CELUI QUE L'OCTET FAIT NAÎTRE : les deux sources
  // restent d'accord.
  for (const ro::render::EffectSpiralEmitterPattern& pattern :
       ro::render::EffectSpiralEmitterPatternTable()) {
    PooledEffectNode node = Spirale(pattern.byte, 1);
    CHECK(ro::render::SpiralEmitterChildOf(node.tracks[0], 0, 0) == pattern.spawned_effect_id);
    CHECK(ro::render::KnowsProceduralEffect(pattern.effect_id));
  }
}

TEST_CASE("Émetteur en spirale : la piste s'allume, et l'écart tourne à vingt-cinq au-dessus",
          "[effect_spawn_request]") {
  // Au semis, l'alpha est nul : rien ne naît avant le premier tic.
  CHECK_FALSE(ro::render::SpiralEmitterChildOf(Spirale(0, 0).tracks[0], 0, 0).has_value());

  const EffectTrack first = Spirale(0, 1).tracks[0];
  CHECK(first.value[ro::render::kSpiralEmitterSlotOfAlpha] == Approx(20.0F));
  CHECK(first.tail[ro::render::kSpiralEmitterSlotOfSpiralRadius] ==
        Approx(5.05F).margin(kTolerance));
  // Rayon TRONQUÉ à cinq, six degrés au premier tic.
  const glm::vec3 offset = ro::render::SpiralEmitterChildOffset(first);
  CHECK(offset.x == Approx(5.0 * std::sin(6.0 * kRadiansPerDegree)).margin(kTolerance));
  CHECK(offset.y == Approx(25.0F));
  CHECK(offset.z == Approx(-5.0 * std::cos(6.0 * kRadiansPerDegree)).margin(kTolerance));

  // L'OCTET UN TOURNE À REBOURS : trois cent cinquante-quatre degrés au premier tic.
  const glm::vec3 rebours = ro::render::SpiralEmitterChildOffset(Spirale(1, 1).tracks[0]);
  CHECK(rebours.x == Approx(5.0 * std::sin(354.0 * kRadiansPerDegree)).margin(kTolerance));

  // Le rayon s'élargit d'une unité tous les vingt tics.
  const glm::vec3 nineteen = ro::render::SpiralEmitterChildOffset(Spirale(0, 19).tracks[0]);
  CHECK(std::hypot(nineteen.x, nineteen.z) == Approx(5.0F));
  const glm::vec3 vingt_et_un = ro::render::SpiralEmitterChildOffset(Spirale(0, 21).tracks[0]);
  CHECK(std::hypot(vingt_et_un.x, vingt_et_un.z) == Approx(6.0F));
}

TEST_CASE("Émetteur en spirale : l'alpha pâlit après 270 tics pour l'octet zéro, 90 sinon",
          "[effect_spawn_request]") {
  CHECK(Spirale(0, 10).tracks[0].value[ro::render::kSpiralEmitterSlotOfAlpha] == Approx(200.0F));
  CHECK(Spirale(0, 270).tracks[0].value[ro::render::kSpiralEmitterSlotOfAlpha] == Approx(200.0F));
  CHECK(Spirale(0, 271).tracks[0].value[ro::render::kSpiralEmitterSlotOfAlpha] == Approx(195.0F));
  CHECK(Spirale(2, 91).tracks[0].value[ro::render::kSpiralEmitterSlotOfAlpha] == Approx(195.0F));
  // L'octet deux s'éteint au tic cent trente, et ne demande plus rien.
  CHECK(Spirale(2, 129).tracks[0].value[ro::render::kSpiralEmitterSlotOfAlpha] == Approx(5.0F));
  CHECK(ro::render::SpiralEmitterChildOf(Spirale(2, 129).tracks[0], 0, 0) == 932U);
  CHECK_FALSE(ro::render::SpiralEmitterChildOf(Spirale(2, 130).tracks[0], 0, 0).has_value());
  // L'octet zéro demande jusqu'au tic deux cent quatre-vingt-dix-neuf, pas au trois centième.
  CHECK(ro::render::SpiralEmitterChildOf(Spirale(0, 299).tracks[0], 0, 0) == 850U);
  CHECK_FALSE(ro::render::SpiralEmitterChildOf(Spirale(0, 300).tracks[0], 0, 0).has_value());
}

TEST_CASE("Émetteur en spirale : la demande va sur l'acteur désigné, à l'écart de la spirale",
          "[effect_spawn_request]") {
  const PooledEffectNode node = Spirale(0, 1);
  CHECK(node.shape == PooledNodeShape::kSpiralEmitter);
  CHECK(ro::render::EffectNodeShapeOnlySpawns(node.shape));
  const std::vector<EffectSpawnRequest> requests = RequestsOf(node);
  REQUIRE(requests.size() == 1);
  CHECK(requests[0].effect_id == 850);
  CHECK(requests[0].place == EffectSpawnPlace::kOnDesignatedActor);
  CHECK(ro::render::DesignatedActorId(requests[0].designated_actor) == kPlayerId);
  CHECK(requests[0].position.y == Approx(25.0F));
}

TEST_CASE("Semeur qui marche : trois sites, un octet chacun, et l'enfant que l'octet choisit",
          "[effect_spawn_request]") {
  const auto table = ro::render::EffectWalkingSowerPatternTable();
  REQUIRE(table.size() == 3);
  for (const ro::render::EffectWalkingSowerPattern& pattern : table) {
    std::uint32_t generator = kGeneratorSeed;
    std::array<EffectTrack, kEffectTracksPerNode> tracks =
        ro::render::SeedWalkingSowerTracks(pattern.byte, glm::vec3(0.0F), generator);
    // Le premier tic zéro de la première piste sème pour les trois octets.
    for (std::int32_t tick = 0; tick < -ro::render::kWalkingSowerFirstTrackTicks; ++tick) {
      ro::render::AdvanceWalkingSowerTracks(tracks, 0);
    }
    CHECK(ro::render::WalkingSowerChildOf(tracks) == pattern.spawned_effect_id);
    CHECK(ro::render::KnowsProceduralEffect(pattern.effect_id));
  }
}

TEST_CASE("Semeur qui marche : quatre pistes retardées, nées six à dix unités au-dessus",
          "[effect_spawn_request]") {
  std::uint32_t generator = kGeneratorSeed;
  const glm::vec3 instance{10.0F, 3.0F, -4.0F};
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      ro::render::SeedWalkingSowerTracks(1, instance, generator);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].ticks ==
          ro::render::kWalkingSowerFirstTrackTicks + static_cast<std::int32_t>(rang));
    CHECK(tracks[rang].anchor.x == Approx(instance.x));
    CHECK(tracks[rang].anchor.z == Approx(instance.z));
    CHECK(tracks[rang].anchor.y >= instance.y + 6.0F);
    CHECK(tracks[rang].anchor.y <= instance.y + 10.0F);
    CHECK(tracks[rang].value[ro::render::kWalkingSowerSlotOfSpeed] == Approx(0.8F));
  }
}

TEST_CASE("Semeur qui marche : le cap se prend au premier tic, puis la piste avance vers la cible",
          "[effect_spawn_request]") {
  std::uint32_t generator = kGeneratorSeed;
  const glm::vec3 instance{0.0F, 0.0F, 0.0F};
  // ⚠ NOTRE PROFONDEUR EST OPPOSÉE À CELLE DU CLIENT : une cible vers nos z
  // négatifs est au cap quatre-vingt-dix.
  const std::optional<glm::vec3> target = glm::vec3{0.0F, 0.0F, -50.0F};
  PooledEffectNode node =
      ro::render::SowWalkingSowerNode({555, 556, "", 1}, instance, target, 500, generator);
  CHECK(ro::render::WalkingSowerClientHeading(instance, target) == 90);
  CHECK(ro::render::WalkingSowerClientHeading(instance, std::nullopt) == 0);
  const glm::vec3 start = node.tracks[0].anchor;
  // Trois tics de retard, puis cinq pas.
  for (std::int32_t tick = 0; tick < 3 + 5; ++tick) {
    ro::render::AdvanceWalkingSowerTracks(
        node.tracks, ro::render::WalkingSowerClientHeading(node.ground_position, target));
  }
  CHECK(node.tracks[0].degrees == 90);
  CHECK(node.tracks[0].anchor.x == Approx(start.x).margin(kTolerance));
  CHECK(node.tracks[0].anchor.z == Approx(start.z - (5.0F * 0.8F)).margin(kTolerance));

  // L'OCTET UN DEMANDE À CHAQUE PEINTURE, là où la piste se trouve.
  const std::vector<EffectSpawnRequest> requests = RequestsOf(node);
  REQUIRE(requests.size() == 1);
  CHECK(requests[0].effect_id == 556);
  CHECK(requests[0].place == EffectSpawnPlace::kAtPoint);
  CHECK(requests[0].position == node.tracks[0].anchor);
}

TEST_CASE("Semeur qui marche : les octets 0 et 99 ne demandent qu'aux tics multiples de huit",
          "[effect_spawn_request]") {
  for (const std::uint8_t octet : {std::uint8_t{0}, std::uint8_t{99}}) {
    std::uint32_t generator = kGeneratorSeed;
    std::array<EffectTrack, kEffectTracksPerNode> tracks =
        ro::render::SeedWalkingSowerTracks(octet, glm::vec3(0.0F), generator);
    std::vector<std::int32_t> sowing_ticks;
    // Au semis déjà, compteur à moins trois : rien.
    for (std::int32_t tour = 0; tour < 20; ++tour) {
      if (ro::render::WalkingSowerChildOf(tracks).has_value()) {
        sowing_ticks.push_back(tracks[0].ticks);
      }
      ro::render::AdvanceWalkingSowerTracks(tracks, 0);
    }
    CHECK(sowing_ticks == std::vector<std::int32_t>{0, 8, 16});
  }
}
