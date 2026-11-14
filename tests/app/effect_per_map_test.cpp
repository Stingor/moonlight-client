// CE QUE LES EFFETS OUBLIENT AU CHANGEMENT DE CARTE.
//
// Le joueur traverse la carte avec ses états et leurs valeurs, et le serveur
// lui redit à l'identique ce qui l'entoure. Ce qui a été éteint doit donc
// revenir de ce que la mémoire des effets a oublié, et non d'un changement que
// rien n'annonce.

#include <cstdint>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/render/state_effect.h"
#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/grf_builder.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsw;
using ro::test_support::GndBuildOptions;
using ro::test_support::RswBuildOptions;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kPlayerId = 150'001;
constexpr std::uint32_t kWarpId = 110'042;
// L'assombrissement du Devil : son effet se calcule sur la valeur de l'état.
constexpr ro::sim::StatusEffectId kDevilState = 152;
constexpr std::int32_t kDevilRank = 5;
// L'effet qu'un warp pose au sol.
constexpr std::uint32_t kWarpGroundEffect = 321;
constexpr std::uint32_t kNowMs = 10'000;

// Combien de fois cet effet a été demandé. Le compte vaut pour la séance et
// survit à `Forget` : c'est ce qui le rend lisible des deux côtés du changement.
std::size_t AskedFor(const moonclient::EffectsInPlay& effects, std::uint32_t effect_id) {
  for (const moonclient::EffectsInPlay::EffectAsked& count : effects.AskedEffects()) {
    if (count.effect_id == effect_id) {
      return count.asked;
    }
  }
  return 0;
}

// Une carte plate, juste assez pour poser les acteurs.
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

ro::sim::Actor Warp() {
  ro::sim::Actor warp;
  warp.id = kWarpId;
  warp.kind = ro::sim::ActorKind::kNonPlayer;
  warp.look.job = ro::sim::kPortalJob;
  return warp;
}

// Une installation vide : aucun effet ne se dessine, mais chacun se DEMANDE,
// et c'est la demande que l'on compte.
struct Bench {
  TempDirectory client;
  ro::vfs::Vfs vfs;
  ro::render::EffectCatalog catalog;
  moonclient::EffectNodesInPlay nodes;
  moonclient::EffectsInPlay effects;
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  Bench() : vfs(MountEmpty(client)), catalog(vfs), nodes(vfs), effects(catalog, nodes, vfs) {
    world.set_player_id(kPlayerId);
  }

  static ro::vfs::Vfs MountEmpty(const TempDirectory& client) {
    client.WriteFile("vide.grf", BuildGrf({}));
    auto rise = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"vide.grf"}});
    REQUIRE(rise.has_value());
    return std::move(*rise);
  }
};

}  // namespace

TEST_CASE("un effet à rang revient après un changement de carte, valeur inchangée",
          "[effect_per_map]") {
  Bench bench;
  const ro::render::StateValueEffectEntry* record = ro::render::StateValueEffectFor(kDevilState);
  REQUIRE(record != nullptr);
  REQUIRE(kDevilRank >= record->min_value);
  REQUIRE(kDevilRank <= record->max_value);
  const auto effect_id = static_cast<std::uint32_t>(kDevilRank + record->offset);

  ro::sim::Actor player;
  player.id = kPlayerId;
  player.status_values[kDevilState] = kDevilRank;
  bench.world.Spawn(player);

  bench.effects.PlayStateValueEffects(bench.world, bench.stage, bench.planner, kNowMs);
  REQUIRE(AskedFor(bench.effects, effect_id) == 1);
  // La même valeur ne rejoue rien : c'est la règle ordinaire.
  bench.effects.PlayStateValueEffects(bench.world, bench.stage, bench.planner, kNowMs + 1);
  REQUIRE(AskedFor(bench.effects, effect_id) == 1);

  // 🔴 LE JOUEUR TRAVERSE AVEC SA VALEUR, et `Forget` éteint l'effet. Si la
  // mémoire des valeurs survivait à la carte, la valeur retrouvée serait
  // « déjà vue » et l'effet ne reviendrait qu'au prochain changement de rang.
  bench.world.EnterMap("prontera", 150, 150);
  REQUIRE(bench.world.player() != nullptr);
  REQUIRE(bench.world.player()->status_values.at(kDevilState) == kDevilRank);
  bench.effects.Forget();
  bench.effects.PlayStateValueEffects(bench.world, bench.stage, bench.planner, kNowMs + 2);
  CHECK(AskedFor(bench.effects, effect_id) == 2);
}

TEST_CASE("un warp réannoncé sous le même identifiant après la carte retrouve ses anneaux",
          "[effect_per_map]") {
  Bench bench;
  bench.world.Spawn(Warp());
  bench.effects.NoticeWarpActors(bench.world, bench.stage, kNowMs);
  REQUIRE(AskedFor(bench.effects, kWarpGroundEffect) == 1);
  // Un warp déjà pourvu ne redemande rien.
  bench.effects.NoticeWarpActors(bench.world, bench.stage, kNowMs + 1);
  REQUIRE(AskedFor(bench.effects, kWarpGroundEffect) == 1);

  // ⚠ LE SERVEUR LE RÉANNONCE AVANT QUE LES EFFETS NE REPASSENT : sans la
  // remise de la carte, son identifiant serait encore marqué « pourvu ».
  bench.world.EnterMap("prontera", 150, 150);
  bench.world.Spawn(Warp());
  bench.effects.Forget();
  bench.effects.NoticeWarpActors(bench.world, bench.stage, kNowMs + 2);
  CHECK(AskedFor(bench.effects, kWarpGroundEffect) == 2);
}
