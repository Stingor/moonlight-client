// LA COULEUR DU CORPS D'UN LORD KNIGHT, depuis les états que le serveur pose.
//
// · Two-Hand Quicken (état 2) pose la traînée 166, qui réécrit le corps en
//   jaune (200, 200, 0) à chaque tic ; Concentration (état 105) pose 369, qui
//   le réécrit en jaune pâle (255, 255, 160). Le client met ses effets à jour
//   dans l'ordre de leur naissance : le dernier posé l'emporte, à chaque tic.
//   La traînée passait par-dessus tout, et la Concentration d'un chevalier sous
//   Two-Hand Quicken — son cas ordinaire — ne se voyait jamais.
// · Au-delà de soixante images par seconde, une image sur deux ne doit aucun
//   tic : l'effet n'y réécrit rien, et l'acteur y regagne huit par composante.
//   Le jaune remonte vers le blanc entre deux tics, puis y retombe : la couleur
//   PAPILLOTE.
//
// La teinte se compose ici comme la scène la compose : les effets de l'acteur
// à leur âge, l'écriture de la traînée à son rang, et les images sans tic.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/afterimage_blur.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_frame.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/render/tint_recovery_clock.h"
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

constexpr std::uint32_t kKnightId = 150'401;
constexpr std::uint32_t kStartMs = 10'000;
constexpr std::uint32_t kFirstPoseMs = 10'480;
constexpr std::uint32_t kSecondPoseMs = 11'008;
constexpr std::uint32_t kLaterMs = 20'000;
constexpr auto kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
// Une image à cent quarante-quatre par seconde, arrondie.
constexpr std::uint32_t kFastFrameMs = 7;
constexpr int kFramesWatched = 12;

constexpr ro::sim::StatusEffectId kTwoHandQuicken = 2;
constexpr ro::sim::StatusEffectId kLordKnightConcentration = 105;

// Rouge, vert, bleu, alpha : ce que la scène donne au corps.
constexpr std::array<std::uint8_t, 4> kWhite = {255, 255, 255, 255};
constexpr std::array<std::uint8_t, 4> kQuickenYellow = {200, 200, 0, 255};
constexpr std::array<std::uint8_t, 4> kConcentrationPale = {255, 255, 160, 255};
// Le jaune après une image sans tic : huit de plus par composante.
constexpr std::array<std::uint8_t, 4> kQuickenYellowRecovered = {208, 208, 8, 255};
constexpr std::uint8_t kRecoveryPerFrame = 8;

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

// Le chemin des états et la teinte du corps, reliés comme dans le jeu.
struct Scene {
  TempDirectory client;
  ro::vfs::Vfs vfs = EmptyClient(client);
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};
  moonclient::ActorGhosts ghosts;
  ro::render::TintRecoveryClock clock;

  Scene() {
    ro::sim::Actor knight;
    knight.id = kKnightId;
    knight.kind = ro::sim::ActorKind::kPlayer;
    knight.position = ro::sim::CellPosition{5, 5, 0};
    world.Spawn(knight);
    world.set_player_id(kKnightId);
    effects.NoticeStateEffects(world, stage, planner, kStartMs);
    moonclient::SettleActorEffects(world, effects.TakeActorEffects());
  }

  // Le serveur pose un état sur NOTRE joueur ; l'image qui suit le remarque.
  void Pose(ro::sim::StatusEffectId state, std::uint32_t now_ms) {
    REQUIRE(world.SetStatusEffect(kKnightId, state, true));
    effects.NoticeStateEffects(world, stage, planner, now_ms);
    moonclient::SettleActorEffects(world, effects.TakeActorEffects());
  }

  // La teinte du corps à cette image.
  std::array<std::uint8_t, 4> BodyTintAt(std::uint32_t now_ms) {
    clock.NoteFrameAt(now_ms);
    const ro::sim::Actor* knight = world.Find(kKnightId);
    REQUIRE(knight != nullptr);
    std::vector<ro::render::ActorEffectPlay> played;
    for (const ro::sim::PlayedEffect& effect : knight->effects) {
      played.push_back({effect.id, (now_ms - effect.since_ms) / kTickMs});
    }
    std::optional<ro::render::TintEveryTickWrite> blur_write;
    if (const std::optional<ro::render::AfterimageBlur> blur =
            ghosts.BlurOf(kKnightId, *knight, glm::vec3(0.0F), now_ms)) {
      blur_write = moonclient::BlurTintWriteOf(*knight, *blur, 0);
    }
    return ro::render::TintAfterEffectsWith(
        kWhite, played,
        ro::render::TintContext{.every_tick_write = blur_write,
                                .idle_frames_since = [this](std::uint32_t ticks_since) {
                                  return clock.IdleFramesSince(ticks_since);
                                }});
  }
};

}  // namespace

TEST_CASE("Lord Knight : sous Two-Hand Quicken, la Concentration teint le corps",
          "[lord_knight][body_tint]") {
  Scene scene;
  scene.Pose(kTwoHandQuicken, kFirstPoseMs);
  CHECK(scene.BodyTintAt(kFirstPoseMs + kTickMs) == kQuickenYellow);
  // Le dernier posé l'emporte, et tout le temps du buff.
  scene.Pose(kLordKnightConcentration, kSecondPoseMs);
  CHECK(scene.BodyTintAt(kSecondPoseMs + kTickMs) == kConcentrationPale);
  CHECK(scene.BodyTintAt(kLaterMs) == kConcentrationPale);
}

TEST_CASE("Lord Knight : Two-Hand Quicken posé APRÈS la Concentration recouvre sa teinte",
          "[lord_knight][body_tint]") {
  Scene scene;
  scene.Pose(kLordKnightConcentration, kFirstPoseMs);
  CHECK(scene.BodyTintAt(kFirstPoseMs + kTickMs) == kConcentrationPale);
  scene.Pose(kTwoHandQuicken, kSecondPoseMs);
  CHECK(scene.BodyTintAt(kLaterMs) == kQuickenYellow);
}

TEST_CASE("Two-Hand Quicken : au-delà de soixante images par seconde, le jaune papillote",
          "[lord_knight][body_tint]") {
  Scene scene;
  scene.Pose(kTwoHandQuicken, kFirstPoseMs);
  // À soixante images par seconde, chaque image doit son tic : le jaune tient.
  std::uint32_t now_ms = kLaterMs;
  for (int frame = 0; frame < kFramesWatched; ++frame, now_ms += kTickMs) {
    CHECK(scene.BodyTintAt(now_ms) == kQuickenYellow);
  }
  // Plus vite, les images sans tic laissent la couleur remonter de huit, puis
  // le tic suivant la ramène au jaune.
  std::set<std::array<std::uint8_t, 4>> seen;
  for (int frame = 0; frame < kFramesWatched; ++frame, now_ms += kFastFrameMs) {
    const std::array<std::uint8_t, 4> tint = scene.BodyTintAt(now_ms);
    seen.insert(tint);
    CHECK(tint[2] % kRecoveryPerFrame == 0);
    CHECK(tint[0] - kQuickenYellow[0] == tint[2]);
  }
  CHECK(seen.contains(kQuickenYellow));
  CHECK(seen.contains(kQuickenYellowRecovered));
}
