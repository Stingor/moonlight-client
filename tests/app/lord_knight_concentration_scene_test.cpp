// CONCENTRATION DU LORD KNIGHT, de la trame reçue à la couleur du corps.
//
// Le serveur pose l'état 105 puis annonce la compétence 357 sans dégâts, dans
// cet ordre et au même instant. La suite de 357 retire 369 de son porteur puis
// l'y repose ; l'état le pose aussi. Le corps doit en sortir jaune pâle
// (255, 255, 160), et le rester tant que l'état tient.
//
// Les notices tournent dans l'ordre d'une image de jeu, et les effets d'acteur
// se règlent en fin d'image : c'est la boucle, et non les fonctions une à une,
// qui est sous essai.

#include <array>
#include <cstdint>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
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
#include "support/net_frames.h"
#include "support/temp_directory.h"
#include "world/actor_tint.h"
#include "world/map_stage.h"

using ro::test_support::BuildGnd;
using ro::test_support::BuildRsw;
using ro::test_support::EmptyClient;
using ro::test_support::GndBuildOptions;
using ro::test_support::RswBuildOptions;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kKnightId = 150'357;
constexpr std::uint32_t kStartMs = 10'000;
constexpr std::uint32_t kPoseMs = 10'480;
// Une image à soixante par seconde, arrondie.
constexpr std::uint32_t kFrameMs = 16;
constexpr int kFramesWatched = 120;

constexpr std::int16_t kLordKnightConcentration = 105;
constexpr std::uint16_t kConcentrationSkill = 357;
constexpr std::uint32_t kConcentrationEffect = 369;
constexpr std::int32_t kSkillLevel = 5;
constexpr std::uint32_t kStateLeftMs = 60'000;

// Rouge, vert, bleu, alpha : ce que le quad du corps reçoit. En entiers, pour
// qu'un échec s'affiche en nombres et non en caractères.
constexpr std::array<int, 4> kConcentrationPale = {255, 255, 160, 255};
constexpr std::array<int, 4> kConcentrationPaleHalfLit = {127, 127, 80, 255};

[[nodiscard]] std::array<int, 4> Channels(const std::array<std::uint8_t, 4>& tint) {
  return {tint[0], tint[1], tint[2], tint[3]};
}

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

// Le monde, le réseau et les effets, reliés comme dans une image de jeu.
struct Game {
  TempDirectory client;
  ro::vfs::Vfs vfs = EmptyClient(client);
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  Game() {
    ro::sim::Actor knight;
    knight.id = kKnightId;
    knight.kind = ro::sim::ActorKind::kPlayer;
    knight.position = ro::sim::CellPosition{5, 5, 0};
    world.Spawn(knight);
    world.set_player_id(kKnightId);
    RunFrame(kStartMs);
  }

  void Receive(const ro::net::Frame& frame, std::uint32_t now_ms) {
    REQUIRE(ro::net::ApplyFrame(frame, world, now_ms) == ro::net::FrameOutcome::kApplied);
  }

  // Ce que le serveur envoie pour Concentration : l'état, puis la compétence.
  void ReceiveConcentration(std::uint32_t now_ms) {
    ro::proto::PacketStatusChange state;
    state.index = kLordKnightConcentration;
    state.AID = kKnightId;
    state.state = 1;
    state.Total = kStateLeftMs;
    state.Left = kStateLeftMs;
    state.val1 = kSkillLevel;
    Receive(ro::test_support::FrameOf(ro::proto::Encode(state)), now_ms);
    ReceiveSkillOnly(now_ms);
  }

  void ReceiveSkillOnly(std::uint32_t now_ms) {
    ro::proto::ZcUseSkill skill;
    skill.SKID = kConcentrationSkill;
    skill.level = kSkillLevel;
    skill.targetAID = kKnightId;
    skill.srcAID = kKnightId;
    skill.result = 1;
    Receive(ro::test_support::FrameOf(ro::proto::Encode(skill)), now_ms);
  }

  // Une image : les notices dans l'ordre du jeu, puis les effets d'acteur
  // réglés sur le monde.
  void RunFrame(std::uint32_t now_ms) {
    effects.NoticeEffectRemovals(world);
    effects.Notice(world, stage, planner);
    effects.NoticeSkills(world, stage, planner, now_ms);
    effects.NoticeStateEffects(world, stage, planner, now_ms);
    moonclient::SettleActorEffects(world, effects.TakeActorEffects());
  }

  [[nodiscard]] const ro::sim::Actor& Knight() const {
    const ro::sim::Actor* knight = world.Find(kKnightId);
    REQUIRE(knight != nullptr);
    return *knight;
  }

  [[nodiscard]] bool Wears(std::uint32_t effect_id) const {
    for (const ro::sim::PlayedEffect& effect : Knight().effects) {
      if (effect.id == effect_id) {
        return true;
      }
    }
    return false;
  }
};

}  // namespace

TEST_CASE("Concentration : de la trame d'état au quad du corps, jaune pâle tant que l'état tient",
          "[lord_knight][body_tint][scene]") {
  Game game;
  game.ReceiveConcentration(kPoseMs);
  std::uint32_t now_ms = kPoseMs;
  for (int frame = 0; frame < kFramesWatched; ++frame, now_ms += kFrameMs) {
    game.RunFrame(now_ms);
  }
  CHECK(game.Wears(kConcentrationEffect));
  CHECK(Channels(moonclient::BodyTintOf(game.Knight(), now_ms, {})) == kConcentrationPale);
}

TEST_CASE("Concentration : la compétence seule repose 369, que son propre retrait ne coupe pas",
          "[lord_knight][body_tint][scene]") {
  Game game;
  game.ReceiveSkillOnly(kPoseMs);
  std::uint32_t now_ms = kPoseMs;
  for (int frame = 0; frame < kFramesWatched; ++frame, now_ms += kFrameMs) {
    game.RunFrame(now_ms);
  }
  CHECK(game.Wears(kConcentrationEffect));
  CHECK(Channels(moonclient::BodyTintOf(game.Knight(), now_ms, {})) == kConcentrationPale);
  // Un retrait réglé par les effets n'est pas un retrait du serveur : il ne
  // s'inscrit pas au journal que les effets relisent pour couper ce qui joue.
  CHECK(game.Knight().removed_effects.empty());
}

TEST_CASE("Concentration : la lumière de la case multiplie le jaune pâle, elle ne l'efface pas",
          "[lord_knight][body_tint][scene]") {
  Game game;
  game.ReceiveConcentration(kPoseMs);
  std::uint32_t now_ms = kPoseMs;
  for (int frame = 0; frame < kFramesWatched; ++frame, now_ms += kFrameMs) {
    game.RunFrame(now_ms);
  }
  constexpr std::array<float, 3> kHalfLight = {0.5F, 0.5F, 0.5F};
  const std::array<std::uint8_t, 4> lit =
      moonclient::BodyTintOf(game.Knight(), now_ms, {.ground_light = kHalfLight});
  CHECK(Channels(lit) == kConcentrationPaleHalfLit);
}
