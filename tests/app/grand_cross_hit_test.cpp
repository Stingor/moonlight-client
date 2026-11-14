// GRAND CROSS VU PAR L'APPLICATION : chaque message de dégâts de la compétence
// fait naître la croix sur SA SOURCE — le lanceur —, et la croix doit se VOIR.
//
// Le serveur pose vingt-neuf unités invisibles et frappe par elles, mais il
// nomme le lanceur comme source de chaque coup : le coup qu'il se porte à
// lui-même, en sorte quatre, et celui de chaque victime, en éclaboussure. Le
// client pose l'effet 226 sur la source à chacun de ces messages.
//
// 🔴 LA CROIX ÉTAIT SEMÉE ET INVISIBLE. Ses deux figures ne donnaient aucune
// coordonnée de texture : chaque sommet lisait le texel du coin, transparent
// dans les deux images de la croix, et rien ne paraissait. L'image de ces essais
// a la même particularité : ses quatre coins sont transparents, son cœur opaque.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/image.h"
#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_frame.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
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

constexpr std::uint16_t kGrandCross = 254;
constexpr std::uint8_t kEnduredType = 4;
constexpr std::uint8_t kSplashType = 5;
constexpr std::int16_t kLevelTen = 10;
constexpr std::uint32_t kCasterId = 150'061;
constexpr std::uint32_t kMonsterId = 150'062;
constexpr std::uint32_t kNowMs = 10'000;
constexpr std::int32_t kGroundCells = 12;
constexpr std::int32_t kImagePixels = 4;
// Le sommet de l'alpha des deux figures : neuf tics de montée.
constexpr std::int32_t kTicksToPeak = 9;
constexpr std::size_t kChannels = 4;
constexpr std::size_t kAlphaChannel = 3;
constexpr ro::sim::CellPosition kCasterCell{.x = 5, .y = 5};
constexpr ro::sim::CellPosition kMonsterCell{.x = 7, .y = 5};
constexpr const char* kRibbonTexture = "data\\texture\\effect\\ring_white.tga";
constexpr const char* kFrameTexture = "data\\texture\\effect\\ring_yellow.tga";

moonclient::MapStage FlatStage() {
  GndBuildOptions ground;
  ground.width = kGroundCells;
  ground.height = kGroundCells;
  auto gnd = ro::formats::GndMap::Parse(BuildGnd(ground));
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

// Une image blanche au cœur opaque et aux quatre coins transparents.
std::vector<std::uint8_t> CorneredTga() {
  std::vector<TgaPixel> pixels(static_cast<std::size_t>(kImagePixels * kImagePixels),
                               TgaPixel{.red = 255, .green = 255, .blue = 255, .alpha = 255});
  constexpr std::int32_t kLast = kImagePixels - 1;
  for (const auto& [x, y] :
       {std::pair{0, 0}, std::pair{kLast, 0}, std::pair{0, kLast}, std::pair{kLast, kLast}}) {
    pixels[static_cast<std::size_t>((y * kImagePixels) + x)].alpha = 0;
  }
  return BuildTga(kImagePixels, kImagePixels, pixels);
}

ro::vfs::Vfs ClientWithCrossImages(const TempDirectory& client) {
  client.WriteFile("croix.grf", BuildGrf({GrfFileSpec{kRibbonTexture, CorneredTga()},
                                          GrfFileSpec{kFrameTexture, CorneredTga()}}));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"croix.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

// L'alpha du texel que le renderer lit au centre d'un quad : la moyenne de ses
// quatre coordonnées, au plus proche. La rangée zéro de l'image est l'ordonnée
// zéro, comme au téléversement.
[[nodiscard]] std::uint8_t AlphaAtCentre(const ro::formats::Image& image,
                                         const ro::render::EffectWorldQuad& quad) {
  glm::vec2 centre{0.0F, 0.0F};
  for (const glm::vec2& uv : quad.texture_uv) {
    centre += uv;
  }
  centre /= static_cast<float>(quad.texture_uv.size());
  const auto column = std::min(
      static_cast<std::int32_t>(centre.x * static_cast<float>(image.width)), image.width - 1);
  const auto row = std::min(static_cast<std::int32_t>(centre.y * static_cast<float>(image.height)),
                            image.height - 1);
  const auto texel = (static_cast<std::size_t>(row) * static_cast<std::size_t>(image.width)) +
                     static_cast<std::size_t>(column);
  return image.pixels[(texel * kChannels) + kAlphaChannel];
}

struct Bench {
  Bench() : vfs(ClientWithCrossImages(client)) {
    ro::sim::Actor caster;
    caster.id = kCasterId;
    caster.kind = ro::sim::ActorKind::kPlayer;
    caster.position = kCasterCell;
    world.Spawn(caster);
    world.set_player_id(kCasterId);
    ro::sim::Actor monster;
    monster.id = kMonsterId;
    monster.kind = ro::sim::ActorKind::kMonster;
    monster.position = kMonsterCell;
    world.Spawn(monster);
  }

  // Un message de dégâts de Grand Cross, dont le lanceur est la source.
  void Strike(std::uint32_t target_id, std::uint8_t damage_type) {
    ro::sim::Strike strike;
    strike.attacker = kCasterId;
    strike.target = target_id;
    strike.started_ms = kNowMs;
    strike.damage = 1;
    strike.hits = 1;
    strike.type = damage_type;
    strike.skill_id = kGrandCross;
    strike.skill_level = static_cast<std::uint16_t>(kLevelTen);
    strike.announced_level = kLevelTen;
    REQUIRE(world.RecordStrike(strike));
  }

  // Ce que la croix montre au sommet de son alpha : pour chaque image, le nombre
  // de quads qui SE VOIENT — un alpha de sommet non nul ET un texel opaque — et
  // les places où ils sont nés.
  struct Seen {
    std::size_t visible_ribbon_quads = 0;
    std::size_t visible_frame_quads = 0;
    std::vector<glm::vec3> birth_places;
  };

  Seen LookAtPeak() {
    effects.NoticeSkills(world, stage, planner, kNowMs);
    const ro::render::Camera camera;
    static_cast<void>(nodes.Drawn(kNowMs, camera));
    static_cast<void>(nodes.Drawn(
        kNowMs + static_cast<std::uint32_t>(kTicksToPeak * ro::render::kEffectTickMs), camera));
    Seen seen;
    for (const ro::render::DrawnWorldQuads& lot : nodes.WorldQuads()) {
      if (lot.texture == nullptr || lot.texture_path == nullptr) {
        continue;
      }
      const bool ribbon = *lot.texture_path == kRibbonTexture;
      const bool frame = *lot.texture_path == kFrameTexture;
      if (!ribbon && !frame) {
        continue;
      }
      seen.birth_places.push_back(lot.ground_position);
      for (const ro::render::EffectWorldQuad& quad : lot.quads) {
        if (quad.tint[kAlphaChannel] == 0 || AlphaAtCentre(*lot.texture, quad) == 0) {
          continue;
        }
        (ribbon ? seen.visible_ribbon_quads : seen.visible_frame_quads) += 1;
      }
    }
    return seen;
  }

  [[nodiscard]] glm::vec3 CasterGround() const {
    const std::array<float, 3> ground = moonclient::GroundPositionAt(
        stage, static_cast<float>(kCasterCell.x), static_cast<float>(kCasterCell.y));
    return {ground[0], ground[1], ground[2]};
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

TEST_CASE("Grand Cross : le coup sur soi fait paraître la croix sur le lanceur",
          "[app][effets][grandcross]") {
  Bench bench;
  bench.Strike(kCasterId, kEnduredType);
  const Bench::Seen seen = bench.LookAtPeak();
  CHECK(seen.visible_ribbon_quads > 0);
  CHECK(seen.visible_frame_quads > 0);
  REQUIRE_FALSE(seen.birth_places.empty());
  for (const glm::vec3& place : seen.birth_places) {
    CHECK(place == bench.CasterGround());
  }
}

TEST_CASE("Grand Cross : l'éclaboussure d'une victime fait paraître la croix sur le lanceur",
          "[app][effets][grandcross]") {
  // La source du coup est le lanceur, la cible le monstre : la croix naît sur
  // la SOURCE, et non sur ce qu'elle frappe.
  Bench bench;
  bench.Strike(kMonsterId, kSplashType);
  const Bench::Seen seen = bench.LookAtPeak();
  CHECK(seen.visible_ribbon_quads > 0);
  CHECK(seen.visible_frame_quads > 0);
  REQUIRE_FALSE(seen.birth_places.empty());
  for (const glm::vec3& place : seen.birth_places) {
    CHECK(place == bench.CasterGround());
  }
}
