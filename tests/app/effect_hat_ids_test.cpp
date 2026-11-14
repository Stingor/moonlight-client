// LES CHAPEAUX QUI NOMMENT UN IDENTIFIANT : un changement d'acteur se pose une
// fois sur le porteur et part avec le costume ; un cercle procédural se sème et
// s'éteint quand on l'ôte ; un `.str` se pose à l'ancre du chapeau, avant les
// corps quand la fiche le demande.

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/actor_scale.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/hat_effect_table.h"
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
#include "items/doll_body.h"
#include "support/effect_builder.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/map_builder.h"
#include "support/temp_directory.h"
#include "world/actor_ghosts.h"
#include "world/actor_tint.h"
#include "world/map_stage.h"

using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsw;
using ro::test_support::Bytes;
using ro::test_support::GndBuildOptions;
using ro::test_support::GrfFileSpec;
using ro::test_support::RswBuildOptions;
using ro::test_support::TempDirectory;
using ActorEffectToRecord = moonclient::EffectsInPlay::ActorEffectToRecord;

namespace {

constexpr std::uint32_t kBearerId = 150'303;
constexpr std::uint32_t kWearMs = 10'000;
constexpr std::uint32_t kLaterMs = 10'500;
constexpr std::uint32_t kRemoveMs = 11'000;

// Les rangs de l'installation, et l'identifiant que chacun nomme.
constexpr ro::sim::HatEffectId kShrinkRank = 17;         // 421
constexpr ro::sim::HatEffectId kCirclePowerRank = 26;    // 1122
constexpr ro::sim::HatEffectId kKagemusyaRank = 31;      // 1004
constexpr ro::sim::HatEffectId kWhiteBodyRank = 36;      // 1065
constexpr ro::sim::HatEffectId kPoisonMasterRank = 163;  // 2310
constexpr ro::sim::HatEffectId kCosmicRank = 222;        // un `.str`, -11
constexpr ro::sim::HatEffectId kHeadGlowRank = 223;      // un `.str` qui suit la tête
constexpr std::uint32_t kShrinkEffect = 421;
constexpr std::uint32_t kCirclePowerEffect = 1122;
constexpr std::uint32_t kKagemusyaEffect = 1004;
constexpr std::uint32_t kWhiteBodyEffect = 1065;
constexpr std::uint32_t kPoisonMasterEffect = 2310;

constexpr std::string_view kHatTable = R"(
version: 1
hat_effects:
  17: {name: 'HAT_EF_Shrink', effect_id: 421}
  26: {name: 'HAT_EF_CIRCLEPOWER', effect_id: 1122}
  31: {name: 'HAT_EF_KAGEMUSYA', effect_id: 1004}
  36: {name: 'HAT_EF_WHITEBODY2', effect_id: 1065}
  163: {name: 'HAT_EF_POISON_MASTER', effect_id: 2310}
  222: {name: 'HAT_EF_Cosmic_Connection', file: 'cosmic.str', offset: -11,
        render_before_character: true}
  223: {name: 'HAT_EF_Glow_Of_New_Year', file: 'cosmic.str', offset: -11, attached_head: true}
)";
constexpr std::string_view kCosmicPath = "data\\texture\\effect\\cosmic.str";
constexpr std::string_view kCircleTexture = "data\\texture\\effect\\emp shock.tga";

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

ro::vfs::Vfs HatClient(const TempDirectory& client) {
  const ro::test_support::TgaPixel white{255, 255, 255, 255};
  std::vector<GrfFileSpec> files = {
      {std::string(ro::render::kHatEffectTablePath), Bytes(kHatTable)},
      {std::string(kCircleTexture), ro::test_support::BuildTga(2, 2, {white, white, white, white})},
      {std::string(kCosmicPath), ro::test_support::BuildStr({.last_frame = 30})},
  };
  client.WriteFile("chapeaux.grf", BuildGrf(files));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"chapeaux.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

struct Bench {
  TempDirectory client;
  ro::vfs::Vfs vfs = HatClient(client);
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};

  Bench() {
    ro::sim::Actor bearer;
    bearer.id = kBearerId;
    bearer.kind = ro::sim::ActorKind::kPlayer;
    world.Spawn(bearer);
  }

  std::vector<ActorEffectToRecord> Wear(ro::sim::HatEffectId rank, bool on, std::uint32_t now_ms) {
    REQUIRE(world.SetHatEffect(kBearerId, rank, on));
    return Notice(now_ms);
  }

  std::vector<ActorEffectToRecord> Notice(std::uint32_t now_ms) {
    effects.NoticeHatEffects(world, stage, planner, now_ms);
    std::vector<ActorEffectToRecord> poses = effects.TakeActorEffects();
    moonclient::SettleActorEffects(world, poses);
    return poses;
  }

  [[nodiscard]] bool Wears(std::uint32_t effect_id) const {
    const auto& played = world.Find(kBearerId)->effects;
    return std::ranges::any_of(played, [effect_id](const ro::sim::PlayedEffect& effect) {
      return effect.id == effect_id;
    });
  }
};

[[nodiscard]] std::size_t Count(std::span<const ActorEffectToRecord> poses, std::uint32_t effect_id,
                                bool withdrawn) {
  return static_cast<std::size_t>(
      std::ranges::count_if(poses, [&](const ActorEffectToRecord& pose) {
        return pose.effect_id == effect_id && pose.withdrawn == withdrawn &&
               pose.actor_id == kBearerId;
      }));
}

// La hauteur qu'une monture du shop ajoute à l'ancre d'un adulte : 23 au lieu
// de 18, en unités du monde.
constexpr float kCashMountRiseUnits = 5.0F;
constexpr float kAnchorToleranceUnits = 0.001F;

// LE PANTIN : un corps d'un calque, dont l'animation dure une seconde.
constexpr float kDollCycleMs = 1000.0F;
constexpr std::uint32_t kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
const glm::vec3 kDollFeet{0.0F, 0.0F, 0.0F};

[[nodiscard]] moonclient::DollBody OneLayerDoll() {
  moonclient::DollBody body;
  body.composed.quads.resize(1);
  body.composed.sources.resize(1);
  body.animation_duration_ms = kDollCycleMs;
  return body;
}

// Ce que le pantin peint à cet instant pour le porteur du banc, ou rien quand
// l'image composée suffit.
struct DollPaint {
  bool overlaid = false;
  std::vector<ro::render::DrawnActor> draws;
};

[[nodiscard]] DollPaint PaintDoll(const Bench& bench, const moonclient::DollBody& body,
                                  moonclient::ActorGhosts& ghosts, std::uint32_t now_ms) {
  std::vector<ro::render::ActorEffectPlay> played;
  moonclient::PlayedEffectsOf(*bench.world.Find(kBearerId), now_ms, played);
  DollPaint paint;
  paint.overlaid = moonclient::ComposeChangedDollBody(played, body, kBearerId, kDollFeet, ghosts,
                                                      now_ms, paint.draws);
  return paint;
}

}  // namespace

TEST_CASE("Chapeau : l'ancre d'un .str se relit à chaque image, monture du shop comprise",
          "[hat_ids]") {
  Bench bench;
  bench.effects.SeeScene(ro::render::Camera{}, {});
  static_cast<void>(bench.Wear(kCosmicRank, true, kWearMs));
  const std::array<float, 3> feet = moonclient::GroundPositionAt(
      bench.stage, static_cast<float>(bench.world.Find(kBearerId)->position.x),
      static_cast<float>(bench.world.Find(kBearerId)->position.y));
  const glm::vec3 feet_position(feet[0], feet[1], feet[2]);
  static_cast<void>(bench.effects.Drawn(kWearMs, bench.stage, bench.world, bench.planner));
  REQUIRE(bench.effects.DrawnBehindActors().size() == 1);
  // À pied, -11 : aux pieds.
  CHECK(glm::distance(bench.effects.DrawnBehindActors().front().ground_position, feet_position) ==
        Catch::Approx(0.0F).margin(kAnchorToleranceUnits));
  // En selle sur une monture du shop, la base passe à vingt-trois : 23 - 11 - 7,
  // cinq unités plus haut — À L'IMAGE SUIVANTE, sans attendre un resemis.
  REQUIRE(bench.world.SetStatusEffect(kBearerId, ro::sim::kStatusAllRiding, true));
  // Deux tics plus tard : le fichier joue encore, et rien ne l'a resemé.
  constexpr std::uint32_t kTwoTicksLaterMs = kWearMs + 2 * kTickMs;
  static_cast<void>(bench.effects.Drawn(kTwoTicksLaterMs, bench.stage, bench.world, bench.planner));
  REQUIRE(bench.effects.DrawnBehindActors().size() == 1);
  CHECK(glm::distance(bench.effects.DrawnBehindActors().front().ground_position, feet_position) ==
        Catch::Approx(kCashMountRiseUnits).margin(kAnchorToleranceUnits));
}

TEST_CASE("Chapeau : une fiche qui suit la tête descend avec elle quand on s'assied", "[hat_ids]") {
  // Assis, la tête d'un novice descend de dix-neuf pixels de sprite.
  constexpr float kSeatedHeadDropPixels = 19.0F;
  Bench bench;
  std::map<std::uint32_t, glm::vec2> head_shifts;
  moonclient::EffectActorView view;
  view.head_shifts = &head_shifts;
  bench.effects.SeeScene(ro::render::Camera{}, view);
  static_cast<void>(bench.Wear(kHeadGlowRank, true, kWearMs));
  std::span<const ro::render::DrawnEffect> drawn =
      bench.effects.Drawn(kWearMs, bench.stage, bench.world, bench.planner);
  REQUIRE(drawn.size() == 1);
  const glm::vec3 standing = drawn.front().ground_position;
  // La scène suivante compose le porteur assis : l'effet suit à l'image même.
  head_shifts[kBearerId] = glm::vec2(0.0F, kSeatedHeadDropPixels);
  constexpr std::uint32_t kTwoTicksLaterMs = kWearMs + 2 * kTickMs;
  drawn = bench.effects.Drawn(kTwoTicksLaterMs, bench.stage, bench.world, bench.planner);
  REQUIRE(drawn.size() == 1);
  const glm::vec3 seated = drawn.front().ground_position;
  CHECK(glm::distance(seated, standing) ==
        Catch::Approx(kSeatedHeadDropPixels / ro::render::kSpritePixelsPerWorldUnit)
            .margin(kAnchorToleranceUnits));
  // Vers le BAS de l'écran : la caméra regarde de haut, l'effet perd de la
  // hauteur.
  CHECK(seated.y < standing.y);
  // Relevé, il revient.
  head_shifts.clear();
  drawn =
      bench.effects.Drawn(kTwoTicksLaterMs + 2 * kTickMs, bench.stage, bench.world, bench.planner);
  REQUIRE(drawn.size() == 1);
  CHECK(glm::distance(drawn.front().ground_position, standing) ==
        Catch::Approx(0.0F).margin(kAnchorToleranceUnits));
}

TEST_CASE("Chapeau : le .str d'un porteur qui vole monte avec sa lévitation", "[hat_ids]") {
  // Debout, la lévitation du vol de l'union tient vingt pixels de sprite.
  constexpr float kHoverPixels = 20.0F;
  Bench bench;
  std::map<std::uint32_t, float> hover_pixels;
  moonclient::EffectActorView view;
  view.hover_pixels = &hover_pixels;
  bench.effects.SeeScene(ro::render::Camera{}, view);
  static_cast<void>(bench.Wear(kHeadGlowRank, true, kWearMs));
  std::span<const ro::render::DrawnEffect> drawn =
      bench.effects.Drawn(kWearMs, bench.stage, bench.world, bench.planner);
  REQUIRE(drawn.size() == 1);
  const glm::vec3 grounded = drawn.front().ground_position;
  // La scène suivante compose le porteur en vol : l'effet monte à l'image même.
  hover_pixels[kBearerId] = kHoverPixels;
  constexpr std::uint32_t kTwoTicksLaterMs = kWearMs + 2 * kTickMs;
  drawn = bench.effects.Drawn(kTwoTicksLaterMs, bench.stage, bench.world, bench.planner);
  REQUIRE(drawn.size() == 1);
  const glm::vec3 hovering = drawn.front().ground_position;
  CHECK(glm::distance(hovering, grounded) ==
        Catch::Approx(kHoverPixels / ro::render::kSpritePixelsPerWorldUnit)
            .margin(kAnchorToleranceUnits));
  CHECK(hovering.y > grounded.y);
  // Posé, il redescend.
  hover_pixels.clear();
  drawn =
      bench.effects.Drawn(kTwoTicksLaterMs + 2 * kTickMs, bench.stage, bench.world, bench.planner);
  REQUIRE(drawn.size() == 1);
  CHECK(glm::distance(drawn.front().ground_position, grounded) ==
        Catch::Approx(0.0F).margin(kAnchorToleranceUnits));
}

TEST_CASE("Pantin : sans chapeau qui change l'acteur, l'image composée suffit", "[hat_ids]") {
  Bench bench;
  const moonclient::DollBody body = OneLayerDoll();
  moonclient::ActorGhosts ghosts;
  CHECK_FALSE(PaintDoll(bench, body, ghosts, kWearMs).overlaid);
  // Un `.str` ne change pas l'acteur : le pantin reste l'image.
  static_cast<void>(bench.Wear(kCosmicRank, true, kWearMs));
  const DollPaint paint = PaintDoll(bench, body, ghosts, kLaterMs);
  CHECK_FALSE(paint.overlaid);
  CHECK(paint.draws.empty());
}

TEST_CASE("Pantin : Shrink le rapetisse de moitié, aux pieds", "[hat_ids]") {
  Bench bench;
  const moonclient::DollBody body = OneLayerDoll();
  moonclient::ActorGhosts ghosts;
  static_cast<void>(bench.Wear(kShrinkRank, true, kWearMs));
  const DollPaint paint = PaintDoll(bench, body, ghosts, kLaterMs);
  REQUIRE(paint.overlaid);
  REQUIRE(paint.draws.size() == 1);
  CHECK(paint.draws.back().actor == &body.composed);
  CHECK(paint.draws.back().scale == Catch::Approx(ro::render::kSmallBodyScale));
  CHECK(paint.draws.back().ground_position == kDollFeet);
  CHECK(paint.draws.back().additive_parts == 0);
  // Ôté, le pantin redevient l'image.
  static_cast<void>(bench.Wear(kShrinkRank, false, kRemoveMs));
  CHECK_FALSE(PaintDoll(bench, body, ghosts, kRemoveMs).overlaid);
}

TEST_CASE("Pantin : WhiteBody2 rend additifs le corps et ce qu'il porte sur la tête", "[hat_ids]") {
  Bench bench;
  const moonclient::DollBody body = OneLayerDoll();
  moonclient::ActorGhosts ghosts;
  static_cast<void>(bench.Wear(kWhiteBodyRank, true, kWearMs));
  const DollPaint paint = PaintDoll(bench, body, ghosts, kLaterMs);
  REQUIRE(paint.overlaid);
  REQUIRE(paint.draws.size() == 1);
  CHECK(paint.draws.back().additive_parts == ro::render::kBodyAndHeadwearParts);
  CHECK(paint.draws.back().scale == Catch::Approx(1.0F));
  // La teinte qu'il écrit est le blanc : la couleur ne change pas.
  CHECK(paint.draws.back().tint == std::array<std::uint8_t, 4>{255, 255, 255, 255});
}

TEST_CASE("Pantin : Kagemusya sème ses ombres derrière lui, grises et translucides", "[hat_ids]") {
  Bench bench;
  const moonclient::DollBody body = OneLayerDoll();
  moonclient::ActorGhosts ghosts;
  static_cast<void>(bench.Wear(kKagemusyaRank, true, kWearMs));
  const ro::render::ActorEffectChange* change = ro::render::ActorEffectChangeFor(kKagemusyaEffect);
  REQUIRE(change != nullptr);
  REQUIRE(change->clone_period_ticks > 0);
  // Une ombre par période, pendant trois périodes.
  constexpr std::uint32_t kPeriods = 3;
  DollPaint paint;
  for (std::uint32_t tick = 0; tick <= kPeriods * change->clone_period_ticks; ++tick) {
    paint = PaintDoll(bench, body, ghosts, kWearMs + tick * kTickMs);
  }
  REQUIRE(paint.overlaid);
  REQUIRE(paint.draws.size() == kPeriods + 2);
  // Le corps en dernier, opaque ; ses ombres avant lui, translucides.
  CHECK(paint.draws.back().actor == &body.composed);
  CHECK(paint.draws.back().tint[3] == 255);
  for (std::size_t rank = 0; rank + 1 < paint.draws.size(); ++rank) {
    CHECK(paint.draws[rank].tint[3] < 255);
  }
}

TEST_CASE("Chapeau : un changement d'acteur se pose une fois et part avec le costume",
          "[hat_ids]") {
  for (const auto& [rank, effect_id] :
       {std::pair{kShrinkRank, kShrinkEffect}, std::pair{kKagemusyaRank, kKagemusyaEffect},
        std::pair{kWhiteBodyRank, kWhiteBodyEffect}}) {
    INFO("rang " << rank << ", effet " << effect_id);
    Bench bench;
    CHECK(Count(bench.Wear(rank, true, kWearMs), effect_id, false) == 1);
    CHECK(bench.Wears(effect_id));
    // Redemandé à l'image suivante, il n'est pas reposé.
    CHECK(Count(bench.Notice(kLaterMs), effect_id, false) == 0);
    CHECK(Count(bench.Wear(rank, false, kRemoveMs), effect_id, true) == 1);
    CHECK_FALSE(bench.Wears(effect_id));
  }
}

TEST_CASE("Chapeau : le cercle de 1122 se sème, suit son porteur et part avec le costume",
          "[hat_ids]") {
  Bench bench;
  static_cast<void>(bench.Wear(kCirclePowerRank, true, kWearMs));
  CHECK(bench.effects.Carries(kCirclePowerEffect, kBearerId));
  static_cast<void>(bench.Wear(kCirclePowerRank, false, kRemoveMs));
  CHECK_FALSE(bench.effects.Carries(kCirclePowerEffect, kBearerId));
}

TEST_CASE("Chapeau : 2310 ne montre rien, comme chez le client", "[hat_ids]") {
  Bench bench;
  CHECK(bench.Wear(kPoisonMasterRank, true, kWearMs).empty());
  CHECK_FALSE(bench.effects.Carries(kPoisonMasterEffect, kBearerId));
}

TEST_CASE("Chapeau : un .str à -11 se pose aux pieds, sans relèvement, avant les corps",
          "[hat_ids]") {
  Bench bench;
  static_cast<void>(bench.Wear(kCosmicRank, true, kWearMs));
  static_cast<void>(bench.effects.Drawn(kWearMs, bench.stage, bench.world, bench.planner));
  const std::span<const ro::render::DrawnEffect> behind = bench.effects.DrawnBehindActors();
  REQUIRE(behind.size() == 1);
  CHECK(behind.front().lift_pixels == 0.0F);
  const std::array<float, 3> feet = moonclient::GroundPositionAt(
      bench.stage, static_cast<float>(bench.world.Find(kBearerId)->position.x),
      static_cast<float>(bench.world.Find(kBearerId)->position.y));
  CHECK(behind.front().ground_position == glm::vec3(feet[0], feet[1], feet[2]));
  // Ôté, il s'éteint à l'image même.
  static_cast<void>(bench.Wear(kCosmicRank, false, kRemoveMs));
  static_cast<void>(bench.effects.Drawn(kRemoveMs, bench.stage, bench.world, bench.planner));
  CHECK(bench.effects.DrawnBehindActors().empty());
}
