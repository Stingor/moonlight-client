// UN EFFET 44 ANNONCÉ SUR UN ACTEUR DEMANDE UNE CHEMINÉE, et non le patron du
// primitif : le gestionnaire d'effets tient une classe pour ce numéro, et elle
// passe devant tout fichier et tout nœud.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_effect_table.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/grf_builder.h"
#include "support/map_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

using Catch::Approx;
using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsw;
using ro::test_support::BuildSpr;
using ro::test_support::GndBuildOptions;
using ro::test_support::GrfFileSpec;
using ro::test_support::RswBuildOptions;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kPlayerId = 150'001;
constexpr std::uint32_t kNpcId = 110'044;
constexpr std::uint16_t kNpcCellX = 3;
constexpr std::uint16_t kNpcCellY = 4;
constexpr std::uint32_t kNowMs = 10'000;
constexpr auto kSmokeEffect = static_cast<std::uint32_t>(ro::render::kMapChimneySmokeEffect);
// Le couple que le patron du primitif nomme : présent dans l'archive, il ferait
// semer des bouffées si l'effet passait par les nœuds.
constexpr std::string_view kSmokeSprite = "data\\sprite\\이팩트\\굴뚝연기";
constexpr std::int32_t kPuffPixels = 8;

std::vector<std::uint8_t> PuffSpr() {
  SprBuildOptions options;
  options.indexed.push_back(SprIndexedFrameSpec{
      kPuffPixels, kPuffPixels,
      std::vector<std::uint8_t>(static_cast<std::size_t>(kPuffPixels * kPuffPixels), 1)});
  options.palette = {SpriteColor{}, SpriteColor{.red = 200, .green = 200, .blue = 200}};
  return BuildSpr(options);
}

std::vector<std::uint8_t> PuffAct() {
  ActBuildOptions options;
  options.actions.push_back(ActActionSpec{{ActFrameSpec{{ActLayerSpec{}}, {}, -1}}});
  options.speeds = {4.0F};
  return BuildAct(options);
}

// Une carte plate, juste assez pour poser l'acteur.
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

// Combien de fois cet effet a été demandé.
std::size_t AskedFor(const moonclient::EffectsInPlay& effects, std::uint32_t effect_id) {
  for (const moonclient::EffectsInPlay::EffectAsked& count : effects.AskedEffects()) {
    if (count.effect_id == effect_id) {
      return count.asked;
    }
  }
  return 0;
}

struct Bench {
  TempDirectory client;
  ro::vfs::Vfs vfs;
  ro::render::EffectCatalog catalog;
  moonclient::EffectNodesInPlay nodes;
  moonclient::EffectsInPlay effects;
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  Bench() : vfs(MountSmoke(client)), catalog(vfs), nodes(vfs), effects(catalog, nodes, vfs) {
    world.set_player_id(kPlayerId);
    ro::sim::Actor pnj;
    pnj.id = kNpcId;
    pnj.kind = ro::sim::ActorKind::kNonPlayer;
    pnj.position = {.x = kNpcCellX, .y = kNpcCellY};
    world.Spawn(pnj);
  }

  static ro::vfs::Vfs MountSmoke(const TempDirectory& client) {
    const auto cp949 = [](std::string_view utf8) {
      auto converti = ro::vfs::Utf8ToCp949(utf8);
      REQUIRE(converti.has_value());
      return *converti;
    };
    client.WriteFile("fumee.grf",
                     BuildGrf({GrfFileSpec{cp949(std::string(kSmokeSprite) + ".spr"), PuffSpr()},
                               GrfFileSpec{cp949(std::string(kSmokeSprite) + ".act"), PuffAct()}}));
    auto rise = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"fumee.grf"}});
    REQUIRE(rise.has_value());
    return std::move(*rise);
  }
};

}  // namespace

TEST_CASE("un effet 44 annoncé sur un acteur demande une cheminée à sa place", "[effect_chimney]") {
  Bench bench;
  REQUIRE(bench.world.RecordEffect(kNpcId, kSmokeEffect, kNowMs));
  bench.effects.Notice(bench.world, bench.stage, bench.planner);

  const std::vector<std::pair<glm::vec3, std::uint32_t>> cheminees = bench.effects.TakeChimneys();
  REQUIRE(cheminees.size() == 1);
  const auto& [place, bearer] = cheminees[0];
  const std::array<float, 3> sol = moonclient::GroundPositionAt(
      bench.stage, static_cast<float>(kNpcCellX), static_cast<float>(kNpcCellY));
  CHECK(place.x == Approx(sol[0]));
  CHECK(place.y == Approx(sol[1]));
  CHECK(place.z == Approx(sol[2]));
  // ELLE APPARTIENT À L'ACTEUR QUI L'A REÇUE : elle le suivra, et partira avec
  // lui.
  CHECK(bearer == kNpcId);
  // 🔴 LE PATRON DU PRIMITIF N'EST PAS SEMÉ : la classe passe devant les nœuds.
  CHECK(bench.nodes.living() == 0);
  // La demande se compte comme toute autre : c'est le serveur qui l'a faite.
  CHECK(AskedFor(bench.effects, kSmokeEffect) == 1);
  // Emportée, la file est vide ; et le même lancer ne se redemande pas.
  CHECK(bench.effects.TakeChimneys().empty());
  bench.effects.Notice(bench.world, bench.stage, bench.planner);
  CHECK(bench.effects.TakeChimneys().empty());
}

TEST_CASE("chaque nouveau lancer du même effet demande une cheminée de plus", "[effect_chimney]") {
  Bench bench;
  REQUIRE(bench.world.RecordEffect(kNpcId, kSmokeEffect, kNowMs));
  bench.effects.Notice(bench.world, bench.stage, bench.planner);
  REQUIRE(bench.world.RecordEffect(kNpcId, kSmokeEffect, kNowMs + 1));
  bench.effects.Notice(bench.world, bench.stage, bench.planner);
  // La classe ne remplace pas la précédente : elle s'y ajoute.
  CHECK(bench.effects.TakeChimneys().size() == 2);
}

TEST_CASE("une cheminée demandée avant un changement de carte n'y survit pas", "[effect_chimney]") {
  Bench bench;
  REQUIRE(bench.world.RecordEffect(kNpcId, kSmokeEffect, kNowMs));
  bench.effects.Notice(bench.world, bench.stage, bench.planner);
  bench.effects.Forget();
  CHECK(bench.effects.TakeChimneys().empty());
}
