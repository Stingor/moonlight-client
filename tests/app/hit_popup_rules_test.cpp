#include "world/hit_popup_rules.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/damage_mark.h"
#include "ro/render/floating_number.h"
#include "ro/sim/combat_notice.h"

#include "world/hit_popup_look.h"

namespace {

using moonclient::CarriedTotal;
using moonclient::EffectNumber;
using moonclient::HitPopupLook;
using moonclient::HitSplit;
using moonclient::HitSplitOf;
using moonclient::OpacityOf;
using moonclient::PackAbgr;
using moonclient::PlanStrike;
using moonclient::RgbOf;
using moonclient::StrikePlan;
using moonclient::StrikeSides;
using moonclient::StrikeValueAt;
using moonclient::SupportNumber;
using moonclient::TargetTally;
using ro::render::DamageMark;
using ro::render::FloatingNumber;
using ro::render::NumberStyle;

using Rgb = std::array<std::uint8_t, 3>;

constexpr Rgb kWhite = {255, 255, 255};
constexpr Rgb kRed = {255, 0, 0};
constexpr Rgb kYellow = {255, 255, 0};
constexpr Rgb kSpBlue = {30, 144, 255};

// Les quatre côtés d'un coup qu'on rencontre en jeu. L'attaquant y est toujours
// CONNU — son acteur est en vue —, sans quoi le mot « Miss », qui naît sur lui,
// n'aurait nulle part où naître.
constexpr StrikeSides kOnMonsterByOther{.attacker_is_known = true, .attacker_is_player = true};
constexpr StrikeSides kOnMonsterByMe{
    .attacker_is_self = true, .attacker_is_known = true, .attacker_is_player = true};
// Un monstre qui frappe un autre joueur, et un monstre qui me frappe.
constexpr StrikeSides kOnPlayerByOther{.target_is_player = true, .attacker_is_known = true};
constexpr StrikeSides kOnMe{
    .target_is_player = true, .target_is_self = true, .attacker_is_known = true};

ro::sim::Strike Blow(std::int32_t damage, std::uint8_t type = 0, std::uint16_t hits = 1) {
  ro::sim::Strike strike;
  strike.damage = damage;
  strike.type = type;
  strike.hits = hits;
  return strike;
}

// Le même coup, porté par une compétence — n'importe laquelle —, et de la sorte
// qu'un paquet de compétence porte : aux sortes que l'impact ne traite pas, il
// ne montre rien.
constexpr std::uint16_t kSomeSkillId = 5;
constexpr std::uint8_t kSkillPacketSort = 6;

ro::sim::Strike Skill(std::int32_t damage, std::uint8_t type = kSkillPacketSort,
                      std::uint16_t hits = 1) {
  ro::sim::Strike strike = Blow(damage, type, hits);
  strike.skill_id = kSomeSkillId;
  return strike;
}

}  // namespace

TEST_CASE("Dégâts : le réglage vide est le client", "[app][degats]") {
  // Blanc et JAILLISSANT sur un monstre — coup ordinaire ou compétence, le genre
  // 14 des messages 26 et 96 —, rouge et jaillissant sur un joueur, jaune et
  // gros pour un critique, bleu pour la magie, et le cumul d'un coup multiple en
  // jaune montant à côté des parts. Un défaut qui dévierait ici ferait dévier
  // tous les joueurs qui n'ont rien réglé — et c'est arrivé : le premier port
  // faisait monter les coups sur un monstre.
  const HitPopupLook look;

  const StrikePlan on_monster = PlanStrike(Blow(10), kOnMonsterByOther, look);
  REQUIRE(on_monster.shown);
  CHECK_FALSE(on_monster.mark.has_value());
  CHECK(on_monster.number.color == kWhite);
  CHECK(on_monster.number.style == NumberStyle::kBursting);
  CHECK(PlanStrike(Skill(10), kOnMonsterByOther, look).number.style == NumberStyle::kBursting);
  CHECK(HitSplitOf(look.split) == HitSplit::kPartsAndTotal);
  const StrikePlan triple =
      PlanStrike(Blow(30, ro::render::kMultipleBlows, 3), kOnMonsterByOther, look);
  REQUIRE(triple.total.has_value());
  CHECK(triple.total->color == kYellow);
  CHECK(triple.total->style == NumberStyle::kRising);
  CHECK(on_monster.number.opacity == Catch::Approx(1.0F));
  CHECK_FALSE(on_monster.star);
  CHECK(on_monster.count == 1);

  const StrikePlan on_player = PlanStrike(Blow(10), kOnPlayerByOther, look);
  CHECK(on_player.number.color == kRed);
  CHECK(on_player.number.style == NumberStyle::kBursting);

  const StrikePlan critical =
      PlanStrike(Blow(10, ro::render::kCriticalBlow), kOnMonsterByOther, look);
  CHECK(critical.number.color == kYellow);
  CHECK(critical.number.style == NumberStyle::kCriticalBurst);
  CHECK(critical.star);

  // Le client ne distingue pas qui frappe : mes coups sur un monstre sont
  // blancs, comme les autres.
  CHECK(PlanStrike(Blow(10), kOnMonsterByMe, look).number.color == kWhite);
  CHECK(PlanStrike(Blow(10), kOnMe, look).number.color == kRed);
}

TEST_CASE("Dégâts : la magie l'emporte sur le critique, qui l'emporte sur la cible",
          "[app][degats]") {
  const HitPopupLook look;
  ro::sim::Strike strike = Blow(10, ro::render::kCriticalBlow);
  strike.sp_damage = true;
  const StrikePlan plan = PlanStrike(strike, kOnPlayerByOther, look);
  // La couleur est celle de la magie, l'allure reste celle du critique : c'est
  // l'ordre dans lequel le client écrase l'un par l'autre.
  CHECK(plan.number.color == kSpBlue);
  CHECK(plan.number.style == NumberStyle::kCriticalBurst);
  CHECK(plan.star);

  ro::sim::Strike plain = Blow(10);
  plain.sp_damage = true;
  CHECK(PlanStrike(plain, kOnPlayerByOther, look).number.color == kSpBlue);
  CHECK(PlanStrike(plain, kOnPlayerByOther, look).number.style == NumberStyle::kBursting);
}

TEST_CASE("Dégâts : trois interrupteurs disent qui frappe qui", "[app][degats]") {
  HitPopupLook look;
  look.others = false;
  CHECK_FALSE(PlanStrike(Blow(10), kOnMonsterByOther, look).shown);
  CHECK_FALSE(PlanStrike(Blow(10), kOnPlayerByOther, look).shown);
  CHECK(PlanStrike(Blow(10), kOnMonsterByMe, look).shown);
  CHECK(PlanStrike(Blow(10), kOnMe, look).shown);

  look = HitPopupLook{};
  look.own_dealt = false;
  CHECK_FALSE(PlanStrike(Blow(10), kOnMonsterByMe, look).shown);
  CHECK(PlanStrike(Blow(10), kOnMonsterByOther, look).shown);
  CHECK(PlanStrike(Blow(10), kOnMe, look).shown);

  look = HitPopupLook{};
  look.own_taken = false;
  CHECK_FALSE(PlanStrike(Blow(10), kOnMe, look).shown);
  // Un coup que je me porte est un coup REÇU, pas un coup porté.
  constexpr StrikeSides self_inflicted{
      .target_is_player = true, .target_is_self = true, .attacker_is_self = true};
  CHECK_FALSE(PlanStrike(Blow(10), self_inflicted, look).shown);
  CHECK(PlanStrike(Blow(10), kOnMonsterByMe, look).shown);

  // L'interrupteur général tait tout, les soins et les effets compris.
  look = HitPopupLook{};
  look.shown = false;
  CHECK_FALSE(PlanStrike(Blow(10), kOnMonsterByMe, look).shown);
  CHECK_FALSE(PlanStrike(Blow(0), kOnMonsterByMe, look).shown);
  CHECK_FALSE(SupportNumber(ro::render::kHealSkill, look).has_value());
  CHECK_FALSE(EffectNumber(ro::render::kHpOverTimeEffect, look).has_value());
}

TEST_CASE("Dégâts : la cible décide de la couleur, puis qui frappe", "[app][degats]") {
  HitPopupLook look;
  look.color_own_dealt = PackAbgr(1, 2, 3);
  look.color_on_monster = PackAbgr(4, 5, 6);
  look.color_on_player = PackAbgr(7, 8, 9);
  look.color_on_self = PackAbgr(10, 11, 12);

  CHECK(PlanStrike(Blow(10), kOnMonsterByMe, look).number.color == Rgb{1, 2, 3});
  CHECK(PlanStrike(Blow(10), kOnMonsterByOther, look).number.color == Rgb{4, 5, 6});
  CHECK(PlanStrike(Blow(10), kOnPlayerByOther, look).number.color == Rgb{7, 8, 9});
  CHECK(PlanStrike(Blow(10), kOnMe, look).number.color == Rgb{10, 11, 12});

  // Mes coups sur un joueur : la couleur des joueurs prime, comme au client
  // où c'est la cible qui décide.
  constexpr StrikeSides on_player_by_me{.target_is_player = true, .attacker_is_self = true};
  CHECK(PlanStrike(Blow(10), on_player_by_me, look).number.color == Rgb{7, 8, 9});
  // Et un coup que je me porte est « sur moi ».
  constexpr StrikeSides self_inflicted{
      .target_is_player = true, .target_is_self = true, .attacker_is_self = true};
  CHECK(PlanStrike(Blow(10), self_inflicted, look).number.color == Rgb{10, 11, 12});
}

TEST_CASE("Dégâts : la transparence d'un nuancier devient l'opacité du nombre", "[app][degats]") {
  HitPopupLook look;
  look.color_critical = PackAbgr(255, 255, 0, 128);
  const StrikePlan plan = PlanStrike(Blow(10, ro::render::kCriticalBlow), kOnMonsterByOther, look);
  CHECK(plan.number.color == kYellow);
  CHECK(plan.number.opacity == Catch::Approx(128.0F / 255.0F));

  // Une couleur ABGR se lit octet par octet, l'alpha en tête.
  CHECK(PackAbgr(1, 2, 3, 4) == 0x04030201U);
  CHECK(RgbOf(PackAbgr(1, 2, 3, 4)) == Rgb{1, 2, 3});
  CHECK(OpacityOf(PackAbgr(0, 0, 0, 255)) == Catch::Approx(1.0F));
  CHECK(OpacityOf(PackAbgr(0, 0, 0, 0)) == Catch::Approx(0.0F));
}

TEST_CASE("Dégâts : le « Miss » naît sur celui qui a raté, et de SA couleur", "[app][degats]") {
  // 🔴 CE N'EST PAS UN NOMBRE : un nombre va sur la cible, le mot va sur
  // l'ATTAQUANT. Quand je rate un monstre, le « Miss » est ROUGE et sur MOI ;
  // quand un monstre me rate, il est BLANC et sur le monstre. Le premier port
  // les posait tous deux sur la cible, à la couleur de la cible — un « Miss »
  // blanc paraissait sur le monstre, et le joueur devant les deux clients l'a
  // vu.
  const HitPopupLook look;

  const StrikePlan missed_by_me = PlanStrike(Blow(0), kOnMonsterByMe, look);
  REQUIRE(missed_by_me.shown);
  REQUIRE(missed_by_me.mark.has_value());
  CHECK(*missed_by_me.mark == DamageMark::kMiss);
  CHECK(missed_by_me.mark_on_attacker);
  CHECK(missed_by_me.number.color == kRed);

  const StrikePlan missed_me = PlanStrike(Blow(0), kOnMe, look);
  REQUIRE(missed_me.shown);
  CHECK(missed_me.mark_on_attacker);
  CHECK(missed_me.number.color == kWhite);

  // Un autre joueur qui rate un monstre porte son mot en rouge, lui aussi : la
  // couleur suit le PORTEUR et non celui qui regarde.
  CHECK(PlanStrike(Blow(0), kOnMonsterByOther, look).number.color == kRed);

  // SANS ATTAQUANT, RIEN : le client cherche son acteur et renonce quand il ne
  // le trouve pas. C'est le cas d'une unité au sol, qui n'en a pas.
  constexpr StrikeSides kByNobody{};
  CHECK_FALSE(PlanStrike(Blow(0), kByNobody, look).shown);
}

TEST_CASE("Dégâts : les mots ont leur interrupteur, et l'esquive passe avant le zéro",
          "[app][degats]") {
  HitPopupLook look;
  const StrikePlan miss = PlanStrike(Blow(0), kOnMonsterByOther, look);
  REQUIRE(miss.shown);
  REQUIRE(miss.mark.has_value());
  CHECK(*miss.mark == DamageMark::kMiss);

  // La bulle passe avant le zéro : un montant ne l'empêche pas, elle ne se
  // teinte pas, et elle reste sur CELUI QUI A ESQUIVÉ — la cible.
  const StrikePlan lucky =
      PlanStrike(Blow(50, ro::render::kLuckyDodgeBlow), kOnPlayerByOther, look);
  REQUIRE(lucky.mark.has_value());
  CHECK(*lucky.mark == DamageMark::kLucky);
  CHECK(lucky.number.color == kWhite);
  CHECK_FALSE(lucky.mark_on_attacker);

  look.miss = false;
  CHECK_FALSE(PlanStrike(Blow(0), kOnMonsterByOther, look).shown);
  CHECK(PlanStrike(Blow(0, ro::render::kLuckyDodgeBlow), kOnMonsterByOther, look).shown);

  look = HitPopupLook{};
  look.lucky = false;
  CHECK_FALSE(PlanStrike(Blow(0, ro::render::kLuckyDodgeBlow), kOnMonsterByOther, look).shown);
  CHECK(PlanStrike(Blow(0), kOnMonsterByOther, look).shown);
}

TEST_CASE("Dégâts : un montant négatif est un coup factice, ni raté ni nombre", "[app][degats]") {
  // Cart Revolution fait précéder ses coups d'une annonce FACTICE sur la
  // première cible de sa zone : la compétence, la sorte « compétence », un
  // coup, et −30000 pour montant. Le client la tait ; la prendre pour un raté
  // faisait paraître un « Miss » rouge sur le lanceur.
  constexpr std::uint16_t kCartRevolutionId = 153;
  constexpr std::int32_t kPreambleDamage = -30000;
  ro::sim::Strike preamble = Skill(kPreambleDamage);
  preamble.skill_id = kCartRevolutionId;

  HitPopupLook look;
  const StrikePlan silent = PlanStrike(preamble, kOnMonsterByMe, look);
  CHECK_FALSE(silent.shown);
  CHECK_FALSE(silent.mark.has_value());
  // Ni sous un autre partage, ni vu d'un autre joueur, ni reçu.
  for (int split = 0; split < static_cast<int>(HitSplit::kCount); ++split) {
    look.split = split;
    CHECK_FALSE(PlanStrike(preamble, kOnMonsterByMe, look).shown);
    CHECK_FALSE(PlanStrike(preamble, kOnMonsterByOther, look).shown);
    CHECK_FALSE(PlanStrike(preamble, kOnMe, look).shown);
  }

  // LES VRAIS RATÉS PARAISSENT TOUJOURS : une attaque ordinaire à zéro, et la
  // même compétence à zéro.
  look = HitPopupLook{};
  const StrikePlan plain_miss = PlanStrike(Blow(0), kOnMonsterByMe, look);
  REQUIRE(plain_miss.mark.has_value());
  CHECK(*plain_miss.mark == DamageMark::kMiss);
  ro::sim::Strike skill_miss = Skill(0);
  skill_miss.skill_id = kCartRevolutionId;
  const StrikePlan skill_missed = PlanStrike(skill_miss, kOnMonsterByMe, look);
  REQUIRE(skill_missed.mark.has_value());
  CHECK(*skill_missed.mark == DamageMark::kMiss);
}

TEST_CASE("Dégâts : l'étoile du critique se tait sans changer le nombre", "[app][degats]") {
  HitPopupLook look;
  look.critical_star = false;
  const StrikePlan plan = PlanStrike(Blow(10, ro::render::kCriticalBlow), kOnMonsterByOther, look);
  REQUIRE(plan.shown);
  CHECK_FALSE(plan.star);
  CHECK(plan.number.color == kYellow);
  CHECK(plan.number.style == NumberStyle::kCriticalBurst);
}

TEST_CASE("Dégâts : un coup multiple se partage de quatre façons", "[app][degats]") {
  const ro::sim::Strike triple = Blow(30, ro::render::kMultipleBlows, 3);
  HitPopupLook look;

  // Le client : chaque part, et à côté la somme des coups déjà portés — dix,
  // puis dix et vingt, puis dix et trente.
  StrikePlan plan = PlanStrike(triple, kOnMonsterByOther, look);
  REQUIRE(plan.count == 3);
  REQUIRE(plan.total.has_value());
  CHECK(StrikeValueAt(triple, plan, 0) == 10);
  CHECK(StrikeValueAt(triple, plan, 2) == 10);
  CHECK(StrikeTotalAt(triple, plan, 0) == 10);
  CHECK(StrikeTotalAt(triple, plan, 1) == 20);
  CHECK(StrikeTotalAt(triple, plan, 2) == 30);
  // Sur un joueur, le client ne montre que les parts ; et un coup seul n'a pas
  // de cumul.
  CHECK_FALSE(PlanStrike(triple, kOnPlayerByOther, look).total.has_value());
  CHECK_FALSE(PlanStrike(Blow(30), kOnMonsterByOther, look).total.has_value());
  // La magie l'emporte sur la couleur du cumul comme sur celle des parts.
  ro::sim::Strike sp_triple = triple;
  sp_triple.sp_damage = true;
  CHECK(PlanStrike(sp_triple, kOnMonsterByOther, look).total->color == kSpBlue);

  // En cumulant : un tiers, deux tiers, le tout — et plus de cumul à côté.
  look.split = static_cast<int>(HitSplit::kCumulative);
  plan = PlanStrike(triple, kOnMonsterByOther, look);
  REQUIRE(plan.count == 3);
  CHECK_FALSE(plan.total.has_value());
  CHECK(StrikeValueAt(triple, plan, 0) == 10);
  CHECK(StrikeValueAt(triple, plan, 1) == 20);
  CHECK(StrikeValueAt(triple, plan, 2) == 30);

  look.split = static_cast<int>(HitSplit::kEachPart);
  plan = PlanStrike(triple, kOnMonsterByOther, look);
  REQUIRE(plan.count == 3);
  CHECK(StrikeValueAt(triple, plan, 0) == 10);
  CHECK(StrikeValueAt(triple, plan, 2) == 10);

  look.split = static_cast<int>(HitSplit::kTotalOnce);
  plan = PlanStrike(triple, kOnMonsterByOther, look);
  CHECK(plan.count == 1);
  CHECK_FALSE(plan.total.has_value());
  CHECK(StrikeValueAt(triple, plan, 0) == 30);

  // Un coup ordinaire qui annoncerait trois coups n'en montre qu'un, quel que
  // soit le partage : le champ ne compte que pour les sortes qui l'annoncent.
  look.split = static_cast<int>(HitSplit::kCumulative);
  plan = PlanStrike(Blow(30, 0, 3), kOnMonsterByOther, look);
  CHECK(plan.count == 1);
  CHECK(StrikeValueAt(Blow(30, 0, 3), plan, 0) == 30);
}

TEST_CASE("Dégâts : le coup d'une unité au sol porte son cumul même seul", "[app][degats]") {
  // Storm Gust : le serveur n'annonce qu'UNE frappe, et le client montre
  // pourtant deux nombres du même montant — la part blanche et le cumul jaune
  // qui monte. Son répartiteur à lui passe le cumul sans condition, là où celui
  // d'un acteur ne le passe qu'au-delà d'une frappe.
  HitPopupLook look;
  const ro::sim::Strike blizzard = Skill(107116);
  constexpr StrikeSides from_ground_unit{.attacker_is_ground_unit = true};

  const StrikePlan plan = PlanStrike(blizzard, from_ground_unit, look);
  REQUIRE(plan.count == 1);
  REQUIRE(plan.total.has_value());
  CHECK(plan.total->color == kYellow);
  CHECK(plan.total->style == NumberStyle::kRising);
  CHECK(StrikeValueAt(blizzard, plan, 0) == 107116);
  CHECK(StrikeTotalAt(blizzard, plan, 0) == 107116);
  // Le même coup parti d'un ACTEUR n'en montre qu'un.
  CHECK_FALSE(PlanStrike(blizzard, kOnMonsterByOther, look).total.has_value());
  // Sur un joueur, les parts seules — l'unité au sol n'y change rien.
  constexpr StrikeSides on_player_from_ground_unit{.target_is_player = true,
                                                   .attacker_is_ground_unit = true};
  CHECK_FALSE(PlanStrike(blizzard, on_player_from_ground_unit, look).total.has_value());

  // ET SON DÉCOUPAGE NE LIT PAS LA SORTE DU COUP : trois frappes annoncées font
  // trois parts, quand un acteur qui annoncerait la même chose n'en ferait
  // qu'une, faute d'une sorte qui dise « multiple ».
  const ro::sim::Strike three = Skill(30, 0, 3);
  const StrikePlan split = PlanStrike(three, from_ground_unit, look);
  CHECK(split.count == 3);
  CHECK(StrikeValueAt(three, split, 0) == 10);
  CHECK(StrikeTotalAt(three, split, 2) == 30);
  CHECK(PlanStrike(three, kOnMonsterByOther, look).count == 1);

  // Le partage garde le dernier mot : « le total, une fois » n'a pas de cumul.
  look.split = static_cast<int>(HitSplit::kTotalOnce);
  const StrikePlan once = PlanStrike(blizzard, from_ground_unit, look);
  CHECK(once.count == 1);
  CHECK_FALSE(once.total.has_value());
}

namespace {

// Les sortes de coup qu'un paquet de compétence porte : e_damage_type.
constexpr std::uint8_t kSplashBlow = 5;
constexpr std::uint8_t kSkillBlow = 6;
constexpr std::uint16_t kStormKick = 413;
constexpr std::uint16_t kTigerFist = 371;

ro::sim::Strike Kick(std::uint16_t skill_id, std::int32_t damage, std::uint8_t type,
                     std::uint16_t hits = 1) {
  ro::sim::Strike strike = Blow(damage, type, hits);
  strike.skill_id = skill_id;
  return strike;
}

}  // namespace

TEST_CASE("Dégâts : un coup de pied écrit sa somme, qui monte, blanche, étoilée au premier",
          "[app][degats]") {
  const HitPopupLook look;
  // Trois frappes de 333 : 333, 666, 999 — un nombre par frappe, la SOMME des
  // frappes déjà portées, et rien à côté.
  const ro::sim::Strike three = Kick(kStormKick, 999, ro::render::kMultipleBlows, 3);
  const StrikePlan plan = PlanStrike(three, kOnMonsterByMe, look);
  REQUIRE(plan.shown);
  CHECK(plan.count == 3);
  CHECK(plan.number.style == NumberStyle::kRising);
  CHECK(plan.number.color == kWhite);
  CHECK_FALSE(plan.total.has_value());
  CHECK_FALSE(plan.star);
  CHECK(plan.rising_star);
  CHECK(StrikeValueAt(three, plan, 0) == 333);
  CHECK(StrikeValueAt(three, plan, 1) == 666);
  CHECK(StrikeValueAt(three, plan, 2) == 999);

  // Une frappe seule, sorte « compétence » : le même nombre, sans total jaune.
  const StrikePlan single = PlanStrike(Kick(kStormKick, 500, kSkillBlow), kOnMonsterByOther, look);
  CHECK(single.count == 1);
  CHECK(single.number.style == NumberStyle::kRising);
  CHECK_FALSE(single.total.has_value());
  CHECK(single.rising_star);

  // MÊME CRITIQUE : le client remplace la sorte du coup par la sienne, et le
  // nombre ne prend ni le jaune ni l'allure du critique.
  const StrikePlan critical = PlanStrike(
      Kick(kStormKick, 900, ro::render::kCriticalMultipleBlows, 3), kOnMonsterByMe, look);
  CHECK(critical.number.color == kWhite);
  CHECK(critical.number.style == NumberStyle::kRising);
  CHECK_FALSE(critical.star);
  CHECK(critical.rising_star);

  // Sur un joueur, les parts en rouge, qui jaillissent : rien ne change.
  const StrikePlan on_player = PlanStrike(three, kOnPlayerByOther, look);
  CHECK(on_player.number.color == kRed);
  CHECK(on_player.number.style == NumberStyle::kBursting);
  CHECK_FALSE(on_player.rising_star);
  CHECK(StrikeValueAt(three, on_player, 1) == 333);

  // Le coup qui éclabousse passe par une autre voie, qui ne lit pas la liste…
  constexpr std::uint16_t kDownKick = 415;
  const StrikePlan splash = PlanStrike(Kick(kDownKick, 500, kSplashBlow), kOnMonsterByMe, look);
  CHECK(splash.number.style == NumberStyle::kBursting);
  CHECK_FALSE(splash.rising_star);
  // … sauf pour les compétences que l'impact y range à part : Storm Kick y
  // prend la voie longue, et y monte.
  CHECK(PlanStrike(Kick(kStormKick, 500, kSplashBlow), kOnMonsterByMe, look).rising_star);

  // Un autre partage est un choix du joueur : le coup de pied le suit.
  HitPopupLook parts;
  parts.split = static_cast<int>(HitSplit::kEachPart);
  const StrikePlan chosen = PlanStrike(three, kOnMonsterByMe, parts);
  CHECK_FALSE(chosen.rising_star);
  CHECK(StrikeValueAt(three, chosen, 2) == 333);

  // Il prend le type du total, sa vie et sa trajectoire : qui règle l'un règle
  // aussi l'autre.
  const StrikePlan kicked = PlanStrike(three, kOnMonsterByMe, look);
  CHECK(kicked.path == moonclient::DamagePath::kTotal);
  CHECK(kicked.number.style == NumberStyle::kRising);
}

TEST_CASE("Dégâts : neuf compétences portent leur total même en une frappe", "[app][degats]") {
  const HitPopupLook look;
  // Tiger Fist, une frappe : la part qui jaillit, et le total jaune qui monte.
  const ro::sim::Strike fist = Kick(kTigerFist, 700, kSkillBlow);
  const StrikePlan plan = PlanStrike(fist, kOnMonsterByMe, look);
  CHECK(plan.count == 1);
  CHECK(plan.number.style == NumberStyle::kBursting);
  REQUIRE(plan.total.has_value());
  CHECK(plan.total->color == kYellow);
  CHECK(plan.total->style == NumberStyle::kRising);
  CHECK(StrikeValueAt(fist, plan, 0) == 700);
  CHECK(StrikeTotalAt(fist, plan, 0) == 700);
  // Une compétence quelconque n'en montre qu'un ; sur un joueur, la part seule ;
  // et la sorte qui éclabousse n'y passe pas.
  CHECK_FALSE(PlanStrike(Skill(700, kSkillBlow), kOnMonsterByMe, look).total.has_value());
  CHECK_FALSE(PlanStrike(fist, kOnPlayerByOther, look).total.has_value());
  CHECK_FALSE(
      PlanStrike(Kick(kTigerFist, 700, kSplashBlow), kOnMonsterByMe, look).total.has_value());
}

TEST_CASE("Dégâts : l'impact d'une compétence divise par le compte annoncé, même en un nombre",
          "[app][degats]") {
  constexpr std::uint8_t kEnduredBlow = 4;
  constexpr std::uint8_t kSplashBlowEndured = 14;
  HitPopupLook look;
  // Encaissé, trois frappes annoncées : UN nombre, et c'est le tiers. Le reste
  // ne paraît nulle part.
  const ro::sim::Strike endured = Skill(900, kEnduredBlow, 3);
  const StrikePlan plan = PlanStrike(endured, kOnMonsterByOther, look);
  CHECK(plan.count == 1);
  CHECK(StrikeValueAt(endured, plan, 0) == 300);
  // La sorte « compétence » et les deux éclaboussures, de même.
  for (const std::uint8_t sort : {kSkillBlow, kSplashBlow, kSplashBlowEndured}) {
    const ro::sim::Strike blow = Skill(900, sort, 3);
    const StrikePlan one = PlanStrike(blow, kOnMonsterByOther, look);
    CHECK(one.count == 1);
    CHECK(StrikeValueAt(blow, one, 0) == 300);
  }
  // Un coup de pied qui ne montre qu'un nombre écrit aussi la part.
  const ro::sim::Strike kick = Kick(kStormKick, 900, kSkillBlow, 3);
  CHECK(StrikeValueAt(kick, PlanStrike(kick, kOnMonsterByMe, look), 0) == 300);
  // Un compte que le client ne lit pas ne divise rien.
  const ro::sim::Strike absurd = Skill(900, kEnduredBlow, ro::render::kLargestHitCount + 1);
  CHECK(StrikeValueAt(absurd, PlanStrike(absurd, kOnMonsterByOther, look), 0) == 900);
  // Le coup ordinaire ne passe pas par là : sa sorte encaissée ne divise pas.
  const ro::sim::Strike plain = Blow(900, kEnduredBlow, 3);
  CHECK(StrikeValueAt(plain, PlanStrike(plain, kOnMonsterByOther, look), 0) == 900);
  // « Le total, une fois » est un choix du joueur : le montant entier.
  look.split = static_cast<int>(HitSplit::kTotalOnce);
  CHECK(StrikeValueAt(endured, PlanStrike(endured, kOnMonsterByOther, look), 0) == 900);
}

TEST_CASE("Dégâts : une compétence hors des sortes de l'impact, ou sans lanceur connu, se tait",
          "[app][degats]") {
  const HitPopupLook look;
  // Un critique simple, un coup ordinaire : l'impact rend la main sans rien.
  CHECK_FALSE(PlanStrike(Skill(900, ro::render::kCriticalBlow), kOnMonsterByOther, look).shown);
  CHECK_FALSE(PlanStrike(Skill(900, 0), kOnMonsterByOther, look).shown);
  CHECK_FALSE(PlanStrike(Skill(0, 0), kOnMonsterByOther, look).shown);
  // Un lanceur hors de vue : l'impact n'a personne à qui s'envoyer.
  constexpr StrikeSides unseen{.attacker_is_player = true};
  CHECK_FALSE(PlanStrike(Skill(900), unseen, look).shown);
  // Un coup ordinaire n'est pas concerné ; une unité au sol non plus.
  CHECK(PlanStrike(Blow(900, ro::render::kCriticalBlow), kOnMonsterByOther, look).shown);
  constexpr StrikeSides from_ground_unit{.attacker_is_ground_unit = true};
  CHECK(PlanStrike(Skill(900, 0), from_ground_unit, look).shown);
}

TEST_CASE("Dégâts : la seconde main paraît pour les métiers qui la montrent", "[app][degats]") {
  const HitPopupLook look;
  ro::sim::Strike strike = Blow(200, ro::render::kMultipleBlows, 2);
  strike.second_damage = 50;
  StrikeSides assassin = kOnMonsterByOther;
  assassin.attacker_job = 12;
  const StrikePlan plan = PlanStrike(strike, assassin, look);
  CHECK(plan.second_hand);
  constexpr std::uint32_t kStepMs = 200;
  // Les frappes suivantes partent ensemble, un demi-pas après la première.
  CHECK(moonclient::StrikeHitOffsetMs(plan, 0, kStepMs) == 0);
  CHECK(moonclient::StrikeHitOffsetMs(plan, 1, kStepMs) == kStepMs / 2);
  // Un chevalier ne la montre pas ; une compétence non plus.
  StrikeSides knight = kOnMonsterByOther;
  knight.attacker_job = 7;
  const StrikePlan plain = PlanStrike(strike, knight, look);
  CHECK_FALSE(plain.second_hand);
  CHECK(moonclient::StrikeHitOffsetMs(plain, 1, kStepMs) == kStepMs);
  ro::sim::Strike skill = Skill(200, ro::render::kMultipleBlows, 2);
  skill.second_damage = 50;
  CHECK_FALSE(PlanStrike(skill, assassin, look).second_hand);
}

TEST_CASE("Dégâts : l'impact d'une compétence ne teint jamais en bleu", "[app][degats]") {
  // L'octet « magie » du message de dégâts n'y est pas posé : ni la part, ni le
  // total, ni le nombre d'un coup de pied ne prennent la couleur de la magie.
  const HitPopupLook look;
  ro::sim::Strike triple = Skill(900, ro::render::kMultipleBlows, 3);
  triple.sp_damage = true;
  const StrikePlan plan = PlanStrike(triple, kOnMonsterByOther, look);
  CHECK(plan.number.color == kWhite);
  REQUIRE(plan.total.has_value());
  CHECK(plan.total->color == kYellow);
  ro::sim::Strike kick = Kick(kStormKick, 900, ro::render::kMultipleBlows, 3);
  kick.sp_damage = true;
  CHECK(PlanStrike(kick, kOnMonsterByMe, look).number.color == kWhite);
  // Hors de ce chemin, la magie garde la priorité.
  ro::sim::Strike blow = Blow(900);
  blow.sp_damage = true;
  CHECK(PlanStrike(blow, kOnMonsterByOther, look).number.color == kSpBlue);
}

TEST_CASE("Dégâts : le vrai cumul survit d'un coup à l'autre, puis s'oublie", "[app][degats]") {
  // Le client recommence à chaque annonce ; le réglage, lui, garde la somme
  // tant que les coups s'enchaînent sur la même cible.
  constexpr float kForgetMs = 2000.0F;
  constexpr TargetTally tally{.total = 1000, .last_ms = 10000};

  CHECK(CarriedTotal(tally, 10000, kForgetMs) == 1000);
  CHECK(CarriedTotal(tally, 12000, kForgetMs) == 1000);
  // Une milliseconde de trop, et la cible est oubliée.
  CHECK(CarriedTotal(tally, 12001, kForgetMs) == 0);
  // Un coup qui touche AVANT le précédent appartient à la même rafale : les
  // durées d'animation les rendent dans le désordre.
  CHECK(CarriedTotal(tally, 9000, kForgetMs) == 1000);
  // Une cible dont on ne sait rien ne porte rien.
  CHECK(CarriedTotal(TargetTally{}, 50000, kForgetMs) == 0);
  // Un délai nul oublie dès la milliseconde suivante, et jamais le coup même.
  CHECK(CarriedTotal(tally, 10000, 0.0F) == 1000);
  CHECK(CarriedTotal(tally, 10001, 0.0F) == 0);
}

TEST_CASE("Dégâts : chaque famille a son type, et l'allure du client pour ce type",
          "[app][degats]") {
  using moonclient::DamagePath;
  const HitPopupLook look;
  const StrikePlan blow = PlanStrike(Blow(10), kOnMonsterByOther, look);
  CHECK(blow.path == DamagePath::kOnMonster);
  CHECK(blow.number.style == NumberStyle::kBursting);
  // La compétence a son type sur un monstre — et sur un joueur, c'est le type
  // des joueurs, compétence ou non.
  CHECK(PlanStrike(Skill(10), kOnMonsterByOther, look).path == DamagePath::kSkillOnMonster);
  CHECK(PlanStrike(Blow(10), kOnPlayerByOther, look).path == DamagePath::kOnPlayer);
  CHECK(PlanStrike(Skill(10), kOnPlayerByOther, look).path == DamagePath::kOnPlayer);
  // Le critique l'emporte sur la compétence aussi, et jaillit un peu plus gros.
  const StrikePlan critical =
      PlanStrike(Skill(10, ro::render::kCriticalMultipleBlows), kOnMonsterByOther, look);
  CHECK(critical.path == DamagePath::kCritical);
  CHECK(critical.number.style == NumberStyle::kCriticalBurst);
  const StrikePlan triple =
      PlanStrike(Blow(30, ro::render::kMultipleBlows, 3), kOnMonsterByOther, look);
  REQUIRE(triple.total.has_value());
  CHECK(triple.total->style == NumberStyle::kRising);

  // Un rang hors des listes retombe sur le client.
  CHECK(HitSplitOf(-1) == HitSplit::kCumulative);
  CHECK(HitSplitOf(99) == HitSplit::kCumulative);
  CHECK(HitSplitOf(1) == HitSplit::kEachPart);
  CHECK(moonclient::DamagePathOf(99) == DamagePath::kOnMonster);
}

TEST_CASE("Dégâts : le soin et la magie rendue suivent leurs nuanciers et l'allure du soin",
          "[app][degats]") {
  HitPopupLook look;
  const std::optional<FloatingNumber> heal = SupportNumber(ro::render::kHealSkill, look);
  REQUIRE(heal.has_value());
  CHECK(heal->color == ro::render::kHealNumberColor);
  CHECK(heal->style == NumberStyle::kHealing);
  const std::optional<FloatingNumber> magic = SupportNumber(ro::render::kSpRecoverySkill, look);
  REQUIRE(magic.has_value());
  CHECK(magic->color == ro::render::kSpRecoveryNumberColor);

  // Les autres compétences sans dégâts n'en portent pas, et l'interrupteur les
  // tait toutes deux.
  CHECK_FALSE(SupportNumber(1, look).has_value());
  look.heals = false;
  CHECK_FALSE(SupportNumber(ro::render::kHealSkill, look).has_value());

  look = HitPopupLook{};
  look.color_heal = PackAbgr(9, 9, 9);
  const std::optional<FloatingNumber> tinted = SupportNumber(ro::render::kHealSkill, look);
  REQUIRE(tinted.has_value());
  CHECK(tinted->color == Rgb{9, 9, 9});
  CHECK(tinted->style == NumberStyle::kHealing);
}

TEST_CASE(
    "Dégâts : les nombres des effets gardent leur couleur de script, sauf les deux qui "
    "comptent",
    "[app][degats]") {
  HitPopupLook look;
  look.color_heal = PackAbgr(9, 9, 9);
  look.color_sp_recovery = PackAbgr(8, 8, 8);

  const std::optional<FloatingNumber> life = EffectNumber(ro::render::kHpOverTimeEffect, look);
  REQUIRE(life.has_value());
  CHECK(life->color == Rgb{9, 9, 9});
  CHECK(life->style == NumberStyle::kHealing);
  const std::optional<FloatingNumber> magic = EffectNumber(ro::render::kSpOverTimeEffect, look);
  REQUIRE(magic.has_value());
  CHECK(magic->color == Rgb{8, 8, 8});

  // Le troisième chiffre coloré est ROUGE chez le client, et le reste : c'est
  // le script du serveur qui a choisi cette couleur.
  const std::optional<FloatingNumber> scripted =
      EffectNumber(ro::render::kFirstColoredNumberEffect + 2, look);
  REQUIRE(scripted.has_value());
  CHECK(scripted->color == kRed);

  CHECK_FALSE(EffectNumber(0, look).has_value());
  look.effect_numbers = false;
  CHECK_FALSE(EffectNumber(ro::render::kHpOverTimeEffect, look).has_value());
}

TEST_CASE("Dégâts : la dispersion tire un écart stable autour du point d'arrivée",
          "[app][degats]") {
  using moonclient::ScatterOf;
  using moonclient::ScatterSeed;
  using Spread = std::array<float, 2>;

  // Sans dispersion, aucun écart — quel que soit le germe.
  moonclient::NumberPath path;
  CHECK(ScatterOf(path, ScatterSeed(7, 1000, 0)) == Spread{0.0F, 0.0F});

  path.scatter = true;
  path.scatter_x_pixels = 12.0F;
  path.scatter_y_pixels = 6.0F;
  const std::uint64_t seed = ScatterSeed(7, 1000, 0);
  const Spread first = ScatterOf(path, seed);
  // Le même germe rend le même écart : un nombre ne saute pas d'une image à
  // l'autre.
  CHECK(ScatterOf(path, seed) == first);
  // Il reste dans les rayons, et il n'est pas nul : la dispersion disperse.
  CHECK(first[0] >= -12.0F);
  CHECK(first[0] <= 12.0F);
  CHECK(first[1] >= -6.0F);
  CHECK(first[1] <= 6.0F);
  CHECK(first != Spread{0.0F, 0.0F});

  // Trois nombres d'un même coup ne tombent pas au même endroit, et une autre
  // cible au même instant non plus.
  const Spread second = ScatterOf(path, ScatterSeed(7, 1000, 1));
  const Spread third = ScatterOf(path, ScatterSeed(7, 1000, 2));
  CHECK(first != second);
  CHECK(second != third);
  CHECK(ScatterOf(path, ScatterSeed(8, 1000, 0)) != first);

  // Cent germes, tous dans les rayons.
  for (std::uint16_t index = 0; index < 100; ++index) {
    const Spread spread = ScatterOf(path, ScatterSeed(7, 1000, index));
    CHECK(spread[0] >= -12.0F);
    CHECK(spread[0] <= 12.0F);
    CHECK(spread[1] >= -6.0F);
    CHECK(spread[1] <= 6.0F);
  }

  // Un rayon nul ou négatif vaut le plus petit, comme le volet le montre.
  path.scatter_y_pixels = 0.0F;
  path.scatter_x_pixels = -5.0F;
  const Spread smallest = ScatterOf(path, seed);
  CHECK(std::abs(smallest[0]) <= moonclient::kSmallestScatterPixels);
  CHECK(smallest[0] != 0.0F);
}

TEST_CASE("Dégâts : l'aperçu en boucle rejoue chaque famille, puis recommence", "[app][degats]") {
  using moonclient::kRehearsalCount;
  using moonclient::Rehearsal;
  using moonclient::RehearsalAt;
  const HitPopupLook look;

  // Chaque entrée montre quelque chose sous le réglage du client : un coup que
  // les règles laissent paraître, ou un soin.
  for (std::size_t index = 0; index < kRehearsalCount; ++index) {
    const Rehearsal shown = RehearsalAt(index);
    if (shown.support_skill_id != 0) {
      CHECK(SupportNumber(shown.support_skill_id, look).has_value());
      CHECK(shown.support_amount > 0);
    } else {
      CHECK(PlanStrike(shown.strike, shown.sides, look).shown);
    }
  }
  // Et la boucle recommence.
  CHECK(RehearsalAt(kRehearsalCount).strike.damage == RehearsalAt(0).strike.damage);
  CHECK(RehearsalAt(kRehearsalCount + 1).strike.skill_id == RehearsalAt(1).strike.skill_id);

  // Les familles qu'on règle y sont toutes : mon coup, ma compétence, le coup
  // multiple avec son cumul, le critique et son étoile, le coup reçu, celui des
  // autres, les deux mots, la magie, le soin, la magie rendue.
  CHECK(PlanStrike(RehearsalAt(0).strike, RehearsalAt(0).sides, look).number.color == kWhite);
  CHECK(RehearsalAt(1).strike.skill_id != 0);
  const StrikePlan multiple = PlanStrike(RehearsalAt(2).strike, RehearsalAt(2).sides, look);
  CHECK(multiple.count == 3);
  CHECK(multiple.total.has_value());
  CHECK(PlanStrike(RehearsalAt(3).strike, RehearsalAt(3).sides, look).star);
  CHECK(PlanStrike(RehearsalAt(4).strike, RehearsalAt(4).sides, look).number.color == kRed);
  HitPopupLook without_others = look;
  without_others.others = false;
  CHECK_FALSE(PlanStrike(RehearsalAt(5).strike, RehearsalAt(5).sides, without_others).shown);
  // Le « Miss » de l'aperçu naît sur le personnage, comme en jeu : c'est lui
  // qui rate. Sans l'attaquant dans ses côtés, l'aperçu ne montrerait rien.
  const StrikePlan rehearsed_miss = PlanStrike(RehearsalAt(6).strike, RehearsalAt(6).sides, look);
  CHECK(rehearsed_miss.mark == DamageMark::kMiss);
  CHECK(rehearsed_miss.mark_on_attacker);
  CHECK(rehearsed_miss.number.color == kRed);
  CHECK(PlanStrike(RehearsalAt(7).strike, RehearsalAt(7).sides, look).mark == DamageMark::kLucky);
  CHECK(PlanStrike(RehearsalAt(8).strike, RehearsalAt(8).sides, look).number.color == kSpBlue);
  CHECK(RehearsalAt(9).support_skill_id == ro::render::kHealSkill);
  CHECK(RehearsalAt(10).support_skill_id == ro::render::kSpRecoverySkill);
}
