// LES CLASSES D'EFFET VUES PAR L'APPLICATION : le niveau que le paquet d'une
// compétence annonce parvient à la classe qui le reçoit, et le givre d'une
// gelure sort en quads d'écran, sur l'image du client.

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_class.h"
#include "ro/render/effect_dye.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/render/skill_suite_table.h"
#include "ro/sim/actor.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_class_view.h"
#include "effects/effect_draw_gate.h"
#include "effects/effect_layers.h"
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

constexpr std::uint32_t kBearerId = 150'303;
constexpr std::uint32_t kPoseMs = 10'000;
constexpr std::uint32_t kTickMs = 16;
constexpr std::uint32_t kCarrotBeatClass = 1099;
constexpr std::uint32_t kCatnipMeteorClass = 1106;
constexpr ro::sim::StatusEffectId kFrostbite = 1208;
constexpr std::uint32_t kFrostbiteScreen = 2272;
constexpr float kViewportWidth = 800.0F;
constexpr float kViewportHeight = 600.0F;
constexpr std::int32_t kFrostSide = 4;
// Un tour de teinte d'un tiers, qui envoie le rouge au vert.
constexpr int kGreenTurnDegrees = 120;

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

// Un client dont l'archive ne porte que l'image du givre, carrée de quatre.
ro::vfs::Vfs FrostClient(const TempDirectory& client) {
  const std::vector<TgaPixel> pixels(static_cast<std::size_t>(kFrostSide * kFrostSide),
                                     TgaPixel{200, 220, 255, 255});
  client.WriteFile("givre.grf",
                   BuildGrf({GrfFileSpec{"data\\texture\\effect\\state_effect\\frostbite\\"
                                         "frostbite_screen\\frostbite_screen.tga",
                                         BuildTga(kFrostSide, kFrostSide, pixels)}}));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"givre.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

struct Bench {
  TempDirectory client;
  ro::vfs::Vfs vfs = FrostClient(client);
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
    world.set_player_id(kBearerId);
    effects.Classes().SeeScene(ro::render::Camera{}, kViewportWidth, kViewportHeight, false);
    effects.NoticeStateEffects(world, stage, planner, kPoseMs);
    // Le premier passage ne fait que poser l'horloge des classes.
    effects.Classes().Advance(world, stage, planner, kPoseMs);
  }

  // Une étape de suite qui fait naître cette classe dans le monde, près de la
  // cible, comme les étapes des Summoners.
  bool SpawnSkill(std::uint32_t effect_id, std::int32_t skill_level) {
    ro::render::SkillSuiteStep step;
    step.kind = ro::render::SkillSuiteKind::kEffect;
    step.carrier = ro::render::SkillSuiteCarrier::kWorldNearTarget;
    step.effect_id = effect_id;
    const ro::sim::Actor* bearer = world.Find(kBearerId);
    REQUIRE(bearer != nullptr);
    return moonclient::SpawnSkillClass(effects.Classes(), world, stage, planner, *bearer, kBearerId,
                                       kPoseMs, step, 0, skill_level)
        .has_value();
  }

  void Beat() { effects.Classes().Advance(world, stage, planner, kPoseMs + kTickMs); }
};

}  // namespace

TEST_CASE("le niveau annoncé par le paquet parvient aux carottes de Lunatic Carrot Beat",
          "[effect_class_app]") {
  // Au niveau trois, cinq carottes tombent : la classe vit tant qu'elles
  // tombent.
  Bench with_level;
  REQUIRE(with_level.SpawnSkill(kCarrotBeatClass, 3));
  with_level.Beat();
  CHECK(with_level.effects.Classes().Carries(kCarrotBeatClass, 0));
  // Un niveau nul ne sème aucune carotte : la classe n'a rien à montrer et
  // s'en va au premier battement.
  Bench without_level;
  REQUIRE(without_level.SpawnSkill(kCarrotBeatClass, 0));
  without_level.Beat();
  CHECK_FALSE(without_level.effects.Classes().Carries(kCarrotBeatClass, 0));
  // La boule de Catnip Meteor ne reçoit pas le niveau : elle tombe, quel qu'il
  // soit.
  Bench catnip;
  REQUIRE(catnip.SpawnSkill(kCatnipMeteorClass, 0));
  catnip.Beat();
  CHECK(catnip.effects.Classes().Carries(kCatnipMeteorClass, 0));
}

TEST_CASE("le givre d'une gelure sort en un lot de quatre quads d'écran, au demi-texel du client",
          "[effect_class_app]") {
  Bench bench;
  REQUIRE(bench.world.SetStatusEffect(kBearerId, kFrostbite, true, std::nullopt));
  bench.effects.NoticeStateEffects(bench.world, bench.stage, bench.planner, kPoseMs);
  REQUIRE(bench.effects.Carries(kFrostbiteScreen, kBearerId));
  bench.Beat();
  REQUIRE(bench.effects.Classes().Carries(kFrostbiteScreen, kBearerId));
  const std::span<const ro::render::DrawnScreenQuads> lots = bench.effects.Classes().ScreenQuads();
  REQUIRE(lots.size() == 1);
  REQUIRE(lots.front().quads.size() == 4);
  REQUIRE(lots.front().texture != nullptr);
  CHECK(lots.front().texture->width == kFrostSide);
  // Le quadrant haut-gauche lit l'image de droite à gauche : son coin haut-gauche
  // est au bord droit de l'image, un demi-texel plus loin.
  const ro::render::EffectScreenQuad& top_left = lots.front().quads[0];
  CHECK(top_left.corners[0] == glm::vec2(0.0F, 0.0F));
  CHECK(top_left.corners[2] == glm::vec2(400.0F, 300.0F));
  CHECK(top_left.texture_uv[0].x == Approx(1.125F));
  CHECK(top_left.texture_uv[0].y == Approx(0.125F));
  CHECK(top_left.texture_uv[2].x == Approx(0.125F));
  CHECK(top_left.texture_uv[2].y == Approx(1.125F));
  // La gelure levée, l'écran se dégage.
  REQUIRE(bench.world.SetStatusEffect(kBearerId, kFrostbite, false, std::nullopt));
  bench.effects.NoticeStateEffects(bench.world, bench.stage, bench.planner, kPoseMs + kTickMs);
  bench.effects.Classes().Advance(bench.world, bench.stage, bench.planner, kPoseMs + 2 * kTickMs);
  CHECK(bench.effects.Classes().ScreenQuads().empty());
}

TEST_CASE("une classe dont le porteur se cache vit, mais ne se dessine pas", "[effect_class_app]") {
  // 🔴 LE CLIENT GARDE AUSSI LE DESSIN DES CLASSES : la case qui le précède dans
  // leur table virtuelle juge le porteur comme la porte des nœuds. L'instance
  // bat ; seul son dessin saute.
  Bench bench;
  REQUIRE(bench.world.SetStatusEffect(kBearerId, kFrostbite, true, std::nullopt));
  bench.effects.NoticeStateEffects(bench.world, bench.stage, bench.planner, kPoseMs);
  bench.Beat();
  REQUIRE(bench.effects.Classes().ScreenQuads().size() == 1);
  // Le porteur devient un autre joueur que nous, sous Cloaking.
  constexpr std::uint32_t kSomeoneElse = 150'304;
  bench.world.set_player_id(kSomeoneElse);
  REQUIRE(bench.world.SetStates(kBearerId, 0, 0, ro::sim::kOptionCloak, kPoseMs));
  bench.effects.NoticeDrawGate(bench.world, nullptr);
  bench.effects.Classes().Advance(bench.world, bench.stage, bench.planner, kPoseMs + 2 * kTickMs);
  CHECK(bench.effects.Classes().Carries(kFrostbiteScreen, kBearerId));
  CHECK(bench.effects.Classes().ScreenQuads().empty());
  // Il se montre : le givre revient.
  REQUIRE(bench.world.SetStates(kBearerId, 0, 0, 0, kPoseMs));
  bench.effects.NoticeDrawGate(bench.world, nullptr);
  bench.effects.Classes().Advance(bench.world, bench.stage, bench.planner, kPoseMs + 3 * kTickMs);
  CHECK(bench.effects.Classes().ScreenQuads().size() == 1);
}

TEST_CASE("l'ombre de Shadow Exceed demande l'aura de son porteur en s'en allant",
          "[effect_class_app]") {
  constexpr std::uint32_t kShadowExceedClass = 1867;
  constexpr std::uint32_t kShadowExceedAura = 1004;
  Bench bench;
  const ro::sim::Actor* bearer = bench.world.Find(kBearerId);
  REQUIRE(bearer != nullptr);
  const std::array<float, 3> ground = GroundPositionAt(
      bench.stage, static_cast<float>(bearer->position.x), static_cast<float>(bearer->position.y));
  // Semée par son numéro, la classe suit son porteur.
  REQUIRE(bench.effects.PlayById(kShadowExceedClass, glm::vec3(ground[0], ground[1], ground[2]),
                                 kPoseMs, kBearerId));
  CHECK(bench.effects.Classes().Carries(kShadowExceedClass, kBearerId));
  // Aucun de ses `.str` ne se lit dans cette archive : ses paires se vident au
  // premier battement, et l'aura est demandée sur le porteur, une fois.
  bench.Beat();
  const std::vector<moonclient::EffectClassesInPlay::OwnerSpawn> asked =
      bench.effects.Classes().TakeOwnerSpawns();
  REQUIRE(asked.size() == 1);
  CHECK(asked.front().effect_id == kShadowExceedAura);
  CHECK(asked.front().owner_id == kBearerId);
  CHECK(bench.effects.Classes().TakeOwnerSpawns().empty());
}

TEST_CASE("l'aura que l'ombre laisse s'inscrit sur son porteur, et une seule fois",
          "[effect_class_app]") {
  constexpr std::uint32_t kShadowExceedClass = 1867;
  constexpr std::uint32_t kShadowExceedAura = 1004;
  Bench bench;
  const ro::sim::Actor* bearer = bench.world.Find(kBearerId);
  REQUIRE(bearer != nullptr);
  const std::array<float, 3> ground = GroundPositionAt(
      bench.stage, static_cast<float>(bearer->position.x), static_cast<float>(bearer->position.y));
  const glm::vec3 place(ground[0], ground[1], ground[2]);
  REQUIRE(bench.effects.PlayById(kShadowExceedClass, place, kPoseMs, kBearerId));
  bench.Beat();
  // L'aura ne dessine rien : elle change son porteur, et c'est sur lui qu'elle
  // s'inscrit quand les demandes des classes sont servies.
  bench.effects.SowRequestedEffects(bench.world, bench.stage, bench.planner, kPoseMs + kTickMs);
  moonclient::SettleActorEffects(bench.world, bench.effects.TakeActorEffects());
  const auto worn = [&]() {
    return std::ranges::find(bearer->effects, kShadowExceedAura, &ro::sim::PlayedEffect::id);
  };
  REQUIRE(worn() != bearer->effects.end());
  const std::uint32_t since_ms = worn()->since_ms;
  // Une seconde ombre qui s'en va ne repose pas l'aura : le porteur l'a déjà.
  REQUIRE(bench.effects.PlayById(kShadowExceedClass, place, kPoseMs + kTickMs, kBearerId));
  bench.effects.Classes().Advance(bench.world, bench.stage, bench.planner, kPoseMs + 2 * kTickMs);
  bench.effects.SowRequestedEffects(bench.world, bench.stage, bench.planner, kPoseMs + 2 * kTickMs);
  moonclient::SettleActorEffects(bench.world, bench.effects.TakeActorEffects());
  REQUIRE(worn() != bearer->effects.end());
  CHECK(worn()->since_ms == since_ms);
}

TEST_CASE("une classe née d'un chapeau prend la teinture de son porteur, pas celle d'un état",
          "[effect_class_app]") {
  // La même classe, le même porteur : seule la marque « chapeau » de
  // l'instance décide. Le givre né de la gelure reste d'origine.
  Bench bench;
  REQUIRE(bench.world.SetStatusEffect(kBearerId, kFrostbite, true, std::nullopt));
  bench.effects.NoticeStateEffects(bench.world, bench.stage, bench.planner, kPoseMs);
  moonclient::EffectDrawGate gate;
  gate.hat_dyes.emplace(std::pair{kBearerId, kFrostbiteScreen},
                        ro::render::MakeEffectDye(kGreenTurnDegrees, 0, 0, false));
  bench.effects.Classes().NoticeDrawGate(gate);
  bench.Beat();
  REQUIRE(bench.effects.Classes().ScreenQuads().size() == 1);
  CHECK_FALSE(bench.effects.Classes().ScreenQuads().front().dye.has_value());

  // Née d'un chapeau, elle se teint.
  Bench hat;
  REQUIRE(hat.world.SetStatusEffect(kBearerId, kFrostbite, true, std::nullopt));
  REQUIRE(hat.effects.Classes().Spawn(
      {.effect_id = kFrostbiteScreen, .owner_id = kBearerId, .from_hat = true}, kPoseMs));
  hat.effects.Classes().NoticeDrawGate(gate);
  hat.Beat();
  REQUIRE_FALSE(hat.effects.Classes().ScreenQuads().empty());
  const auto& lot = hat.effects.Classes().ScreenQuads().front();
  REQUIRE(lot.dye.has_value());
  CHECK(*lot.dye == ro::render::MakeEffectDye(kGreenTurnDegrees, 0, 0, false));
}
