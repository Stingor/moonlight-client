// LA CADENCE DES DEMANDES D'ESTIMATION DE DÉGÂTS.
//
// 🔴 LE SERVEUR NE CALCULE QU'UNE DEMANDE TOUTES LES 200 MS, et répond « trop
// rapproché » aux autres. Ce banc garde les trois règles qui l'évitent ou s'en
// remettent : la frappe dans le champ de cible attend de se poser, deux envois
// restent séparés, et un refus relance la question une fois — la dernière.

#include "character/skill_facts_asker.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/skill_facts.h"
#include "ro/ui/description_window.h"

using moonclient::kSkillDamageSettleMs;
using moonclient::kSkillDamageSpacingMs;
using moonclient::SkillFactsAsker;
using ro::sim::SkillDamageQuestion;
using ro::sim::SkillFacts;
using ro::ui::SkillDescriptionAction;

namespace {

constexpr std::uint16_t kBolt = 19;
constexpr std::uint16_t kHeal = 28;
constexpr std::uint32_t kStart = 100'000;
// Une image à soixante par seconde, arrondie.
constexpr std::uint32_t kFrameMs = 16;

[[nodiscard]] SkillDescriptionAction Wants(std::uint16_t skill_id, std::uint32_t target) {
  SkillDescriptionAction asked;
  asked.want_damage = skill_id;
  asked.damage_target = target;
  return asked;
}

}  // namespace

TEST_CASE("Estimation : l'onglet ouvert demande tout de suite, une fois", "[app][grimoire]") {
  SkillFactsAsker asker;
  const SkillFacts told;
  const auto first = asker.Plan(Wants(kBolt, 0), told, kStart);
  REQUIRE(first.damage.has_value());
  CHECK(*first.damage == SkillDamageQuestion{.skill_id = kBolt, .target_mob_id = 0});
  for (std::uint32_t frame = 1; frame < 30; ++frame) {
    CHECK_FALSE(asker.Plan(Wants(kBolt, 0), told, kStart + frame * kFrameMs).damage.has_value());
  }
}

TEST_CASE("Estimation : taper un numéro de monstre ne pose qu'une question", "[app][grimoire]") {
  SkillFactsAsker asker;
  const SkillFacts told;
  std::uint32_t now = kStart;
  REQUIRE(asker.Plan(Wants(kBolt, 0), told, now).damage.has_value());

  // « 1002 », une frappe toutes les cent millisecondes, une image entre deux.
  constexpr std::uint32_t kKeystrokeMs = 100;
  for (const std::uint32_t typed : {1U, 10U, 100U, 1002U}) {
    now += kKeystrokeMs;
    CHECK_FALSE(asker.Plan(Wants(kBolt, typed), told, now).damage.has_value());
    CHECK_FALSE(asker.Plan(Wants(kBolt, typed), told, now + kFrameMs).damage.has_value());
  }
  // La frappe posée, la dernière cible part, seule.
  const std::uint32_t typed_at = now;
  CHECK_FALSE(
      asker.Plan(Wants(kBolt, 1002), told, typed_at + kSkillDamageSettleMs - 1).damage.has_value());
  const auto settled = asker.Plan(Wants(kBolt, 1002), told, typed_at + kSkillDamageSettleMs);
  REQUIRE(settled.damage.has_value());
  CHECK(settled.damage->target_mob_id == 1002);
  CHECK_FALSE(
      asker.Plan(Wants(kBolt, 1002), told, typed_at + 2 * kSkillDamageSettleMs).damage.has_value());
}

TEST_CASE("Estimation : jamais deux envois à moins de l'écart", "[app][grimoire]") {
  SkillFactsAsker asker;
  const SkillFacts told;
  REQUIRE(asker.Plan(Wants(kBolt, 0), told, kStart).damage.has_value());
  // Une autre compétence n'attend pas la frappe, mais attend l'écart.
  CHECK_FALSE(asker.Plan(Wants(kHeal, 0), told, kStart + kFrameMs).damage.has_value());
  CHECK_FALSE(
      asker.Plan(Wants(kHeal, 0), told, kStart + kSkillDamageSpacingMs - 1).damage.has_value());
  const auto spaced = asker.Plan(Wants(kHeal, 0), told, kStart + kSkillDamageSpacingMs);
  REQUIRE(spaced.damage.has_value());
  CHECK(spaced.damage->skill_id == kHeal);
  CHECK(kSkillDamageSpacingMs >= ro::net::kSkillDamageServerSpacingMs);

  // « Calculer » non plus ne passe pas outre l'écart.
  SkillDescriptionAction refresh = Wants(kHeal, 0);
  refresh.refresh = true;
  const std::uint32_t second = kStart + kSkillDamageSpacingMs;
  CHECK_FALSE(asker.Plan(refresh, told, second + kFrameMs).damage.has_value());
  CHECK(asker.Plan(Wants(kHeal, 0), told, second + kSkillDamageSpacingMs).damage.has_value());
}

TEST_CASE("Estimation : un refus « trop rapproché » relance la question, une fois",
          "[app][grimoire]") {
  SkillFactsAsker asker;
  SkillFacts told;
  const SkillDamageQuestion question{.skill_id = kBolt, .target_mob_id = 1002};
  REQUIRE(asker.Plan(Wants(kBolt, 1002), told, kStart).damage.has_value());

  told.NoteDamageTooSoon(question);
  // La relance attend l'écart, puis part d'elle-même.
  CHECK_FALSE(asker.Plan(Wants(kBolt, 1002), told, kStart + kFrameMs).damage.has_value());
  const std::uint32_t resent_at = kStart + kSkillDamageSpacingMs;
  const auto resent = asker.Plan(Wants(kBolt, 1002), told, resent_at);
  REQUIRE(resent.damage.has_value());
  CHECK(*resent.damage == question);

  // Refusée à son tour, elle ne relance plus.
  told.NoteDamageTooSoon(question);
  for (std::uint32_t frame = 1; frame < 60; ++frame) {
    CHECK_FALSE(
        asker.Plan(Wants(kBolt, 1002), told, resent_at + frame * kFrameMs).damage.has_value());
  }
}

TEST_CASE("Estimation : la relance porte la dernière question", "[app][grimoire]") {
  SkillFactsAsker asker;
  SkillFacts told;
  REQUIRE(asker.Plan(Wants(kBolt, 1002), told, kStart).damage.has_value());
  told.NoteDamageTooSoon(SkillDamageQuestion{.skill_id = kBolt, .target_mob_id = 1002});
  // La cible a changé entre-temps : c'est elle qui part, et pas deux questions.
  const std::uint32_t later = kStart + kSkillDamageSpacingMs + kSkillDamageSettleMs;
  const auto sent = asker.Plan(Wants(kHeal, 1031), told, later);
  REQUIRE(sent.damage.has_value());
  CHECK(*sent.damage == SkillDamageQuestion{.skill_id = kHeal, .target_mob_id = 1031});
  CHECK_FALSE(
      asker.Plan(Wants(kHeal, 1031), told, later + kSkillDamageSpacingMs).damage.has_value());
}

TEST_CASE("Estimation : le refus d'une question dépassée ne relance rien", "[app][grimoire]") {
  SkillFactsAsker asker;
  SkillFacts told;
  REQUIRE(asker.Plan(Wants(kBolt, 0), told, kStart).damage.has_value());
  const std::uint32_t second = kStart + kSkillDamageSpacingMs;
  REQUIRE(asker.Plan(Wants(kHeal, 0), told, second).damage.has_value());
  told.NoteDamageTooSoon(SkillDamageQuestion{.skill_id = kBolt, .target_mob_id = 0});
  CHECK_FALSE(
      asker.Plan(Wants(kHeal, 0), told, second + 2 * kSkillDamageSpacingMs).damage.has_value());
}

TEST_CASE("Estimation : l'onglet fermé abandonne ce qui attendait", "[app][grimoire]") {
  SkillFactsAsker asker;
  const SkillFacts told;
  REQUIRE(asker.Plan(Wants(kBolt, 0), told, kStart).damage.has_value());
  CHECK_FALSE(asker.Plan(Wants(kBolt, 7), told, kStart + kFrameMs).damage.has_value());
  CHECK_FALSE(asker.Plan(SkillDescriptionAction{}, told, kStart + 2 * kFrameMs).damage.has_value());
  CHECK_FALSE(asker.Plan(SkillDescriptionAction{}, told, kStart + 10 * kSkillDamageSettleMs)
                  .damage.has_value());
  // Rouvert sur la cible jamais envoyée, il la demande.
  const auto reopened = asker.Plan(Wants(kBolt, 7), told, kStart + 11 * kSkillDamageSettleMs);
  REQUIRE(reopened.damage.has_value());
  CHECK(reopened.damage->target_mob_id == 7);
}
