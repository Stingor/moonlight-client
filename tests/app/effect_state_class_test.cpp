// LES CLASSES QU'UN ÉTAT POSE SUR SON PORTEUR : elles naissent à la pose avec
// la valeur de l'état, renaissent quand la valeur change, et partent à la
// levée. Et un effet d'état sans figure fait entendre son bruit.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/vec3.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_class.h"
#include "ro/render/effect_sound_table.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/render/state_effect.h"
#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_layers.h"
#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/client_vfs.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/actor_sounds.h"
#include "world/map_stage.h"

using ro::test_support::BuildGnd;
using ro::test_support::BuildRsw;
using ro::test_support::EmptyClient;
using ro::test_support::GndBuildOptions;
using ro::test_support::RswBuildOptions;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kBearerId = 150'303;
constexpr std::uint32_t kPoseMs = 10'000;
constexpr std::uint32_t kLaterMs = 12'000;
constexpr std::uint32_t kReleaseMs = 20'000;

// Les états et les classes que les essais nomment.
constexpr ro::sim::StatusEffectId kSunStance = 1040;  // trois soleils, 1214
constexpr std::uint32_t kSunStanceClass = 1214;
constexpr ro::sim::StatusEffectId kClimax = 1152;  // Climax, 1800, niveau en valeur
constexpr std::uint32_t kClimaxClass = 1800;
constexpr ro::sim::StatusEffectId kSlashCount = 1196;  // le compte d'Eternal Slash, 1913
constexpr std::uint32_t kSlashCountClass = 1913;
constexpr ro::sim::StatusEffectId kSteelBody = 87;  // 1132 ne fait que sonner
constexpr std::uint32_t kSteelBodySound = 1132;

moonclient::MapStage FlatStage() {
  auto gnd = ro::formats::GndMap::Parse(BuildGnd(GndBuildOptions{}));
  REQUIRE(gnd.has_value());
  auto rsw = ro::formats::RswScene::Parse(BuildRsw(RswBuildOptions{}));
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

struct Bench {
  TempDirectory client;
  ro::vfs::Vfs vfs = EmptyClient(client);
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  Bench() {
    ro::sim::Actor bearer;
    bearer.id = kBearerId;
    bearer.kind = ro::sim::ActorKind::kPlayer;
    world.Spawn(bearer);
    effects.NoticeStateEffects(world, stage, planner, kPoseMs);
  }

  void Set(ro::sim::StatusEffectId state, bool on, std::uint32_t now_ms,
           std::optional<std::int32_t> value = std::nullopt) {
    REQUIRE(world.SetStatusEffect(kBearerId, state, on, value));
    effects.NoticeStateEffects(world, stage, planner, now_ms);
  }

  // Le porteur devient le personnage du joueur.
  void MakeBearerThePlayer() { world.set_player_id(kBearerId); }

  [[nodiscard]] bool Carries(std::uint32_t effect_id) const {
    return effects.Carries(effect_id, kBearerId);
  }
};

}  // namespace

TEST_CASE("une classe qu'un état pose naît sur son porteur et part à la levée", "[state_class]") {
  Bench bench;
  REQUIRE(ro::render::IsPortedEffectClass(kSunStanceClass));
  bench.Set(kSunStance, true, kPoseMs);
  CHECK(bench.Carries(kSunStanceClass));
  bench.Set(kSunStance, false, kReleaseMs);
  CHECK_FALSE(bench.Carries(kSunStanceClass));
}

TEST_CASE("chaque classe que la pose d'un état fait naître est portée par son porteur",
          "[state_class]") {
  std::vector<std::pair<ro::sim::StatusEffectId, std::uint32_t>> couples;
  for (const ro::render::StateEffectEntry& entry : ro::render::StateEffectTable()) {
    if (entry.moment != ro::render::StateEffectMoment::kOnRelease &&
        ro::render::IsPortedEffectClass(entry.effect_id)) {
      couples.emplace_back(entry.state_id, entry.effect_id);
    }
  }
  // Les postures, les Doram, l'immunité aux éléments, Climax, le Meister…
  REQUIRE(couples.size() >= 20);
  for (const auto& [state, effect_id] : couples) {
    INFO("état " << state << ", classe " << effect_id);
    Bench bench;
    // Une classe réservée au joueur ne naît que sur son personnage.
    if (ro::render::EffectClassOwnPlayerOnly(effect_id)) {
      bench.MakeBearerThePlayer();
    }
    // Une valeur dans les bornes de toutes : un compte, un niveau.
    bench.Set(state, true, kPoseMs, 1);
    CHECK(bench.Carries(effect_id));
  }
}

TEST_CASE("le givre d'une gelure ne couvre l'écran que pour son propre personnage",
          "[state_class]") {
  constexpr ro::sim::StatusEffectId kFrostbite = 1208;
  constexpr std::uint32_t kFrostbiteScreen = 2272;
  REQUIRE(ro::render::EffectClassOwnPlayerOnly(kFrostbiteScreen));
  Bench other;
  other.Set(kFrostbite, true, kPoseMs);
  CHECK_FALSE(other.Carries(kFrostbiteScreen));
  Bench self;
  self.MakeBearerThePlayer();
  self.Set(kFrostbite, true, kPoseMs);
  CHECK(self.Carries(kFrostbiteScreen));
  self.Set(kFrostbite, false, kReleaseMs);
  CHECK_FALSE(self.Carries(kFrostbiteScreen));
}

TEST_CASE("une valeur nouvelle refait naître la classe de l'état, sans l'empiler",
          "[state_class]") {
  Bench bench;
  bench.Set(kClimax, true, kPoseMs, 1);
  REQUIRE(bench.Carries(kClimaxClass));
  // Le niveau change : Climax part, puis renaît avec le fond du nouveau niveau.
  bench.Set(kClimax, true, kLaterMs, 4);
  CHECK(bench.Carries(kClimaxClass));
  bench.Set(kClimax, false, kReleaseMs);
  CHECK_FALSE(bench.Carries(kClimaxClass));
  // Le compte d'Eternal Slash n'a rien sans valeur, et naît avec elle.
  bench.Set(kSlashCount, true, kPoseMs, 2);
  CHECK(bench.Carries(kSlashCountClass));
}

TEST_CASE("un effet d'état sans figure fait entendre son bruit", "[state_class]") {
  Bench bench;
  static_cast<void>(bench.effects.TakeLaunchedEffects());
  bench.Set(kSteelBody, true, kPoseMs);
  const std::vector<moonclient::LaunchedEffect> launched = bench.effects.TakeLaunchedEffects();
  CHECK(std::ranges::any_of(launched, [](const moonclient::LaunchedEffect& effect) {
    return effect.effect_id == kSteelBodySound;
  }));
}

TEST_CASE("un objet lancé par une compétence fait entendre son bruit", "[state_class]") {
  constexpr std::uint32_t kAcidTerrorFlask = 298;
  REQUIRE(ro::render::IsPortedEffectClass(kAcidTerrorFlask));
  REQUIRE_FALSE(ro::render::EffectSounds(kAcidTerrorFlask).empty());
  Bench bench;
  static_cast<void>(bench.effects.TakeLaunchedEffects());
  REQUIRE(bench.effects.PlayById(kAcidTerrorFlask, glm::vec3(0.0F), kPoseMs, kBearerId));
  const std::vector<moonclient::LaunchedEffect> launched = bench.effects.TakeLaunchedEffects();
  CHECK(std::ranges::any_of(launched, [](const moonclient::LaunchedEffect& effect) {
    return effect.effect_id == kAcidTerrorFlask;
  }));
}

TEST_CASE("la classe qu'une compétence lance fait entendre son bruit", "[state_class]") {
  // Acid Terror lance son flacon, Syuriken son étoile : deux classes, posées par
  // l'étape de leur suite au coup.
  struct Throw {
    std::uint16_t skill_id;
    std::uint32_t thrown_class;
  };
  constexpr Throw kThrows[] = {{.skill_id = 230, .thrown_class = 298},
                               {.skill_id = 523, .thrown_class = 613}};
  for (const auto& [skill_id, thrown_class] : kThrows) {
    INFO("compétence " << skill_id);
    REQUIRE(ro::render::IsPortedEffectClass(thrown_class));
    Bench bench;
    static_cast<void>(bench.effects.TakeLaunchedEffects());
    bench.effects.PlaySkillOn(bench.world, bench.stage, bench.planner, *bench.world.Find(kBearerId),
                              skill_id, kBearerId, kPoseMs, 1, moonclient::SkillMoment::kImpact, 0);
    const std::vector<moonclient::LaunchedEffect> launched = bench.effects.TakeLaunchedEffects();
    CHECK(std::ranges::any_of(launched, [thrown_class](const moonclient::LaunchedEffect& effect) {
      return effect.effect_id == thrown_class;
    }));
  }
}

namespace {

constexpr ro::sim::StatusEffectId kRunning = 133;
constexpr ro::sim::StatusEffectId kWallBounce = 134;
constexpr std::uint16_t kTaekwonRun = 411;
constexpr std::uint32_t kFootprints = 442;
constexpr std::uint32_t kStopMark = 443;
constexpr std::uint32_t kStopRing = 444;
constexpr std::uint32_t kShaking = 426;

// UNE IMAGE DU JEU, DANS SON ORDRE : les états de la rafale, la compétence
// annoncée s'il y en a une, puis la liste des états qui tiennent.
void RunFrame(Bench& bench, std::uint32_t now_ms, bool announced) {
  bench.effects.NoticeStateChanges(bench.world, bench.stage, bench.planner, now_ms);
  if (announced) {
    bench.effects.PlaySkillOn(bench.world, bench.stage, bench.planner, *bench.world.Find(kBearerId),
                              kTaekwonRun, kBearerId, now_ms, 1,
                              moonclient::SkillMoment::kAnnounced, 0);
  }
  bench.effects.NoticeStateEffects(bench.world, bench.stage, bench.planner, now_ms);
}

std::size_t CountPosed(const std::vector<moonclient::EffectsInPlay::ActorEffectToRecord>& poses,
                       std::uint32_t effect_id, bool withdrawn) {
  return static_cast<std::size_t>(std::ranges::count_if(
      poses, [effect_id, withdrawn](const moonclient::EffectsInPlay::ActorEffectToRecord& pose) {
        return pose.effect_id == effect_id && pose.withdrawn == withdrawn;
      }));
}

}  // namespace

TEST_CASE("la course du Taekwon laisse ses traces, puis pose son arrêt", "[state_class]") {
  // Le serveur pose l'état AVANT d'annoncer la compétence, dans la même rafale :
  // l'état ne trouve rien, l'annonce pose les traces. Levé, l'état pose le
  // marqueur d'arrêt sur qui court encore et retire les traces ; l'annonce qui
  // suit en fait l'anneau de l'arrêt.
  Bench bench;
  REQUIRE(bench.world.SetStatusEffect(kBearerId, kRunning, true));
  RunFrame(bench, kPoseMs, true);
  std::vector<moonclient::EffectsInPlay::ActorEffectToRecord> poses =
      bench.effects.TakeActorEffects();
  CHECK(CountPosed(poses, kFootprints, false) == 1);
  CHECK(CountPosed(poses, kFootprints, true) == 0);
  CHECK(bench.Carries(kFootprints));
  CHECK_FALSE(bench.Carries(kStopMark));

  REQUIRE(bench.world.SetStatusEffect(kBearerId, kRunning, false));
  static_cast<void>(bench.effects.TakeLaunchedEffects());
  RunFrame(bench, kLaterMs, true);
  poses = bench.effects.TakeActorEffects();
  CHECK(CountPosed(poses, kFootprints, true) == 1);
  CHECK(CountPosed(poses, kFootprints, false) == 0);
  CHECK_FALSE(bench.Carries(kFootprints));
  CHECK(bench.Carries(kStopMark));
  const std::vector<moonclient::LaunchedEffect> launched = bench.effects.TakeLaunchedEffects();
  CHECK(std::ranges::any_of(launched, [](const moonclient::LaunchedEffect& effect) {
    return effect.effect_id == kStopRing;
  }));
}

TEST_CASE("le coureur qui heurte un mur tremble deux fois, au bruit du rebond", "[state_class]") {
  // Le serveur pose le rebond, lève la course, puis lève le rebond, dans la
  // même rafale : la liste des états n'en garde rien. Chaque paquet joue ses
  // gestes : deux secousses, deux bruits, les traces retirées, et pas
  // d'anneau d'arrêt, puisqu'il n'y a plus de traces quand la course tombe.
  Bench bench;
  REQUIRE(bench.world.SetStatusEffect(kBearerId, kRunning, true));
  RunFrame(bench, kPoseMs, true);
  static_cast<void>(bench.effects.TakeActorEffects());
  static_cast<void>(bench.effects.TakeSownNamedSounds());

  REQUIRE(bench.world.SetStatusEffect(kBearerId, kWallBounce, true));
  REQUIRE(bench.world.SetStatusEffect(kBearerId, kRunning, false));
  REQUIRE(bench.world.SetStatusEffect(kBearerId, kWallBounce, false));
  RunFrame(bench, kLaterMs, false);
  const std::vector<moonclient::EffectsInPlay::ActorEffectToRecord> poses =
      bench.effects.TakeActorEffects();
  CHECK(CountPosed(poses, kShaking, false) == 2);
  CHECK(CountPosed(poses, kFootprints, true) >= 1);
  CHECK(CountPosed(poses, kStopMark, false) == 0);
  CHECK_FALSE(bench.Carries(kFootprints));
  CHECK_FALSE(bench.Carries(kStopMark));
  const std::string_view bounce = ro::render::StateSoundFor(kWallBounce);
  REQUIRE_FALSE(bounce.empty());
  const std::vector<moonclient::SownNamedSound> sounds = bench.effects.TakeSownNamedSounds();
  CHECK(std::ranges::count_if(sounds, [bounce](const moonclient::SownNamedSound& sound) {
          return sound.sound == bounce;
        }) == 2);
}
