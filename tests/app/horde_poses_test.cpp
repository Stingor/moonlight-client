// LES POSES DE LA HORDE : un état figé de la simulation donne, image après
// image, la liste de poses attendue — phases de marche décalées, blessure
// bridée, fondu de naissance, chute puis fondu, élite, gel, interpolation.

#include "survivors/horde_poses.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/ailment_sprites.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/movement.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_run.h"

#include "support/survivors_fixture.h"
#include "survivors/survivors_looks.h"

using moonclient::HordeBodyKind;
using moonclient::HordeMotionTimes;
using moonclient::HordePose;
using moonclient::HordePoses;
using moonclient::HordeTimesOf;
using ro::render::Motion;

namespace {

constexpr std::int32_t kArenaSide = 48;
constexpr std::uint64_t kSeed = 7;
// Les durées de toute apparence de l'essai.
constexpr HordeMotionTimes kTimes{
    .walk_ms = 800.0F, .attack_ms = 600.0F, .hurt_ms = 400.0F, .die_ms = 500.0F};
// Le nord : le cap zéro, la face quatre d'un sprite sous la caméra de l'arène.
constexpr std::uint8_t kNorthHeading = 0;
constexpr std::size_t kNorthDirection = 4;
constexpr std::uint32_t kStepMs = ro::sim::kSurvivorsStepMs;
constexpr std::uint32_t kNoBlend = 0;
constexpr std::uint32_t kLastBlend = 999;
constexpr std::uint8_t kOpaque = 255;
constexpr std::size_t kAlpha = 3;

[[nodiscard]] HordeTimesOf FixedTimes() {
  return [](HordeBodyKind /*kind*/, std::uint16_t /*look_id*/) { return kTimes; };
}

// Une partie silencieuse, au pas `step`, sans monstre ni gemme.
[[nodiscard]] ro::sim::SurvivorsRun QuietRun(std::uint32_t step) {
  auto run = ro::sim::SurvivorsRun::Start(ro::test_support::QuietRules(),
                                          ro::test_support::PillarArena(kArenaSide),
                                          ro::test_support::TinyStart(kSeed));
  REQUIRE(run.has_value());
  ro::sim::SurvivorsField& field = run->field_for_tests();
  field.step = step;
  field.foes.clear();
  field.gems.clear();
  field.plants.clear();
  field.pickups.clear();
  field.hero.previous_position = field.hero.position;
  field.hero.facing = kNorthHeading;
  return std::move(*run);
}

// Un monstre au nord du héros, tourné au nord, né au premier pas sans avoir
// touché personne : son fondu de naissance est fini quand les essais le posent.
ro::sim::SurvivorsFoe& AddFoe(ro::sim::SurvivorsField& field, std::uint16_t foe_id,
                              std::int32_t cells_north) {
  const ro::sim::FieldPoint at =
      field.hero.position + ro::sim::FieldPoint{0, ro::sim::Cells(cells_north)};
  const std::uint32_t serial = ro::test_support::PlaceFoe(field, foe_id, at);
  REQUIRE(serial != 0);
  ro::sim::SurvivorsFoe& foe = field.foes.back();
  foe.facing = kNorthHeading;
  foe.born_step = 0;
  foe.touch_ready_step = 0;
  return foe;
}

[[nodiscard]] const HordePose& PoseOf(const HordePoses& poses, HordeBodyKind kind,
                                      std::uint32_t sim_id) {
  const HordePose* found = nullptr;
  for (const HordePose& pose : poses.poses()) {
    if (pose.kind == kind && pose.sim_id == sim_id) found = &pose;
  }
  REQUIRE(found != nullptr);
  return *found;
}

[[nodiscard]] bool HasPose(const HordePoses& poses, HordeBodyKind kind, std::uint32_t sim_id) {
  for (const HordePose& pose : poses.poses()) {
    if (pose.kind == kind && pose.sim_id == sim_id) return true;
  }
  return false;
}

[[nodiscard]] glm::vec2 CellOf(ro::sim::FieldPoint point) {
  return moonclient::CellOfField(
      glm::vec2(static_cast<float>(point.x), static_cast<float>(point.y)));
}

}  // namespace

TEST_CASE("Poses de la horde : un état figé donne la liste attendue", "[app][survivors]") {
  constexpr std::uint32_t kStep = 100;
  ro::sim::SurvivorsRun run = QuietRun(kStep);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const ro::sim::SurvivorsFoe walker = AddFoe(field, ro::test_support::kTinyLunaticId, 2);
  ro::sim::SurvivorsFoe& elite = AddFoe(field, ro::test_support::kTinyPoringId, 3);
  elite.elite = true;
  const ro::sim::SurvivorsFoe elite_copy = elite;
  field.gems.push_back(ro::sim::SurvivorsGem{
      .position = field.hero.position, .previous_position = field.hero.position, .exp = 12});

  HordePoses poses;
  const std::uint64_t presentation_ms = std::uint64_t{kStep} * kStepMs;
  poses.Pose(field, presentation_ms, kNoBlend, FixedTimes());
  const auto at_ms = static_cast<float>(presentation_ms);
  const std::vector<HordePose> expected = {
      HordePose{.kind = HordeBodyKind::kHero,
                .motion = Motion::kReady,
                .motion_ms = at_ms,
                .direction = kNorthDirection,
                .cell = CellOf(field.hero.position)},
      HordePose{.kind = HordeBodyKind::kFoe,
                .sim_id = walker.serial,
                .look_id = walker.foe_id,
                .motion = Motion::kWalk,
                .motion_ms = moonclient::WalkingMs(presentation_ms, kTimes.walk_ms, walker.serial),
                .direction = kNorthDirection,
                .cell = CellOf(walker.position)},
      HordePose{
          .kind = HordeBodyKind::kFoe,
          .sim_id = elite_copy.serial,
          .look_id = elite_copy.foe_id,
          .motion = Motion::kWalk,
          .motion_ms = moonclient::WalkingMs(presentation_ms, kTimes.walk_ms, elite_copy.serial),
          .direction = kNorthDirection,
          .cell = CellOf(elite_copy.position),
          .tint = moonclient::kEliteTint,
          .scale = moonclient::kEliteScale},
      HordePose{.kind = HordeBodyKind::kGem,
                .sim_id = 0,
                .look_id = moonclient::kRedGemstoneItem,
                .motion_ms = at_ms,
                .cell = CellOf(field.hero.position)},
  };
  CHECK(std::vector<HordePose>(poses.poses().begin(), poses.poses().end()) == expected);
}

TEST_CASE("Poses de la horde : la marche d'une espèce a quatre phases décalées par serial",
          "[app][survivors]") {
  ro::sim::SurvivorsRun run = QuietRun(0);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  constexpr std::size_t kFoes = 5;
  for (std::size_t index = 0; index < kFoes; ++index) {
    AddFoe(field, ro::test_support::kTinyLunaticId, 2 + static_cast<std::int32_t>(index));
  }
  HordePoses poses;
  constexpr std::uint64_t kAtMs = 1000;
  poses.Pose(field, kAtMs, kNoBlend, FixedTimes());
  const float quarter_ms = kTimes.walk_ms / static_cast<float>(moonclient::kHordeWalkPhaseCount);
  for (const ro::sim::SurvivorsFoe& foe : field.foes) {
    const HordePose& pose = PoseOf(poses, HordeBodyKind::kFoe, foe.serial);
    CHECK(pose.motion == Motion::kWalk);
    const auto phase = static_cast<float>(foe.serial % moonclient::kHordeWalkPhaseCount);
    CHECK(pose.motion_ms == Catch::Approx(static_cast<float>(kAtMs) + quarter_ms * phase));
  }
  // Deux serials à quatre d'écart marchent du même pas.
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, field.foes[0].serial).motion_ms ==
        PoseOf(poses, HordeBodyKind::kFoe, field.foes[4].serial).motion_ms);
}

TEST_CASE("Poses de la horde : la blessure se rejoue au plus une fois par 300 ms",
          "[app][survivors]") {
  ro::sim::SurvivorsRun run = QuietRun(10);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  ro::sim::SurvivorsFoe& foe = AddFoe(field, ro::test_support::kTinyLunaticId, 2);
  const std::uint32_t serial = foe.serial;
  HordePoses poses;

  // Touché au pas 10 (200 ms) : la blessure commence.
  foe.hurt_step = 10;
  poses.Pose(field, 220, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).motion == Motion::kHurt);
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).motion_ms == Catch::Approx(20.0F));

  // Retouché au pas 17 (340 ms), moins de 300 ms après : elle continue.
  field.step = 17;
  field.foes.back().hurt_step = 17;
  poses.Pose(field, 360, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).motion == Motion::kHurt);
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).motion_ms == Catch::Approx(160.0F));

  // Retouché au pas 26 (520 ms), 320 ms après son début : elle repart.
  field.step = 26;
  field.foes.back().hurt_step = 26;
  poses.Pose(field, 540, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).motion == Motion::kHurt);
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).motion_ms == Catch::Approx(20.0F));

  // Sa durée passée, il remarche.
  poses.Pose(field, 1000, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).motion == Motion::kWalk);
}

TEST_CASE("Poses de la horde : un mort joue sa chute, puis s'efface en une seconde",
          "[app][survivors]") {
  ro::sim::SurvivorsRun run = QuietRun(50);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  ro::sim::SurvivorsFoe& foe = AddFoe(field, ro::test_support::kTinyPoringId, 2);
  foe.elite = true;
  const ro::sim::SurvivorsFoe fallen = foe;
  HordePoses poses;
  poses.Pose(field, 1000, kNoBlend, FixedTimes());

  // La simulation l'a retiré ; la nouvelle de sa chute arrive.
  field.foes.clear();
  const std::vector<ro::sim::SurvivorsFall> falls = {ro::sim::SurvivorsFall{
      .foe_serial = fallen.serial, .foe_id = fallen.foe_id, .at = fallen.position}};
  constexpr std::uint64_t kFellMs = 1000;
  poses.NoticeFalls(falls, kFellMs);

  poses.Pose(field, kFellMs + 200, kNoBlend, FixedTimes());
  const HordePose& falling = PoseOf(poses, HordeBodyKind::kFalling, fallen.serial);
  CHECK(falling.motion == Motion::kDie);
  CHECK(falling.motion_ms == Catch::Approx(200.0F));
  CHECK(falling.direction == kNorthDirection);
  CHECK(falling.scale == moonclient::kEliteScale);
  CHECK(falling.tint[kAlpha] == kOpaque);

  const auto die_ms = static_cast<std::uint64_t>(kTimes.die_ms);
  // Fondu à 0, 500 et 1 000 ms après la chute jouée.
  poses.Pose(field, kFellMs + die_ms, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFalling, fallen.serial).tint[kAlpha] == kOpaque);
  poses.Pose(field, kFellMs + die_ms + 500, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFalling, fallen.serial).tint[kAlpha] == kOpaque / 2);
  poses.Pose(field, kFellMs + die_ms + moonclient::kFallFadeMs, kNoBlend, FixedTimes());
  CHECK_FALSE(HasPose(poses, HordeBodyKind::kFalling, fallen.serial));
  CHECK(poses.falling() == 0);
}

TEST_CASE("Poses de la horde : au plus 128 morts s'effacent, le plus ancien cède",
          "[app][survivors]") {
  HordePoses poses;
  std::vector<ro::sim::SurvivorsFall> falls;
  for (std::uint32_t serial = 1; serial <= moonclient::kMostFallingBodies + 2; ++serial) {
    falls.push_back(
        ro::sim::SurvivorsFall{.foe_serial = serial, .foe_id = ro::test_support::kTinyPoringId});
  }
  poses.NoticeFalls(falls, 0);
  CHECK(poses.falling() == moonclient::kMostFallingBodies);
  ro::sim::SurvivorsRun run = QuietRun(0);
  poses.Pose(run.field(), 0, kNoBlend, FixedTimes());
  CHECK_FALSE(HasPose(poses, HordeBodyKind::kFalling, 1));
  CHECK_FALSE(HasPose(poses, HordeBodyKind::kFalling, 2));
  CHECK(HasPose(poses, HordeBodyKind::kFalling, 3));
}

TEST_CASE("Poses de la horde : gelé, glacé et attaque au contact", "[app][survivors]") {
  constexpr std::uint32_t kStep = 200;
  ro::sim::SurvivorsRun run = QuietRun(kStep);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  // ⚠ CHAQUE AJOUT PEUT DÉPLACER LA LISTE : un monstre se règle, et son serial
  // se lit, avant le suivant.
  ro::sim::SurvivorsFoe& frozen = AddFoe(field, ro::test_support::kTinyLunaticId, 2);
  frozen.freeze_until_step = kStep + 1;
  frozen.elite = true;
  const std::uint32_t frozen_serial = frozen.serial;
  ro::sim::SurvivorsFoe& chilled = AddFoe(field, ro::test_support::kTinyLunaticId, 3);
  chilled.chill_until_step = kStep + 1;
  const std::uint32_t chilled_serial = chilled.serial;
  ro::sim::SurvivorsFoe& striker = AddFoe(field, ro::test_support::kTinyPoringId, 4);
  // Il a touché le héros au pas 190 : sa prochaine touche suit son intervalle.
  constexpr std::uint32_t kTouchStep = 190;
  const ro::sim::FoeRule* poring = ro::sim::FindFoe(*field.rules, ro::test_support::kTinyPoringId);
  REQUIRE(poring != nullptr);
  striker.touch_ready_step = kTouchStep + ro::sim::StepsOf(poring->touch_interval_ms);
  striker.born_step = 0;
  const std::uint32_t striker_serial = striker.serial;

  HordePoses poses;
  const std::uint64_t presentation_ms = std::uint64_t{kStep} * kStepMs;
  poses.Pose(field, presentation_ms, kNoBlend, FixedTimes());
  const HordePose& frozen_pose = PoseOf(poses, HordeBodyKind::kFoe, frozen_serial);
  CHECK(frozen_pose.tint == ro::render::kFrozenTint);
  CHECK(frozen_pose.motion == Motion::kIdle);
  CHECK(frozen_pose.motion_ms == 0.0F);
  CHECK(frozen_pose.scale == moonclient::kEliteScale);
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, chilled_serial).tint == moonclient::kChilledTint);
  const HordePose& striking = PoseOf(poses, HordeBodyKind::kFoe, striker_serial);
  CHECK(striking.motion == Motion::kAttack);
  CHECK(striking.motion_ms ==
        Catch::Approx(static_cast<float>(presentation_ms - kTouchStep * kStepMs)));
}

TEST_CASE("Poses de la horde : un monstre qui naît paraît en fondu", "[app][survivors]") {
  constexpr std::uint32_t kStep = 300;
  ro::sim::SurvivorsRun run = QuietRun(kStep);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  ro::sim::SurvivorsFoe& newborn = AddFoe(field, ro::test_support::kTinyLunaticId, 2);
  newborn.born_step = kStep;
  const std::uint32_t newborn_serial = newborn.serial;
  ro::sim::SurvivorsFoe& elite = AddFoe(field, ro::test_support::kTinyPoringId, 3);
  elite.born_step = kStep;
  elite.elite = true;
  const std::uint32_t elite_serial = elite.serial;
  const std::uint64_t born_ms = std::uint64_t{kStep} * kStepMs;
  HordePoses poses;

  // À la naissance, transparent ; la teinte dorée de l'élite reste.
  poses.Pose(field, born_ms, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, newborn_serial).tint[kAlpha] == 0);
  // L'ombre ronde ne fond pas : un nouveau-né n'en pose aucune.
  CHECK_FALSE(moonclient::CastsRoundShadow(PoseOf(poses, HordeBodyKind::kFoe, newborn_serial)));
  const HordePose& elite_born = PoseOf(poses, HordeBodyKind::kFoe, elite_serial);
  CHECK(elite_born.tint[kAlpha] == 0);
  CHECK(elite_born.tint[0] == moonclient::kEliteTint[0]);

  // À mi-fondu, à demi ; le fondu passé, plein.
  poses.Pose(field, born_ms + moonclient::kHordeBirthFadeMs / 2, kNoBlend, FixedTimes());
  const std::uint8_t halfway = PoseOf(poses, HordeBodyKind::kFoe, newborn_serial).tint[kAlpha];
  CHECK(halfway > 0);
  CHECK(halfway < kOpaque);
  poses.Pose(field, born_ms + moonclient::kHordeBirthFadeMs, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, newborn_serial).tint[kAlpha] == kOpaque);
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, elite_serial).tint == moonclient::kEliteTint);
  CHECK(moonclient::CastsRoundShadow(PoseOf(poses, HordeBodyKind::kFoe, newborn_serial)));
  CHECK(moonclient::CastsRoundShadow(PoseOf(poses, HordeBodyKind::kFoe, elite_serial)));
  CHECK(moonclient::CastsRoundShadow(poses.poses().front()));
}

TEST_CASE("Poses de la horde : la position s'interpole sur la part du pas", "[app][survivors]") {
  ro::sim::SurvivorsRun run = QuietRun(30);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  ro::sim::SurvivorsFoe& foe = AddFoe(field, ro::test_support::kTinyLunaticId, 2);
  const ro::sim::FieldPoint from = foe.position;
  foe.previous_position = from;
  foe.position = from + ro::sim::FieldPoint{ro::sim::kSubunitsPerCell, 0};
  const std::uint32_t serial = foe.serial;
  HordePoses poses;

  poses.Pose(field, 600, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).cell.x == Catch::Approx(CellOf(from).x));

  poses.Pose(field, 600, kLastBlend, FixedTimes());
  const float expected_x =
      CellOf(from).x + static_cast<float>(kLastBlend) / 1000.0F;  // une case entière
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).cell.x == Catch::Approx(expected_x));
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).cell.y == Catch::Approx(CellOf(from).y));
}

TEST_CASE("Poses de la horde : le héros marche, puis tombe", "[app][survivors]") {
  ro::sim::SurvivorsRun run = QuietRun(40);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  field.hero.previous_position = field.hero.position - ro::sim::FieldPoint{0, 64};
  HordePoses poses;
  poses.Pose(field, 800, kNoBlend, FixedTimes());
  REQUIRE_FALSE(poses.poses().empty());
  CHECK(poses.poses().front().kind == HordeBodyKind::kHero);
  CHECK(poses.poses().front().motion == Motion::kWalk);
  CHECK_FALSE(poses.poses().front().held_frame.has_value());

  // Immobile, il se tient en garde.
  field.hero.previous_position = field.hero.position;
  poses.Pose(field, 850, kNoBlend, FixedTimes());
  CHECK(poses.poses().front().motion == Motion::kReady);
  CHECK_FALSE(poses.poses().front().held_frame.has_value());

  field.tally.outcome = ro::sim::SurvivorsOutcome::kFell;
  poses.Pose(field, 900, kNoBlend, FixedTimes());
  CHECK(poses.poses().front().motion == Motion::kDie);
  CHECK(poses.poses().front().motion_ms == Catch::Approx(100.0F));
}

TEST_CASE("Poses de la horde : l'auto-attaque qui touche fait attaquer le héros à l'arrêt",
          "[app][survivors]") {
  constexpr std::uint16_t kWeaponSkillId = 9001;
  constexpr std::uint16_t kOtherSkillId = 9002;
  constexpr std::uint64_t kHitMs = 1000;
  ro::sim::SurvivorsRun run = QuietRun(40);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  ro::sim::ClassRule armed = *field.hero_class;
  armed.weapon_skill_id = kWeaponSkillId;
  field.hero_class = &armed;
  field.hero.previous_position = field.hero.position - ro::sim::FieldPoint{0, 64};
  HordePoses poses;
  // Un autre skill qui touche ne fait pas attaquer un héros qui marche.
  const std::array<ro::sim::SurvivorsHit, 1> other{{{.foe_serial = 1, .skill_id = kOtherSkillId}}};
  poses.NoticeHits(other, field, kHitMs);
  poses.Pose(field, kHitMs, kNoBlend, FixedTimes());
  CHECK(poses.poses().front().motion == Motion::kWalk);

  // Son auto-attaque non plus : en marche, il marche.
  const std::array<ro::sim::SurvivorsHit, 1> swing{{{.foe_serial = 1, .skill_id = kWeaponSkillId}}};
  poses.NoticeHits(swing, field, kHitMs);
  poses.Pose(field, kHitMs + 1, kNoBlend, FixedTimes());
  CHECK(poses.poses().front().motion == Motion::kWalk);
  // Arrêté, il frappe à chaque coup, puis revient à sa garde.
  field.hero.previous_position = field.hero.position;
  poses.NoticeHits(swing, field, kHitMs);
  poses.Pose(field, kHitMs + 1, kNoBlend, FixedTimes());
  CHECK(poses.poses().front().motion == Motion::kAttack);
  CHECK(poses.poses().front().motion_ms == Catch::Approx(1.0F));
  poses.Pose(field, kHitMs + static_cast<std::uint64_t>(kTimes.attack_ms), kNoBlend, FixedTimes());
  CHECK(poses.poses().front().motion == Motion::kReady);
}

TEST_CASE("Poses de la horde : le héros immobile lance de temps à autre", "[app][survivors]") {
  ro::sim::SurvivorsRun run = QuietRun(40);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  REQUIRE(field.hero_class != nullptr);
  const std::array<ro::sim::SurvivorsCast, 1> shot = {
      ro::sim::SurvivorsCast{.kind = ro::sim::CastKind::kShot}};
  const std::array<ro::sim::SurvivorsCast, 1> folded = {
      ro::sim::SurvivorsCast{.kind = ro::sim::CastKind::kOrbitOff}};
  HordePoses poses;
  const auto hero_at = [&](std::uint64_t presentation_ms) {
    poses.Pose(field, presentation_ms, kNoBlend, FixedTimes());
    return poses.poses().front();
  };

  // Une orbite qui se replie ne fait pas de geste.
  poses.NoticeCasts(folded, field, 1000);
  CHECK(hero_at(1100).motion == Motion::kReady);

  // Un lancer fait incanter, la première image tenue.
  poses.NoticeCasts(shot, field, 1000);
  const HordePose casting = hero_at(1100);
  CHECK(casting.motion == Motion::kCast);
  CHECK(casting.motion_ms == Catch::Approx(100.0F));
  CHECK(casting.held_frame == std::optional<std::size_t>(0));
  CHECK(hero_at(1000 + moonclient::kHeroCastHoldMs).motion == Motion::kReady);

  // Pas d'autre geste avant que la période ne s'écoule.
  poses.NoticeCasts(shot, field, 1000 + moonclient::kHeroGestureEveryMs - 1);
  CHECK(hero_at(1000 + moonclient::kHeroGestureEveryMs).motion == Motion::kReady);

  // Une classe de mêlée frappe, le temps de son attaque.
  ro::sim::ClassRule striker = *field.hero_class;
  striker.attack_gesture = true;
  field.hero_class = &striker;
  constexpr std::uint64_t kStruckMs = 1000 + moonclient::kHeroGestureEveryMs;
  poses.NoticeCasts(shot, field, kStruckMs);
  CHECK(hero_at(kStruckMs + 100).motion == Motion::kAttack);
  CHECK(hero_at(kStruckMs + static_cast<std::uint64_t>(kTimes.attack_ms)).motion == Motion::kReady);

  // En marche, un lancer ne fait pas de geste.
  field.hero.previous_position = field.hero.position - ro::sim::FieldPoint{0, 64};
  poses.NoticeCasts(shot, field, 2 * kStruckMs);
  CHECK(hero_at(2 * kStruckMs + 100).motion == Motion::kWalk);
}

TEST_CASE("Poses de la horde : un allié fait ses propres gestes", "[app][survivors]") {
  ro::sim::SurvivorsRun run = QuietRun(40);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  REQUIRE(field.hero_class != nullptr);
  ro::sim::ClassRule striker = *field.hero_class;
  striker.attack_gesture = true;
  field.allies.emplace_back();
  field.allies.back().hero_class = field.hero_class;
  field.allies.emplace_back();
  field.allies.back().hero_class = &striker;

  constexpr std::size_t kOwnPose = 0;
  constexpr std::size_t kCasterPose = 1;
  constexpr std::size_t kFighterPose = 2;
  constexpr std::uint8_t kCasterIndex = 1;
  constexpr std::uint8_t kFighterIndex = 2;
  constexpr std::uint64_t kNoticedMs = 1000;
  constexpr std::uint64_t kSeenMs = kNoticedMs + 100;
  HordePoses poses;
  const auto motion_of = [&](std::size_t rank, std::uint64_t presentation_ms) {
    poses.Pose(field, presentation_ms, kNoBlend, FixedTimes());
    return poses.poses()[rank].motion;
  };

  // Le lancer de l'allié ne fait bouger que lui.
  const std::array<ro::sim::SurvivorsCast, 1> caster_shot = {
      ro::sim::SurvivorsCast{.kind = ro::sim::CastKind::kShot, .hero_index = kCasterIndex}};
  poses.NoticeCasts(caster_shot, field, kNoticedMs);
  CHECK(motion_of(kCasterPose, kSeenMs) == Motion::kCast);
  CHECK(motion_of(kOwnPose, kSeenMs) == Motion::kReady);
  CHECK(motion_of(kFighterPose, kSeenMs) == Motion::kReady);

  // Un allié de mêlée frappe, et son geste a sa propre période.
  const std::array<ro::sim::SurvivorsCast, 1> fighter_shot = {
      ro::sim::SurvivorsCast{.kind = ro::sim::CastKind::kShot, .hero_index = kFighterIndex}};
  poses.NoticeCasts(fighter_shot, field, kSeenMs);
  CHECK(motion_of(kFighterPose, kSeenMs + 1) == Motion::kAttack);
  CHECK(motion_of(kCasterPose, kSeenMs + 1) == Motion::kCast);

  // Le lancer du joueur ne donne de geste qu'à lui.
  const std::array<ro::sim::SurvivorsCast, 1> own_shot = {
      ro::sim::SurvivorsCast{.kind = ro::sim::CastKind::kShot}};
  poses.NoticeCasts(own_shot, field, kSeenMs);
  CHECK(motion_of(kOwnPose, kSeenMs + 1) == Motion::kCast);

  // En marche, un allié ne fait pas de geste.
  field.allies.front().hero.previous_position =
      field.allies.front().hero.position - ro::sim::FieldPoint{0, 64};
  CHECK(motion_of(kCasterPose, kSeenMs + 1) == Motion::kWalk);
}

TEST_CASE("Poses de la horde : le héros emporté par Back Slide se tient en garde",
          "[app][survivors]") {
  ro::sim::SurvivorsRun run = QuietRun(40);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  constexpr std::size_t kOwnPose = 0;
  constexpr std::uint64_t kSeenMs = 5000;
  constexpr std::uint8_t kLeapStepsLeft = 3;
  HordePoses poses;
  const auto pose_now = [&] {
    poses.Pose(field, kSeenMs, kNoBlend, FixedTimes());
    return poses.poses()[kOwnPose];
  };
  // Il bouge : sans bond, il marche.
  field.hero.previous_position = field.hero.position - ro::sim::FieldPoint{0, 64};
  CHECK(pose_now().motion == Motion::kWalk);
  // Le même pas sous un bond : il glisse, en garde, à sa vraie position.
  field.hero.leap_steps_left = kLeapStepsLeft;
  const HordePose leaping = pose_now();
  CHECK(leaping.motion == Motion::kReady);
  CHECK(leaping.cell == pose_now().cell);
}

TEST_CASE("Poses de la horde : le corps de la rencontre se tient face à la caméra",
          "[app][survivors]") {
  constexpr ro::sim::FieldPoint kAt = ro::sim::CellCenter(10, 20);
  const HordePose pose = moonclient::StrangerPoseAt(kAt, 1.0F);
  CHECK(pose.kind == HordeBodyKind::kStranger);
  CHECK(pose.motion == Motion::kIdle);
  CHECK(pose.held_frame == std::optional<std::size_t>(0));
  CHECK(pose.direction == 0U);
  CHECK(pose.cell == glm::vec2(10.0F, 20.0F));
  CHECK(pose.tint[kAlpha] == kOpaque);
  CHECK(moonclient::CastsRoundShadow(pose));
  // Il s'efface sans ombre, comme tout corps transparent.
  const HordePose leaving = moonclient::StrangerPoseAt(kAt, 0.5F);
  CHECK(leaving.tint[kAlpha] < kOpaque);
  CHECK_FALSE(moonclient::CastsRoundShadow(leaving));
}

TEST_CASE("Poses de la horde : un monstre caché n'a pas de pose", "[app][survivors]") {
  // Les règles silencieuses, dont le Lunatic se cache : elles survivent à la
  // partie qui les lit.
  static const std::array<ro::sim::FoeRule, 1> kHidingFoes = [] {
    std::array<ro::sim::FoeRule, 1> foes{ro::test_support::kTinyFoes[0]};
    foes[0].behaviour.traits = ro::sim::kFoeHides;
    return foes;
  }();
  static const ro::sim::SurvivorsRules kHidingRules = [] {
    ro::sim::SurvivorsRules rules = ro::test_support::QuietRules();
    rules.foes = kHidingFoes;
    return rules;
  }();
  constexpr std::uint32_t kStep = 60;
  ro::sim::SurvivorsRun run = QuietRun(kStep);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  field.rules = &kHidingRules;
  ro::sim::SurvivorsFoe& foe = AddFoe(field, ro::test_support::kTinyLunaticId, 2);
  const std::uint32_t serial = foe.serial;
  HordePoses poses;
  poses.Pose(field, std::uint64_t{kStep} * kStepMs, kNoBlend, FixedTimes());
  CHECK(HasPose(poses, HordeBodyKind::kFoe, serial));

  field.foes.back().act_until_step = kStep + 1;
  poses.Pose(field, std::uint64_t{kStep} * kStepMs, kNoBlend, FixedTimes());
  CHECK_FALSE(HasPose(poses, HordeBodyKind::kFoe, serial));

  // Sorti de terre, il ressurgit en fondu depuis ce pas, sans ombre ronde
  // tant qu'il est transparent.
  constexpr std::uint32_t kOutStep = kStep + 1;
  field.step = kOutStep;
  const std::uint64_t out_ms = std::uint64_t{kOutStep} * kStepMs;
  poses.Pose(field, out_ms, kNoBlend, FixedTimes());
  REQUIRE(HasPose(poses, HordeBodyKind::kFoe, serial));
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).tint[kAlpha] == 0);
  CHECK_FALSE(moonclient::CastsRoundShadow(PoseOf(poses, HordeBodyKind::kFoe, serial)));
  poses.Pose(field, out_ms + moonclient::kHordeBirthFadeMs, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).tint[kAlpha] == kOpaque);
}

TEST_CASE("Poses de la horde : un tireur joue son attaque à chaque tir", "[app][survivors]") {
  constexpr std::uint32_t kStep = 60;
  constexpr std::uint32_t kShotLifeSteps = 40;
  ro::sim::SurvivorsRun run = QuietRun(kStep);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const std::uint32_t serial = AddFoe(field, ro::test_support::kTinyLunaticId, 4).serial;
  const std::uint64_t now_ms = std::uint64_t{kStep} * kStepMs;
  HordePoses poses;
  poses.Pose(field, now_ms, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).motion != Motion::kAttack);

  field.foe_shots.push_back(
      ro::sim::SurvivorsFoeShot{.expires_step = kStep + kShotLifeSteps, .from_serial = serial});
  poses.Pose(field, now_ms, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).motion == Motion::kAttack);
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).motion_ms == Catch::Approx(0.0F));
  // Le geste fini, le même tir encore en vol ne le relance pas.
  const auto after_ms = now_ms + static_cast<std::uint64_t>(kTimes.attack_ms);
  poses.Pose(field, after_ms, kNoBlend, FixedTimes());
  CHECK(PoseOf(poses, HordeBodyKind::kFoe, serial).motion != Motion::kAttack);
}

TEST_CASE("Poses de la horde : Sphere Mine pose sa Marine Sphere sur la mine", "[app][survivors]") {
  constexpr std::uint32_t kStep = 60;
  constexpr std::uint16_t kLandMineSkill = 116;
  ro::sim::SurvivorsRun run = QuietRun(kStep);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const ro::sim::FieldPoint mine_at =
      field.hero.position + ro::sim::FieldPoint{ro::sim::Cells(2), 0};
  field.zones.push_back(ro::sim::SurvivorsZone{
      .serial = 1, .skill_id = moonclient::kSphereMineSkill, .from = mine_at, .to = mine_at});
  field.zones.push_back(ro::sim::SurvivorsZone{
      .serial = 2, .skill_id = kLandMineSkill, .from = mine_at, .to = mine_at});
  HordePoses poses;
  poses.Pose(field, std::uint64_t{kStep} * kStepMs, kNoBlend, FixedTimes());
  std::size_t spheres = 0;
  for (const HordePose& pose : poses.poses()) {
    if (pose.kind != HordeBodyKind::kSummon) continue;
    ++spheres;
    CHECK(pose.look_id == moonclient::kMarineSphereClass);
    CHECK(pose.motion == Motion::kIdle);
  }
  // La mine terrestre n'a pas de sphère : une seule pose.
  CHECK(spheres == 1);
}
