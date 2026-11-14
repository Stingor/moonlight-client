// CE QU'UNE INCANTATION ÉTEINT EN CHEMIN. Interrompue, elle éteint son aura à
// l'image même ; et quand son lanceur bronche — un coup, Spell Breaker qui lui
// prend ses PV —, le réticule qu'il a posé sur sa cible disparaît aussitôt.

#include <cstdint>
#include <initializer_list>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_layered_ring_seed.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_screen_seed.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/render/skill_cast_extra.h"
#include "ro/render/skill_cast_table.h"
#include "ro/render/skill_impl.h"
#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/client_vfs.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;
using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsw;
using ro::test_support::BuildTga;
using ro::test_support::Cp949;
using ro::test_support::GndBuildOptions;
using ro::test_support::GrfFileSpec;
using ro::test_support::RswBuildOptions;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::uint16_t kFireBolt = 19;
// L'élément FEU, tel que le serveur le numérote.
constexpr std::uint32_t kFire = 3;
constexpr std::uint32_t kMageId = 150'031;
constexpr std::uint32_t kTargetId = 150'032;
constexpr std::uint32_t kStartMs = 10'000;
constexpr std::uint32_t kCastMs = 3'000;
// L'interruption tombe en plein milieu, et l'image suivante la voit.
constexpr std::uint32_t kBrokenMs = kStartMs + 500;
constexpr std::uint32_t kNextFrameMs = kBrokenMs + 16;
constexpr std::int32_t kPixels = 4;
constexpr std::int32_t kGroundCells = 4;

// L'aura que le mage fait tourner pendant son Fire Bolt.
std::uint32_t FireAura() {
  return static_cast<std::uint32_t>(
      ro::render::CastEffectForElement(ro::render::SkillImplFor(kFireBolt).CastEffectId(), kFire));
}

// Un client qui porte une texture pour chaque figure de ces effets : motifs,
// anneaux en couches et nœuds d'écran.
ro::vfs::Vfs ClientFor(const TempDirectory& client, std::initializer_list<std::uint32_t> effects) {
  const std::vector<TgaPixel> tga(static_cast<std::size_t>(kPixels * kPixels),
                                  TgaPixel{.red = 200, .green = 120, .blue = 60});
  const std::vector<BmpPixel> bmp(static_cast<std::size_t>(kPixels * kPixels),
                                  BmpPixel{.red = 200, .green = 120, .blue = 60});
  std::vector<GrfFileSpec> fichiers;
  std::set<std::string> seen;
  const auto add = [&](std::string_view name) {
    if (seen.insert(std::string(name)).second) {
      fichiers.push_back(GrfFileSpec{Cp949(name), name.ends_with(".bmp")
                                                      ? BuildBmp(kPixels, kPixels, bmp)
                                                      : BuildTga(kPixels, kPixels, tga)});
    }
  };
  for (const std::uint32_t effect_id : effects) {
    for (const ro::render::EffectNodePattern& pattern :
         ro::render::EffectNodePatternsFor(effect_id)) {
      for (const std::string_view name : ro::render::EffectNodePatternTextures(pattern)) {
        add(name);
      }
    }
    for (const ro::render::EffectLayeredRingNodePattern& ring :
         ro::render::EffectLayeredRingNodePatternsFor(effect_id)) {
      add(ring.texture);
    }
    for (const ro::render::EffectScreenPattern& screen :
         ro::render::EffectScreenPatternsFor(effect_id)) {
      for (const std::string& name : ro::render::EffectScreenTexturePaths(screen)) {
        add(name);
      }
    }
  }
  client.WriteFile("effets.grf", BuildGrf(fichiers));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

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

// Un mage qui incante Fire Bolt sur un joueur.
struct Bench {
  Bench() : vfs(ClientFor(client, {FireAura(), ro::render::kLockOnReticleEffect})) {
    ro::sim::Actor mage;
    mage.id = kMageId;
    mage.kind = ro::sim::ActorKind::kPlayer;
    mage.position = ro::sim::CellPosition{.x = 1, .y = 1};
    mage.cast = ro::sim::Cast{.skill_id = kFireBolt,
                              .target_id = kTargetId,
                              .element = kFire,
                              .started_ms = kStartMs,
                              .duration_ms = kCastMs};
    world.Spawn(mage);
    ro::sim::Actor target;
    target.id = kTargetId;
    target.kind = ro::sim::ActorKind::kPlayer;
    target.position = ro::sim::CellPosition{.x = 2, .y = 2};
    world.Spawn(target);
  }

  // La vue des acteurs, réduite à la question du sursaut.
  void SeeFlinching(bool flinching) {
    moonclient::EffectsInPlay::NamedActorView view;
    view.flinches_at = [flinching](const ro::sim::Actor& actor, std::uint32_t) {
      return flinching && actor.id == kMageId;
    };
    effects.SeeScene(camera, std::move(view));
  }

  TempDirectory client;
  ro::vfs::Vfs vfs;
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};
  ro::render::Camera camera;
};

}  // namespace

TEST_CASE("une incantation interrompue éteint son aura à l'image même", "[effect_cast_cancel]") {
  Bench bench;
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kStartMs);
  REQUIRE(bench.nodes.CarriesEffect(FireAura(), kMageId));

  REQUIRE(bench.world.CancelCast(kMageId, kBrokenMs));
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNextFrameMs);
  CHECK_FALSE(bench.nodes.CarriesEffect(FireAura(), kMageId));
}

TEST_CASE("le lanceur qui bronche éteint le réticule posé sur sa cible", "[effect_cast_cancel]") {
  Bench bench;
  bench.SeeFlinching(false);
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kStartMs);
  REQUIRE(bench.effects.Carries(ro::render::kLockOnReticleEffect, kTargetId));

  // ⚠ L'INTERRUPTION SEULE NE LE TOUCHE PAS : le client vide alors le second
  // emplacement du LANCEUR, et le réticule est dans celui de la cible.
  REQUIRE(bench.world.CancelCast(kMageId, kBrokenMs));
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNextFrameMs);
  CHECK(bench.effects.Carries(ro::render::kLockOnReticleEffect, kTargetId));

  bench.SeeFlinching(true);
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNextFrameMs + 16);
  CHECK_FALSE(bench.effects.Carries(ro::render::kLockOnReticleEffect, kTargetId));
}
