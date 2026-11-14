#include "ro/render/skill_suite_table.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/vec3.hpp>

#include "ro/render/floating_number.h"

using ro::render::SkillSuiteCarrier;
using ro::render::SkillSuiteKind;
using ro::render::SkillSuiteMoment;
using ro::render::SkillSuiteOf;
using ro::render::SkillSuiteStep;
using ro::render::SkillSuiteStepPlays;

namespace {

// Les effets d'une suite qui jouent pour cet argument, dans l'ordre.
std::vector<std::uint32_t> EffectsPlayed(std::uint16_t skill_id, SkillSuiteMoment moment,
                                         std::int32_t argument) {
  std::vector<std::uint32_t> effects;
  for (const SkillSuiteStep& step : SkillSuiteOf(skill_id, moment)) {
    if (step.kind == SkillSuiteKind::kEffect && SkillSuiteStepPlays(step, argument)) {
      effects.push_back(step.effect_id);
    }
  }
  return effects;
}

constexpr std::int32_t kOneHit = 1;

}  // namespace

TEST_CASE("Suite : la table est triée comme la recherche le suppose", "[render][skillsuite]") {
  const std::span<const SkillSuiteStep> table = ro::render::SkillSuiteTable();
  REQUIRE_FALSE(table.empty());
  CHECK(std::ranges::is_sorted(table, {}, [](const SkillSuiteStep& step) {
    return std::tuple{step.skill_id, step.moment};
  }));
}

TEST_CASE("Suite : Spear Boomerang lance son boomerang PUIS pose son éclat",
          "[render][skillsuite]") {
  // Le client en fait naître deux, sur le lanceur ; n'en garder qu'un tronquait
  // la compétence sans rien dire.
  constexpr std::uint16_t kSpearBoomerang = 59;
  CHECK(EffectsPlayed(kSpearBoomerang, SkillSuiteMoment::kImpact, kOneHit) ==
        std::vector<std::uint32_t>{80, 151});
}

TEST_CASE("Suite : Heal choisit son effet selon ce qu'il soigne", "[render][skillsuite]") {
  constexpr std::uint16_t kHeal = 28;
  CHECK(EffectsPlayed(kHeal, SkillSuiteMoment::kAnnounced, 199) == std::vector<std::uint32_t>{7});
  CHECK(EffectsPlayed(kHeal, SkillSuiteMoment::kAnnounced, 200) == std::vector<std::uint32_t>{312});
  CHECK(EffectsPlayed(kHeal, SkillSuiteMoment::kAnnounced, 3999) ==
        std::vector<std::uint32_t>{313});
  CHECK(EffectsPlayed(kHeal, SkillSuiteMoment::kAnnounced, 4000) ==
        std::vector<std::uint32_t>{325});
}

TEST_CASE("Suite : Slow Poison pose son effet sur sa cible, sous la configuration coréenne",
          "[render][skillsuite]") {
  // Le client ne le pose que si sa configuration dit « korea », et c'est celle
  // de l'installation du joueur.
  constexpr std::uint16_t kSlowPoison = 71;
  constexpr std::uint32_t kSlowPoisonEffect = 136;
  const std::span<const SkillSuiteStep> suite =
      SkillSuiteOf(kSlowPoison, SkillSuiteMoment::kAnnounced);
  REQUIRE(suite.size() == 1);
  CHECK(suite.front().effect_id == kSlowPoisonEffect);
  CHECK(suite.front().carrier == SkillSuiteCarrier::kTarget);
}

TEST_CASE("Suite : l'effet visé de Soul Strike reçoit le nombre de coups", "[render][skillsuite]") {
  // C'est ce qui lui donne ses boules : cinq au niveau dix, une au niveau un.
  constexpr std::uint16_t kSoulStrike = 13;
  const std::span<const SkillSuiteStep> suite =
      SkillSuiteOf(kSoulStrike, SkillSuiteMoment::kImpact);
  REQUIRE(suite.size() == 1);
  CHECK(suite.front().receives_param);
  CHECK(suite.front().carrier == SkillSuiteCarrier::kCasterTowardTarget);
}

TEST_CASE("Suite : Two-Hand Quicken retire son aura AVANT d'en semer une neuve",
          "[render][skillsuite]") {
  // Relancée, la compétence ne double pas son aura : le retrait vient d'abord,
  // sur le lanceur, puis le même effet renaît.
  constexpr std::uint16_t kTwoHandQuicken = 60;
  constexpr std::uint32_t kAura = 130;
  const std::span<const SkillSuiteStep> suite =
      SkillSuiteOf(kTwoHandQuicken, SkillSuiteMoment::kAnnounced);
  REQUIRE(suite.size() >= 2);
  CHECK(suite[0].kind == SkillSuiteKind::kRemoveEffect);
  CHECK(suite[0].carrier == SkillSuiteCarrier::kCaster);
  CHECK(suite[0].effect_id == kAura);
  CHECK(suite[1].kind == SkillSuiteKind::kEffect);
  CHECK(suite[1].effect_id == kAura);
}

TEST_CASE("Suite : Brandish Spear vise ce qu'il frappe, depuis ce qu'il frappe",
          "[render][skillsuite]") {
  // Née sur la cible, l'instance ne vise PAS le lanceur : le client lui donne
  // la place de la cible elle-même.
  constexpr std::uint16_t kBrandishSpear = 57;
  const std::span<const SkillSuiteStep> suite =
      SkillSuiteOf(kBrandishSpear, SkillSuiteMoment::kImpact);
  REQUIRE_FALSE(suite.empty());
  CHECK(suite.front().carrier == SkillSuiteCarrier::kTarget);
  CHECK(suite.front().aim == ro::render::SkillSuiteAim::kTarget);
  std::uint32_t generator = 1;
  const glm::vec3 target(10.0F, 0.0F, 20.0F);
  CHECK(ro::render::SkillSuiteAimPoint(suite.front(), glm::vec3(0.0F), target, glm::vec3(0.0F),
                                       generator) == target);
}

TEST_CASE("Suite : Jack Frost lance dix éclats vers un anneau autour du lanceur",
          "[render][skillsuite]") {
  constexpr std::uint16_t kJackFrost = 2204;
  constexpr float kRadius = 80.0F;
  const glm::vec3 caster(100.0F, 0.0F, 200.0F);
  std::uint32_t generator = 1;
  std::size_t eclats = 0;
  for (const SkillSuiteStep& step : SkillSuiteOf(kJackFrost, SkillSuiteMoment::kAnnounced)) {
    if (step.kind != SkillSuiteKind::kEffect) {
      continue;
    }
    REQUIRE(step.aim == ro::render::SkillSuiteAim::kCasterRing);
    CHECK(step.aim_arc_first_degrees == static_cast<std::int16_t>(36 * eclats));
    const std::optional<glm::vec3> point =
        ro::render::SkillSuiteAimPoint(step, std::nullopt, glm::vec3(0.0F), caster, generator);
    REQUIRE(point.has_value());
    const glm::vec3 offset = *point - caster;
    CHECK(std::abs(std::hypot(offset.x, offset.z) - kRadius) < 1e-3F);
    ++eclats;
  }
  CHECK(eclats == 10);
}

TEST_CASE("Suite : Crimson Rock pose sa variante allégée sous /mineffect", "[render][skillsuite]") {
  constexpr std::uint16_t kCrimsonRock = 2211;
  const auto effects = [](bool allege) {
    std::vector<std::uint32_t> seen;
    for (const SkillSuiteStep& step : SkillSuiteOf(kCrimsonRock, SkillSuiteMoment::kImpact)) {
      if (step.kind == SkillSuiteKind::kEffect && ro::render::SkillSuiteStepShows(step, allege)) {
        seen.push_back(step.effect_id);
      }
    }
    std::ranges::sort(seen);
    return seen;
  };
  const std::vector<std::uint32_t> pleins = effects(false);
  const std::vector<std::uint32_t> lightened = effects(true);
  CHECK(std::ranges::binary_search(pleins, 1274U));
  CHECK_FALSE(std::ranges::binary_search(pleins, 1441U));
  CHECK(std::ranges::binary_search(lightened, 1441U));
  CHECK_FALSE(std::ranges::binary_search(lightened, 1274U));
}

TEST_CASE("Suite : 2054 ne resème pas le cœur de gel que sa cible porte déjà",
          "[render][skillsuite]") {
  constexpr std::uint16_t kSkillId = 2054;
  constexpr std::uint32_t kCoeurDeGel = 833;
  bool vu = false;
  for (const SkillSuiteStep& step : SkillSuiteOf(kSkillId, SkillSuiteMoment::kAnnounced)) {
    if (step.kind == SkillSuiteKind::kEffect && step.effect_id == kCoeurDeGel) {
      vu = true;
      CHECK(step.condition_effect_id == kCoeurDeGel);
      CHECK(step.condition_carrier == SkillSuiteCarrier::kTarget);
      CHECK_FALSE(step.condition_carried);
    }
  }
  CHECK(vu);
}

TEST_CASE("Suite : Arrow Storm tire ses trois volées à 600, 800 et 1 000 ms",
          "[render][skillsuite]") {
  constexpr std::uint16_t kArrowStorm = 2233;
  constexpr std::uint32_t kVolee = 1294;
  std::vector<std::uint16_t> delais;
  for (const SkillSuiteStep& step : SkillSuiteOf(kArrowStorm, SkillSuiteMoment::kImpact)) {
    if (step.kind == SkillSuiteKind::kEffect && step.effect_id == kVolee) {
      delais.push_back(step.start_delay_ms);
    }
  }
  CHECK(delais == std::vector<std::uint16_t>{600, 800, 1000});
}

TEST_CASE("Suite : Steel Body fait trembler l'écran six cent cinquante millisecondes",
          "[render][skillsuite]") {
  constexpr std::uint16_t kSteelBody = 268;
  constexpr std::uint16_t kPlainWanderMs = 650;
  std::size_t tremblements = 0;
  for (const SkillSuiteStep& step : SkillSuiteOf(kSteelBody, SkillSuiteMoment::kAnnounced)) {
    if (step.kind == SkillSuiteKind::kScreenQuake) {
      ++tremblements;
      CHECK(step.quake_ms == kPlainWanderMs);
    }
  }
  CHECK(tremblements == 1);
}

TEST_CASE("Suite : huit compétences font monter leur montant, en vert ou en bleu",
          "[render][skillsuite]") {
  // Le soin et ses parentes sont vertes ; les récupérations de magie, bleues.
  // Le relevé les donne sous un balayage de l'argument, qui ne garde que les
  // gestes visibles : le montant vient de la passe de base.
  constexpr std::array<std::uint8_t, 3> kVert = {0, 255, 0};
  constexpr std::array<std::uint8_t, 3> kBlue = {0, 0, 255};
  for (const std::uint16_t verte : std::array<std::uint16_t, 6>{28, 334, 729, 2043, 2051, 8001}) {
    CHECK(ro::render::SupportNumberColor(verte) == kVert);
  }
  for (const std::uint16_t bleue : std::array<std::uint16_t, 2>{9, 335}) {
    CHECK(ro::render::SupportNumberColor(bleue) == kBlue);
  }
  constexpr std::uint16_t kBash = 5;
  CHECK_FALSE(ro::render::SupportNumberColor(kBash).has_value());
}

TEST_CASE("Suite : 411 pose 444 sur 443, sinon 442 s'il ne le porte pas déjà",
          "[render][skillsuite]") {
  constexpr std::uint16_t kSkillId = 411;
  bool vu_442 = false;
  bool vu_444 = false;
  for (const SkillSuiteStep& step : SkillSuiteOf(kSkillId, SkillSuiteMoment::kAnnounced)) {
    if (step.kind != SkillSuiteKind::kEffect) {
      continue;
    }
    if (step.effect_id == 442) {
      vu_442 = true;
      CHECK(step.condition_effect_id == 443);
      CHECK_FALSE(step.condition_carried);
      CHECK(step.condition2_effect_id == 442);
      CHECK_FALSE(step.condition2_carried);
    }
    if (step.effect_id == 444) {
      vu_444 = true;
      CHECK(step.condition_effect_id == 443);
      CHECK(step.condition_carried);
      CHECK(step.condition2_effect_id == 0);
    }
  }
  CHECK(vu_442);
  CHECK(vu_444);
}

TEST_CASE("Suite : 5280 ne resème ni 1927 ni 1928 sur une cible qui les porte",
          "[render][skillsuite]") {
  constexpr std::uint16_t kSkillId = 5280;
  std::size_t seen = 0;
  for (const SkillSuiteStep& step : SkillSuiteOf(kSkillId, SkillSuiteMoment::kAnnounced)) {
    if (step.kind == SkillSuiteKind::kEffect &&
        (step.effect_id == 1927 || step.effect_id == 1928)) {
      ++seen;
      CHECK(step.condition_effect_id == step.effect_id);
      CHECK(step.condition_carrier == SkillSuiteCarrier::kTarget);
      CHECK_FALSE(step.condition_carried);
    }
  }
  CHECK(seen == 2);
}

TEST_CASE("Suite : un « tant qu'il le porte, le retirer » est un retrait simple",
          "[render][skillsuite]") {
  // 426 retire 446 et 445 du lanceur ; la boucle du client ne demande rien de
  // plus qu'un retrait qui ôte tout.
  constexpr std::uint16_t kSkillId = 426;
  std::vector<std::uint32_t> retires;
  for (const SkillSuiteStep& step : SkillSuiteOf(kSkillId, SkillSuiteMoment::kAnnounced)) {
    if (step.kind == SkillSuiteKind::kRemoveEffect) {
      retires.push_back(step.effect_id);
      CHECK(step.condition_effect_id == 0);
    }
  }
  std::ranges::sort(retires);
  CHECK(retires == std::vector<std::uint32_t>{445, 446});
}

TEST_CASE("Suite : Judex fait tomber douze paires d'éclats autour de sa cible",
          "[render][skillsuite]") {
  constexpr std::uint16_t kJudex = 2038;
  constexpr std::uint32_t kFirst = 1633;
  constexpr std::uint32_t kSecond = 1634;
  constexpr float kRadiusMin = 2.0F;
  constexpr float kRadiusMax = 14.0F;
  constexpr std::uint32_t kDelayStepMs = 50;
  constexpr std::uint32_t kDelaiMaxMs = 450;
  std::uint32_t generator = 1;
  std::size_t paires = 0;
  for (const SkillSuiteStep& step : SkillSuiteOf(kJudex, SkillSuiteMoment::kImpact)) {
    if (step.kind != SkillSuiteKind::kEffect ||
        (step.effect_id != kFirst && step.effect_id != kSecond)) {
      continue;
    }
    // Le second d'une paire reprend le tirage du premier.
    CHECK(step.reuse_previous_draw == (step.effect_id == kSecond));
    if (step.reuse_previous_draw) {
      continue;
    }
    ++paires;
    const ro::render::SkillSuiteScatter drawn = ro::render::SkillSuiteDrawScatter(step, generator);
    const float radius = std::hypot(drawn.offset.x, drawn.offset.z);
    CHECK(radius >= kRadiusMin - 1e-3F);
    CHECK(radius <= kRadiusMax + 1e-3F);
    CHECK(drawn.delay_ms % kDelayStepMs == 0);
    CHECK(drawn.delay_ms <= kDelaiMaxMs);
  }
  CHECK(paires == 12);
}

TEST_CASE("Suite : Earth Shaker sonne à l'impact, sauf aux sortes 5 et 14",
          "[render][skillsuite]") {
  constexpr std::uint16_t kEarthShaker = 2328;
  constexpr std::uint8_t kSingleHit = 4;
  constexpr std::uint8_t kSilentKind = 5;
  const SkillSuiteStep* son = nullptr;
  for (const SkillSuiteStep& step : SkillSuiteOf(kEarthShaker, SkillSuiteMoment::kImpact)) {
    if (step.kind == SkillSuiteKind::kSound && step.sound == "effect\\SR_EARTHSHAKER.wav") {
      son = &step;
    }
  }
  REQUIRE(son != nullptr);
  CHECK(son->carrier == SkillSuiteCarrier::kCaster);
  CHECK(ro::render::SkillSuiteStepFitsDamageType(*son, kSingleHit));
  CHECK_FALSE(ro::render::SkillSuiteStepFitsDamageType(*son, kSilentKind));
}

TEST_CASE("Suite : Rampage Blaster pose ses effets d'impact, pas aux éclaboussures",
          "[render][skillsuite]") {
  constexpr std::uint16_t kRampageBlaster = 2332;
  constexpr std::uint8_t kSingleHit = 4;
  constexpr std::uint8_t kEclaboussure = 5;
  std::vector<std::uint32_t> per_hit;
  std::vector<std::uint32_t> a_l_eclaboussure;
  for (const SkillSuiteStep& step : SkillSuiteOf(kRampageBlaster, SkillSuiteMoment::kImpact)) {
    if (step.kind != SkillSuiteKind::kEffect) {
      continue;
    }
    if (ro::render::SkillSuiteStepFitsDamageType(step, kSingleHit)) {
      per_hit.push_back(step.effect_id);
    }
    if (ro::render::SkillSuiteStepFitsDamageType(step, kEclaboussure)) {
      a_l_eclaboussure.push_back(step.effect_id);
    }
  }
  std::ranges::sort(per_hit);
  CHECK(std::ranges::binary_search(per_hit, 549U));
  CHECK(std::ranges::binary_search(per_hit, 550U));
  CHECK(std::ranges::binary_search(per_hit, 891U));
  CHECK(std::ranges::find(a_l_eclaboussure, 891U) == a_l_eclaboussure.end());
}

TEST_CASE("Suite : Tetra Vortex joue ses trois sons ENSEMBLE", "[render][skillsuite]") {
  // Le relevé à la main les croyait alternatifs, « la garde n'est pas lue » : il
  // n'y a pas de garde.
  constexpr std::uint16_t kTetraVortexFire = 2218;
  std::size_t sounds = 0;
  for (const SkillSuiteStep& step : SkillSuiteOf(kTetraVortexFire, SkillSuiteMoment::kAnnounced)) {
    sounds += step.kind == SkillSuiteKind::kSound ? 1U : 0U;
  }
  CHECK(sounds == 3);
}

TEST_CASE("Suite : l'aura d'Intimidate naît sur le joueur de la caméra, ni lanceur ni cible",
          "[render][skillsuite]") {
  // Le sort téléporte le lanceur et sa cible : l'aura s'éteint avec la carte de
  // qui regarde, et c'est sur lui qu'elle doit naître.
  constexpr std::uint16_t kIntimidate = 219;
  constexpr std::uint32_t kIntimidateAura = 227;
  const std::span<const SkillSuiteStep> suite =
      SkillSuiteOf(kIntimidate, SkillSuiteMoment::kImpact);
  REQUIRE(suite.size() == 1);
  CHECK(suite.front().effect_id == kIntimidateAura);
  CHECK(suite.front().carrier == SkillSuiteCarrier::kLocalPlayer);
}

TEST_CASE("Suite : la plaisanterie et le cri ne naissent que sur le joueur local qui les lance",
          "[render][skillsuite]") {
  // Ces deux effets font PARLER leur porteur : semés sur un autre barde, ils
  // feraient dire sa réplique au joueur local.
  struct Spoken {
    std::uint16_t skill_id;
    std::uint32_t effect_id;
  };
  constexpr std::array<Spoken, 2> kSpoken = {{{318, 295}, {326, 296}}};
  for (const auto& [skill_id, effect_id] : kSpoken) {
    const std::span<const SkillSuiteStep> suite =
        SkillSuiteOf(skill_id, SkillSuiteMoment::kAnnounced);
    REQUIRE(suite.size() == 1);
    CHECK(suite.front().effect_id == effect_id);
    CHECK(suite.front().carrier == SkillSuiteCarrier::kCaster);
    CHECK(ro::render::SkillSuiteStepPlaysForCaster(suite.front(), true));
    CHECK_FALSE(ro::render::SkillSuiteStepPlaysForCaster(suite.front(), false));
  }
  // Et ce sont les seules : toute autre étape joue quel que soit le lanceur.
  std::size_t gated = 0;
  for (const SkillSuiteStep& step : ro::render::SkillSuiteTable()) {
    gated += step.cast_by_local_player_only ? 1U : 0U;
  }
  CHECK(gated == kSpoken.size());
}

TEST_CASE("Suite : les traits de Piercing Shot restent sur la cible", "[render][skillsuite]") {
  // Le relevé les faisait naître sur le lanceur ; le joueur les a vus deux fois
  // rester sur la cible, en côte à côte avec le client d'origine.
  constexpr std::uint16_t kPiercingShot = 514;
  constexpr std::uint32_t kPiercingBeams = 623;
  const auto steps = SkillSuiteOf(kPiercingShot, SkillSuiteMoment::kImpact);
  REQUIRE(steps.size() == 1);
  CHECK(steps.front().effect_id == kPiercingBeams);
  CHECK(steps.front().carrier == ro::render::SkillSuiteCarrier::kTarget);
  CHECK(steps.front().aim == ro::render::SkillSuiteAim::kBeyondTarget);
}

TEST_CASE("Suite : viser au-delà de la cible, c'est prolonger le tir", "[render][skillsuite]") {
  ro::render::SkillSuiteStep step;
  step.aim = ro::render::SkillSuiteAim::kBeyondTarget;
  std::uint32_t generator = 1;
  const glm::vec3 target(10.0F, 0.0F, 20.0F);
  const glm::vec3 caster(4.0F, 0.0F, 12.0F);
  const auto aim = ro::render::SkillSuiteAimPoint(step, caster, target, caster, generator);
  REQUIRE(aim.has_value());
  CHECK(aim->x == 16.0F);
  CHECK(aim->z == 28.0F);
  // Sans lanceur, pas de sens : l'autre bout, comme par défaut.
  CHECK(ro::render::SkillSuiteAimPoint(step, std::nullopt, target, std::nullopt, generator) ==
        std::nullopt);
}
