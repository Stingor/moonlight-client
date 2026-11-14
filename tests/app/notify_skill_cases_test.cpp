// CE QUE LE PAQUET DES DÉGÂTS D'UNE COMPÉTENCE MONTRE DE LUI-MÊME : l'effet
// qu'il pose sur la cible, et les conditions qui le gardent.

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_class_view.h"
#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/effect_builder.h"
#include "support/grf_builder.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

namespace {

constexpr std::uint32_t kCasterId = 110'001;
constexpr std::uint32_t kTargetId = 110'002;
constexpr std::uint32_t kUnseenCasterId = 110'003;
constexpr std::uint32_t kNowMs = 20'000;

constexpr std::uint16_t kHellJudgement = 662;
constexpr std::uint32_t kHellJudgementEffect = 372;
constexpr std::uint16_t kRuneKnightWindCutter = 2005;
constexpr std::uint32_t kWindCutterEffect = 1693;
constexpr std::uint16_t kVenomSplasher = 141;
constexpr std::uint32_t kSplasherBurstEffect = 129;
// Le niveau des victimes alentour, puis celui de la cible centrale.
constexpr std::int16_t kSplashedLevel = -1;
constexpr std::int16_t kCentreLevel = -2;
constexpr std::uint16_t kRuneKnightDragonBreath = 2008;
constexpr std::string_view kBreathSound = "effect\\폭염룡.wav";
constexpr std::uint16_t kMechanicArmsCannon = 2261;
constexpr std::uint32_t kLastShellOnTarget = 1355;
constexpr std::uint16_t kWarlockJackFrost = 2204;
constexpr std::array<std::uint32_t, 3> kFrostShards = {1443, 1444, 1445};
constexpr std::uint16_t kAbyssUnluckyRush = 5315;
constexpr std::uint32_t kCrimsonArrow = 2154;
constexpr std::uint16_t kGlanzenSpies = 8056;
constexpr std::uint32_t kGlanzenSpiesEffect = 2335;
constexpr std::uint16_t kColdBolt = 14;
constexpr std::uint32_t kColdBoltArrows = 26;
// Les sortes de coup : à coups multiples, simple, et critique.
constexpr std::uint8_t kMultiHit = 8;
constexpr std::uint8_t kSingleHit = 6;
constexpr std::uint8_t kCriticalHit = 10;

moonclient::MapStage FlatStage() {
  auto gnd =
      ro::formats::GndMap::Parse(ro::test_support::BuildGnd(ro::test_support::GndBuildOptions{}));
  REQUIRE(gnd.has_value());
  auto rsw =
      ro::formats::RswScene::Parse(ro::test_support::BuildRsw(ro::test_support::RswBuildOptions{}));
  REQUIRE(rsw.has_value());
  ro::render::LightmapAtlas atlas = ro::render::BuildLightmapAtlas(*gnd);
  const ro::render::SceneLight light = ro::render::MakeSceneLight(rsw->light());
  return moonclient::MapStage{.name = "plate",
                              .scene = ro::render::MapScene{
                                  .gnd = std::move(*gnd),
                                  .rsw = std::move(*rsw),
                                  .atlas = std::move(atlas),
                                  .light = light,
                              }};
}

// Le fichier de chacun des effets, réduit à une durée.
constexpr std::string_view kHellJudgementFile = "data\\texture\\effect\\devil.str";
constexpr std::string_view kWindCutterFile =
    "data\\texture\\effect\\mineffect\\new_windcutter\\new_windcutter_target\\"
    "new_windcutter_target.str";
constexpr std::string_view kSplasherBurstFile = "data\\texture\\effect\\VenomSplasher.str";
constexpr std::string_view kShellFile =
    "data\\texture\\effect\\new_armscannon\\new_armscannon_explosion\\new_armscannon_explosion.str";
constexpr std::array<std::string_view, 3> kFrostShardFiles = {
    "data\\texture\\effect\\new_jackfrost\\new_jackfrost\\new_jackfrost_00.str",
    "data\\texture\\effect\\new_jackfrost\\new_jackfrost\\new_jackfrost_01.str",
    "data\\texture\\effect\\new_jackfrost\\new_jackfrost\\new_jackfrost_02.str"};
constexpr std::string_view kGlanzenSpiesFile =
    "data\\texture\\effect\\glanzen_spies\\glanzen_spies\\glanzen_spies.str";
constexpr std::int32_t kLastFrame = 30;

ro::vfs::Vfs EffectClient(const ro::test_support::TempDirectory& client) {
  std::vector<ro::test_support::GrfFileSpec> files;
  for (const std::string_view path :
       {kHellJudgementFile, kWindCutterFile, kSplasherBurstFile, kShellFile, kFrostShardFiles[0],
        kFrostShardFiles[1], kFrostShardFiles[2], kGlanzenSpiesFile}) {
    files.push_back({std::string(path), ro::test_support::BuildStr({.last_frame = kLastFrame})});
  }
  client.WriteFile("effet.grf", ro::test_support::BuildGrf(files));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effet.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

struct Bench {
  ro::test_support::TempDirectory client;
  ro::vfs::Vfs vfs = EffectClient(client);
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  Bench() {
    for (const std::uint32_t id : {kCasterId, kTargetId}) {
      ro::sim::Actor actor;
      actor.id = id;
      actor.kind = ro::sim::ActorKind::kMonster;
      actor.position = {.x = 10, .y = 10};
      world.Spawn(actor);
    }
  }

  void Strike(std::uint32_t attacker, std::uint32_t target, std::uint16_t skill_id,
              std::int16_t announced_level = 1, std::uint8_t damage_type = kMultiHit) {
    ro::sim::Strike strike;
    strike.attacker = attacker;
    strike.target = target;
    strike.started_ms = kNowMs;
    strike.damage = 1;
    strike.type = damage_type;
    strike.skill_id = skill_id;
    strike.skill_level = 1;
    strike.announced_level = announced_level;
    REQUIRE(world.RecordStrike(strike));
    effects.NoticeSkills(world, stage, planner, kNowMs);
  }
};

}  // namespace

TEST_CASE("Paquet des dégâts : Hell Judgement pose son effet sur la cible", "[app][effets]") {
  Bench bench;
  bench.Strike(kCasterId, kTargetId, kHellJudgement);
  CHECK(bench.effects.Carries(kHellJudgementEffect, kTargetId));
  CHECK_FALSE(bench.effects.Carries(kHellJudgementEffect, kCasterId));
}

TEST_CASE("Paquet des dégâts : Hell Judgement sur son lanceur ne montre rien", "[app][effets]") {
  Bench bench;
  bench.Strike(kCasterId, kCasterId, kHellJudgement);
  CHECK_FALSE(bench.effects.Carries(kHellJudgementEffect, kCasterId));
}

TEST_CASE("Paquet des dégâts : un cas de la cible seule joue sans lanceur à l'écran",
          "[app][effets]") {
  // Hell Judgement demande les deux acteurs ; Wind Cutter, la cible seule.
  Bench bench;
  bench.Strike(kUnseenCasterId, kTargetId, kHellJudgement);
  CHECK_FALSE(bench.effects.Carries(kHellJudgementEffect, kTargetId));
  bench.Strike(kUnseenCasterId, kTargetId, kRuneKnightWindCutter);
  CHECK(bench.effects.Carries(kWindCutterEffect, kTargetId));
}

TEST_CASE("Paquet des dégâts : Venom Splasher n'éclabousse que les victimes alentour",
          "[app][effets]") {
  Bench bench;
  bench.Strike(kCasterId, kTargetId, kVenomSplasher, kCentreLevel);
  CHECK_FALSE(bench.effects.Carries(kSplasherBurstEffect, kTargetId));
  bench.Strike(kCasterId, kTargetId, kVenomSplasher, kSplashedLevel);
  CHECK(bench.effects.Carries(kSplasherBurstEffect, kTargetId));
}

TEST_CASE("Paquet des dégâts : le souffle du dragon attend le lanceur, et sonne", "[app][effets]") {
  const auto breath_heard = [](moonclient::EffectsInPlay& effects) {
    return std::ranges::any_of(effects.TakeSownNamedSounds(),
                               [](const auto& heard) { return heard.sound == kBreathSound; });
  };
  Bench bench;
  bench.Strike(kUnseenCasterId, kTargetId, kRuneKnightDragonBreath);
  CHECK_FALSE(breath_heard(bench.effects));
  bench.Strike(kCasterId, kTargetId, kRuneKnightDragonBreath);
  CHECK(breath_heard(bench.effects));
  // La gueule et le souffle viennent plus tard : rien encore sur personne.
  for (const std::uint32_t effect_id : {1378U, 1379U, 1380U, 1381U, 1382U, 1383U}) {
    CHECK_FALSE(bench.effects.Carries(effect_id, kTargetId));
    CHECK_FALSE(bench.effects.Carries(effect_id, kCasterId));
  }
}

TEST_CASE("Paquet des dégâts : les obus d'Arms Cannon finissent sur la cible, et sonnent",
          "[app][effets]") {
  Bench bench;
  bench.Strike(kCasterId, kTargetId, kMechanicArmsCannon);
  CHECK(bench.effects.Carries(kLastShellOnTarget, kTargetId));
  CHECK(std::ranges::any_of(bench.effects.TakeSownNamedSounds(),
                            [](const auto& heard) { return heard.sound == kBreathSound; }));
}

TEST_CASE("Paquet des dégâts : Jack Frost sème trois éclats sans porteur", "[app][effets]") {
  Bench bench;
  bench.Strike(kUnseenCasterId, kTargetId, kWarlockJackFrost);
  // Ils naissent autour de la cible, et personne ne les porte.
  for (const std::uint32_t shard : kFrostShards) {
    CHECK_FALSE(bench.effects.Carries(shard, kTargetId));
  }
  CHECK(std::ranges::any_of(kFrostShards,
                            [&](std::uint32_t shard) { return bench.effects.Carries(shard, 0); }));
}

TEST_CASE("Paquet des dégâts : la flèche d'Unlucky Rush part du lanceur", "[app][effets]") {
  Bench bench;
  bench.Strike(kCasterId, kTargetId, kAbyssUnluckyRush);
  CHECK(bench.effects.Classes().Carries(kCrimsonArrow, kCasterId));
}

TEST_CASE("Paquet des dégâts : les lames de Glanzen Spies naissent sur le lanceur",
          "[app][effets]") {
  Bench bench;
  bench.Strike(kCasterId, kTargetId, kGlanzenSpies);
  CHECK(bench.effects.Carries(kGlanzenSpiesEffect, kCasterId));
  CHECK_FALSE(bench.effects.Carries(kGlanzenSpiesEffect, kTargetId));
}

TEST_CASE("Paquet des dégâts : la suite d'impact ne joue qu'aux sortes que le visuel connaît",
          "[app][effets]") {
  Bench single;
  single.Strike(kCasterId, kTargetId, kColdBolt, 1, kSingleHit);
  CHECK(single.effects.Carries(kColdBoltArrows, kTargetId));
  Bench critical;
  critical.Strike(kCasterId, kTargetId, kColdBolt, 1, kCriticalHit);
  CHECK_FALSE(critical.effects.Carries(kColdBoltArrows, kTargetId));
}

TEST_CASE("Paquet des dégâts : sans lanceur à l'écran, la suite d'impact ne joue pas",
          "[app][effets]") {
  Bench bench;
  bench.Strike(kUnseenCasterId, kTargetId, kColdBolt, 1, kSingleHit);
  CHECK_FALSE(bench.effects.Carries(kColdBoltArrows, kTargetId));
}
