// L'EFFET D'EMPLOI QU'UN COUP DE COMPÉTENCE FAIT NAÎTRE SUR SON LANCEUR.
//
// Mesuré sur le client d'origine : pour Jupitel Thunder comme pour Frost Nova,
// le cercle douze naît sur le lanceur dans la milliseconde où le message de
// dégâts arrive, avant les deux boules 93 de Jupitel — celle du répartiteur des
// dégâts, puis l'effet visé. Seuls l'effet de frappe et celui de la carte des
// dégâts attendent l'image de frappe : Frost Nova pose son 28 sur la victime
// cent quarante et une millisecondes plus tard.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_scene.h"
#include "ro/render/scene_light.h"
#include "ro/render/skill_hit_effect.h"
#include "ro/render/skill_use_motion.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/strike_play.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_param.h"
#include "effects/effect_view.h"
#include "support/client_vfs.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/map_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"
#include "world/map_stage.h"

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

constexpr std::uint16_t kJupitelThunder = 84;
constexpr std::uint16_t kFrostNova = 88;
constexpr std::uint16_t kBrandishSpear = 57;
constexpr std::uint16_t kColdBolt = 14;
// Une compétence que le client laisse passer à l'éclaboussure, et dont le
// résolveur d'emploi garde le défaut.
constexpr std::uint16_t kSkillOnSplash = 5046;
constexpr std::int16_t kBeginSpellEffect = 12;
constexpr std::int16_t kJupitelBallEffect = 93;
constexpr std::int16_t kLanceEffect = 144;
constexpr std::int16_t kBashEffect = 16;
constexpr std::int32_t kFrostDiverEffect = 28;
constexpr std::uint8_t kMultiHitType = 8;
constexpr std::uint8_t kSplashType = 5;
constexpr std::uint8_t kCriticalType = 10;
constexpr std::int16_t kLevelTen = 10;
// Le niveau que le serveur annonce moins un pour le cacher.
constexpr std::int16_t kHiddenLevel = -1;
constexpr std::uint16_t kJupitelHits = 3;
constexpr std::uint32_t kWizardJob = 9;
// La durée d'attaque du mage de la mesure, et le retard qu'elle donne.
constexpr std::uint32_t kMeasuredAttackMotionMs = 106;
constexpr std::uint32_t kMeasuredWoundDelayMs = 141;
// Les rangs d'un corps de joueur, et leur cadence : quatre pas de 24 ms, comme
// tous ceux des corps de mage de l'installation.
constexpr std::size_t kPlayerAnimations = 13;
constexpr std::size_t kFramesPerAction = 8;
constexpr float kBodySpeed = 4.0F;
constexpr std::uint32_t kCasterId = 150'031;
constexpr std::uint32_t kTargetId = 150'032;
constexpr std::uint32_t kUnseenId = 150'039;
constexpr std::uint32_t kNowMs = 10'000;
constexpr std::int32_t kGroundCells = 4;
constexpr std::int32_t kImagePixels = 8;
constexpr const char* kBeginSpellRingTexture = "data\\texture\\effect\\ring_yellow.tga";
constexpr ro::sim::CellPosition kCasterCell{.x = 1, .y = 1};
constexpr ro::sim::CellPosition kTargetCell{.x = 2, .y = 1};

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

// Un corps de mage synthétique : treize rangs de huit images, tous à la même
// cadence.
struct WizardBody {
  WizardBody()
      : sprite(*ro::formats::SprSprite::Parse(SpriteBytes())),
        animation(*ro::formats::ActAnimation::Parse(AnimationBytes())),
        outfit(Pieces(sprite, animation)) {}

  static std::vector<std::uint8_t> SpriteBytes() {
    ro::test_support::SprBuildOptions options;
    options.indexed = {{2, 2, {1, 1, 1, 1}}};
    options.palette = {{0, 0, 0}, {255, 0, 0}};
    return ro::test_support::BuildSpr(options);
  }

  static std::vector<std::uint8_t> AnimationBytes() {
    ro::test_support::ActBuildOptions options;
    for (std::size_t action = 0; action < kPlayerAnimations * ro::render::kDirectionCount;
         ++action) {
      ro::test_support::ActActionSpec spec;
      for (std::size_t frame = 0; frame < kFramesPerAction; ++frame) {
        ro::test_support::ActLayerSpec layer;
        layer.frame_index = 0;
        spec.frames.push_back(ro::test_support::ActFrameSpec{{layer}, {}, -1});
      }
      options.actions.push_back(std::move(spec));
      options.speeds.push_back(kBodySpeed);
    }
    return ro::test_support::BuildAct(options);
  }

  static std::vector<ro::render::ActorPiece> Pieces(const ro::formats::SprSprite& body_sprite,
                                                    const ro::formats::ActAnimation& body_act) {
    ro::render::ActorPiece body;
    body.sprite = &body_sprite;
    body.animation = &body_act;
    body.part = ro::render::ActorPart::kBody;
    return {body};
  }

  ro::formats::SprSprite sprite;
  ro::formats::ActAnimation animation;
  ro::render::ActorOutfit outfit;
};

std::vector<std::uint8_t> PlainTga() {
  return BuildTga(kImagePixels, kImagePixels,
                  std::vector<TgaPixel>(static_cast<std::size_t>(kImagePixels * kImagePixels),
                                        TgaPixel{.red = 200, .green = 200, .blue = 60}));
}

std::vector<std::uint8_t> OneFrameSpr() {
  ro::test_support::SprBuildOptions options;
  options.indexed.push_back(ro::test_support::SprIndexedFrameSpec{
      kImagePixels, kImagePixels,
      std::vector<std::uint8_t>(static_cast<std::size_t>(kImagePixels * kImagePixels), 1)});
  options.palette = {ro::test_support::SpriteColor{},
                     ro::test_support::SpriteColor{.red = 0, .green = 200, .blue = 60}};
  return ro::test_support::BuildSpr(options);
}

std::vector<std::uint8_t> OneFrameAct() {
  ro::test_support::ActBuildOptions options;
  options.actions.push_back(ro::test_support::ActActionSpec{
      {ro::test_support::ActFrameSpec{{ro::test_support::ActLayerSpec{}}, {}, -1}}});
  options.speeds = {1.0F};
  return ro::test_support::BuildAct(options);
}

// Un client qui porte les images des patrons de nœuds du cercle et des boules :
// sans elles, rien ne se sème.
ro::vfs::Vfs ClientWithCircleAndBalls(const TempDirectory& client) {
  // Le cercle douze est une famille d'anneaux, hors des patrons : son image.
  std::vector<GrfFileSpec> files{{.name = kBeginSpellRingTexture, .content = PlainTga()}};
  for (const std::uint32_t effect_id : {static_cast<std::uint32_t>(kBeginSpellEffect),
                                        static_cast<std::uint32_t>(kJupitelBallEffect)}) {
    for (const ro::render::EffectNodePattern& pattern :
         ro::render::EffectNodePatternsFor(effect_id)) {
      for (const std::string_view name : ro::render::EffectNodePatternTextures(pattern)) {
        if (pattern.shape == ro::render::EffectNodeShape::kSprite) {
          files.push_back(GrfFileSpec{Cp949(std::string(name) + ".spr"), OneFrameSpr()});
          files.push_back(GrfFileSpec{Cp949(std::string(name) + ".act"), OneFrameAct()});
        } else {
          files.push_back(GrfFileSpec{Cp949(name), PlainTga()});
        }
      }
    }
  }
  client.WriteFile("effet.grf", BuildGrf(files));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effet.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

struct Bench {
  Bench() : vfs(ClientWithCircleAndBalls(client)) {
    ro::sim::Actor caster;
    caster.id = kCasterId;
    caster.kind = ro::sim::ActorKind::kPlayer;
    caster.position = kCasterCell;
    world.Spawn(caster);
    ro::sim::Actor target;
    target.id = kTargetId;
    target.kind = ro::sim::ActorKind::kMonster;
    target.position = kTargetCell;
    world.Spawn(target);
  }

  // Le message de dégâts d'une compétence, reçu maintenant.
  void Strike(std::uint16_t skill_id, std::uint8_t damage_type, std::uint32_t target_id,
              std::int16_t announced_level = kLevelTen) {
    ro::sim::Strike strike;
    strike.attacker = kCasterId;
    strike.target = target_id;
    strike.started_ms = kNowMs;
    strike.damage = 1;
    strike.hits = kJupitelHits;
    strike.type = damage_type;
    strike.skill_id = skill_id;
    strike.skill_level = static_cast<std::uint16_t>(announced_level > 0 ? announced_level : 0);
    strike.announced_level = announced_level;
    REQUIRE(world.RecordStrike(strike));
  }

  // Ce qui est né sur le lanceur, dans l'ordre.
  std::vector<std::uint32_t> BornOnCaster() {
    std::vector<std::uint32_t> born;
    for (const moonclient::LaunchedEffect& launched : effects.TakeLaunchedEffects()) {
      if (launched.bearer_id == kCasterId) {
        born.push_back(launched.effect_id);
      }
    }
    return born;
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

TEST_CASE("Jupitel Thunder : le cercle d'emploi naît au paquet, avant les deux boules",
          "[app][effets][emploi]") {
  // 🔴 LE CERCLE ATTENDAIT L'IMAGE DE FRAPPE, avec l'impact de la cible : cent
  // quarante millisecondes après les boules, au lieu de les précéder.
  Bench bench;
  bench.Strike(kJupitelThunder, kMultiHitType, kTargetId);
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNowMs);
  const std::vector<std::uint32_t> expected = {static_cast<std::uint32_t>(kBeginSpellEffect),
                                               static_cast<std::uint32_t>(kJupitelBallEffect),
                                               static_cast<std::uint32_t>(kJupitelBallEffect)};
  CHECK(bench.BornOnCaster() == expected);
}

TEST_CASE("Frost Nova : le cercle d'emploi naît au paquet, sur le lanceur",
          "[app][effets][emploi]") {
  Bench bench;
  bench.Strike(kFrostNova, kMultiHitType, kTargetId);
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNowMs);
  const std::vector<std::uint32_t> expected = {static_cast<std::uint32_t>(kBeginSpellEffect)};
  CHECK(bench.BornOnCaster() == expected);
  // Et le 28 de la victime vient de la carte des dégâts, à l'image de frappe.
  CHECK(ro::render::SkillDamageMsgEffect(kFrostNova) == kFrostDiverEffect);
}

TEST_CASE("Effet d'emploi au coup : le niveau moins deux le tait", "[app][effets][emploi]") {
  Bench bench;
  bench.Strike(kJupitelThunder, kMultiHitType, kTargetId, ro::render::kLevelWithoutUseEffect);
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNowMs);
  const std::vector<std::uint32_t> balls_only = {static_cast<std::uint32_t>(kJupitelBallEffect),
                                                 static_cast<std::uint32_t>(kJupitelBallEffect)};
  CHECK(bench.BornOnCaster() == balls_only);
}

TEST_CASE("Effet d'emploi au coup : il naît aussi quand la cible est hors de vue",
          "[app][effets][emploi]") {
  // Le message part vers le lanceur, que la cible soit connue ou non.
  Bench bench;
  bench.Strike(kJupitelThunder, kMultiHitType, kUnseenId);
  bench.effects.NoticeSkills(bench.world, bench.stage, bench.planner, kNowMs);
  const std::vector<std::uint32_t> born = bench.BornOnCaster();
  REQUIRE_FALSE(born.empty());
  CHECK(born.front() == static_cast<std::uint32_t>(kBeginSpellEffect));
}

TEST_CASE("Effet d'emploi au coup : la règle du client, sorte par sorte", "[app][effets][emploi]") {
  using ro::render::kSkillUseEffectNone;
  using ro::render::SkillHitUseEffect;
  CHECK(SkillHitUseEffect(kJupitelThunder, kMultiHitType, kLevelTen, true) == kBeginSpellEffect);
  CHECK(SkillHitUseEffect(kFrostNova, kMultiHitType, kLevelTen, true) == kBeginSpellEffect);
  // Une sorte que le visuel du coup ne connaît pas ne montre rien.
  CHECK(SkillHitUseEffect(kJupitelThunder, kCriticalType, kLevelTen, true) == kSkillUseEffectNone);
  // Les boulons tiennent leur effet du répartiteur des dégâts.
  CHECK(SkillHitUseEffect(kColdBolt, kMultiHitType, kLevelTen, true) == kSkillUseEffectNone);
  // L'éclaboussure d'une cible vue repart, sauf pour la liste du client ;
  // celle d'une cible hors de vue poursuit jusqu'à l'emploi.
  CHECK(SkillHitUseEffect(kBrandishSpear, kSplashType, kLevelTen, true) == kSkillUseEffectNone);
  CHECK(SkillHitUseEffect(kBrandishSpear, kSplashType, kLevelTen, false) == kLanceEffect);
  CHECK(SkillHitUseEffect(kSkillOnSplash, kSplashType, kLevelTen, true) == kBashEffect);
  CHECK(SkillHitUseEffect(kJupitelThunder, kMultiHitType, ro::render::kLevelWithoutUseEffect,
                          true) == kSkillUseEffectNone);
  // Moins un n'est pas moins deux : le niveau caché laisse l'effet.
  CHECK(SkillHitUseEffect(kJupitelThunder, kMultiHitType, kHiddenLevel, true) == kBeginSpellEffect);
}

TEST_CASE("Frost Nova : la blessure de la victime part cent quarante et une ms après le coup",
          "[app][effets][emploi]") {
  // Le mage mesuré frappe à une durée d'attaque de 106 ms ; ses rangs vont à
  // quatre pas, 96 ms par image. L'état d'emploi sept pose l'image six : le
  // client programme l'effet de la carte des dégâts à 6 × 4 × 106/432 × 24 ms,
  // tronqués, et la mesure le voit naître sur le monstre 140 à 141 ms après le
  // paquet.
  const WizardBody wizard;
  ro::render::ActorLook look;
  look.player = true;
  look.job = kWizardJob;
  ro::render::StrikeFacts facts;
  facts.skill_id = kFrostNova;
  facts.damage_type = kMultiHitType;
  facts.attack_motion_ms = kMeasuredAttackMotionMs;
  const ro::render::SpriteCatalog catalog;
  const ro::render::StrikePlay play = ro::render::StrikePlayOf(catalog, wizard.outfit, look, facts);
  CHECK(play.state == std::optional<std::int8_t>{ro::render::kStrikeUseState});
  CHECK(play.hit_frame == Catch::Approx(ro::render::kUseStateHitFrame));
  CHECK(static_cast<std::uint32_t>(play.lands_at_ms) == kMeasuredWoundDelayMs);
}

TEST_CASE("Paramètre d'instance d'un coup : au-delà de trente-six coups, il retombe à un",
          "[app][effets][emploi]") {
  ro::sim::Strike strike;
  strike.type = kMultiHitType;
  strike.hits = kJupitelHits;
  CHECK(moonclient::EffectParamOfStrike(strike) == kJupitelHits);
  strike.hits = moonclient::kEffectParamCap;
  CHECK(moonclient::EffectParamOfStrike(strike) == moonclient::kEffectParamCap);
  strike.hits = moonclient::kEffectParamCap + 1;
  CHECK(moonclient::EffectParamOfStrike(strike) == moonclient::kRawEffectParam);
  strike.type = kSplashType;
  strike.hits = kJupitelHits;
  CHECK(moonclient::EffectParamOfStrike(strike) == moonclient::kRawEffectParam);
}
