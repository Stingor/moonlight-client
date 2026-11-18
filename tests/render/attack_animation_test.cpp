#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"

#include "support/sprite_builder.h"

using ro::render::ActorLook;
using ro::render::ArrowDelayMs;
using ro::render::ArrowSpriteOf;
using ro::render::AttackAnimation;
using ro::render::AttackMotionFactor;
using ro::render::AttackSoundFrame;
using ro::render::AttackVariantAnimation;
using ro::render::DamageFrame;
using ro::render::HurtMotionFactor;
using ro::render::IsHomunculusJob;
using ro::render::kAnimationAttack2;
using ro::render::kAnimationAttack3;
using ro::render::kCreatureAnimationAttack;
using ro::render::kHomunculusAnimationAttack2;
using ro::render::ProjectileDelayMs;
using ro::render::ShootsArrows;
using ro::render::SkillHitProjectileDelayMs;
using ro::render::SpriteCatalog;

namespace {

// Un objet par classe d'arme, et l'arbalète, classe étendue de l'arc.
struct ItemViewSpec {
  std::uint32_t item;
  std::uint16_t view;
};
constexpr std::array<ItemViewSpec, 17> kItemViews = {{{1201, 1},
                                                      {1101, 2},
                                                      {1151, 3},
                                                      {1401, 4},
                                                      {1451, 5},
                                                      {1301, 6},
                                                      {1501, 8},
                                                      {1601, 10},
                                                      {1701, 11},
                                                      {1801, 12},
                                                      {1550, 15},
                                                      {1250, 16},
                                                      {13100, 17},
                                                      {1702, 31},
                                                      {2001, 23},
                                                      {1616, 69},
                                                      {2004, 97}}};
constexpr std::uint32_t kDagger = 1201;
constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kTwoHandSword = 1151;
constexpr std::uint32_t kSpear = 1401;
constexpr std::uint32_t kTwoHandSpear = 1451;
constexpr std::uint32_t kAxe = 1301;
constexpr std::uint32_t kMace = 1501;
constexpr std::uint32_t kRod = 1601;
constexpr std::uint32_t kBow = 1701;
constexpr std::uint32_t kKnuckle = 1801;
constexpr std::uint32_t kBook = 1550;
constexpr std::uint32_t kKatar = 1250;
constexpr std::uint32_t kHandgun = 13100;
constexpr std::uint32_t kCrossbow = 1702;
// Le bâton à deux mains, et deux sous-types de bâton que la table des classes
// étendues replie : l'un sur le bâton, l'autre sur le bâton à deux mains.
constexpr std::uint32_t kTwoHandRod = 2001;
constexpr std::uint32_t kArcWand = 1616;
constexpr std::uint32_t kWizardryStaff = 2004;

SpriteCatalog Catalog() {
  SpriteCatalog catalog;
  catalog.tables.weapon_families = {{31, 11}, {69, 10}, {97, 23}};
  for (const auto& [item, view] : kItemViews) {
    catalog.items.Put(item, ro::vfs::ItemEntry{.view_id = view});
  }
  return catalog;
}

constexpr std::uint8_t kFemale = 0;
constexpr std::uint8_t kMale = 1;

ActorLook Armed(std::uint32_t job, std::uint8_t sex, std::uint32_t weapon,
                std::uint32_t shield = 0) {
  ActorLook look;
  look.player = true;
  look.job = job;
  look.sex = sex;
  look.weapon = weapon;
  look.shield = shield;
  return look;
}

// Une créature de cette espèce.
ActorLook Creature(std::uint32_t job) {
  ActorLook look;
  look.player = false;
  look.job = job;
  return look;
}

constexpr auto npos = std::string_view::npos;

// Le rang d'attaque, hors tirage au sort.
std::size_t Rank(const SpriteCatalog& catalog, const ActorLook& look) {
  return AttackAnimation(catalog, look, 0);
}

constexpr std::uint32_t kNovice = 0;
constexpr std::uint32_t kSwordman = 1;
constexpr std::uint32_t kMagician = 2;
constexpr std::uint32_t kArcher = 3;
constexpr std::uint32_t kMerchant = 5;
constexpr std::uint32_t kThief = 6;
constexpr std::uint32_t kKnight = 7;
constexpr std::uint32_t kPriest = 8;
constexpr std::uint32_t kWizard = 9;
constexpr std::uint32_t kBlacksmith = 10;
constexpr std::uint32_t kHunter = 11;
constexpr std::uint32_t kAssassin = 12;
constexpr std::uint32_t kMonk = 15;
constexpr std::uint32_t kSage = 16;
constexpr std::uint32_t kAlchemist = 18;
constexpr std::uint32_t kGunslinger = 24;
constexpr std::uint32_t kNinja = 25;
constexpr std::uint32_t kTaekwon = 4046;
constexpr std::uint32_t kRebellion = 4215;
constexpr std::uint32_t kSummoner = 4217;

}  // namespace

TEST_CASE("Attaque : chaque famille de métiers a son arme du rang onze", "[render][attack]") {
  const SpriteCatalog catalog = Catalog();
  // Les épéistes brandissent la lance ; les mages et marchands, la dague ; la
  // novice aussi, mais pas le novice.
  CHECK(Rank(catalog, Armed(kSwordman, kMale, kSpear)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kKnight, kMale, kTwoHandSpear)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kSwordman, kMale, kSword)) == kAnimationAttack2);
  CHECK(Rank(catalog, Armed(kMagician, kFemale, kDagger)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kMerchant, kMale, kAxe)) == kAnimationAttack2);
  CHECK(Rank(catalog, Armed(kNovice, kFemale, kDagger)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kNovice, kMale, kDagger)) == kAnimationAttack2);
  // L'archer tire au rang dix et frappe au onze ; le chasseur, l'inverse.
  CHECK(Rank(catalog, Armed(kArcher, kMale, kBow)) == kAnimationAttack2);
  CHECK(Rank(catalog, Armed(kArcher, kMale, kDagger)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kHunter, kMale, kBow)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kHunter, kMale, kDagger)) == kAnimationAttack2);
  // Une arbalète est un arc, par sa famille.
  CHECK(Rank(catalog, Armed(kHunter, kMale, kCrossbow)) == kAnimationAttack3);
  // Le prêtre et son livre ; le sorcier, sa dague s'il est homme, son bâton si
  // elle est femme.
  CHECK(Rank(catalog, Armed(kPriest, kFemale, kBook)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kPriest, kFemale, kMace)) == kAnimationAttack2);
  CHECK(Rank(catalog, Armed(kWizard, kMale, kDagger)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kWizard, kFemale, kDagger)) == kAnimationAttack2);
  CHECK(Rank(catalog, Armed(kWizard, kFemale, kRod)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kWizard, kMale, kRod)) == kAnimationAttack2);
  // Forgerons et alchimistes : l'épée, la hache, la masse — pas l'épée à deux mains.
  CHECK(Rank(catalog, Armed(kBlacksmith, kMale, kSword)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kBlacksmith, kMale, kAxe)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kAlchemist, kMale, kMace)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kBlacksmith, kMale, kTwoHandSword)) == kAnimationAttack2);
  // L'assassin : le katar, ou deux armes.
  CHECK(Rank(catalog, Armed(kAssassin, kMale, kKatar)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kAssassin, kMale, kDagger, kDagger)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kAssassin, kMale, kDagger)) == kAnimationAttack2);
  // Le moine : les poings nus ou le poing ; le sage : le bâton et le livre.
  CHECK(Rank(catalog, Armed(kMonk, kMale, 0)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kMonk, kMale, kKnuckle)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kMonk, kMale, kMace)) == kAnimationAttack2);
  CHECK(Rank(catalog, Armed(kSage, kFemale, kRod)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kSage, kFemale, kBook)) == kAnimationAttack3);
  CHECK(Rank(catalog, Armed(kSage, kFemale, kDagger)) == kAnimationAttack2);
  // Le tireur et son fusil ; le pistolet reste au rang dix.
  CHECK(Rank(catalog, Armed(kGunslinger, kMale, kHandgun)) == kAnimationAttack2);
  CHECK(Rank(catalog, Armed(kRebellion, kMale, kHandgun)) == kAnimationAttack2);
  CHECK(Rank(catalog, Armed(kNinja, kMale, kDagger)) == kAnimationAttack2);
  // Sans table des objets, rien ne se reconnaît : le rang dix.
  SpriteCatalog blind = Catalog();
  blind.items = ro::vfs::ItemTable{};
  CHECK(Rank(blind, Armed(kSwordman, kMale, kSpear)) == kAnimationAttack2);
  // Une créature attaque toujours au rang deux.
  ActorLook creature;
  creature.job = 1002;
  CHECK(Rank(catalog, creature) == kCreatureAnimationAttack);
}

TEST_CASE("Attaque : le taekwon tire au sort, sept fois sur dix le rang onze", "[render][attack]") {
  const SpriteCatalog catalog = Catalog();
  for (std::uint8_t tenth = 0; tenth < 7; ++tenth) {
    CHECK(AttackAnimation(catalog, Armed(kTaekwon, kMale, 0), tenth) == kAnimationAttack3);
  }
  for (std::uint8_t tenth = 7; tenth < 10; ++tenth) {
    CHECK(AttackAnimation(catalog, Armed(kTaekwon, kMale, 0), tenth) == kAnimationAttack2);
  }
}

TEST_CASE("Attaque : l'image où le coup porte", "[render][attack]") {
  // La sixième, sauf le marchand et le voleur au rang dix, et le novice homme
  // au rang onze. L'assassin ne voit la troisième que pour une CLASSE de katar,
  // que le serveur n'envoie pas.
  CHECK(DamageFrame(Armed(kSwordman, kMale, kSword), kAnimationAttack2) == 6.0F);
  CHECK(DamageFrame(Armed(kMerchant, kMale, kAxe), kAnimationAttack2) == 5.85F);
  CHECK(DamageFrame(Armed(kThief, kMale, kDagger), kAnimationAttack2) == 5.75F);
  CHECK(DamageFrame(Armed(kThief, kMale, kDagger), kAnimationAttack3) == 6.0F);
  CHECK(DamageFrame(Armed(kNovice, kMale, kDagger), kAnimationAttack3) == 5.85F);
  CHECK(DamageFrame(Armed(kNovice, kFemale, kDagger), kAnimationAttack3) == 6.0F);
  CHECK(DamageFrame(Armed(kAssassin, kMale, kKatar), kAnimationAttack3) == 6.0F);
  CHECK(DamageFrame(Armed(kAssassin, kMale, 16), kAnimationAttack3) == 3.0F);
}

TEST_CASE("Attaque : la cadence suit la durée que le serveur donne, plafonnée sauf pour un arc",
          "[render][attack]") {
  const SpriteCatalog catalog = Catalog();
  CHECK(AttackMotionFactor(catalog, Armed(kSwordman, kMale, kSword), 432) == 1.0F);
  CHECK(AttackMotionFactor(catalog, Armed(kSwordman, kMale, kSword), 216) == 0.5F);
  CHECK(AttackMotionFactor(catalog, Armed(kSwordman, kMale, kSword), 1000) == 1.0F);
  CHECK(AttackMotionFactor(catalog, Armed(kHunter, kMale, kBow), 864) == 2.0F);
  // Une durée nulle vaut 1440 ms, donc le plafond.
  CHECK(AttackMotionFactor(catalog, Armed(kSwordman, kMale, kSword), 0) == 1.0F);
}

TEST_CASE("Attaque : seul un arc tire des flèches, et c'est l'arme qui le dit",
          "[render][attack]") {
  // `IsItemUsingArrow` chez le client ne regarde que la CLASSE de l'arme : un
  // arc dans les mains d'un épéiste tire, une épée dans celles d'un archer non.
  const SpriteCatalog catalog = Catalog();
  CHECK(ShootsArrows(catalog, Armed(kSwordman, kMale, kBow)));
  CHECK(ShootsArrows(catalog, Armed(kArcher, kMale, kBow)));
  CHECK_FALSE(ShootsArrows(catalog, Armed(kArcher, kMale, kSword)));
  // Et le vol d'une flèche est le retard qu'on donne déjà à la blessure.
  CHECK(ProjectileDelayMs(catalog, Armed(kArcher, kMale, kBow)) == ro::render::kArrowFlightMs);
}

TEST_CASE("Attaque : un joueur tire la flèche générique, une créature celle de son espèce",
          "[render][attack]") {
  const SpriteCatalog catalog = Catalog();
  // Le joueur : par son arme, et toujours la même flèche.
  CHECK(ArrowSpriteOf(catalog, Armed(kArcher, kMale, kBow), false) == ro::render::kArrowSpriteBase);
  CHECK(ArrowSpriteOf(catalog, Armed(kArcher, kMale, kSword), false).empty());

  // La créature : par son espèce. L'Archer Squelette tire la flèche générique,
  // le Stone Shooter sa pierre, et un Poring ne tire rien.
  CHECK(ArrowSpriteOf(catalog, Creature(1016), false) == ro::render::kArrowSpriteBase);
  CHECK(ArrowSpriteOf(catalog, Creature(1495), false).find("STONE_SHOOTER_BULLET") != npos);
  CHECK(ArrowSpriteOf(catalog, Creature(1002), false).empty());
  // L'effet 2404 vole comme la flèche : le canon tire son boulet, et l'espèce
  // 22256, seule de sa classe sans munition à elle, la flèche générique.
  CHECK(ArrowSpriteOf(catalog, Creature(20936), false).find("canon_bullet") != npos);
  CHECK(ArrowSpriteOf(catalog, Creature(22467), false).find("CH1_SCABIOSA_bullet") != npos);
  CHECK(ArrowSpriteOf(catalog, Creature(22256), false) == ro::render::kArrowSpriteBase);
  // ⚠ Une espèce dont le coup S'ABAT au lieu de voler ne tire rien ici.
  CHECK(ArrowSpriteOf(catalog, Creature(1020), false).empty());
}

TEST_CASE("Attaque : la munition en sprite défile à chaque tic, la flèche tient sa première image",
          "[render][attack]") {
  using ro::render::ProjectileImageRule;
  using ro::render::ProjectileImageRuleOf;
  // Les espèces que l'aiguillage du client envoie à la munition en sprite —
  // le canon, la Scabiosa, et 22256 qui en porte la flèche générique.
  CHECK(ProjectileImageRuleOf(Creature(20936)) == ProjectileImageRule::kSpriteBullet);
  CHECK(ProjectileImageRuleOf(Creature(22467)) == ProjectileImageRule::kSpriteBullet);
  CHECK(ProjectileImageRuleOf(Creature(22256)) == ProjectileImageRule::kSpriteBullet);
  // Celles que sa table habille tirent une flèche : l'Archer Squelette comme
  // le Stone Shooter, dont la pierre est pourtant à lui.
  CHECK(ProjectileImageRuleOf(Creature(1016)) == ProjectileImageRule::kArrow);
  CHECK(ProjectileImageRuleOf(Creature(1495)) == ProjectileImageRule::kArrow);
  // Un joueur, quel qu'il soit, et une espèce qui ne tire rien.
  CHECK(ProjectileImageRuleOf(Armed(kArcher, kMale, kBow)) == ProjectileImageRule::kArrow);
  CHECK(ProjectileImageRuleOf(Creature(1002)) == ProjectileImageRule::kArrow);
  // Huit mises à jour sur la première image.
  CHECK(ro::render::kArrowFirstImageHoldTicks == 8);
}

TEST_CASE("Attaque : un Doram tire son âme, et seulement sous l'état qui la donne",
          "[render][attack]") {
  const SpriteCatalog catalog = Catalog();
  // L'invocateur à mains nues : l'âme sous EFST_SOULATTACK, rien sans.
  CHECK(ArrowSpriteOf(catalog, Armed(kSummoner, kFemale, 0), true) ==
        ro::render::kSoulAttackSpriteBase);
  CHECK(ArrowSpriteOf(catalog, Armed(kSummoner, kFemale, 0), false).empty());
  // L'état sur un autre métier ne tire rien : c'est le métier que le client
  // teste, et les sept métiers qu'il retient seulement.
  CHECK(ArrowSpriteOf(catalog, Armed(kArcher, kMale, kSword), true).empty());
  CHECK(ro::render::IsDoramJob(4315));
  CHECK_FALSE(ro::render::IsDoramJob(kRebellion));
  // Et une créature n'a pas d'âme à tirer, quel que soit l'état.
  CHECK(ArrowSpriteOf(catalog, Creature(1002), true).empty());
}

TEST_CASE("Attaque : la Mandragore frappe aux pieds de sa cible au lieu de tirer",
          "[render][attack]") {
  const SpriteCatalog catalog = Catalog();
  const auto mandragore = ro::render::StrikeSpriteOf(Creature(1020));
  REQUIRE(mandragore.has_value());
  CHECK(mandragore->sprite_base.find("mandragora_atk") != npos);
  CHECK(mandragore->frame_period_units == 0.0F);
  // L'Hydre, seule à cadence forcée : deux tics par image.
  const auto hydre = ro::render::StrikeSpriteOf(Creature(1068));
  REQUIRE(hydre.has_value());
  CHECK(hydre->sprite_base.find("hydra_atk") != npos);
  CHECK(hydre->frame_period_units == 2.0F);
  // 3791 frappe, et ne tire pas de flèche.
  const auto evenement = ro::render::StrikeSpriteOf(Creature(3791));
  REQUIRE(evenement.has_value());
  CHECK(evenement->sprite_base.find("jp_2017evt_atk") != npos);
  CHECK(ArrowSpriteOf(catalog, Creature(3791), false).empty());
  // Un archer tire, il ne frappe pas ; un joueur non plus.
  CHECK_FALSE(ro::render::StrikeSpriteOf(Creature(1016)).has_value());
  CHECK_FALSE(ro::render::StrikeSpriteOf(Armed(kArcher, kMale, kBow)).has_value());

  // UNE ESPÈCE TIRE OU FRAPPE, JAMAIS LES DEUX : l'aiguillage du client n'a
  // qu'une sortie par mob id.
  std::size_t both = 0;
  for (std::uint32_t job = 1000; job < 23000; ++job) {
    if (!ArrowSpriteOf(catalog, Creature(job), false).empty() &&
        ro::render::StrikeSpriteOf(Creature(job)).has_value()) {
      ++both;
    }
  }
  CHECK(both == 0);
}

TEST_CASE("Attaque : le coup d'un projectile lent porte plus tard", "[render][attack]") {
  const SpriteCatalog catalog = Catalog();
  CHECK(ProjectileDelayMs(catalog, Armed(kSwordman, kMale, kSword)) == 0.0F);
  CHECK(ProjectileDelayMs(catalog, Armed(kHunter, kMale, kBow)) == 192.0F);
  CHECK(ProjectileDelayMs(catalog, Armed(kHunter, kMale, kCrossbow)) == 192.0F);
  ActorLook archer_skeleton;
  archer_skeleton.job = 1016;
  CHECK(ProjectileDelayMs(catalog, archer_skeleton) == 192.0F);
  ActorLook gargoyle;
  gargoyle.job = 1253;
  CHECK(ProjectileDelayMs(catalog, gargoyle) == 0.0F);
  ActorLook injustice;
  injustice.job = 1285;
  CHECK(ProjectileDelayMs(catalog, injustice) == 912.0F);
}

TEST_CASE("Attaque : l'âme d'un Doram recule son coup comme une flèche", "[render][attack]") {
  // 🔴 LE CLIENT RECULE L'IMAGE DE FRAPPE DE HUIT PAS pour l'âme comme pour la
  // flèche, et les deux s'ajoutent. Sans ce recul, la cible d'un Doram
  // bronchait cent quatre-vingt-douze millisecondes avant que l'âme n'arrive.
  const SpriteCatalog catalog = Catalog();
  CHECK(ArrowDelayMs(catalog, Armed(kSummoner, kFemale, 0), true) == 192.0F);
  CHECK(ArrowDelayMs(catalog, Armed(kSummoner, kFemale, 0), false) == 0.0F);
  CHECK(ProjectileDelayMs(catalog, Armed(kSummoner, kFemale, 0), true) == 192.0F);
  CHECK(ProjectileDelayMs(catalog, Armed(kSummoner, kFemale, 0)) == 0.0F);
  CHECK(ArrowDelayMs(catalog, Armed(kSummoner, kFemale, kBow), true) == 384.0F);
  // L'état sur un autre métier ne recule rien, et une créature n'a pas d'âme.
  CHECK(ArrowDelayMs(catalog, Armed(kArcher, kMale, kSword), true) == 0.0F);
  CHECK(ArrowDelayMs(catalog, Creature(1016), true) == 0.0F);
}

TEST_CASE("Attaque : un joueur à baguette tire un trait, trois fois plus lent qu'une flèche",
          "[render][attack]") {
  // ⚠ ÉCART VOULU AU CLIENT D'ORIGINE : toute arme de la
  // famille des bâtons, sous-types repliés compris, tire à l'attaque ordinaire.
  using ro::render::ProjectileFlightOf;
  using ro::render::ProjectileImageRule;
  using ro::render::ShootsWandBolts;
  const SpriteCatalog catalog = Catalog();
  for (const std::uint32_t wand : {kRod, kTwoHandRod, kArcWand, kWizardryStaff}) {
    CAPTURE(wand);
    CHECK(ShootsWandBolts(catalog, Armed(kWizard, kMale, wand)));
    CHECK_FALSE(ShootsArrows(catalog, Armed(kWizard, kMale, wand)));
    CHECK(ArrowSpriteOf(catalog, Armed(kWizard, kMale, wand), false) ==
          ro::render::kWandBoltSpriteBase);
    // La blessure attend le vol entier, trois fois celui de la flèche.
    CHECK(ArrowDelayMs(catalog, Armed(kWizard, kMale, wand), false) == 576.0F);
    CHECK(ProjectileDelayMs(catalog, Armed(kWizard, kMale, wand)) == 576.0F);
    // Ses images suivent la règle de la flèche, à l'horloge divisée par
    // trois, et son image tourne de trois quarts de tour de plus.
    const ro::render::ProjectileFlight flight =
        ProjectileFlightOf(catalog, Armed(kWizard, kMale, wand));
    CHECK(flight.image_rule == ProjectileImageRule::kArrow);
    CHECK(flight.clock_slowdown == 3);
    CHECK(flight.turn_degrees == 270);
  }
  CHECK(ro::render::kWandBoltFlightMs == 3.0F * ro::render::kArrowFlightMs);

  // L'arc et l'épée n'y changent rien.
  CHECK_FALSE(ShootsWandBolts(catalog, Armed(kArcher, kMale, kBow)));
  CHECK(ProjectileDelayMs(catalog, Armed(kArcher, kMale, kBow)) == 192.0F);
  CHECK(ProjectileDelayMs(catalog, Armed(kSwordman, kMale, kSword)) == 0.0F);
  CHECK(ArrowSpriteOf(catalog, Armed(kSwordman, kMale, kSword), false).empty());
  const ro::render::ProjectileFlight bow = ProjectileFlightOf(catalog, Armed(kArcher, kMale, kBow));
  CHECK(bow.image_rule == ProjectileImageRule::kArrow);
  CHECK(bow.clock_slowdown == 1);
  CHECK(bow.turn_degrees == 0);
  // Une créature garde la règle de son espèce, à l'horloge du client.
  const ro::render::ProjectileFlight canon = ProjectileFlightOf(catalog, Creature(20936));
  CHECK(canon.image_rule == ProjectileImageRule::kSpriteBullet);
  CHECK(canon.clock_slowdown == 1);
  CHECK(canon.turn_degrees == 0);
  // Un vol qui doit tenir un temps donné en reçoit la vie : le trait, d'une
  // horloge trois fois plus lente, trois fois moins de tics que la flèche.
  const ro::render::ProjectileFlight trait =
      ProjectileFlightOf(catalog, Armed(kWizard, kMale, kRod));
  CHECK(ro::render::FlightLifeTicksOf(960, bow) == 60);
  CHECK(ro::render::FlightLifeTicksOf(960, trait) == 20);
  CHECK(ro::render::FlightLifeTicksOf(0, trait) == 1);

  // UN DORAM À BAGUETTE tire le trait, l'arme passant avant l'âme comme pour
  // la flèche ; sous l'état qui donne l'âme, les deux retards s'ajoutent.
  CHECK(ArrowSpriteOf(catalog, Armed(kSummoner, kFemale, kRod), true) ==
        ro::render::kWandBoltSpriteBase);
  CHECK(ArrowDelayMs(catalog, Armed(kSummoner, kFemale, kRod), true) == 768.0F);
}

TEST_CASE("Attaque : le coup d'une compétence ne rattrape que deux espèces", "[render][attack]") {
  // Le rattrapage par espèce du coup ORDINAIRE va jusqu'à neuf cent douze
  // millisecondes ; celui d'une compétence n'existe que pour l'Archer Squelette
  // et sa variante, du vol d'une flèche.
  CHECK(SkillHitProjectileDelayMs(Creature(1016)) == 192.0F);
  CHECK(SkillHitProjectileDelayMs(Creature(1420)) == 192.0F);
  CHECK(SkillHitProjectileDelayMs(Creature(1285)) == 0.0F);
  CHECK(SkillHitProjectileDelayMs(Creature(1002)) == 0.0F);
  CHECK(SkillHitProjectileDelayMs(Armed(kHunter, kMale, kBow)) == 0.0F);
}

TEST_CASE("Attaque : l'homoncule tire son rang à pile ou face, sauf Eleanor", "[render][attack]") {
  // Un dixième pair donne le rang six, un impair le rang deux ; Eleanor n'a
  // que le deux, et un monstre aussi.
  const SpriteCatalog catalog = Catalog();
  CHECK(IsHomunculusJob(6001));
  CHECK(IsHomunculusJob(6016));
  CHECK(IsHomunculusJob(6048));
  CHECK(IsHomunculusJob(6052));
  // Les mercenaires humains coupent la plage en deux.
  CHECK_FALSE(IsHomunculusJob(6017));
  CHECK_FALSE(IsHomunculusJob(6046));
  CHECK_FALSE(IsHomunculusJob(1002));
  CHECK(AttackAnimation(catalog, Creature(6001), 0) == kHomunculusAnimationAttack2);
  CHECK(AttackAnimation(catalog, Creature(6001), 1) == kCreatureAnimationAttack);
  CHECK(AttackAnimation(catalog, Creature(6001), 4) == kHomunculusAnimationAttack2);
  CHECK(AttackAnimation(catalog, Creature(6052), 0) == kCreatureAnimationAttack);
  CHECK(AttackAnimation(catalog, Creature(1002), 0) == kCreatureAnimationAttack);
}

TEST_CASE("Attaque : la variante d'attaque ne tire pas au sort", "[render][attack]") {
  // L'état neuf prend le rang que l'arme décide, sans le tirage du taekwon.
  const SpriteCatalog catalog = Catalog();
  CHECK(AttackVariantAnimation(catalog, Armed(kTaekwon, kMale, 0)) == kAnimationAttack2);
  CHECK(AttackVariantAnimation(catalog, Armed(kHunter, kMale, kBow)) == kAnimationAttack3);
  CHECK(AttackVariantAnimation(catalog, Armed(kHunter, kMale, kDagger)) == kAnimationAttack2);
  CHECK(AttackVariantAnimation(catalog, Creature(6001)) == kCreatureAnimationAttack);
}

namespace {

// Un .act d'une action : `frames` images, et le son de la liste que porte
// chacune, moins un pour aucun.
ro::formats::ActAnimation ActionWithSounds(std::vector<std::string> sound_files,
                                           std::vector<std::int32_t> frame_sounds) {
  ro::test_support::ActBuildOptions options;
  ro::test_support::ActActionSpec action;
  for (const std::int32_t sound : frame_sounds) {
    ro::test_support::ActLayerSpec layer;
    layer.frame_index = 0;
    action.frames.push_back(ro::test_support::ActFrameSpec{{layer}, {}, sound});
  }
  options.actions.push_back(action);
  options.speeds = {4.0F};
  options.sound_files = std::move(sound_files);
  auto parsed = ro::formats::ActAnimation::Parse(ro::test_support::BuildAct(options));
  REQUIRE(parsed.has_value());
  return *parsed;
}

}  // namespace

TEST_CASE("Attaque : l'image de frappe d'une créature est celle du marqueur du coup",
          "[render][attack]") {
  // 🔴 LA PREMIÈRE IMAGE DONT LE SON EST « atk », et non la sixième d'une table
  // de joueur : c'est elle qui date la munition, la blessure et les impacts.
  CHECK(AttackSoundFrame(ActionWithSounds({"atk"}, {-1, -1, 0, -1, -1}), 0) == 2);
  CHECK(AttackSoundFrame(ActionWithSounds({"_STOP_", "atk"}, {1, 1, -1}), 0) == 0);
  // Sans marqueur, l'avant-dernière image ; une seule image, la seule.
  CHECK(AttackSoundFrame(ActionWithSounds({}, {-1, -1, -1, -1, -1}), 0) == 3);
  CHECK(AttackSoundFrame(ActionWithSounds({}, {-1, -1}), 0) == 0);
  CHECK(AttackSoundFrame(ActionWithSounds({}, {-1}), 0) == 0);
  // Le marqueur se compare entier : un son « atk.wav » n'en est pas un.
  CHECK(AttackSoundFrame(ActionWithSounds({"atk.wav"}, {0, -1, -1, -1}), 0) == 2);
  // Hors du fichier, rien.
  CHECK(AttackSoundFrame(ActionWithSounds({"atk"}, {0}), 3) == 0);
}

TEST_CASE("Blessure : la cadence se rapporte à 288 ms, et n'est pas plafonnée",
          "[render][attack]") {
  // 288 ms est la durée que la plupart des monstres annoncent : c'est l'unité,
  // pas une durée à tenir. Rien ne la plafonne — contrairement à l'attaque.
  CHECK(HurtMotionFactor(288, 0) == 1.0F);
  CHECK(HurtMotionFactor(144, 0) == 0.5F);
  CHECK(HurtMotionFactor(576, 0) == 2.0F);
  CHECK(HurtMotionFactor(1440, 8) == 5.0F);

  // Une durée NULLE : le coup ordinaire et le coup encaissé la gardent, et
  // gardent donc la cadence de leur fichier ; toute autre sorte prend 1440 ms.
  CHECK(HurtMotionFactor(0, 0) == 1.0F);
  CHECK(HurtMotionFactor(0, 4) == 1.0F);
  CHECK(HurtMotionFactor(0, 8) == 1440.0F / 288.0F);
  CHECK(HurtMotionFactor(0, 10) == 1440.0F / 288.0F);
}

TEST_CASE("Blessure : la cadence laissée n'est pas celle d'un facteur", "[render]") {
  using ro::render::HurtCadenceSteps;
  constexpr float kFileCadenceSteps = 4.0F;
  // UN JOUEUR voit sa cadence multipliée par le quotient quand il ne dépasse
  // pas un, et gardée au-delà.
  CHECK(HurtCadenceSteps(kFileCadenceSteps, 144, 0, true) == Catch::Approx(2.0F));
  CHECK(HurtCadenceSteps(kFileCadenceSteps, 288, 0, true) == Catch::Approx(4.0F));
  CHECK(HurtCadenceSteps(kFileCadenceSteps, 576, 0, true) == Catch::Approx(4.0F));
  // 🔴 UNE CRÉATURE la voit REMPLACÉE par le quotient, sans plancher : 288 ms
  // donnent un pas par image, quoi que déclare son fichier.
  CHECK(HurtCadenceSteps(kFileCadenceSteps, 288, 0, false) == Catch::Approx(1.0F));
  CHECK(HurtCadenceSteps(kFileCadenceSteps, 144, 0, false) == Catch::Approx(0.5F));
  CHECK(HurtCadenceSteps(kFileCadenceSteps, 576, 0, false) == Catch::Approx(4.0F));
  // Un coup ordinaire sans durée ne retouche rien ; une autre sorte prend le
  // défaut de 1440, au-delà de un.
  CHECK(HurtCadenceSteps(kFileCadenceSteps, 0, 0, false) == Catch::Approx(4.0F));
  CHECK(HurtCadenceSteps(kFileCadenceSteps, 0, 8, false) == Catch::Approx(4.0F));
}

TEST_CASE("Blessure : ce que l'écran montre, cadence et tenue", "[render]") {
  using ro::render::HurtFrameAt;
  using ro::render::HurtPlay;
  using ro::render::HurtPlayFor;
  // Un fichier à quatre pas par image (96 ms), trois images.
  constexpr float kFileCadenceSteps = 4.0F;
  constexpr std::size_t kImages = 3;

  // JOUEUR, q = 0,5 : deux pas par image, un passage, puis fini.
  const HurtPlay short_player_play = HurtPlayFor(kFileCadenceSteps, 144, 0, true);
  CHECK(short_player_play.cadence_steps == Catch::Approx(2.0F));
  CHECK(short_player_play.passes == Catch::Approx(1.0F));
  CHECK(HurtFrameAt(short_player_play, kImages, 0.0F) == std::optional<std::size_t>{0});
  CHECK(HurtFrameAt(short_player_play, kImages, 48.0F) == std::optional<std::size_t>{1});
  CHECK(HurtFrameAt(short_player_play, kImages, 143.0F) == std::optional<std::size_t>{2});
  CHECK_FALSE(HurtFrameAt(short_player_play, kImages, 144.0F).has_value());

  // 🔴 CRÉATURE, q = 1 : la cadence est REMPLACÉE par un pas, quoi que dise le
  // fichier — trois images en 72 ms.
  const HurtPlay creature = HurtPlayFor(kFileCadenceSteps, 288, 0, false);
  CHECK(creature.cadence_steps == Catch::Approx(1.0F));
  CHECK(HurtFrameAt(creature, kImages, 24.0F) == std::optional<std::size_t>{1});
  CHECK_FALSE(HurtFrameAt(creature, kImages, 72.0F).has_value());

  // q = 2, des DEUX côtés : cadence du fichier, et la dernière image TENUE
  // jusqu'à deux passages.
  for (const bool player : {true, false}) {
    const HurtPlay long_hit_play = HurtPlayFor(kFileCadenceSteps, 576, 0, player);
    CHECK(long_hit_play.cadence_steps == Catch::Approx(kFileCadenceSteps));
    CHECK(long_hit_play.passes == Catch::Approx(2.0F));
    CHECK(HurtFrameAt(long_hit_play, kImages, 200.0F) == std::optional<std::size_t>{2});
    CHECK(HurtFrameAt(long_hit_play, kImages, 400.0F) == std::optional<std::size_t>{2});
    CHECK(HurtFrameAt(long_hit_play, kImages, 575.0F) == std::optional<std::size_t>{2});
    CHECK_FALSE(HurtFrameAt(long_hit_play, kImages, 576.0F).has_value());
  }

  // q = 0, coup ordinaire : la cadence du fichier, un passage.
  const HurtPlay nul = HurtPlayFor(kFileCadenceSteps, 0, 0, false);
  CHECK(nul.cadence_steps == Catch::Approx(kFileCadenceSteps));
  CHECK(HurtFrameAt(nul, kImages, 287.0F) == std::optional<std::size_t>{2});
  CHECK_FALSE(HurtFrameAt(nul, kImages, 288.0F).has_value());

  // Avant qu'elle porte, rien.
  CHECK_FALSE(HurtFrameAt(nul, kImages, -1.0F).has_value());
  // Et la cadence d'un fichier trop vif est relevée à un pas.
  CHECK(ro::render::FileCadenceSteps(12.0F) == Catch::Approx(1.0F));
  CHECK(ro::render::FileCadenceSteps(96.0F) == Catch::Approx(4.0F));
}
