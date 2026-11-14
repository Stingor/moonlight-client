// CE QUE LE GESTIONNAIRE DE ZC_NOTIFY_GROUNDSKILL POSE LUI-MÊME, et dans quel
// ordre il passe la main : le répartiteur du message au lanceur d'abord, ses
// propres gestes ensuite, le registre des effets nommés enfin — sauf quand il
// en sort plus tôt.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <numbers>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ro/render/ground_skill_visual.h"
#include "ro/render/skill_sound_table.h"

using ro::render::GeneratedGroundSkillPacketTables;
using ro::render::GroundSkillPacketCaster;
using ro::render::GroundSkillPacketGesture;
using ro::render::GroundSkillPacketPlan;
using ro::render::GroundSkillPacketStep;
using ro::render::GroundSkillPacketTables;
using ro::render::GroundSkillSound;
using ro::render::GroundSkillStepFitsViewAngle;
using ro::render::GroundSkillVisualBearer;
using ro::render::GroundSkillVisualEntry;
using ro::render::PlanGroundSkillPacket;

namespace {

constexpr std::int32_t kAnyLevel = 5;
constexpr std::uint16_t kNoTableEffect = 0;
// 5369 : deux effets sur la case, et le gestionnaire sort avant le registre.
constexpr std::uint16_t kTwoCellEffectsSkill = 5369;
constexpr std::uint16_t kFirstCellEffect = 2027;
constexpr std::uint16_t kSecondCellEffect = 2026;
// SO_PSYCHIC_WAVE et sa jumelle de créature : le tour de boucle suit le niveau.
constexpr std::uint16_t kPsychicWaveSkill = 2449;
constexpr std::uint16_t kCreaturePsychicWaveSkill = 736;
constexpr std::string_view kPsychicWaveSound = "effect\\S사이킥웨이브.wav";
constexpr std::uint16_t kCasterMark = 886;
constexpr std::uint16_t kFullWave = 1288;
constexpr std::uint16_t kFullWaveTail = 1289;
constexpr std::uint16_t kMinimalWave = 1454;
constexpr std::int32_t kWaveTurnDelayMs = 500;
constexpr std::int32_t kWaveTurnsCap = 7;
constexpr std::int32_t kWaveTurnsAboveLevel = 2;
// Le piège de ronces : quinze tours autour de la case.
constexpr std::uint16_t kThornTrapSkill = 2446;
constexpr std::uint16_t kThornEffect = 1465;
constexpr std::uint16_t kThornTailEffect = 1622;
constexpr std::int32_t kThornTurns = 15;
constexpr std::int32_t kThornTurnDelayMs = 70;
constexpr float kThornTurnDegrees = 72.0F;
constexpr std::int32_t kThornRadiusModulo = 20;
constexpr float kThornRadiusBase = 5.0F;
constexpr float kThornScale = 0.5F;
constexpr float kHalfTurnDegrees = 180.0F;
constexpr float kDirectionTolerance = 1e-5F;
// Overbrand : l'effet suit le secteur de l'angle.
constexpr std::uint16_t kOverbrandSkill = 2317;
constexpr std::string_view kOverbrandSound = "effect\\LG_OVERBRAND.wav";
// Les huit faces de la charge, par secteur de quarante-cinq degrés environ.
constexpr std::uint16_t kOverbrandFront = 1682;
constexpr std::uint16_t kOverbrandFrontRight = 1681;
constexpr std::uint16_t kOverbrandRightBack = 1679;
constexpr std::uint16_t kOverbrandBack = 1686;
// Des sons de compétence que seul le gestionnaire de la case joue.
constexpr std::array<std::uint32_t, 8> kSoundsOnlyOnGroundPacket = {2301, 2302, 2303, 2446,
                                                                    2449, 2450, 2483, 2518};
// Lullaby : l'emploi sans dégâts joue aussi le sien.
constexpr std::uint32_t kLullabySkill = 2422;
// 712 chez une créature : 409 sur elle, et rien d'autre.
constexpr std::uint16_t kCreatureOnlySkill = 712;
constexpr std::uint16_t kCreatureOnlyEffect = 409;
// La compétence de l'essai de l'ordre : aucune table ne la porte.
constexpr std::uint16_t kUnlistedSkill = 60'000;
constexpr std::uint16_t kDispatchedEffect = 11;
constexpr std::uint16_t kHandledEffect = 22;
constexpr std::uint16_t kFallbackEffect = 33;

GroundSkillPacketPlan PlanFor(std::uint16_t skill_id, std::int32_t level = kAnyLevel,
                              bool minimal_effects = false,
                              GroundSkillPacketCaster caster = GroundSkillPacketCaster::kPlayer) {
  return PlanGroundSkillPacket(GeneratedGroundSkillPacketTables(), skill_id, kNoTableEffect, level,
                               minimal_effects, caster, /*has_named_effects=*/false);
}

std::vector<std::uint16_t> EffectsOf(const GroundSkillPacketPlan& plan,
                                     GroundSkillPacketGesture gesture) {
  std::vector<std::uint16_t> effects;
  for (const GroundSkillPacketStep& step : plan.steps) {
    if (step.gesture == gesture) {
      effects.push_back(step.effect_id);
    }
  }
  return effects;
}

std::vector<GroundSkillPacketStep> FittingAngle(const GroundSkillPacketPlan& plan, float degrees) {
  std::vector<GroundSkillPacketStep> kept;
  std::ranges::copy_if(plan.steps, std::back_inserter(kept),
                       [degrees](const GroundSkillPacketStep& step) {
                         return GroundSkillStepFitsViewAngle(step, degrees);
                       });
  return kept;
}

}  // namespace

TEST_CASE("Gestionnaire : deux effets sur la case, puis aucun registre", "[render][groundskill]") {
  const GroundSkillPacketPlan plan = PlanFor(kTwoCellEffectsSkill);
  // Rien de cela n'était joué : le gestionnaire n'était pas relevé.
  CHECK(EffectsOf(plan, GroundSkillPacketGesture::kOnCell) ==
        std::vector<std::uint16_t>{kFirstCellEffect, kSecondCellEffect});
  CHECK_FALSE(plan.reaches_registry);
}

TEST_CASE("Gestionnaire : l'onde psychique déroule niveau plus deux tours, sept au plus",
          "[render][groundskill]") {
  for (const std::int32_t level : {0, 3, 9}) {
    const GroundSkillPacketPlan plan = PlanFor(kPsychicWaveSkill, level);
    const std::int32_t turns = std::min(level + kWaveTurnsAboveLevel, kWaveTurnsCap);
    REQUIRE(plan.steps.size() == static_cast<std::size_t>(2 + 2 * turns));
    // Son bruit à lui, puis la marque sur le lanceur, puis les tours.
    CHECK(plan.steps[0].gesture == GroundSkillPacketGesture::kSound);
    CHECK(plan.steps[0].sound == kPsychicWaveSound);
    CHECK(plan.steps[1].gesture == GroundSkillPacketGesture::kOnCaster);
    CHECK(plan.steps[1].effect_id == kCasterMark);
    for (std::int32_t turn = 0; turn < turns; ++turn) {
      const GroundSkillPacketStep& head = plan.steps[static_cast<std::size_t>(2 + 2 * turn)];
      const GroundSkillPacketStep& tail = plan.steps[static_cast<std::size_t>(3 + 2 * turn)];
      CHECK(head.effect_id == kFullWave);
      CHECK(tail.effect_id == kFullWaveTail);
      CHECK(head.str_start_delay_ms == turn * kWaveTurnDelayMs);
      CHECK(tail.str_start_delay_ms == turn * kWaveTurnDelayMs);
    }
    CHECK_FALSE(plan.reaches_registry);
  }
  // Sous /mineffect, les effets allégés.
  CHECK(PlanFor(kPsychicWaveSkill, kAnyLevel, /*minimal_effects=*/true).steps[2].effect_id ==
        kMinimalWave);
}

TEST_CASE("Gestionnaire : une créature n'a ni bannière ni registre, et deux compétences",
          "[render][groundskill]") {
  const GroundSkillPacketPlan creature =
      PlanFor(kCreatureOnlySkill, kAnyLevel, false, GroundSkillPacketCaster::kCreature);
  CHECK(EffectsOf(creature, GroundSkillPacketGesture::kOnCaster) ==
        std::vector<std::uint16_t>{kCreatureOnlyEffect});
  CHECK_FALSE(creature.reaches_registry);
  // Un joueur qui l'emploie passe au registre, sans rien d'autre.
  const GroundSkillPacketPlan player = PlanFor(kCreatureOnlySkill);
  CHECK(player.steps.empty());
  CHECK(player.reaches_registry);
  // L'onde de la créature est celle du joueur.
  CHECK(PlanFor(kCreaturePsychicWaveSkill, kAnyLevel, false, GroundSkillPacketCaster::kCreature)
            .steps.size() == PlanFor(kPsychicWaveSkill).steps.size());
  // Et une compétence qu'elle n'a pas : rien, pas même le registre.
  const GroundSkillPacketPlan other =
      PlanFor(kTwoCellEffectsSkill, kAnyLevel, false, GroundSkillPacketCaster::kCreature);
  CHECK(other.steps.empty());
  CHECK_FALSE(other.reaches_registry);
}

TEST_CASE("Gestionnaire : les ronces tournent autour de la case, à un rayon tiré",
          "[render][groundskill]") {
  const GroundSkillPacketPlan plan = PlanFor(kThornTrapSkill);
  std::vector<GroundSkillPacketStep> ring;
  std::ranges::copy_if(plan.steps, std::back_inserter(ring), [](const GroundSkillPacketStep& s) {
    return s.gesture == GroundSkillPacketGesture::kOnCell;
  });
  REQUIRE(ring.size() == static_cast<std::size_t>(2 * kThornTurns));
  for (std::int32_t turn = 0; turn < kThornTurns; ++turn) {
    const float radians =
        static_cast<float>(turn) * kThornTurnDegrees * std::numbers::pi_v<float> / kHalfTurnDegrees;
    for (const std::size_t rank :
         {static_cast<std::size_t>(2 * turn), static_cast<std::size_t>(2 * turn + 1)}) {
      const GroundSkillPacketStep& step = ring[rank];
      CHECK(step.effect_id == (rank % 2 == 0 ? kThornEffect : kThornTailEffect));
      CHECK(step.str_start_delay_ms == turn * kThornTurnDelayMs);
      CHECK(step.str_scale == kThornScale);
      CHECK(step.ring_draw_modulo == kThornRadiusModulo);
      CHECK(step.ring_draw_base == kThornRadiusBase);
      // Les deux effets d'un tour partagent leur tirage ; chaque tour a le sien.
      CHECK(step.ring_draw_rank == turn);
      // Dans le repère du client : (cos, −sin).
      CHECK_THAT(step.client_ring_direction[0],
                 Catch::Matchers::WithinAbs(std::cos(radians), kDirectionTolerance));
      CHECK_THAT(step.client_ring_direction[1],
                 Catch::Matchers::WithinAbs(-std::sin(radians), kDirectionTolerance));
    }
  }
}

TEST_CASE("Gestionnaire : Overbrand choisit son effet par l'angle de la caméra et du lanceur",
          "[render][groundskill]") {
  const GroundSkillPacketPlan plan = PlanFor(kOverbrandSkill);
  const auto effect_at = [&plan](float degrees) {
    std::vector<std::uint16_t> effects;
    bool heard = false;
    for (const GroundSkillPacketStep& step : FittingAngle(plan, degrees)) {
      if (step.gesture == GroundSkillPacketGesture::kSound) {
        heard = heard || step.sound == kOverbrandSound;
      } else {
        effects.push_back(step.effect_id);
      }
    }
    // Le bruit part sous tous les angles, même quand aucun effet ne naît.
    CHECK(heard);
    return effects;
  };
  CHECK(effect_at(10.0F) == std::vector<std::uint16_t>{kOverbrandFront});
  CHECK(effect_at(30.0F) == std::vector<std::uint16_t>{kOverbrandFrontRight});
  CHECK(effect_at(120.0F) == std::vector<std::uint16_t>{kOverbrandRightBack});
  CHECK(effect_at(200.0F) == std::vector<std::uint16_t>{kOverbrandBack});
  CHECK(effect_at(340.0F) == std::vector<std::uint16_t>{kOverbrandFront});
  // Le client ne retranche qu'un tour.
  CHECK(effect_at(400.0F) == std::vector<std::uint16_t>{kOverbrandFrontRight});
  CHECK(effect_at(750.0F).empty());
  CHECK(effect_at(-5.0F).empty());
  // Notre lacet tourne à l'envers du sien.
  CHECK(ro::render::GroundSkillViewAngleDegrees(-30.0F, 100.0F) == 130.0F);
  CHECK(ro::render::GroundSkillViewAngleDegrees(30.0F, 0.0F) == 330.0F);
}

TEST_CASE(
    "Case visée : le répartiteur passe AVANT le gestionnaire et le registre, même "
    "quand des effets nommés existent",
    "[render][groundskill]") {
  // Une compétence que le répartiteur connaît et que le registre connaît aussi :
  // aucune ne l'est aujourd'hui, d'où ces tables. L'application excluait le
  // répartiteur dès qu'un effet nommé existait ; le client lui envoie le
  // message avant même d'arriver au registre.
  const std::array<GroundSkillVisualEntry, 1> dispatcher = {
      GroundSkillVisualEntry{.skill_id = kUnlistedSkill,
                             .effect_id = kDispatchedEffect,
                             .bearer = GroundSkillVisualBearer::kCell,
                             .max_level = kAnyLevel}};
  const std::array<GroundSkillSound, 0> sounds{};
  const std::array<GroundSkillPacketStep, 1> handler = {
      GroundSkillPacketStep{.skill_id = kUnlistedSkill,
                            .max_level = kAnyLevel,
                            .gesture = GroundSkillPacketGesture::kOnCell,
                            .effect_id = kHandledEffect}};
  const std::array<std::uint16_t, 0> skips{};
  const GroundSkillPacketTables tables{dispatcher, sounds, handler, skips};
  const GroundSkillPacketPlan plan =
      PlanGroundSkillPacket(tables, kUnlistedSkill, kFallbackEffect, kAnyLevel, false,
                            GroundSkillPacketCaster::kPlayer, /*has_named_effects=*/true);
  CHECK(EffectsOf(plan, GroundSkillPacketGesture::kOnCell) ==
        std::vector<std::uint16_t>{kDispatchedEffect, kHandledEffect});
  CHECK(plan.reaches_registry);
}

TEST_CASE("Case visée : l'effet des tables n'est qu'un repli, que tout autre chemin écarte",
          "[render][groundskill]") {
  const std::array<GroundSkillVisualEntry, 0> dispatcher{};
  const std::array<GroundSkillSound, 0> sounds{};
  const std::array<GroundSkillPacketStep, 1> handler = {
      GroundSkillPacketStep{.skill_id = kUnlistedSkill,
                            .caster = GroundSkillPacketCaster::kCreature,
                            .max_level = kAnyLevel,
                            .gesture = GroundSkillPacketGesture::kOnCaster,
                            .effect_id = kHandledEffect}};
  const std::array<std::uint16_t, 0> skips{};
  const GroundSkillPacketTables empty{dispatcher, sounds, {}, skips};
  const GroundSkillPacketTables handled{dispatcher, sounds, handler, skips};
  const auto cell_effects = [](const GroundSkillPacketTables& tables, bool named) {
    return EffectsOf(PlanGroundSkillPacket(tables, kUnlistedSkill, kFallbackEffect, kAnyLevel,
                                           false, GroundSkillPacketCaster::kPlayer, named),
                     GroundSkillPacketGesture::kOnCell);
  };
  CHECK(cell_effects(empty, false) == std::vector<std::uint16_t>{kFallbackEffect});
  CHECK(cell_effects(empty, true).empty());
  // Le gestionnaire la connaît, ne serait-ce que pour une créature : pas de repli.
  CHECK(cell_effects(handled, false).empty());
}

TEST_CASE("Sons de compétence : ceux du gestionnaire de la case ne sonnent plus au coup",
          "[render][groundskill]") {
  // Ils n'appartiennent qu'au paquet de la case ; rangés parmi les sons de
  // compétence, ils sonnaient aussi à chaque coup et à chaque emploi.
  for (const std::uint32_t skill_id : kSoundsOnlyOnGroundPacket) {
    CHECK(ro::render::SkillSound(skill_id).empty());
  }
  CHECK_FALSE(ro::render::SkillSound(kLullabySkill).empty());
}

TEST_CASE("Case visée : un lanceur hors de vue garde la case, pas ce qui naît sur lui",
          "[render][groundskill]") {
  // 🔴 LE PAQUET ÉTAIT PERDU. Le client prend alors le chemin du joueur, sans
  // le répartiteur du lanceur : ses sons, ses effets sur la case et son
  // registre, mais aucun effet sur le lanceur.
  constexpr std::uint16_t kIceWallSkill = 87;
  constexpr std::uint16_t kCloudKillSkill = 739;
  constexpr std::uint16_t kInvisibilitySkill = 2417;
  constexpr std::uint16_t kLonelyEffect = 1232;
  constexpr std::uint16_t kLonelySkill = 2587;
  const auto unseen = [](std::uint16_t skill_id) {
    return PlanFor(skill_id, kAnyLevel, false, GroundSkillPacketCaster::kUnseen);
  };
  // Ni le répartiteur — le message ne part qu'à un acteur connu —, ni son repli.
  CHECK_FALSE(PlanFor(kIceWallSkill).steps.empty());
  CHECK(unseen(kIceWallSkill).steps.empty());
  CHECK(unseen(kIceWallSkill).reaches_registry);
  // Le son du nuage, sans la marque sur le lanceur.
  const GroundSkillPacketPlan cloud = unseen(kCloudKillSkill);
  REQUIRE(cloud.steps.size() == 1);
  CHECK(cloud.steps.front().gesture == GroundSkillPacketGesture::kSound);
  // L'onde de ronces se déroule autour de la case, la marque en moins.
  const GroundSkillPacketPlan thorns = unseen(kThornTrapSkill);
  CHECK(EffectsOf(thorns, GroundSkillPacketGesture::kOnCaster).empty());
  CHECK(thorns.steps.size() == PlanFor(kThornTrapSkill).steps.size() - 1);
  // 🔴 2417 ATTEND SON 863 POUR CONTINUER : sans lui, ni tremblement ni son.
  CHECK(PlanFor(kInvisibilitySkill).steps.size() == 3);
  CHECK(unseen(kInvisibilitySkill).steps.empty());
  // La berceuse a sonné avant d'attendre son 858.
  const GroundSkillPacketPlan lullaby = unseen(static_cast<std::uint16_t>(kLullabySkill));
  REQUIRE(lullaby.steps.size() == 1);
  CHECK(lullaby.steps.front().gesture == GroundSkillPacketGesture::kSound);
  // Overbrand lit le regard du lanceur, et 2587 l'attache à lui : rien.
  CHECK(unseen(kOverbrandSkill).steps.empty());
  CHECK(EffectsOf(PlanFor(kLonelySkill), GroundSkillPacketGesture::kOnCaster) ==
        std::vector<std::uint16_t>{kLonelyEffect});
  CHECK(unseen(kLonelySkill).steps.empty());
}
