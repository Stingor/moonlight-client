// CE QU'UNE COMPÉTENCE POSÉE SUR UNE CASE MONTRE, ET COMBIEN DE TEMPS.
//
// Le client fait l'image d'un ZC_NOTIFY_GROUNDSKILL par un répartiteur qui,
// selon la compétence, sème l'effet par un LANCEUR sans corps posé sur la case
// — qui le porte et l'emporte en mourant —, le pose sur la case, ou le pose sur
// le lanceur du sort. Il joue aussi ses propres sons, et commande ce qu'il
// vient de faire naître.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/formats/str.h"
#include "ro/net/world_update.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_duration.h"
#include "ro/render/effect_fading_streak_seed.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_table.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/effect_builder.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/map_builder.h"
#include "support/net_frames.h"
#include "support/temp_directory.h"
#include "world/actor_sounds.h"
#include "world/map_stage.h"

using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;
using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsw;
using ro::test_support::BuildStr;
using ro::test_support::BuildTga;
using ro::test_support::GndBuildOptions;
using ro::test_support::GrfFileSpec;
using ro::test_support::RswBuildOptions;
using ro::test_support::StrBuildOptions;
using ro::test_support::StrKeyFrameSpec;
using ro::test_support::StrLayerSpec;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::uint32_t kCasterId = 150'021;
constexpr std::uint32_t kNowMs = 10'000;
constexpr std::uint16_t kSkillLevel = 5;
constexpr ro::sim::CellPosition kCasterCell{.x = 1, .y = 1};
constexpr ro::sim::CellPosition kAimedCell{.x = 3, .y = 3};
constexpr std::int32_t kGroundCells = 6;
// Ice Wall : 74 sur la case, porté par un lanceur de (250 + 10) × 24 ms.
constexpr std::uint16_t kIceWallSkill = 87;
constexpr std::uint32_t kIceWallEffect = 74;
constexpr std::int32_t kIceWallLauncherDeathTick = (250 + 10) * 24 / ro::render::kEffectTickMs + 1;
// Max Pain : 1032 sur le lanceur du sort.
constexpr std::uint16_t kMaxPainSkill = 717;
constexpr std::uint32_t kMaxPainEffect = 1032;
constexpr const char* kKyrieShellTexture = "data\\texture\\effect\\guardK.tga";
constexpr const char* kKyrieShellInnerTexture = "data\\texture\\effect\\guardK2.tga";
// Ground Drive : 1399 sur la case, que son instance emporte avant le lanceur.
constexpr std::uint16_t kGroundDriveSkill = 770;
constexpr std::uint32_t kGroundDriveEffect = 1399;
// NC_COLDSLOWER : 791 sur le lanceur, qui vise la case.
constexpr std::uint16_t kColdSlowerSkill = 2260;
constexpr std::uint32_t kColdSlowerEffect = 791;
// ABC_ABYSS_STRIKE : 2091 sur la case, dont le fichier part 880 ms plus tard.
constexpr std::uint16_t kAbyssStrikeSkill = 5319;
constexpr std::uint32_t kAbyssGroundEffect = 2091;
constexpr std::uint32_t kAbyssStartDelayMs = 880;
// La compétence 774 : 1062 sur la case, dont le fichier saute 0,4 de ses images.
constexpr std::uint16_t kHammerSkill = 774;
constexpr std::uint32_t kHammerEffect = 1062;
constexpr std::int32_t kStrLastFrame = 100;
constexpr std::int32_t kHammerSkippedFrames = 40;
// HW_GANBANTEIN : son bruit à lui, poussé à l'origine du repère du client.
constexpr std::uint16_t kGanbanteinSkill = 483;
constexpr std::string_view kGanbanteinSound = "effect\\EF_FireWall.wav";
// SC_CHAOSPANIC : son bruit, que le gestionnaire du paquet joue lui-même.
constexpr std::uint16_t kChaosPanicSkill = 2301;
constexpr std::string_view kChaosPanicSound = "effect\\chaospanic.wav";
// 2417 : le gestionnaire fait trembler l'écran six cent cinquante millisecondes.
constexpr std::uint16_t kQuakingSkill = 2417;
constexpr std::uint32_t kQuakeMs = 650;
// 736 lancée par une créature — un métier affiché de créature.
constexpr std::uint16_t kCreaturePsychicWaveSkill = 736;
constexpr std::string_view kPsychicWaveSound = "effect\\S사이킥웨이브.wav";
constexpr std::uint16_t kCreatureJob = 1002;
// Sous le plafond de rattrapage de la troupe.
constexpr std::int32_t kSliceTicks = 30;
constexpr std::int32_t kTexturePixels = 4;

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

// Un client qui ne porte qu'une image blanche pour chaque texture que les
// patrons des trois effets demandent : sans elle, un nœud ne se sème pas.
ro::vfs::Vfs WhiteTexturesClient(const TempDirectory& client) {
  const std::vector<TgaPixel> white(static_cast<std::size_t>(kTexturePixels * kTexturePixels),
                                    TgaPixel{.red = 255, .green = 255, .blue = 255});
  const std::vector<BmpPixel> white_bmp(static_cast<std::size_t>(kTexturePixels * kTexturePixels),
                                        BmpPixel{.red = 255, .green = 255, .blue = 255});
  std::vector<GrfFileSpec> files;
  for (const std::uint32_t effect_id : {kIceWallEffect, kMaxPainEffect, kGroundDriveEffect}) {
    for (const ro::render::EffectNodePattern& pattern :
         ro::render::EffectNodePatternsFor(effect_id)) {
      for (const std::string_view path : ro::render::EffectNodePatternTextures(pattern)) {
        // Le décodeur se choisit par l'extension.
        const bool bmp = path.ends_with(".bmp");
        files.push_back(
            GrfFileSpec{.name = std::string(path),
                        .content = bmp ? BuildBmp(kTexturePixels, kTexturePixels, white_bmp)
                                       : BuildTga(kTexturePixels, kTexturePixels, white)});
      }
    }
  }
  // Et les deux images du bouclier de 1032, qu'aucun patron de nœud ne nomme.
  for (const char* path : {kKyrieShellTexture, kKyrieShellInnerTexture}) {
    files.push_back(
        GrfFileSpec{.name = path, .content = BuildTga(kTexturePixels, kTexturePixels, white)});
  }
  // Les images de la traînée de 791.
  for (const ro::render::EffectFadingStreakPattern& pattern :
       ro::render::EffectFadingStreakPatternsFor(kColdSlowerEffect)) {
    for (const std::string_view path : pattern.textures) {
      files.push_back(GrfFileSpec{.name = std::string(path),
                                  .content = BuildTga(kTexturePixels, kTexturePixels, white)});
    }
  }
  // Et les fichiers des deux effets que le répartiteur commande, d'une seule
  // couche : c'est leur horloge qu'on regarde.
  StrKeyFrameSpec image;
  image.offset_x = ro::formats::kStrOriginX;
  image.offset_y = ro::formats::kStrOriginY;
  StrLayerSpec layer;
  layer.key_frames = {image};
  StrBuildOptions str;
  str.last_frame = kStrLastFrame;
  str.layers = {layer};
  for (const std::uint32_t effect_id : {kAbyssGroundEffect, kHammerEffect}) {
    files.push_back(GrfFileSpec{
        .name = std::string(ro::render::kEffectFolder) + ro::render::EffectFile(effect_id, 0),
        .content = BuildStr(str)});
  }
  client.WriteFile("white.grf", BuildGrf(files));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"white.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

struct Bench {
  explicit Bench(std::uint16_t caster_job = 0) : vfs(WhiteTexturesClient(client)) {
    ro::sim::Actor caster;
    caster.id = kCasterId;
    caster.kind = caster_job == 0 ? ro::sim::ActorKind::kPlayer : ro::sim::ActorKind::kMonster;
    caster.look.job = caster_job;
    caster.position = kCasterCell;
    world.Spawn(caster);
  }

  void CastOnGround(std::uint16_t skill_id) {
    REQUIRE(world.RecordGroundSkill(
        kCasterId,
        ro::sim::GroundSkillUse{
            .skill_id = skill_id, .level = kSkillLevel, .cell = kAimedCell, .started_ms = kNowMs}));
    effects.NoticeSkills(world, stage, planner, kNowMs);
    static_cast<void>(nodes.Drawn(kNowMs, camera));
  }

  // La même, lancée par quelqu'un que le monde ne connaît pas.
  void CastOnGroundUnseen(std::uint16_t skill_id) {
    constexpr std::uint32_t kUnseenCasterId = 424'242;
    REQUIRE_FALSE(world.RecordGroundSkill(
        kUnseenCasterId,
        ro::sim::GroundSkillUse{
            .skill_id = skill_id, .level = kSkillLevel, .cell = kAimedCell, .started_ms = kNowMs}));
    effects.NoticeSkills(world, stage, planner, kNowMs);
    static_cast<void>(nodes.Drawn(kNowMs, camera));
  }

  // Avance la troupe jusqu'à `tick` tics après le lancer, par tranches.
  void AdvanceTo(std::int32_t tick) {
    while (elapsed_ticks < tick) {
      elapsed_ticks = std::min(tick, elapsed_ticks + kSliceTicks);
      static_cast<void>(nodes.Drawn(
          kNowMs + static_cast<std::uint32_t>(elapsed_ticks * ro::render::kEffectTickMs), camera));
    }
  }

  [[nodiscard]] std::size_t LivingFrom(std::uint32_t effect_id) const {
    std::size_t count = 0;
    for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
      count += node.sown_by_effect_id == effect_id ? 1U : 0U;
    }
    return count;
  }

  TempDirectory client;
  ro::vfs::Vfs vfs;
  ro::render::EffectCatalog catalog{vfs};
  moonclient::EffectNodesInPlay nodes{vfs};
  moonclient::EffectsInPlay effects{catalog, nodes, vfs};
  ro::sim::World world;
  moonclient::MapStage stage = FlatStage();
  ro::sim::WalkPlanner planner{moonclient::PlannedMapOf(stage)};
  const ro::render::Camera camera;
  std::int32_t elapsed_ticks = 0;
};

}  // namespace

TEST_CASE("Ice Wall : les pointes de la case partent avec leur lanceur", "[effect_ground_skill]") {
  Bench bench;
  bench.CastOnGround(kIceWallSkill);
  // Rien ne naissait sur la case : les tables fondues ne connaissent pas Ice Wall.
  REQUIRE(bench.LivingFrom(kIceWallEffect) > 0);
  for (const ro::render::PooledEffectNode& node : bench.nodes.living_nodes()) {
    CHECK(node.anchor_id == 0);
  }
  // L'instance se déclare dix mille tics ; le lanceur meurt bien avant.
  REQUIRE(ro::render::EffectNodeLifeFor(kIceWallEffect) > kIceWallLauncherDeathTick);
  bench.AdvanceTo(kIceWallLauncherDeathTick - 1);
  CHECK(bench.LivingFrom(kIceWallEffect) > 0);
  bench.AdvanceTo(kIceWallLauncherDeathTick);
  CHECK(bench.LivingFrom(kIceWallEffect) == 0);
}

TEST_CASE("Ice Wall : le bloc posé ne relance pas ses pointes", "[effect_ground_skill]") {
  // L'unité 141 entretient 74, un visuel PERMANENT : le client le crée une fois
  // et repousse la fin de son instance à chaque tic. Nous le reposions à la
  // durée de l'effet — deux cent cinquante tics, quatre secondes —, et les
  // pointes rejaillissaient du sol.
  constexpr std::uint32_t kIceWallUnitId = 7'700;
  constexpr std::uint32_t kIceWallUnit = 141;
  Bench bench;
  bench.world.PlaceSkillUnit(ro::sim::SkillUnit{.id = kIceWallUnitId,
                                                .cell = kAimedCell,
                                                .unit_id = kIceWallUnit,
                                                .skill_level = kSkillLevel,
                                                .placed_ms = kNowMs,
                                                .look_since_ms = kNowMs});
  bench.effects.NoticeSkillUnitVisuals(bench.world, bench.stage, kNowMs);
  static_cast<void>(bench.nodes.Drawn(kNowMs, bench.camera));
  const std::size_t sown = bench.LivingFrom(kIceWallEffect);
  REQUIRE(sown > 0);

  const std::int32_t past_duration_ticks = ro::render::EffectDurationTicks(kIceWallEffect) + 1;
  REQUIRE(past_duration_ticks < ro::render::EffectNodeLifeFor(kIceWallEffect));
  bench.AdvanceTo(past_duration_ticks);
  const std::uint32_t later_ms =
      kNowMs + static_cast<std::uint32_t>(past_duration_ticks * ro::render::kEffectTickMs);
  bench.effects.NoticeSkillUnitVisuals(bench.world, bench.stage, later_ms);
  static_cast<void>(bench.nodes.Drawn(later_ms, bench.camera));
  CHECK(bench.LivingFrom(kIceWallEffect) == sown);
  for (const ro::render::PooledEffectNode& node : bench.nodes.living_nodes()) {
    CHECK(node.node.age_ticks > 0);
  }
}

TEST_CASE("Ice Wall : cinq unités d'un groupe dressent cinq amas, chacun sur sa case",
          "[effect_ground_skill]") {
  // Le serveur pose le mur en cinq unités 141, une par case, par cinq 0x09CA.
  // Chacune sème ses trois éclats de 74, accrochés à elle et plantés sur SA
  // case : ni la compétence, ni l'effet, ni le groupe ne les confondent.
  constexpr std::uint32_t kFirstUnitId = 125'277;
  constexpr std::uint8_t kIceWallUnit = 141;
  constexpr std::uint8_t kWallCells = 5;
  constexpr std::size_t kShardsPerUnit = 3;
  // Un éclat s'écarte de son point de moins de deux unités, en profondeur.
  constexpr float kShardSpread = 2.0F;
  constexpr std::uint8_t kWallRow = 3;
  Bench bench;
  for (std::uint8_t rank = 0; rank < kWallCells; ++rank) {
    const std::uint32_t id = kFirstUnitId + rank;
    const std::vector<std::uint8_t> placed = {0xCA,
                                              0x09,
                                              0x17,
                                              0x00,
                                              static_cast<std::uint8_t>(id & 0xFFU),
                                              static_cast<std::uint8_t>((id >> 8U) & 0xFFU),
                                              static_cast<std::uint8_t>((id >> 16U) & 0xFFU),
                                              static_cast<std::uint8_t>(id >> 24U),
                                              static_cast<std::uint8_t>(kCasterId & 0xFFU),
                                              static_cast<std::uint8_t>((kCasterId >> 8U) & 0xFFU),
                                              static_cast<std::uint8_t>((kCasterId >> 16U) & 0xFFU),
                                              static_cast<std::uint8_t>(kCasterId >> 24U),
                                              rank,
                                              0x00,
                                              kWallRow,
                                              0x00,
                                              kIceWallUnit,
                                              0x00,
                                              0x00,
                                              0x00,
                                              0x00,
                                              0x01,
                                              static_cast<std::uint8_t>(kSkillLevel)};
    REQUIRE(ro::net::ApplyFrame(ro::test_support::FrameOf(placed), bench.world, kNowMs) ==
            ro::net::FrameOutcome::kApplied);
  }
  REQUIRE(bench.world.skill_units().size() == kWallCells);
  bench.effects.NoticeSkillUnitVisuals(bench.world, bench.stage, kNowMs);
  static_cast<void>(bench.nodes.Drawn(kNowMs, bench.camera));

  for (std::uint8_t rank = 0; rank < kWallCells; ++rank) {
    const std::uint32_t id = kFirstUnitId + rank;
    const std::array<float, 3> cell_ground =
        moonclient::GroundPositionAt(bench.stage, static_cast<float>(rank), kWallRow);
    std::size_t shards = 0;
    for (const ro::render::PooledEffectNode& node : bench.nodes.living_nodes()) {
      if (node.sown_by_effect_id != kIceWallEffect || node.anchor_id != id) {
        continue;
      }
      ++shards;
      CHECK(node.ground_position.x == cell_ground[0]);
      CHECK(std::abs(node.ground_position.z - cell_ground[2]) <= kShardSpread);
    }
    CHECK(shards == kShardsPerUnit);
  }
  CHECK(bench.LivingFrom(kIceWallEffect) == kWallCells * kShardsPerUnit);
}

TEST_CASE("Max Pain : 1032 naît sur le lanceur du sort", "[effect_ground_skill]") {
  Bench bench;
  bench.CastOnGround(kMaxPainSkill);
  // Le bouclier se lance sous les pieds du lanceur, qui le porte ; la case
  // visée ne reçoit rien.
  const std::array<float, 3> feet = moonclient::GroundPositionAt(
      bench.stage, static_cast<float>(kCasterCell.x), static_cast<float>(kCasterCell.y));
  const std::vector<moonclient::LaunchedEffect> launched = bench.effects.TakeLaunchedEffects();
  REQUIRE(launched.size() == 1);
  CHECK(launched[0].effect_id == kMaxPainEffect);
  CHECK(launched[0].bearer_id == kCasterId);
  CHECK(launched[0].ground == feet);
}

TEST_CASE("Ground Drive : 1399 naît sur la case et vit sa propre vie", "[effect_ground_skill]") {
  Bench bench;
  bench.CastOnGround(kGroundDriveSkill);
  REQUIRE(bench.LivingFrom(kGroundDriveEffect) > 0);
  const std::int32_t instance_life = ro::render::EffectNodeLifeFor(kGroundDriveEffect);
  bench.AdvanceTo(instance_life - 1);
  CHECK(bench.LivingFrom(kGroundDriveEffect) > 0);
  bench.AdvanceTo(instance_life);
  CHECK(bench.LivingFrom(kGroundDriveEffect) == 0);
}

TEST_CASE("Ganbantein : le répartiteur sonne de l'origine du repère du client",
          "[effect_ground_skill]") {
  Bench bench;
  bench.CastOnGround(kGanbanteinSkill);
  // Son bruit à lui n'était joué par aucun chemin : ni la table des sons
  // d'effet, ni celle des unités ne le connaissent pour cette compétence.
  const std::vector<moonclient::SownNamedSound> heard = bench.effects.TakeSownNamedSounds();
  const auto found = std::ranges::find(heard, kGanbanteinSound, &moonclient::SownNamedSound::sound);
  REQUIRE(found != heard.end());
  const glm::vec3 origin =
      ro::render::MapEffectFrame{kGroundCells, kGroundCells}.ToWorld(glm::vec3(0.0F));
  REQUIRE(found->point.has_value());
  CHECK((*found->point)[0] == origin.x);
  CHECK((*found->point)[2] == origin.z);
  CHECK(found->hearing_range == ro::audio::kHearingRange);
  CHECK(found->near_distance == ro::audio::kNearDistance);
  CHECK(found->volume == 1.0F);
}

TEST_CASE("NC_COLDSLOWER : la traînée de 791 vole vers la case visée", "[effect_ground_skill]") {
  Bench bench;
  bench.CastOnGround(kColdSlowerSkill);
  // Sans point visé, la traînée ne semait rien : le répartiteur le lui donne
  // par une commande, et c'est la case.
  CHECK(bench.LivingFrom(kColdSlowerEffect) > 0);
}

TEST_CASE("ABC_ABYSS_STRIKE : le fichier de 2091 part 880 ms après la pose",
          "[effect_ground_skill]") {
  Bench bench;
  bench.CastOnGround(kAbyssStrikeSkill);
  const auto frame_of = [&bench](std::uint32_t now_ms) -> std::optional<std::int32_t> {
    for (const ro::render::DrawnEffect& drawn :
         bench.effects.Drawn(now_ms, bench.stage, bench.world, bench.planner)) {
      if (drawn.effect != nullptr) {
        return drawn.frame;
      }
    }
    return std::nullopt;
  };
  CHECK_FALSE(frame_of(kNowMs + kAbyssStartDelayMs - ro::render::kEffectTickMs).has_value());
  const std::optional<std::int32_t> first = frame_of(kNowMs + kAbyssStartDelayMs);
  REQUIRE(first.has_value());
  CHECK(*first == 0);
}

TEST_CASE("774 : le fichier de 1062 naît à 0,4 de ses images", "[effect_ground_skill]") {
  Bench bench;
  bench.CastOnGround(kHammerSkill);
  const std::span<const ro::render::DrawnEffect> drawn =
      bench.effects.Drawn(kNowMs, bench.stage, bench.world, bench.planner);
  REQUIRE(drawn.size() == 1);
  CHECK(drawn[0].frame == kHammerSkippedFrames);
}

TEST_CASE("Chaos Panic : le gestionnaire du paquet sonne lui-même", "[effect_ground_skill]") {
  Bench bench;
  bench.CastOnGround(kChaosPanicSkill);
  // Ni le répartiteur ni le registre ne le jouent : c'est le gestionnaire du
  // paquet, avant eux deux, qui ne l'était pas.
  const std::vector<moonclient::SownNamedSound> heard = bench.effects.TakeSownNamedSounds();
  CHECK(std::ranges::find(heard, kChaosPanicSound, &moonclient::SownNamedSound::sound) !=
        heard.end());
}

TEST_CASE("2417 : le gestionnaire du paquet fait trembler l'écran", "[effect_ground_skill]") {
  Bench bench;
  bench.CastOnGround(kQuakingSkill);
  const std::optional<std::uint32_t> quake = bench.effects.TakeScreenQuake();
  REQUIRE(quake.has_value());
  CHECK(*quake == kNowMs + kQuakeMs);
}

TEST_CASE("Onde psychique d'une créature : le chemin de la créature la joue aussi",
          "[effect_ground_skill]") {
  Bench bench(kCreatureJob);
  bench.CastOnGround(kCreaturePsychicWaveSkill);
  const std::vector<moonclient::SownNamedSound> heard = bench.effects.TakeSownNamedSounds();
  CHECK(std::ranges::find(heard, kPsychicWaveSound, &moonclient::SownNamedSound::sound) !=
        heard.end());
}

TEST_CASE("Lanceur hors de vue : la case sonne quand même", "[effect_ground_skill]") {
  // 🔴 LE PAQUET ÉTAIT IGNORÉ quand le monde ne connaissait pas le lanceur ; le
  // client, lui, joue le chemin du joueur sans ce qui naît sur le lanceur.
  Bench bench;
  bench.CastOnGroundUnseen(kChaosPanicSkill);
  const std::vector<moonclient::SownNamedSound> heard = bench.effects.TakeSownNamedSounds();
  CHECK(std::ranges::find(heard, kChaosPanicSound, &moonclient::SownNamedSound::sound) !=
        heard.end());
}

TEST_CASE("Lanceur hors de vue : 2417 ne tremble pas sans son effet sur le lanceur",
          "[effect_ground_skill]") {
  Bench bench;
  bench.CastOnGroundUnseen(kQuakingSkill);
  CHECK_FALSE(bench.effects.TakeScreenQuake().has_value());
}

TEST_CASE("Lanceur hors de vue : ce que pose le répartiteur ne paraît pas",
          "[effect_ground_skill]") {
  // 774 ne se pose que par le message au lanceur : sans lui, rien sur la case.
  Bench bench;
  bench.CastOnGroundUnseen(kHammerSkill);
  CHECK(bench.effects.Drawn(kNowMs, bench.stage, bench.world, bench.planner).empty());
}
