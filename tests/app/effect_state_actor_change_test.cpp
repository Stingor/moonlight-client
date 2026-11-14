// LES EFFETS D'ACTEUR QU'UN ÉTAT POSE : ceux qui n'ont ni fichier, ni nœuds, ni
// classe, et qui ne font que changer leur porteur — un drapeau de rendu, une
// teinte, des copies. Ils vont sur l'acteur à la pose, en partent à la levée,
// et ceux dont l'instance meurt d'elle-même en partent à la fin de leur vie.

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_class.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_table.h"
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
#include "world/actor_ghosts.h"
#include "world/actor_view.h"
#include "world/map_stage.h"

using ro::test_support::BuildGnd;
using ro::test_support::BuildRsw;
using ro::test_support::EmptyClient;
using ro::test_support::GndBuildOptions;
using ro::test_support::RswBuildOptions;
using ro::test_support::TempDirectory;
using ActorEffectToRecord = moonclient::EffectsInPlay::ActorEffectToRecord;

namespace {

constexpr std::uint32_t kBearerId = 150'202;
constexpr std::uint32_t kPoseMs = 10'000;
constexpr std::uint32_t kReleaseMs = 20'000;
constexpr auto kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);

// Les états et les effets que les essais nomment, et ce qu'ils font.
constexpr ro::sim::StatusEffectId kAssumptio = 110;  // pose 375, deux copies qui respirent
constexpr std::uint32_t kBreathingGlowEffect = 375;
constexpr ro::sim::StatusEffectId kRedTintState = 107;  // pose 368, teinte rouge et addition
constexpr std::uint32_t kRedTintEffect = 368;
constexpr ro::sim::StatusEffectId kAuraBladeState = 103;  // pose 474, l'arme en addition
constexpr std::uint32_t kAuraBladeWeaponEffect = 474;
constexpr ro::sim::StatusEffectId kMagicPowerState = 113;  // pose 382, l'arme doublée
constexpr ro::sim::StatusEffectId kRedVeilState = 165;     // pose 544, voile rouge
constexpr std::uint32_t kRedVeilEffect = 544;
constexpr ro::sim::StatusEffectId kOffScreenState = 422;  // pose 823, corps hors de l'écran
constexpr std::uint32_t kOffScreenEffect = 823;
constexpr ro::sim::StatusEffectId kBunshinState = 207;  // pose 617, copies toutes les 20 tics
constexpr std::uint32_t kBunshinEffect = 617;
constexpr ro::sim::StatusEffectId kJitterState = 134;  // pose 426, quatorze tics de tremblement
constexpr std::uint32_t kJitterEffect = 426;
constexpr std::uint32_t kJitterLifeTicks = 14;
constexpr ro::sim::StatusEffectId kUndeadState = 97;  // 655 à la pose, 656 à la levée
constexpr std::uint32_t kUndeadEffect = 655;
constexpr std::uint32_t kUndeadFadeEffect = 656;

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

bool OnlyChangesBearer(std::uint32_t effect_id) {
  return ro::render::ActorEffectChangeFor(effect_id) != nullptr &&
         ro::render::EffectFile(effect_id).empty() &&
         !ro::render::KnowsProceduralEffect(effect_id) &&
         !ro::render::IsPortedEffectClass(effect_id);
}

// Un client vide, une carte plate, un porteur : ce que le chemin des états lit.
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
    static_cast<void>(effects.TakeActorEffects());
  }

  std::vector<ActorEffectToRecord> Set(ro::sim::StatusEffectId state, bool on,
                                       std::uint32_t now_ms) {
    REQUIRE(world.SetStatusEffect(kBearerId, state, on));
    // Les deux passages d'une image, dans son ordre : les états reçus en direct,
    // puis la liste de ceux qui tiennent.
    effects.NoticeStateChanges(world, stage, planner, now_ms);
    effects.NoticeStateEffects(world, stage, planner, now_ms);
    std::vector<ActorEffectToRecord> poses = effects.TakeActorEffects();
    moonclient::SettleActorEffects(world, poses);
    return poses;
  }

  std::vector<ro::render::ActorEffectPlay> Played(std::uint32_t now_ms) const {
    std::vector<ro::render::ActorEffectPlay> played;
    for (const ro::sim::PlayedEffect& effect : world.Find(kBearerId)->effects) {
      played.push_back({effect.id, (now_ms - effect.since_ms) / kTickMs});
    }
    return played;
  }

  bool Wears(std::uint32_t effect_id) const {
    for (const ro::sim::PlayedEffect& effect : world.Find(kBearerId)->effects) {
      if (effect.id == effect_id) {
        return true;
      }
    }
    return false;
  }
};

bool Queued(std::span<const ActorEffectToRecord> poses, std::uint32_t effect_id,
            ro::sim::StatusEffectId state, bool withdrawn) {
  for (const ActorEffectToRecord& pose : poses) {
    if (pose.effect_id == effect_id && pose.posed_by_state == state &&
        pose.withdrawn == withdrawn && pose.actor_id == kBearerId) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("chaque effet d'acteur que la pose d'un état fait naître va sur son porteur",
          "[state_actor_change]") {
  std::set<ro::sim::StatusEffectId> states;
  for (const ro::render::StateEffectEntry& entry : ro::render::StateEffectTable()) {
    if (entry.moment != ro::render::StateEffectMoment::kOnRelease &&
        OnlyChangesBearer(entry.effect_id)) {
      states.insert(entry.state_id);
    }
  }
  // Trente-six couples de la pose, sur vingt-cinq effets : si la table en perd,
  // l'essai ne mesure plus ce qu'il dit.
  REQUIRE(states.size() >= 30);
  for (const ro::sim::StatusEffectId state : states) {
    Bench bench;
    const std::vector<ActorEffectToRecord> poses = bench.Set(state, true, kPoseMs);
    for (const ro::render::StateEffectEntry& entry : ro::render::StateEffectsFor(state)) {
      if (entry.moment == ro::render::StateEffectMoment::kOnRelease ||
          !OnlyChangesBearer(entry.effect_id)) {
        continue;
      }
      INFO("état " << state << ", effet " << entry.effect_id);
      CHECK(Queued(poses, entry.effect_id, state, false));
      CHECK(bench.Wears(entry.effect_id));
    }
    const std::vector<ActorEffectToRecord> released = bench.Set(state, false, kReleaseMs);
    for (const ro::render::StateEffectEntry& removal : ro::render::StateRemovalsFor(state)) {
      if (!OnlyChangesBearer(removal.effect_id)) {
        continue;
      }
      INFO("état " << state << ", retrait de " << removal.effect_id);
      CHECK(Queued(released, removal.effect_id, state, true));
    }
  }
}

TEST_CASE("Assumptio lève le drapeau des deux copies qui respirent, et sa levée l'éteint",
          "[state_actor_change]") {
  Bench bench;
  static_cast<void>(bench.Set(kAssumptio, true, kPoseMs));
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> clones{};
  CHECK(ro::render::BodyCopiesAfterEffects(bench.Played(kPoseMs + kTickMs), clones) == 2);
  static_cast<void>(bench.Set(kAssumptio, false, kReleaseMs));
  CHECK_FALSE(bench.Wears(kBreathingGlowEffect));
  CHECK(ro::render::BodyCopiesAfterEffects(bench.Played(kReleaseMs), clones) == 0);
}

TEST_CASE("l'état 107 teint son porteur en rouge et le compose en addition",
          "[state_actor_change]") {
  Bench bench;
  static_cast<void>(bench.Set(kRedTintState, true, kPoseMs));
  const std::vector<ro::render::ActorEffectPlay> played = bench.Played(kPoseMs + kTickMs);
  constexpr std::array<std::uint8_t, 4> kWhite = {255, 255, 255, 255};
  // Rouge, vert, bleu, alpha : l'alpha reste celui du corps.
  CHECK(ro::render::TintAfterEffectsAt(kWhite, played) ==
        std::array<std::uint8_t, 4>{255, 100, 100, 255});
  // ⚠ LE CORPS, LA TÊTE, LES COIFFES ET LA CAPE s'ajoutent ; l'arme et le
  // bouclier recouvrent toujours.
  const ro::render::ActorBodyPose pose = ro::render::BodyPoseAfterEffects(played);
  CHECK(pose.additive_parts == ro::render::kBodyAndHeadwearParts);
  CHECK((pose.additive_parts & ro::render::PartBit(ro::render::ActorPart::kWeapon)) == 0);
  // Et c'est ce que le corps peint emporte.
  ro::render::DrawnActor body;
  moonclient::BlendLayers(pose, true, body);
  CHECK(body.additive_parts == ro::render::kBodyAndHeadwearParts);
  static_cast<void>(bench.Set(kRedTintState, false, kReleaseMs));
  CHECK_FALSE(bench.Wears(kRedTintEffect));
  CHECK(ro::render::TintAfterEffectsAt(kWhite, bench.Played(kReleaseMs)) == kWhite);
}

TEST_CASE("la lame d'aura ne rend additive que l'ARME de son porteur", "[state_actor_change]") {
  Bench bench;
  static_cast<void>(bench.Set(kAuraBladeState, true, kPoseMs));
  REQUIRE(bench.Wears(kAuraBladeWeaponEffect));
  const ro::render::ActorBodyPose pose =
      ro::render::BodyPoseAfterEffects(bench.Played(kPoseMs + kTickMs));
  ro::render::DrawnActor body;
  moonclient::BlendLayers(pose, true, body);
  CHECK(body.additive_parts == ro::render::PartBit(ro::render::ActorPart::kWeapon));
  CHECK_FALSE(body.doubles_weapon);
}

TEST_CASE("l'amplification magique double l'arme une image sur deux", "[state_actor_change]") {
  Bench bench;
  static_cast<void>(bench.Set(kMagicPowerState, true, kPoseMs));
  const ro::render::ActorBodyPose pose =
      ro::render::BodyPoseAfterEffects(bench.Played(kPoseMs + kTickMs));
  ro::render::DrawnActor body;
  moonclient::BlendLayers(pose, true, body);
  CHECK(body.doubles_weapon);
  moonclient::BlendLayers(pose, false, body);
  CHECK_FALSE(body.doubles_weapon);
}

TEST_CASE("une copie du corps ne reprend que ses calques", "[state_actor_change]") {
  ro::render::ActorBodyCopy copy;
  copy.parts = ro::render::kBodyAndHeadwearParts;
  copy.additive = true;
  ro::render::DrawnActor drawn;
  moonclient::CopyLayers(copy, drawn);
  CHECK(drawn.shown_parts == ro::render::kBodyAndHeadwearParts);
  CHECK(drawn.additive_parts == ro::render::kBodyAndHeadwearParts);
  copy.additive = false;
  moonclient::CopyLayers(copy, drawn);
  CHECK(drawn.additive_parts == 0);
}

TEST_CASE("l'état 165 pose le voile rouge, l'état 422 envoie le corps hors de l'écran",
          "[state_actor_change]") {
  Bench veiled;
  static_cast<void>(veiled.Set(kRedVeilState, true, kPoseMs));
  REQUIRE(veiled.Wears(kRedVeilEffect));
  std::array<ro::render::ActorBodyCopy, ro::render::kMaxBodyCopies> veils{};
  // Le voile teinté se soumet deux fois de suite.
  CHECK(ro::render::BodyVeilsAfterEffects(veiled.Played(kPoseMs + kTickMs), veils) == 2);

  Bench hidden;
  static_cast<void>(hidden.Set(kOffScreenState, true, kPoseMs));
  REQUIRE(hidden.Wears(kOffScreenEffect));
  CHECK(ro::render::BodyPoseAfterEffects(hidden.Played(kPoseMs + kTickMs)).hidden);
  static_cast<void>(hidden.Set(kOffScreenState, false, kReleaseMs));
  CHECK_FALSE(ro::render::BodyPoseAfterEffects(hidden.Played(kReleaseMs)).hidden);
}

TEST_CASE("l'état 207 sème une copie de son porteur tous les vingt tics", "[state_actor_change]") {
  Bench bench;
  static_cast<void>(bench.Set(kBunshinState, true, kPoseMs));
  REQUIRE(bench.Wears(kBunshinEffect));
  ro::render::ComposedActor body;
  body.quads.emplace_back();
  moonclient::ActorGhosts clones;
  constexpr std::array<std::uint8_t, 4> kTint = {155, 155, 255, 255};
  constexpr float kLifeMs = 500.0F;
  constexpr std::uint32_t kPeriodTicks = 20;
  for (std::uint32_t tick = 0; tick <= kPeriodTicks; ++tick) {
    const std::uint32_t now_ms = kPoseMs + tick * kTickMs;
    clones.Notice(kBearerId, bench.Played(now_ms), body, glm::vec3(0.0F), kTint, kLifeMs, now_ms);
  }
  CHECK(clones.size() == 2);
}

TEST_CASE("un effet d'acteur qui meurt de lui-même quitte son porteur, l'état tenant encore",
          "[state_actor_change]") {
  Bench bench;
  static_cast<void>(bench.Set(kJitterState, true, kPoseMs));
  REQUIRE(bench.Wears(kJitterEffect));
  // Encore en vie au dernier tic de sa vie…
  const std::uint32_t before_ms = kPoseMs + (kJitterLifeTicks - 1) * kTickMs;
  bench.effects.NoticeStateEffects(bench.world, bench.stage, bench.planner, before_ms);
  moonclient::SettleActorEffects(bench.world, bench.effects.TakeActorEffects());
  CHECK(bench.Wears(kJitterEffect));
  // … et parti au premier où il n'est plus.
  const std::uint32_t after_ms = kPoseMs + kJitterLifeTicks * kTickMs;
  bench.effects.NoticeStateEffects(bench.world, bench.stage, bench.planner, after_ms);
  moonclient::SettleActorEffects(bench.world, bench.effects.TakeActorEffects());
  CHECK_FALSE(bench.Wears(kJitterEffect));

  // Les copies rembobinent leur âge et ne meurent pas d'elles-mêmes.
  Bench clones;
  static_cast<void>(clones.Set(kBunshinState, true, kPoseMs));
  constexpr std::uint32_t kLongAfterMs = kPoseMs + 20'000 * kTickMs;
  clones.effects.NoticeStateEffects(clones.world, clones.stage, clones.planner, kLongAfterMs);
  moonclient::SettleActorEffects(clones.world, clones.effects.TakeActorEffects());
  CHECK(clones.Wears(kBunshinEffect));
}

TEST_CASE("la levée de l'état 97 retire son contour et pose son fondu", "[state_actor_change]") {
  Bench bench;
  static_cast<void>(bench.Set(kUndeadState, true, kPoseMs));
  REQUIRE(bench.Wears(kUndeadEffect));
  const std::vector<ActorEffectToRecord> released = bench.Set(kUndeadState, false, kReleaseMs);
  CHECK(Queued(released, kUndeadEffect, kUndeadState, true));
  CHECK(Queued(released, kUndeadFadeEffect, kUndeadState, false));
  CHECK_FALSE(bench.Wears(kUndeadEffect));
  CHECK(bench.Wears(kUndeadFadeEffect));
}

TEST_CASE(
    "l'état 1192 pose l'ombre 1004 sur son porteur, grise et translucide, une case derrière "
    "lui, et la levée l'emporte",
    "[state_actor_change]") {
  constexpr ro::sim::StatusEffectId kShadowExceedState = 1192;
  constexpr std::uint32_t kKagemushaShadow = 1004;
  constexpr float kMargin = 1e-3F;
  Bench bench;
  const std::vector<ActorEffectToRecord> posed = bench.Set(kShadowExceedState, true, kPoseMs);
  CHECK(Queued(posed, kKagemushaShadow, kShadowExceedState, false));
  REQUIRE(bench.Wears(kKagemushaShadow));
  // La table : le porteur garde sa couleur ; la copie prend la sienne, chaque
  // tic, cinq unités derrière lui.
  const ro::render::ActorEffectChange* change = ro::render::ActorEffectChangeFor(kKagemushaShadow);
  REQUIRE(change != nullptr);
  CHECK_FALSE(ro::render::TintsActor(*change));
  CHECK(change->clone_tint == std::array<std::int16_t, 4>{100, 192, 192, 192});
  CHECK(change->clone_behind_units == 5.0F);
  CHECK(change->clone_period_ticks == 1);
  // Un porteur qui montre la direction zéro du serveur a le corps à cent
  // quatre-vingts degrés : sa copie naît cinq unités plus loin en profondeur,
  // sa teinte multipliée — 200 × 192 / 255 = 150 en gris, 255 × 100 / 255 en
  // alpha.
  const ro::sim::Actor* bearer = bench.world.Find(kBearerId);
  REQUIRE(bearer != nullptr);
  moonclient::ActorGhosts ghosts;
  static_cast<void>(ghosts.BlurOf(kBearerId, *bearer, glm::vec3(0.0F), kPoseMs));
  ro::render::ComposedActor body;
  body.quads.emplace_back();
  constexpr std::array<std::uint8_t, 4> kLitBearer = {200, 200, 200, 255};
  constexpr glm::vec3 kFeet{10.0F, 0.0F, 20.0F};
  constexpr float kBehind = 5.0F;
  constexpr float kLifeMs = 500.0F;
  ghosts.Notice(kBearerId, bench.Played(kPoseMs), body, kFeet, kLitBearer, kLifeMs, kPoseMs);
  REQUIRE(ghosts.size() == 1);
  std::vector<ro::render::DrawnActor> behind;
  ghosts.Drawn(kPoseMs, behind);
  REQUIRE(behind.size() == 1);
  CHECK(behind.front().tint == std::array<std::uint8_t, 4>{150, 150, 150, 100});
  CHECK(behind.front().ground_position.x == Catch::Approx(kFeet.x).margin(kMargin));
  CHECK(behind.front().ground_position.z == Catch::Approx(kFeet.z + kBehind).margin(kMargin));
  // Une copie par tic : le tic suivant en sème une autre.
  ghosts.Notice(kBearerId, bench.Played(kPoseMs + kTickMs), body, kFeet, kLitBearer, kLifeMs,
                kPoseMs + kTickMs);
  CHECK(ghosts.size() == 2);
  // La levée retire l'ombre par son numéro.
  const std::vector<ActorEffectToRecord> released =
      bench.Set(kShadowExceedState, false, kReleaseMs);
  CHECK(Queued(released, kKagemushaShadow, kShadowExceedState, true));
  CHECK_FALSE(bench.Wears(kKagemushaShadow));
}
