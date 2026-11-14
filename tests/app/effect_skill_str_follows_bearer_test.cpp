// UN FICHIER D'EFFET QU'UNE COMPÉTENCE POSE SUR UN ACTEUR SE REPLACE SUR LUI à
// chaque image : la flamme de Provoke accompagne le monstre qui marche, au lieu
// de rester plantée où elle est née. Et il VIT avec lui : un effet primitif
// meurt quand son porteur quitte le monde, un fichier qui boucle en vivant avec
// son porteur s'éteint avec lui au lieu de boucler sans fin.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/formats/str.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/render/str_effect_setup.h"
#include "ro/sim/actor.h"
#include "ro/sim/movement.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/effect_builder.h"
#include "support/grf_builder.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsw;
using ro::test_support::BuildStr;
using ro::test_support::GndBuildOptions;
using ro::test_support::RswBuildOptions;
using ro::test_support::StrBuildOptions;
using ro::test_support::StrKeyFrameSpec;
using ro::test_support::StrLayerSpec;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kCasterId = 150'011;
constexpr std::uint32_t kTargetId = 150'012;
constexpr std::uint32_t kNowMs = 10'000;
// Un tic plus tard : le fichier est en cours, et la cible a bougé.
constexpr std::uint32_t kLaterMs = kNowMs + 16;
// SM_PROVOKE : son annonce joue Provoke.str, effet PRIMITIF, sur la cible.
constexpr std::uint16_t kProvokeSkill = 6;
constexpr std::uint32_t kProvokeEffect = 67;
constexpr std::int32_t kSkillLevel = 1;
constexpr const char* kProvokeFile = "data\\texture\\effect\\Provoke.str";
// L'annonce de 5246 pose 1975 sur le lanceur, une seconde plus tard : un
// fichier du lecteur de `.str` qui BOUCLE tant que son porteur est là.
constexpr std::uint16_t kFaithPowerSkill = 5246;
constexpr std::uint32_t kFaithPowerEffect = 1975;
constexpr const char* kFaithPowerFile =
    "data\\texture\\effect\\4ig_firstfaithpower\\firstfaithpower\\firstfaithpower.str";
constexpr std::uint32_t kFaithPowerDelayMs = 1'000;
// Bien après la fin de son fichier de dix images : il ne se voit encore que
// parce qu'il boucle.
constexpr std::uint32_t kLongAfterMs = kNowMs + kFaithPowerDelayMs + 2'000;
// Un identifiant que le gestionnaire confie à une classe, sans la lier à la vie
// de son porteur.
constexpr std::uint32_t kClassEffectLivingAlone = 204;
// Assez d'images pour que Provoke vive encore au tic suivant ; dix seulement
// pour le fichier qui boucle.
constexpr std::int32_t kLongStrLastFrame = 60;
constexpr std::int32_t kShortStrLastFrame = 10;
constexpr ro::sim::CellPosition kTargetBirthCell{.x = 1, .y = 1};
constexpr ro::sim::CellPosition kTargetWalkedCell{.x = 3, .y = 2};
constexpr std::int32_t kGroundCells = 4;

moonclient::MapStage FlatStage() {
  GndBuildOptions sol;
  sol.width = kGroundCells;
  sol.height = kGroundCells;
  auto gnd = ro::formats::GndMap::Parse(BuildGnd(sol));
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

ro::vfs::Vfs ClientWithFile(const TempDirectory& client, const std::string& path,
                            std::int32_t last_frame) {
  StrKeyFrameSpec image;
  image.offset_x = ro::formats::kStrOriginX;
  image.offset_y = ro::formats::kStrOriginY;
  StrLayerSpec layer;
  layer.key_frames = {image};
  StrBuildOptions file;
  file.last_frame = last_frame;
  file.layers = {layer};
  client.WriteFile("effet.grf", BuildGrf({{.name = path, .content = BuildStr(file)}}));
  auto rise = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effet.grf"}});
  REQUIRE(rise.has_value());
  return std::move(*rise);
}

// Un lanceur et une cible sur une carte plate, et un client qui ne porte que le
// fichier demandé.
struct Bench {
  Bench(const std::string& path, std::int32_t last_frame)
      : vfs(ClientWithFile(client, path, last_frame)) {
    for (const std::uint32_t id : {kCasterId, kTargetId}) {
      ro::sim::Actor actor;
      actor.id = id;
      actor.kind = id == kTargetId ? ro::sim::ActorKind::kMonster : ro::sim::ActorKind::kPlayer;
      actor.position = kTargetBirthCell;
      world.Spawn(actor);
    }
  }

  // Tout ce qui se peint à cet instant, quelle que soit sa file : devant les
  // corps, derrière eux, ou par-dessus tout.
  std::size_t PaintedAt(std::uint32_t now_ms) {
    const std::size_t devant = effects.Drawn(now_ms, stage, world, planner).size();
    return devant + effects.DrawnBehindActors().size() + effects.DrawnOverEverything().size();
  }

  TempDirectory client;
  ro::vfs::Vfs vfs;
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};
};

}  // namespace

TEST_CASE("le fichier de Provoke suit la cible qui marche", "[effect_skill_str_follows]") {
  Bench bench(kProvokeFile, kLongStrLastFrame);
  bench.effects.PlaySkillOn(bench.world, bench.stage, bench.planner, *bench.world.Find(kTargetId),
                            kProvokeSkill, kCasterId, kNowMs, kSkillLevel,
                            moonclient::SkillMoment::kAnnounced, kSkillLevel);
  REQUIRE(bench.effects.playing() == 1);

  REQUIRE(bench.world.PlaceAt(kTargetId, kTargetWalkedCell));
  const std::span<const ro::render::DrawnEffect> peints =
      bench.effects.Drawn(kLaterMs, bench.stage, bench.world, bench.planner);
  REQUIRE(peints.size() == 1);
  const ro::sim::Actor& target = *bench.world.Find(kTargetId);
  const ro::sim::FractionalCell la =
      ro::sim::InterpolatedCell(target, bench.planner.PathFor(target), kLaterMs);
  const std::array<float, 3> here = moonclient::GroundPositionAt(bench.stage, la.x, la.y);
  const std::array<float, 3> birth = moonclient::GroundPositionAt(
      bench.stage, static_cast<float>(kTargetBirthCell.x), static_cast<float>(kTargetBirthCell.y));
  REQUIRE(here != birth);
  CHECK(peints[0].ground_position.x == here[0]);
  CHECK(peints[0].ground_position.z == here[2]);
}

TEST_CASE("le fichier de Provoke meurt avec la cible qui quitte le monde",
          "[effect_skill_str_follows]") {
  Bench bench(kProvokeFile, kLongStrLastFrame);
  bench.effects.PlaySkillOn(bench.world, bench.stage, bench.planner, *bench.world.Find(kTargetId),
                            kProvokeSkill, kCasterId, kNowMs, kSkillLevel,
                            moonclient::SkillMoment::kAnnounced, kSkillLevel);
  REQUIRE(bench.effects.Drawn(kLaterMs, bench.stage, bench.world, bench.planner).size() == 1);

  REQUIRE(bench.world.Remove(kTargetId));
  CHECK(bench.effects.Drawn(kLaterMs + 16, bench.stage, bench.world, bench.planner).empty());
  CHECK(bench.effects.playing() == 0);
}

TEST_CASE("un fichier de suite qui boucle avec son porteur s'éteint avec lui",
          "[effect_skill_str_follows]") {
  Bench bench(kFaithPowerFile, kShortStrLastFrame);
  bench.effects.PlaySkillOn(bench.world, bench.stage, bench.planner, *bench.world.Find(kTargetId),
                            kFaithPowerSkill, kCasterId, kNowMs, kSkillLevel,
                            moonclient::SkillMoment::kAnnounced, kSkillLevel);
  REQUIRE(bench.effects.playing() == 1);

  // Il boucle tant que le lanceur est là…
  CHECK(bench.PaintedAt(kLongAfterMs) == 1);
  // … et s'éteint avec lui, au lieu de boucler jusqu'au changement de carte.
  REQUIRE(bench.world.Remove(kCasterId));
  CHECK(bench.PaintedAt(kLongAfterMs + 16) == 0);
  CHECK(bench.effects.playing() == 0);
}

TEST_CASE("qui meurt avec son porteur : le primitif, et le lecteur qui vit avec lui",
          "[effect_skill_str_follows]") {
  CHECK(ro::render::StrDiesWithBearer(kProvokeEffect));
  CHECK(ro::render::StrDiesWithBearer(kFaithPowerEffect));
  CHECK_FALSE(ro::render::StrDiesWithBearer(kClassEffectLivingAlone));
}
