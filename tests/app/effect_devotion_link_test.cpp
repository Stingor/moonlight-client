// LES TRAITS DE DEVOTION : une liste de protégés tend un trait du croisé vers
// chacun, et chaque liste nouvelle défait les précédents avant de retendre.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

using Catch::Approx;
using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsw;
using ro::test_support::BuildTga;
using ro::test_support::GndBuildOptions;
using ro::test_support::GrfFileSpec;
using ro::test_support::RswBuildOptions;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::uint32_t kCrusader = 2'000'001;
constexpr std::uint32_t kFirstDevotee = 2'000'002;
constexpr std::uint32_t kSecondDevotee = 2'000'003;
constexpr std::uint32_t kDevotionLink = 232;
constexpr std::uint32_t kNowMs = 10'000;
constexpr std::int32_t kTexturePixels = 4;
const ro::sim::CellPosition kCrusaderCell{1, 1};
const ro::sim::CellPosition kFirstDevoteeCell{3, 1};
const ro::sim::CellPosition kSecondDevoteeCell{1, 3};

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

// Un client qui ne porte que l'image du trait.
ro::vfs::Vfs CloudClient(const TempDirectory& client) {
  const std::vector<TgaPixel> white(static_cast<std::size_t>(kTexturePixels * kTexturePixels),
                                    TgaPixel{.red = 255, .green = 255, .blue = 255});
  client.WriteFile("trait.grf",
                   BuildGrf({GrfFileSpec{"data\\texture\\effect\\cloud11.tga",
                                         BuildTga(kTexturePixels, kTexturePixels, white)}}));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"trait.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

// Le croisé — c'est le joueur — et ses deux protégés, chacun sur sa case.
ro::sim::World CrusaderAndDevotees() {
  ro::sim::World world;
  const std::array<std::pair<std::uint32_t, ro::sim::CellPosition>, 3> cast = {{
      {kCrusader, kCrusaderCell},
      {kFirstDevotee, kFirstDevoteeCell},
      {kSecondDevotee, kSecondDevoteeCell},
  }};
  for (const auto& [id, cell] : cast) {
    ro::sim::Actor actor;
    actor.id = id;
    actor.kind = ro::sim::ActorKind::kPlayer;
    actor.position = cell;
    world.Spawn(actor);
  }
  world.set_player_id(kCrusader);
  return world;
}

// Les traits que le croisé porte, dans l'ordre de leur naissance.
std::vector<ro::render::PooledEffectNode> LinksOf(const moonclient::EffectNodesInPlay& nodes) {
  std::vector<ro::render::PooledEffectNode> links;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    if (node.sown_by_effect_id == kDevotionLink && node.anchor_id == kCrusader) {
      links.push_back(node);
    }
  }
  return links;
}

// Tout ce qu'un essai emploie, bâti dans l'ordre que les références exigent.
struct Scene {
  TempDirectory client;
  ro::vfs::Vfs vfs = CloudClient(client);
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world = CrusaderAndDevotees();
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  void Notice(std::uint32_t now_ms) { effects.NoticeDevotionLinks(world, stage, planner, now_ms); }
};

}  // namespace

TEST_CASE("Devotion : une liste tend un trait du croisé vers chaque protégé", "[app][devotion]") {
  Scene scene;
  scene.Notice(kNowMs);
  CHECK(LinksOf(scene.nodes).empty());

  REQUIRE(scene.world.TellDevotion(kCrusader, {kFirstDevotee, 0, kSecondDevotee, 0, 0}));
  scene.Notice(kNowMs);
  const std::vector<ro::render::PooledEffectNode> links = LinksOf(scene.nodes);
  REQUIRE(links.size() == 2);
  const std::array<std::pair<std::uint32_t, ro::sim::CellPosition>, 2> expected = {{
      {kFirstDevotee, kFirstDevoteeCell},
      {kSecondDevotee, kSecondDevoteeCell},
  }};
  const std::array<float, 3> crusader_ground =
      moonclient::GroundPositionAt(scene.stage, kCrusaderCell.x, kCrusaderCell.y);
  for (std::size_t rank = 0; rank < links.size(); ++rank) {
    const ro::render::PooledEffectNode& link = links[rank];
    CHECK(link.shape == ro::render::PooledNodeShape::kTetheredBeam);
    CHECK(link.target_actor_id == expected[rank].first);
    // LE BOUT PROCHE SUR LE CROISÉ, le lointain sur son protégé.
    CHECK(link.owner_position.x == Approx(crusader_ground[0]));
    CHECK(link.owner_position.z == Approx(crusader_ground[2]));
    const std::array<float, 3> devotee_ground =
        moonclient::GroundPositionAt(scene.stage, expected[rank].second.x, expected[rank].second.y);
    REQUIRE(link.target_position.has_value());
    CHECK(link.target_position->x == Approx(devotee_ground[0]));
    CHECK(link.target_position->z == Approx(devotee_ground[2]));
  }

  // UNE IMAGE DE PLUS, SANS ANNONCE, NE TEND RIEN DE PLUS.
  scene.Notice(kNowMs + 1);
  CHECK(LinksOf(scene.nodes).size() == 2);
}

TEST_CASE("Devotion : chaque annonce défait les traits avant de retendre", "[app][devotion]") {
  Scene scene;
  REQUIRE(scene.world.TellDevotion(kCrusader, {kFirstDevotee, 0, 0, 0, 0}));
  scene.Notice(kNowMs);
  REQUIRE(LinksOf(scene.nodes).size() == 1);

  REQUIRE(scene.world.TellDevotion(kCrusader, {kFirstDevotee, kSecondDevotee, 0, 0, 0}));
  scene.Notice(kNowMs + 1);
  CHECK(LinksOf(scene.nodes).size() == 2);

  // 🔴 LA MÊME LISTE REDITE FAIT REPOUSSER LES TRAITS : ils renaissent, sans
  // s'ajouter à ceux d'avant.
  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  const ro::render::Camera camera;
  static_cast<void>(scene.nodes.Drawn(kNowMs + 1, camera));
  static_cast<void>(scene.nodes.Drawn(kNowMs + 1 + 3 * tick_ms, camera));
  REQUIRE(LinksOf(scene.nodes).front().tracks[0].ticks > 0);
  REQUIRE(scene.world.TellDevotion(kCrusader, {kFirstDevotee, kSecondDevotee, 0, 0, 0}));
  scene.Notice(kNowMs + 2);
  REQUIRE(LinksOf(scene.nodes).size() == 2);
  CHECK(LinksOf(scene.nodes).front().tracks[0].ticks == 0);

  // LA LISTE VIDE défait tout.
  REQUIRE(scene.world.TellDevotion(kCrusader, {}));
  scene.Notice(kNowMs + 3);
  CHECK(LinksOf(scene.nodes).empty());
}

TEST_CASE("Devotion : un croisé qui sort et revient entre deux images retend sa liste neuve",
          "[app][devotion]") {
  Scene scene;
  REQUIRE(scene.world.TellDevotion(kCrusader, {kFirstDevotee, 0, 0, 0, 0}));
  scene.Notice(kNowMs);
  REQUIRE(LinksOf(scene.nodes).size() == 1);

  // 🔴 TOUT ARRIVE D'UN BLOC, après une image lente : il sort de la vue, y
  // rentre, et le serveur lui renvoie sa liste — qui a pris un protégé pendant
  // son absence. C'est sa PREMIÈRE annonce comme acteur neuf, comme l'était
  // celle déjà tendue pour l'ancien.
  REQUIRE(scene.world.Remove(kCrusader));
  ro::sim::Actor back;
  back.id = kCrusader;
  back.kind = ro::sim::ActorKind::kPlayer;
  back.position = kCrusaderCell;
  scene.world.Spawn(back);
  REQUIRE(scene.world.TellDevotion(kCrusader, {kFirstDevotee, kSecondDevotee, 0, 0, 0}));
  scene.Notice(kNowMs + 1);

  const std::vector<ro::render::PooledEffectNode> links = LinksOf(scene.nodes);
  REQUIRE(links.size() == 2);
  CHECK(links[0].target_actor_id == kFirstDevotee);
  CHECK(links[1].target_actor_id == kSecondDevotee);
}

TEST_CASE("Devotion : /effect coupé, l'annonce est perdue", "[app][devotion]") {
  Scene scene;
  REQUIRE(scene.world.TellDevotion(kCrusader, {kFirstDevotee, 0, 0, 0, 0}));
  scene.Notice(kNowMs);
  REQUIRE(LinksOf(scene.nodes).size() == 1);

  // ⚠ LE CLIENT NE DÉFAIT RIEN NON PLUS : le trait d'avant reste.
  scene.effects.SetEffectSwitches(false, false);
  REQUIRE(scene.world.TellDevotion(kCrusader, {}));
  scene.Notice(kNowMs + 1);
  CHECK(LinksOf(scene.nodes).size() == 1);

  // ET RALLUMER NE LA REJOUE PAS.
  scene.effects.SetEffectSwitches(true, false);
  scene.Notice(kNowMs + 2);
  CHECK(LinksOf(scene.nodes).size() == 1);
}

TEST_CASE("Devotion : le protégé qui s'en va éteint son trait", "[app][devotion]") {
  Scene scene;
  REQUIRE(scene.world.TellDevotion(kCrusader, {kFirstDevotee, kSecondDevotee, 0, 0, 0}));
  scene.Notice(kNowMs);
  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  const ro::render::Camera camera;
  static_cast<void>(scene.nodes.Drawn(kNowMs, camera));
  static_cast<void>(scene.nodes.Drawn(kNowMs + tick_ms, camera));
  REQUIRE(LinksOf(scene.nodes).size() == 2);
  REQUIRE(LinksOf(scene.nodes)[0].tracks[0].active);

  // IL SORT DE LA VUE : le suivi des porteurs efface sa place, et le trait
  // s'éteint au tic suivant. Celui de l'autre protégé tient.
  REQUIRE(scene.world.Remove(kFirstDevotee));
  scene.effects.NoticeOptionEffects(scene.world, scene.stage, scene.planner, kNowMs + tick_ms);
  static_cast<void>(scene.nodes.Drawn(kNowMs + 2 * tick_ms, camera));
  const std::vector<ro::render::PooledEffectNode> links = LinksOf(scene.nodes);
  REQUIRE(links.size() == 2);
  CHECK_FALSE(links[0].tracks[0].active);
  CHECK(links[1].tracks[0].active);
}
