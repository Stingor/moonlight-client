// L'OMBRE D'UN PROJECTILE EN VOL : la même que celle d'un acteur dans le même
// mode graphique — sa silhouette pour le soleil et les lumières ponctuelles,
// l'ombre ronde au sol sous lui sans soleil —, et aucune pour la lueur
// élémentaire qui l'accompagne.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_track_motion.h"
#include "ro/render/element_glow.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_renderer.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/projectile_casters.h"
#include "support/client_vfs.h"
#include "support/grf_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

using Catch::Approx;
using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildGrf;
using ro::test_support::BuildSpr;
using ro::test_support::Cp949;
using ro::test_support::GrfFileSpec;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;
using ro::test_support::TempDirectory;

namespace {

constexpr std::int32_t kArrowWidthPixels = 4;
constexpr std::int32_t kArrowHeightPixels = 6;
// L'ombre ronde, plus large que haute comme celle du jeu.
constexpr std::int32_t kShadowWidthPixels = 20;
constexpr std::int32_t kShadowHeightPixels = 10;
constexpr std::uint32_t kFirstFrameMs = 100;
constexpr std::int32_t kFlightTicks = 12;
// Le tireur et sa cible à la hauteur d'envol, au-dessus d'un sol à trois unités.
constexpr float kGroundHeight = 3.0F;
constexpr glm::vec3 kShooter{0.0F, kGroundHeight + ro::render::kArrowFlightHeight, 0.0F};
constexpr glm::vec3 kTarget{0.0F, kGroundHeight + ro::render::kArrowFlightHeight, -40.0F};
// Une lueur de feu, rouge à demi-alpha.
constexpr std::array<std::uint8_t, 4> kFireGlow = {255, 64, 0, 128};

// Une image pleine de `width` sur `height`.
std::vector<std::uint8_t> PlainSpr(std::int32_t width, std::int32_t height) {
  SprBuildOptions options;
  options.palette.push_back(SpriteColor{});
  options.indexed.push_back(SprIndexedFrameSpec{
      width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width * height), 1)});
  options.palette.push_back(SpriteColor{.red = 100, .green = 48, .blue = 54});
  return BuildSpr(options);
}

// Une action d'une image, qui montre la première du sprite.
std::vector<std::uint8_t> OneImageAct() {
  ActBuildOptions options;
  ActActionSpec action;
  action.frames.push_back(ActFrameSpec{{ActLayerSpec{}}, {}, -1});
  options.actions.push_back(action);
  options.speeds = {4.0F};
  return BuildAct(options);
}

// Le client de l'archer, avec ou sans le sprite de l'ombre ronde.
void FillClient(const TempDirectory& client, bool with_shadow) {
  const std::string arrow(ro::render::kArrowSpriteBase);
  std::vector<GrfFileSpec> files = {
      GrfFileSpec{Cp949(arrow + ".spr"), PlainSpr(kArrowWidthPixels, kArrowHeightPixels)},
      GrfFileSpec{Cp949(arrow + ".act"), OneImageAct()}};
  if (with_shadow) {
    const std::string shadow(ro::render::kShadowSpritePath);
    files.push_back(
        GrfFileSpec{Cp949(shadow + ".spr"), PlainSpr(kShadowWidthPixels, kShadowHeightPixels)});
    files.push_back(GrfFileSpec{Cp949(shadow + ".act"), OneImageAct()});
  }
  client.WriteFile("projectiles.grf", BuildGrf(files));
}

ro::render::Camera GameCamera() {
  ro::render::Camera camera;
  camera.pitch_degrees = ro::render::kGameElevationDegrees;
  return camera;
}

// Un sol plat à `kGroundHeight`.
ro::render::EffectScatterWorld FlatGround() {
  ro::render::EffectScatterWorld world;
  world.ground_height = [](float /*world_x*/, float /*world_z*/) { return kGroundHeight; };
  return world;
}

// La longueur du bord haut du quad, quel que soit l'angle dont il a tourné.
float QuadWidth(const ro::render::SpriteQuad& quad) {
  return glm::length(quad.corners[1] - quad.corners[0]);
}

}  // namespace

TEST_CASE("une flèche en vol porte la silhouette et l'ombre ronde d'un acteur",
          "[projectile_shadow]") {
  const TempDirectory client;
  FillClient(client, true);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"projectiles.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  nodes.NoticeWorld(FlatGround());
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTarget, kFlightTicks,
                            ro::render::ProjectileFlight{}));
  const std::span<const ro::render::DrawnActor> drawn = nodes.Drawn(kFirstFrameMs, GameCamera());
  REQUIRE(drawn.size() == 1);
  const moonclient::ProjectileCasters& casters = nodes.projectile_casters();

  // SA SILHOUETTE : la même image, au même point de son vol.
  REQUIRE(casters.silhouettes().size() == 1);
  const ro::render::DrawnActor& silhouette = casters.silhouettes().front();
  REQUIRE(silhouette.actor != nullptr);
  CHECK(silhouette.actor->quads.size() == drawn.front().actor->quads.size());
  CHECK(silhouette.ground_position == drawn.front().ground_position);

  // SON OMBRE RONDE, sans corps, posée au sol sous le vol et non à sa hauteur.
  const std::span<const ro::render::DrawnActor> ground = casters.GroundShadows(false);
  REQUIRE(ground.size() == 1);
  CHECK(ground.front().actor == nullptr);
  REQUIRE(ground.front().shadow != nullptr);
  REQUIRE(ground.front().shadow->quads.size() == 1);
  CHECK(ground.front().ground_position == drawn.front().ground_position);
  REQUIRE(ground.front().shadow_ground_height.has_value());
  CHECK(*ground.front().shadow_ground_height == Approx(kGroundHeight));
  // À l'échelle d'un objet au sol.
  CHECK(QuadWidth(ground.front().shadow->quads.front()) ==
        Approx(static_cast<float>(kShadowWidthPixels) * moonclient::kProjectileShadowScale));
}

TEST_CASE("sous le soleil, le projectile n'a plus d'ombre ronde : sa silhouette en porte une",
          "[projectile_shadow]") {
  const TempDirectory client;
  FillClient(client, true);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"projectiles.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTarget, kFlightTicks,
                            ro::render::ProjectileFlight{}));
  static_cast<void>(nodes.Drawn(kFirstFrameMs, GameCamera()));
  const moonclient::ProjectileCasters& casters = nodes.projectile_casters();

  CHECK(casters.GroundShadows(true).empty());
  CHECK(casters.silhouettes().size() == 1);
  // Sans sol connu, l'ombre ronde reste à la hauteur du vol.
  REQUIRE(casters.GroundShadows(false).size() == 1);
  CHECK_FALSE(casters.GroundShadows(false).front().shadow_ground_height.has_value());
}

TEST_CASE("la lueur élémentaire d'une flèche ne porte aucune ombre", "[projectile_shadow]") {
  const TempDirectory client;
  FillClient(client, true);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"projectiles.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTarget, kFlightTicks,
                            ro::render::ProjectileFlight{.glow_tint = kFireGlow}));
  REQUIRE(nodes.living_nodes().size() == 2);
  CHECK(nodes.living_nodes()[0].casts_shadow);
  CHECK_FALSE(nodes.living_nodes()[1].casts_shadow);

  const std::span<const ro::render::DrawnActor> drawn = nodes.Drawn(kFirstFrameMs, GameCamera());
  REQUIRE(drawn.size() == 2);
  const moonclient::ProjectileCasters& casters = nodes.projectile_casters();
  REQUIRE(casters.silhouettes().size() == 1);
  CHECK(casters.GroundShadows(false).size() == 1);
  // La silhouette est celle de la flèche, à sa taille, et non la lueur grandie.
  REQUIRE(casters.silhouettes().front().actor->quads.size() == 1);
  CHECK(QuadWidth(casters.silhouettes().front().actor->quads.front()) ==
        Approx(static_cast<float>(kArrowWidthPixels)));
}

TEST_CASE("le trait d'une baguette porte la même ombre que la flèche", "[projectile_shadow]") {
  const TempDirectory client;
  FillClient(client, true);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"projectiles.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(
      ro::render::kArrowSpriteBase, kShooter, kTarget, kFlightTicks,
      ro::render::ProjectileFlight{.clock_slowdown = ro::render::kWandBoltSlowdown,
                                   .turn_degrees = ro::render::kWandBoltTurnDegrees}));
  static_cast<void>(nodes.Drawn(kFirstFrameMs, GameCamera()));
  CHECK(nodes.projectile_casters().silhouettes().size() == 1);
  CHECK(nodes.projectile_casters().GroundShadows(false).size() == 1);
}

TEST_CASE("sans sprite d'ombre, le projectile garde sa silhouette", "[projectile_shadow]") {
  const TempDirectory client;
  FillClient(client, false);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"projectiles.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTarget, kFlightTicks,
                            ro::render::ProjectileFlight{}));
  static_cast<void>(nodes.Drawn(kFirstFrameMs, GameCamera()));
  CHECK(nodes.projectile_casters().silhouettes().size() == 1);
  CHECK(nodes.projectile_casters().GroundShadows(false).empty());
}

TEST_CASE(
    "un sprite d'effet qui n'est pas un projectile ne porte pas d'ombre, et l'oubli vide tout",
    "[projectile_shadow]") {
  const TempDirectory client;
  FillClient(client, true);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"projectiles.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.PlayActOnce(ro::render::kArrowSpriteBase, kShooter, 0.0F));
  static_cast<void>(nodes.Drawn(kFirstFrameMs, GameCamera()));
  CHECK(nodes.projectile_casters().silhouettes().empty());

  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTarget, kFlightTicks,
                            ro::render::ProjectileFlight{}));
  static_cast<void>(nodes.Drawn(kFirstFrameMs + 1, GameCamera()));
  REQUIRE(nodes.projectile_casters().silhouettes().size() == 1);
  nodes.Forget();
  CHECK(nodes.projectile_casters().silhouettes().empty());
  CHECK(nodes.projectile_casters().GroundShadows(false).empty());
}

TEST_CASE("les lots qui portent ombre se réunissent dans leur ordre", "[projectile_shadow]") {
  std::array<ro::render::DrawnActor, 2> carts{};
  carts[0].ground_position = glm::vec3(1.0F, 0.0F, 0.0F);
  carts[1].ground_position = glm::vec3(2.0F, 0.0F, 0.0F);
  std::array<ro::render::DrawnActor, 1> projectiles{};
  projectiles[0].ground_position = glm::vec3(3.0F, 0.0F, 0.0F);
  const std::vector<ro::render::DrawnActor> joined =
      moonclient::JoinedCasterLots({carts, {}, projectiles});
  REQUIRE(joined.size() == 3);
  CHECK(joined[0].ground_position.x == Approx(1.0F));
  CHECK(joined[2].ground_position.x == Approx(3.0F));
}

namespace {

// Un tic de seize millisecondes et une demi-image entamée.
constexpr std::uint32_t kTickAndAHalfMs = 24;

// Vrai quand deux points se confondent, aux arrondis du mélange près.
bool SamePoint(const glm::vec3& left, const glm::vec3& right) {
  return left.x == Approx(right.x) && left.y == Approx(right.y) && left.z == Approx(right.z);
}

}  // namespace

TEST_CASE("entre deux tics, la flèche, sa lueur et son ombre se peignent au même point du vol",
          "[projectile_shadow][blend]") {
  const TempDirectory client;
  FillClient(client, true);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"projectiles.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  nodes.NoticeWorld(FlatGround());
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTarget, kFlightTicks,
                            ro::render::ProjectileFlight{.glow_tint = kFireGlow}));

  // NÉE DEPUIS LE DERNIER TIC, elle se peint où elle est : rien ne la précède.
  const std::span<const ro::render::DrawnActor> born =
      nodes.Drawn(kFirstFrameMs, GameCamera(), true);
  REQUIRE(born.size() == 2);
  for (const ro::render::DrawnActor& drawn : born) {
    CHECK(drawn.ground_position == nodes.living_nodes()[0].ground_position);
  }

  // UN TIC ET DEMI PLUS TARD, à mi-chemin de ses deux derniers tics.
  const std::span<const ro::render::DrawnActor> flying =
      nodes.Drawn(kFirstFrameMs + kTickAndAHalfMs, GameCamera(), true);
  REQUIRE(flying.size() == 2);
  const ro::render::PooledEffectNode& arrow = nodes.living_nodes()[0];
  REQUIRE(arrow.previous_tick.has_value());
  CHECK(arrow.previous_tick->ground_position != arrow.ground_position);
  const glm::vec3 middle = (arrow.previous_tick->ground_position + arrow.ground_position) * 0.5F;
  for (const ro::render::DrawnActor& drawn : flying) {
    CHECK(SamePoint(drawn.ground_position, middle));
  }
  const moonclient::ProjectileCasters& casters = nodes.projectile_casters();
  REQUIRE(casters.silhouettes().size() == 1);
  CHECK(SamePoint(casters.silhouettes().front().ground_position, middle));
  REQUIRE(casters.GroundShadows(false).size() == 1);
  CHECK(SamePoint(casters.GroundShadows(false).front().ground_position, middle));
}

TEST_CASE("sans l'interpolation, la flèche se peint à son dernier tic",
          "[projectile_shadow][blend]") {
  const TempDirectory client;
  FillClient(client, true);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"projectiles.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kArrowSpriteBase, kShooter, kTarget, kFlightTicks,
                            ro::render::ProjectileFlight{}));
  static_cast<void>(nodes.Drawn(kFirstFrameMs, GameCamera()));
  const std::span<const ro::render::DrawnActor> flying =
      nodes.Drawn(kFirstFrameMs + kTickAndAHalfMs, GameCamera());
  REQUIRE(flying.size() == 1);
  CHECK(flying.front().ground_position == nodes.living_nodes()[0].ground_position);
  CHECK(nodes.projectile_casters().silhouettes().front().ground_position ==
        nodes.living_nodes()[0].ground_position);
}
