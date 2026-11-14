// LE TIR DE BAGUETTE AMÉLIORÉ, dans la troupe : l'option coupée laisse le
// sprite du trait ; cochée, le trait devient une boule d'énergie — cœur, halo,
// étincelles et éclat, ajoutés au décor —, de la couleur de l'élément reçu ou
// arcane, dont seul le cœur porte l'ombre et qui se peint entre deux tics.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/element_glow.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_renderer.h"
#include "ro/render/wand_orb.h"
#include "ro/sim/actor.h"
#include "ro/sim/attack_element.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/projectile_casters.h"
#include "support/client_vfs.h"
#include "support/grf_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"
#include "world/element_glow_of.h"
#include "world/projectile_looks.h"

using Catch::Approx;
using moonclient::ProjectileLooks;
using moonclient::WandOrbRgbOf;
using ro::sim::Element;
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

constexpr std::int32_t kSpriteSidePixels = 6;
constexpr std::uint32_t kFirstFrameMs = 100;
constexpr auto kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
// Un tic et une demi-image entamée.
constexpr std::uint32_t kTickAndAHalfMs = 24;
// Le vol à l'horloge du client : douze tics, que la baguette rend trois fois
// plus longs.
constexpr std::int32_t kFlightTicks = 12;
constexpr std::int32_t kOrbFlightTicks = kFlightTicks * ro::render::kWandBoltSlowdown;
constexpr glm::vec3 kShooter{0.0F, 10.0F, 0.0F};
constexpr glm::vec3 kTarget{0.0F, 10.0F, -40.0F};
constexpr std::array<std::uint8_t, 3> kFireRgb = {255, 140, 40};

// Une image pleine.
std::vector<std::uint8_t> PlainSpr() {
  SprBuildOptions options;
  options.palette.push_back(SpriteColor{});
  options.indexed.push_back(
      SprIndexedFrameSpec{kSpriteSidePixels, kSpriteSidePixels,
                          std::vector<std::uint8_t>(
                              static_cast<std::size_t>(kSpriteSidePixels * kSpriteSidePixels), 1)});
  options.palette.push_back(SpriteColor{.red = 100, .green = 48, .blue = 54});
  return BuildSpr(options);
}

std::vector<std::uint8_t> OneImageAct() {
  ActBuildOptions options;
  ActActionSpec action;
  action.frames.push_back(ActFrameSpec{{ActLayerSpec{}}, {}, -1});
  options.actions.push_back(action);
  options.speeds = {4.0F};
  return BuildAct(options);
}

// Le client : le sprite du trait et celui de l'ombre ronde.
void FillClient(const TempDirectory& client) {
  std::vector<GrfFileSpec> files;
  for (const std::string& base :
       {std::string(ro::render::kWandBoltSpriteBase), std::string(ro::render::kShadowSpritePath)}) {
    files.push_back(GrfFileSpec{Cp949(base + ".spr"), PlainSpr()});
    files.push_back(GrfFileSpec{Cp949(base + ".act"), OneImageAct()});
  }
  client.WriteFile("baguette.grf", BuildGrf(files));
}

ro::render::Camera GameCamera() {
  ro::render::Camera camera;
  camera.pitch_degrees = ro::render::kGameElevationDegrees;
  return camera;
}

// Le vol du trait, et sa boule s'il en a une.
ro::render::ProjectileFlight WandFlight(std::optional<std::array<std::uint8_t, 3>> orb) {
  return ro::render::ProjectileFlight{.clock_slowdown = ro::render::kWandBoltSlowdown,
                                      .turn_degrees = ro::render::kWandBoltTurnDegrees,
                                      .wand_orb_rgb = orb};
}

ro::sim::Actor ShooterWith(Element element) {
  ro::sim::Actor actor;
  actor.attack_element = element;
  return actor;
}

bool SamePoint(const glm::vec3& left, const glm::vec3& right) {
  return left.x == Approx(right.x) && left.y == Approx(right.y) && left.z == Approx(right.z);
}

}  // namespace

TEST_CASE("tir de baguette : l'option coupée garde exactement le sprite du trait",
          "[wand_orb_shot]") {
  const ProjectileLooks off{.element_glow = true, .wand_orb = false};
  const std::optional<std::array<std::uint8_t, 3>> orb =
      WandOrbRgbOf(ShooterWith(Element::kFire), ro::render::kWandBoltSpriteBase, off);
  REQUIRE_FALSE(orb.has_value());

  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"baguette.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kWandBoltSpriteBase, kShooter, kTarget, kFlightTicks,
                            WandFlight(orb)));
  REQUIRE(nodes.living_nodes().size() == 1);
  const ro::render::PooledEffectNode& bolt = nodes.living_nodes().front();
  CHECK(bolt.resource_paths ==
        std::vector<std::string>{std::string(ro::render::kWandBoltSpriteBase)});
  REQUIRE(bolt.node.render_word.has_value());
  CHECK(*bolt.node.render_word == ro::render::kEffectRenderWordOfActorSprite);
  CHECK(bolt.casts_shadow);
  CHECK(bolt.node.life_ticks == kOrbFlightTicks);
}

TEST_CASE("tir de baguette : la couleur de l'élément reçu, arcane sinon", "[wand_orb_shot]") {
  const ProjectileLooks on{};
  const auto rgb_of = [&on](Element element) {
    return WandOrbRgbOf(ShooterWith(element), ro::render::kWandBoltSpriteBase, on);
  };
  CHECK(rgb_of(Element::kFire) == ro::render::ElementHueRgb(ro::render::ElementHue::kFire));
  CHECK(rgb_of(Element::kHoly) == ro::render::ElementHueRgb(ro::render::ElementHue::kHoly));
  CHECK(rgb_of(Element::kNeutral) == ro::render::kWandOrbArcaneRgb);
  // 🔴 RIEN REÇU — un autre joueur en combat entre joueurs — : arcane.
  CHECK(rgb_of(Element::kNone) == ro::render::kWandOrbArcaneRgb);
  // LA LUEUR ÉLÉMENTAIRE COUPÉE : la forme reste, la couleur devient arcane.
  const ProjectileLooks without_glow{.element_glow = false, .wand_orb = true};
  CHECK(WandOrbRgbOf(ShooterWith(Element::kFire), ro::render::kWandBoltSpriteBase, without_glow) ==
        ro::render::kWandOrbArcaneRgb);
  // UNE FLÈCHE N'EST PAS UN TRAIT DE BAGUETTE : elle garde son sprite.
  CHECK_FALSE(
      WandOrbRgbOf(ShooterWith(Element::kFire), ro::render::kArrowSpriteBase, on).has_value());
}

TEST_CASE("tir de baguette : la boule remplace le sprite, ajoutée au décor, le cœur seul ombré",
          "[wand_orb_shot]") {
  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"baguette.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kWandBoltSpriteBase, kShooter, kTarget, kFlightTicks,
                            WandFlight(kFireRgb)));

  const std::span<const ro::render::PooledEffectNode> living = nodes.living_nodes();
  // Cœur, halo, éclat, et des étincelles en nombre borné.
  REQUIRE(living.size() > 3);
  CHECK(living.size() <= 3 + static_cast<std::size_t>(ro::render::kWandOrbSparkCountMax));
  std::size_t shadowed = 0;
  for (const ro::render::PooledEffectNode& pooled : living) {
    CHECK(pooled.resource_paths ==
          std::vector<std::string>{std::string(ro::render::kWandOrbSpritePath)});
    REQUIRE(pooled.node.render_word.has_value());
    CHECK(*pooled.node.render_word == ro::render::kEffectRenderWordOfAdditiveSprite);
    if (pooled.casts_shadow) {
      ++shadowed;
      CHECK(pooled.node.action == ro::render::WandOrbActionOf(ro::render::WandOrbPart::kCore));
    }
  }
  CHECK(shadowed == 1);

  // AU DÉPART, SEULS LE CŒUR ET LE HALO SE MONTRENT : étincelles et éclat
  // attendent sur leur image vide. Une silhouette, une ombre ronde.
  const std::span<const ro::render::DrawnActor> drawn = nodes.Drawn(kFirstFrameMs, GameCamera());
  CHECK(drawn.size() == 2);
  const moonclient::ProjectileCasters& casters = nodes.projectile_casters();
  CHECK(casters.silhouettes().size() == 1);
  CHECK(casters.GroundShadows(false).size() == 1);
  for (const moonclient::EffectNodesInPlay::SpriteNodeBatch& batch : nodes.SpriteBatches()) {
    if (!batch.actors.empty()) {
      CHECK(batch.additive);
    }
  }
}

TEST_CASE("tir de baguette : les étincelles suivent le vol, l'éclat paraît à l'arrivée, puis rien",
          "[wand_orb_shot]") {
  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"baguette.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kWandBoltSpriteBase, kShooter, kTarget, kFlightTicks,
                            WandFlight(kFireRgb)));
  const auto ticks_after = [](std::int32_t ticks) {
    return kFirstFrameMs + static_cast<std::uint32_t>(ticks) * kTickMs;
  };
  std::uint32_t now_ms = kFirstFrameMs;
  static_cast<void>(nodes.Drawn(now_ms, GameCamera()));
  bool sparks_seen = false;
  bool burst_seen = false;
  const std::int32_t all_done = kOrbFlightTicks + ro::render::kWandOrbBurstFrames + 2;
  for (std::int32_t tick = 1; tick <= all_done; ++tick) {
    now_ms = ticks_after(tick);
    const std::span<const ro::render::DrawnActor> drawn = nodes.Drawn(now_ms, GameCamera());
    if (tick < kOrbFlightTicks - 1 && drawn.size() > 2) {
      sparks_seen = true;
    }
    if (tick > kOrbFlightTicks + ro::render::kWandOrbSparkFrames && !drawn.empty()) {
      // Les dernières étincelles éteintes, seul l'éclat reste, sur la cible.
      for (const ro::render::DrawnActor& actor : drawn) {
        CHECK(SamePoint(actor.ground_position, kTarget));
      }
      burst_seen = true;
    }
  }
  CHECK(sparks_seen);
  CHECK(burst_seen);
  CHECK(nodes.living_nodes().empty());
}

TEST_CASE("tir de baguette : entre deux tics, le cœur, le halo et l'ombre se peignent ensemble",
          "[wand_orb_shot][blend]") {
  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"baguette.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.ShootSprite(ro::render::kWandBoltSpriteBase, kShooter, kTarget, kFlightTicks,
                            WandFlight(kFireRgb)));
  static_cast<void>(nodes.Drawn(kFirstFrameMs, GameCamera(), true));
  const std::span<const ro::render::DrawnActor> flying =
      nodes.Drawn(kFirstFrameMs + kTickAndAHalfMs, GameCamera(), true);
  REQUIRE(flying.size() == 2);
  const ro::render::PooledEffectNode& core = nodes.living_nodes().front();
  REQUIRE(core.casts_shadow);
  REQUIRE(core.previous_tick.has_value());
  CHECK(core.previous_tick->ground_position != core.ground_position);
  const glm::vec3 middle = (core.previous_tick->ground_position + core.ground_position) * 0.5F;
  for (const ro::render::DrawnActor& drawn : flying) {
    CHECK(SamePoint(drawn.ground_position, middle));
  }
  const moonclient::ProjectileCasters& casters = nodes.projectile_casters();
  REQUIRE(casters.GroundShadows(false).size() == 1);
  CHECK(SamePoint(casters.GroundShadows(false).front().ground_position, middle));
}
