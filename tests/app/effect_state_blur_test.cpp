// LA TRAÎNÉE D'UN ÉTAT (effet 166) : Energy Coat la pose sur son porteur au
// lieu de chercher à la dessiner, sa levée l'en retire, et le corps qui la
// porte prend sa couleur et sème ses copies en bougeant.

#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/afterimage_blur.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
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
#include "world/map_stage.h"

using ro::test_support::BuildGnd;
using ro::test_support::BuildRsw;
using ro::test_support::EmptyClient;
using ro::test_support::GndBuildOptions;
using ro::test_support::RswBuildOptions;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kMageId = 150'101;
constexpr std::uint32_t kPoseMs = 10'000;
constexpr std::uint32_t kReleaseMs = 70'000;
constexpr ro::sim::StatusEffectId kEnergyCoat = 31;
constexpr std::uint32_t kTickMs = 16;

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

// Un corps qui a une image : une copie sans image n'en est pas une.
ro::render::ComposedActor OneQuadBody() {
  ro::render::ComposedActor body;
  body.quads.emplace_back();
  return body;
}

}  // namespace

TEST_CASE("Energy Coat pose sa traînée sur le mage, et sa levée l'en retire", "[state_blur]") {
  TempDirectory client;
  ro::vfs::Vfs vfs = EmptyClient(client);
  ro::render::EffectCatalog catalog(vfs);
  moonclient::EffectNodesInPlay nodes(vfs);
  moonclient::EffectsInPlay effects(catalog, nodes, vfs);
  ro::sim::World world;
  ro::sim::Actor mage;
  mage.id = kMageId;
  mage.kind = ro::sim::ActorKind::kPlayer;
  world.Spawn(mage);
  const moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  // Un premier passage sans état, pour que la pose soit vue comme telle.
  effects.NoticeStateEffects(world, stage, planner, kPoseMs);
  static_cast<void>(effects.TakeActorEffects());
  REQUIRE(world.SetStatusEffect(kMageId, kEnergyCoat, true));
  effects.NoticeStateEffects(world, stage, planner, kPoseMs);

  const std::vector<moonclient::EffectsInPlay::ActorEffectToRecord> poses =
      effects.TakeActorEffects();
  REQUIRE(poses.size() == 1);
  CHECK(poses[0].actor_id == kMageId);
  CHECK(poses[0].effect_id == ro::render::kAfterimageBlurEffect);
  CHECK(poses[0].posed_by_state == kEnergyCoat);
  CHECK_FALSE(poses[0].withdrawn);

  moonclient::SettleActorEffects(world, poses);
  const ro::sim::Actor& bearer = *world.Find(kMageId);
  REQUIRE(bearer.effects.size() == 1);
  CHECK(bearer.effects[0].id == ro::render::kAfterimageBlurEffect);
  CHECK(bearer.effects[0].posed_by_state == kEnergyCoat);

  REQUIRE(world.SetStatusEffect(kMageId, kEnergyCoat, false));
  effects.NoticeStateEffects(world, stage, planner, kReleaseMs);
  const std::vector<moonclient::EffectsInPlay::ActorEffectToRecord> levee =
      effects.TakeActorEffects();
  REQUIRE(levee.size() == 1);
  CHECK(levee[0].withdrawn);
  moonclient::SettleActorEffects(world, levee);
  CHECK(world.Find(kMageId)->effects.empty());
}

TEST_CASE("le retrait d'une traînée passe avant la pose d'une autre", "[state_blur]") {
  ro::sim::World world;
  ro::sim::Actor mage;
  mage.id = kMageId;
  world.Spawn(mage);
  constexpr ro::sim::StatusEffectId kOverthrust = 25;
  moonclient::SettleActorEffects(world, {{.actor_id = kMageId,
                                          .effect_id = ro::render::kAfterimageBlurEffect,
                                          .started_ms = kPoseMs,
                                          .posed_by_state = kOverthrust},
                                         {.actor_id = kMageId,
                                          .effect_id = ro::render::kAfterimageBlurEffect,
                                          .started_ms = kPoseMs,
                                          .posed_by_state = kEnergyCoat,
                                          .withdrawn = true}});
  const ro::sim::Actor& bearer = *world.Find(kMageId);
  REQUIRE(bearer.effects.size() == 1);
  CHECK(bearer.effects[0].posed_by_state == kOverthrust);
}

TEST_CASE("le porteur prend la couleur de l'état, et sème en marchant", "[state_blur]") {
  ro::sim::Actor mage;
  mage.id = kMageId;
  mage.effects.push_back(ro::sim::PlayedEffect{
      .id = ro::render::kAfterimageBlurEffect, .since_ms = kPoseMs, .posed_by_state = kEnergyCoat});
  moonclient::ActorGhosts ghosts;
  const std::optional<ro::render::AfterimageBlur> blur =
      ghosts.BlurOf(kMageId, mage, glm::vec3(1.0F, 2.0F, 3.0F), kPoseMs + 5 * kTickMs);
  REQUIRE(blur.has_value());
  CHECK(blur->color == std::array<float, 3>{180.0F, 180.0F, 250.0F});
  CHECK(blur->age_ticks == 5);

  const ro::render::ComposedActor body = OneQuadBody();
  const std::array<std::uint8_t, 4> ghost_tint = ro::render::AfterimageGhostTint(*blur);
  const std::uint32_t now_ms = kPoseMs + 5 * kTickMs;
  // Immobile, rien ne naît.
  ghosts.NoticeBlur(kMageId, *blur, ro::render::Motion::kIdle, body, glm::vec3(0.0F), ghost_tint,
                    now_ms);
  CHECK(ghosts.size() == 0);
  ghosts.NoticeBlur(kMageId, *blur, ro::render::Motion::kWalk, body, glm::vec3(0.0F), ghost_tint,
                    now_ms);
  REQUIRE(ghosts.size() == 1);
  // La même image, au même tic, ne sème pas deux fois.
  ghosts.NoticeBlur(kMageId, *blur, ro::render::Motion::kWalk, body, glm::vec3(0.0F), ghost_tint,
                    now_ms);
  CHECK(ghosts.size() == 1);

  std::vector<ro::render::DrawnActor> behind;
  ghosts.Drawn(now_ms, behind);
  REQUIRE(behind.size() == 1);
  CHECK(behind[0].tint == std::array<std::uint8_t, 4>{180, 180, 250, 180});
  behind.clear();
  ghosts.Drawn(now_ms + kTickMs, behind);
  REQUIRE(behind.size() == 1);
  CHECK(behind[0].tint[3] == 172);
  behind.clear();
  ghosts.Drawn(now_ms + ro::render::kAfterimageGhostLifeTicks * kTickMs, behind);
  CHECK(behind.empty());
}

TEST_CASE("sans état, la traînée garde la place où elle est née", "[state_blur]") {
  ro::sim::Actor mage;
  mage.id = kMageId;
  mage.effects.push_back(
      ro::sim::PlayedEffect{.id = ro::render::kAfterimageBlurEffect, .since_ms = kPoseMs});
  moonclient::ActorGhosts ghosts;
  const auto first = ghosts.BlurOf(kMageId, mage, glm::vec3(-37.6F, 12.0F, 300.9F), kPoseMs);
  REQUIRE(first.has_value());
  CHECK(first->color == std::array<float, 3>{-37.6F, 12.0F, 300.9F});
  // Le porteur a marché : la couleur reste celle de la naissance.
  const auto later = ghosts.BlurOf(kMageId, mage, glm::vec3(80.0F, 12.0F, 5.0F), kPoseMs + 400);
  REQUIRE(later.has_value());
  CHECK(later->color == first->color);
}
