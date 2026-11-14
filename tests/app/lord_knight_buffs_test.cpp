// DEUX ÉCARTS DU LORD KNIGHT, relevés en côte à côte avec le client natif.
//
// · CONCENTRATION (état 105) pose l'effet 369 : son cas fait défiler
//   `TwoHand.str` une fois ET réécrit à chaque tic la couleur de son porteur —
//   blanc, blanc, cent soixante de bleu, un jaune pâle — tant que l'instance vit.
//   L'effet ayant un fichier, rien ne le mettait sur l'acteur : la teinte
//   manquait tout le temps du buff.
// · PARRYING pose l'effet 336, le bouclier de Kyrie : son mouvement relit
//   l'angle du CORPS de son porteur à chaque tic, et le bouclier lui fait face.

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_sphere_shell.h"
#include "ro/render/effect_sphere_shell_seed.h"
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
#include "support/effect_bearers.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

using Catch::Approx;
using ro::test_support::BuildGnd;
using ro::test_support::BuildRsw;
using ro::test_support::EmptyClient;
using ro::test_support::GndBuildOptions;
using ro::test_support::RswBuildOptions;
using ro::test_support::TempDirectory;
using ActorEffectToRecord = moonclient::EffectsInPlay::ActorEffectToRecord;

namespace {

constexpr std::uint32_t kBearerId = 150'357;
constexpr std::uint32_t kPoseMs = 10'000;
constexpr std::uint32_t kLaterMs = 25'000;
constexpr std::uint32_t kReleaseMs = 40'000;
constexpr auto kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);

constexpr ro::sim::StatusEffectId kLordKnightConcentration = 105;
constexpr std::uint32_t kConcentrationEffect = 369;
constexpr std::uint32_t kKyrieGuardEffect = 336;

// La teinte que le cas de 369 réécrit, dans l'ordre du client : alpha, rouge,
// vert, bleu — l'alpha laissé tel quel.
constexpr std::array<std::int16_t, 4> kPaleYellowTint = {ro::render::kTintUnchanged, 255, 255, 160};
constexpr std::array<std::uint8_t, 4> kWhite = {255, 255, 255, 255};

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
    effects.NoticeStateEffects(world, stage, planner, now_ms);
    std::vector<ActorEffectToRecord> poses = effects.TakeActorEffects();
    moonclient::SettleActorEffects(world, poses);
    return poses;
  }

  [[nodiscard]] std::vector<ro::render::ActorEffectPlay> Played(std::uint32_t now_ms) const {
    std::vector<ro::render::ActorEffectPlay> played;
    for (const ro::sim::PlayedEffect& effect : world.Find(kBearerId)->effects) {
      played.push_back({effect.id, (now_ms - effect.since_ms) / kTickMs});
    }
    return played;
  }
};

bool Queued(std::span<const ActorEffectToRecord> poses, std::uint32_t effect_id, bool withdrawn) {
  for (const ActorEffectToRecord& pose : poses) {
    if (pose.effect_id == effect_id && pose.withdrawn == withdrawn && pose.actor_id == kBearerId) {
      return true;
    }
  }
  return false;
}

// Le bouclier d'un porteur dont le client tient le corps à `body_degrees`,
// après un tic de mouvement : le milieu de ses panneaux, depuis son centre.
glm::vec2 ShieldDirection(float body_degrees) {
  const std::span<const ro::render::EffectSphereShellPattern> patterns =
      ro::render::EffectSphereShellPatternsFor(kKyrieGuardEffect);
  REQUIRE(patterns.size() == 1);
  const glm::vec3 feet{100.0F, 0.0F, 200.0F};
  ro::render::EffectNodePool pool;
  pool.AnchorNextTo(kBearerId, feet);
  pool.SowFor(kKyrieGuardEffect);
  pool.Add(ro::render::SowEffectSphereShellNode(patterns[0], feet, 1000));
  ro::test_support::MoveBearer(pool, kBearerId, feet, body_degrees);
  static_cast<void>(pool.Advance(kTickMs));
  REQUIRE(pool.nodes().size() == 1);
  const ro::render::PooledEffectNode& node = pool.nodes()[0];
  glm::vec3 sum{0.0F, 0.0F, 0.0F};
  std::size_t corners = 0;
  for (const ro::render::EffectWorldQuad& quad :
       ro::render::ComposeSphereShellQuads(node.tracks[0], node.ground_position)) {
    for (const glm::vec3& corner : quad.corners) {
      sum += corner - node.ground_position;
      ++corners;
    }
  }
  REQUIRE(corners > 0);
  return glm::normalize(glm::vec2{sum.x, sum.z});
}

}  // namespace

TEST_CASE("Concentration du Lord Knight : 369 teint son porteur en jaune pâle",
          "[lord_knight][actor_change]") {
  const ro::render::ActorEffectChange* change =
      ro::render::ActorEffectChangeFor(kConcentrationEffect);
  REQUIRE(change != nullptr);
  CHECK(change->tint == kPaleYellowTint);
  // ⚠ SANS BORNE : le cas réécrit la couleur à chaque tic de sa vie, et c'est
  // la levée de l'état qui retire l'instance.
  CHECK(change->life_ticks == 0);
}

TEST_CASE("Concentration du Lord Knight : la pose met 369 sur le porteur, la levée l'en retire",
          "[lord_knight][actor_change]") {
  Bench bench;
  const std::vector<ActorEffectToRecord> posed = bench.Set(kLordKnightConcentration, true, kPoseMs);
  CHECK(Queued(posed, kConcentrationEffect, false));

  // Quinze secondes plus tard, le corps est toujours teint.
  const std::array<std::uint8_t, 4> tinted =
      ro::render::TintAfterEffectsAt(kWhite, bench.Played(kLaterMs), {});
  CHECK(tinted[0] == 255);
  CHECK(tinted[1] == 255);
  CHECK(tinted[2] == 160);
  CHECK(tinted[3] == 255);

  const std::vector<ActorEffectToRecord> released =
      bench.Set(kLordKnightConcentration, false, kReleaseMs);
  CHECK(Queued(released, kConcentrationEffect, true));
  const std::array<std::uint8_t, 4> after =
      ro::render::TintAfterEffectsAt(kWhite, bench.Played(kReleaseMs), {});
  CHECK(after == kWhite);
}

TEST_CASE("Parrying : le bouclier fait face au corps de son porteur, et le suit",
          "[lord_knight][shell]") {
  // L'angle du CLIENT : `atan2(Δx, −Δz)`, le sud à zéro, l'est à quatre-vingt-dix.
  // Dans notre repère, dont la profondeur est l'opposée, il regarde vers
  // (sin θ, cos θ) en x et z.
  for (const float body_degrees : {90.0F, 135.0F, 180.0F, 270.0F, 405.0F}) {
    INFO("angle du corps " << body_degrees);
    const float radians = body_degrees * std::numbers::pi_v<float> / 180.0F;
    const glm::vec2 facing{std::sin(radians), std::cos(radians)};
    const glm::vec2 shield = ShieldDirection(body_degrees);
    CHECK(shield.x == Approx(facing.x).margin(1e-3));
    CHECK(shield.y == Approx(facing.y).margin(1e-3));
  }
}
