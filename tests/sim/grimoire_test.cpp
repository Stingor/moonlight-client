// Le grimoire : l'arbre du client et ce que le serveur dit, fusionnés. Les
// fiches et la disposition sont écrites ici même — aucune archive, aucun réseau.

#include "ro/sim/grimoire.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/skill_tree.h"

namespace {

using ro::sim::Grimoire;
using ro::sim::GrimoireEntry;
using ro::sim::KnownSkill;
using ro::sim::ReserveOutcome;
using ro::sim::SkillBook;
using ro::sim::SkillTree;

constexpr std::uint16_t kSwordMastery = 2;
constexpr std::uint16_t kTwoHand = 3;
constexpr std::uint16_t kBash = 5;
constexpr std::uint16_t kMagnum = 7;
// Le sommet d'une chaîne de trois : il réclame Magnum, qui réclame Bash, qui ne
// figure PAS dans sa propre liste. C'est le cas qui fait tout l'intérêt.
constexpr std::uint16_t kBowlingBash = 62;
// Dans la grille de l'Épéiste, mais le serveur ne la donne pas.
constexpr std::uint16_t kRecovery = 4;
// Donnée par le serveur, placée par aucune grille.
constexpr std::uint16_t kMovingRecovery = 144;
// Une compétence de quête : le serveur ne l'accorde pas contre des points.
constexpr std::uint16_t kFatalBlow = 145;
constexpr std::uint32_t kSwordman = 1;
constexpr std::uint32_t kKnight = 7;

// La FORME : les cases et les noms. Le rang maximal et les prérequis sont les
// règles du serveur, plus bas.
[[nodiscard]] SkillTree Shape() {
  std::vector<ro::sim::SkillNode> nodes = {
      {.id = kSwordMastery, .idname = "SM_SWORD", .display_name = "Sword Mastery"},
      {.id = kTwoHand, .idname = "SM_TWOHAND", .display_name = "Two-Handed Sword Mastery"},
      {.id = kRecovery, .idname = "SM_RECOVERY", .display_name = "Increase HP Recovery"},
      {.id = kBash, .idname = "SM_BASH", .display_name = "Bash", .level_matters = true},
      {.id = kMagnum, .idname = "SM_MAGNUM", .display_name = "Magnum Break"},
      {.id = kBowlingBash, .idname = "KN_BOWLINGBASH", .display_name = "Bowling Bash"}};
  ro::sim::SkillTreeLayout layout;
  layout.slots_by_job[kSwordman] = {{.position = 1, .skill_id = kSwordMastery},
                                    {.position = 2, .skill_id = kRecovery},
                                    {.position = 3, .skill_id = kBash},
                                    {.position = 8, .skill_id = kTwoHand},
                                    {.position = 10, .skill_id = kMagnum}};
  layout.slots_by_job[kKnight] = {{.position = 8, .skill_id = kBowlingBash}};
  layout.parent_by_job = {{kKnight, kSwordman}};
  return SkillTree::FromTables(std::move(nodes), std::move(layout));
}

// ⚠ LA CHAÎNE EST VOLONTAIREMENT INCOMPLÈTE, comme celles du serveur : Bowling
// Bash ne réclame que Magnum Break, et rien ne dit chez lui qu'il faut Bash
// avant. C'est en remontant Magnum Break qu'on l'apprend.
[[nodiscard]] std::vector<ro::sim::SkillRule> SwordmanRules() {
  return {ro::sim::SkillRule{.skill_id = kSwordMastery, .max_level = 10},
          ro::sim::SkillRule{.skill_id = kTwoHand,
                             .max_level = 10,
                             .needs = {{.skill_id = kSwordMastery, .level = 1}}},
          ro::sim::SkillRule{.skill_id = kBash, .max_level = 10},
          ro::sim::SkillRule{
              .skill_id = kMagnum, .max_level = 10, .needs = {{.skill_id = kBash, .level = 5}}},
          ro::sim::SkillRule{.skill_id = kMovingRecovery, .max_level = 1},
          ro::sim::SkillRule{.skill_id = kFatalBlow, .max_level = 1, .by_points = false}};
}

[[nodiscard]] SkillTree ReadTree(bool skill_free = false) {
  SkillTree tree = Shape();
  ro::sim::SkillRules rules{.skill_free = skill_free};
  rules.by_job[kSwordman] = SwordmanRules();
  rules.by_job[kKnight] = SwordmanRules();
  rules.by_job[kKnight].push_back(ro::sim::SkillRule{
      .skill_id = kBowlingBash, .max_level = 10, .needs = {{.skill_id = kMagnum, .level = 3}}});
  tree.UseServerRules(std::move(rules));
  return tree;
}

[[nodiscard]] KnownSkill Known(std::uint16_t id, std::uint16_t level, std::string name) {
  KnownSkill skill;
  skill.id = id;
  skill.level = level;
  skill.name = std::move(name);
  return skill;
}

}  // namespace

TEST_CASE("le grimoire mêle la grille et ce que le serveur sait", "[sim][grimoire]") {
  const SkillTree tree = ReadTree();
  SkillBook book;
  book.Learn(Known(kBash, 3, "SM_BASH"));

  Grimoire grimoire(tree);
  grimoire.Refresh(kSwordman, book, 5);

  const GrimoireEntry* bash = grimoire.Find(kBash);
  REQUIRE(bash != nullptr);
  // L'ARBRE donne la case, les RÈGLES le rang maximal, le LIVRE le rang appris.
  // Aucun ne suffit seul, et c'est tout l'objet du grimoire.
  CHECK(bash->in_grid);
  CHECK(bash->position == 3);
  CHECK(bash->learned == 3);
  REQUIRE(bash->node != nullptr);
  REQUIRE(bash->rule != nullptr);
  CHECK(bash->rule->max_level == 10);
  CHECK(bash->node->display_name == "Bash");

  // Une case jamais apprise a sa fiche et pas de rang : elle se montre quand
  // même, grisée, sinon l'arbre serait troué.
  const GrimoireEntry* magnum = grimoire.Find(kMagnum);
  REQUIRE(magnum != nullptr);
  CHECK(magnum->learned == 0);
  CHECK(magnum->in_grid);
  CHECK(grimoire.points_left() == 5);
}

TEST_CASE("une compétence hors grille entre quand même", "[sim][grimoire]") {
  // Le serveur en accorde par quête, par objet ou par groupe : les jeter
  // laisserait le joueur sans moyen de voir ce qu'il sait.
  const SkillTree tree = ReadTree();
  SkillBook book;
  book.Learn(Known(9999, 1, "SOMETHING_ELSE"));

  Grimoire grimoire(tree);
  grimoire.Refresh(kSwordman, book, 0);

  const GrimoireEntry* granted = grimoire.Find(9999);
  REQUIRE(granted != nullptr);
  CHECK(granted->learned == 1);
  CHECK_FALSE(granted->in_grid);
  // Ni le client ni l'arbre du métier ne la connaissent : ni nom, ni case, ni
  // rang maximal.
  CHECK(granted->node == nullptr);
  CHECK(granted->rule == nullptr);
}

TEST_CASE("la grille suit le serveur : ce qu'il ne donne pas se tait", "[sim][grimoire]") {
  // L'installation place Increase HP Recovery dans la grille de l'Épéiste ; le
  // serveur ne la lui donne pas, et le joueur ne l'apprendrait jamais.
  const SkillTree tree = ReadTree();
  SkillBook book;
  Grimoire grimoire(tree);
  grimoire.Refresh(kSwordman, book, 5);
  CHECK(grimoire.Find(kRecovery) == nullptr);

  // Ce qu'il donne contre des points sans case entre hors grille ; ce qu'il
  // n'accorde pas contre des points n'entre pas.
  const GrimoireEntry* moving = grimoire.Find(kMovingRecovery);
  REQUIRE(moving != nullptr);
  CHECK_FALSE(moving->in_grid);
  CHECK(grimoire.Find(kFatalBlow) == nullptr);
  CHECK(grimoire.Reserve(kFatalBlow) == ReserveOutcome::kNotByPoints);

  // Ce qui est su se montre toujours, à sa case.
  book.Learn(Known(kRecovery, 1, "SM_RECOVERY"));
  grimoire.Refresh(kSwordman, book, 5);
  const GrimoireEntry* known = grimoire.Find(kRecovery);
  REQUIRE(known != nullptr);
  CHECK(known->in_grid);
  CHECK(known->position == 2);
}

TEST_CASE("sans les règles du serveur, la grille est entière et rien ne se réserve",
          "[sim][grimoire]") {
  const SkillTree tree = Shape();
  const SkillBook book;
  Grimoire grimoire(tree);
  grimoire.Refresh(kSwordman, book, 5);
  CHECK(grimoire.Find(kRecovery) != nullptr);
  CHECK(grimoire.Find(kBash) != nullptr);
  CHECK(grimoire.Reserve(kBash) == ReserveOutcome::kUnknownSkill);
}

TEST_CASE("sous player_skillfree, on ne paie pas les prérequis", "[sim][grimoire]") {
  // Le serveur n'en vérifie aucun : payer Bash et Magnum avant Bowling Bash
  // dépenserait huit points pour rien.
  const SkillTree tree = ReadTree(/*skill_free=*/true);
  const SkillBook book;
  Grimoire grimoire(tree);
  grimoire.Refresh(kKnight, book, 20);
  CHECK(grimoire.Reserve(kBowlingBash) == ReserveOutcome::kReserved);
  CHECK(grimoire.reserved_points() == 1);
  CHECK(grimoire.Find(kBash)->reserved == 0);
}

TEST_CASE("la chaîne d'héritage apporte la grille du métier d'avant", "[sim][grimoire]") {
  const SkillTree tree = ReadTree();
  const SkillBook book;
  Grimoire grimoire(tree);
  grimoire.Refresh(kKnight, book, 0);

  // Un Chevalier voit l'arbre de l'Épéiste ET le sien.
  const GrimoireEntry* from_parent = grimoire.Find(kBash);
  REQUIRE(from_parent != nullptr);
  CHECK(from_parent->job == kSwordman);
  const GrimoireEntry* own = grimoire.Find(kBowlingBash);
  REQUIRE(own != nullptr);
  CHECK(own->job == kKnight);
}

TEST_CASE("réserver remonte TOUTE la chaîne de prérequis", "[sim][grimoire]") {
  // ⚠ C'EST LE CŒUR DU LOT. Bowling Bash ne réclame que Magnum Break 3 ; rien
  // chez lui ne dit qu'il faut Bash 5 avant. S'arrêter au rang direct — ce que
  // fait le client — laisse un trou que le joueur ne comprend pas.
  const SkillTree tree = ReadTree();
  const SkillBook book;
  Grimoire grimoire(tree);
  grimoire.Refresh(kKnight, book, 20);

  CHECK(grimoire.Reserve(kBowlingBash) == ReserveOutcome::kReserved);

  const GrimoireEntry* bash = grimoire.Find(kBash);
  REQUIRE(bash != nullptr);
  CHECK(bash->reserved == 5);
  const GrimoireEntry* magnum = grimoire.Find(kMagnum);
  REQUIRE(magnum != nullptr);
  CHECK(magnum->reserved == 3);
  const GrimoireEntry* bowling = grimoire.Find(kBowlingBash);
  REQUIRE(bowling != nullptr);
  CHECK(bowling->reserved == 1);
  // Cinq rangs de Bash, trois de Magnum, un de Bowling Bash.
  CHECK(grimoire.reserved_points() == 9);
  CHECK(grimoire.points_left() == 11);
}

TEST_CASE("les points vont à la BASE de la chaîne quand ils manquent", "[sim][grimoire]") {
  // Payer le sommet d'abord laisserait le pied verrouillé, et le joueur avec une
  // compétence qu'il ne peut pas lancer.
  const SkillTree tree = ReadTree();
  const SkillBook book;
  Grimoire grimoire(tree);
  grimoire.Refresh(kKnight, book, 4);

  const ReserveOutcome outcome = grimoire.Reserve(kBowlingBash);
  const GrimoireEntry* bash = grimoire.Find(kBash);
  REQUIRE(bash != nullptr);
  CHECK(bash->reserved == 4);
  CHECK(grimoire.Find(kBowlingBash)->reserved == 0);
  CHECK(outcome == ReserveOutcome::kNotEnoughForPrerequisites);
  CHECK(grimoire.points_left() == 0);
}

TEST_CASE("ce qui est déjà appris ne se repaie pas", "[sim][grimoire]") {
  const SkillTree tree = ReadTree();
  SkillBook book;
  book.Learn(Known(kBash, 5, "SM_BASH"));
  Grimoire grimoire(tree);
  grimoire.Refresh(kKnight, book, 20);

  CHECK(grimoire.Reserve(kBowlingBash) == ReserveOutcome::kReserved);
  // Bash est déjà au rang réclamé : trois rangs de Magnum et un de Bowling Bash.
  CHECK(grimoire.reserved_points() == 4);
  CHECK(grimoire.Find(kBash)->reserved == 0);
}

TEST_CASE("réserver jusqu'au maximum s'arrête au rang maximal", "[sim][grimoire]") {
  const SkillTree tree = ReadTree();
  const SkillBook book;
  Grimoire grimoire(tree);
  grimoire.Refresh(kSwordman, book, 99);

  CHECK(grimoire.Reserve(kBash, /*up_to_max=*/true) == ReserveOutcome::kReserved);
  CHECK(grimoire.Find(kBash)->reserved == 10);
  CHECK(grimoire.reserved_points() == 10);
  // Et une fois au maximum, il n'y a plus rien à réserver.
  CHECK(grimoire.Reserve(kBash) == ReserveOutcome::kAlreadyAtMax);
}

TEST_CASE("les refus de réservation se distinguent", "[sim][grimoire]") {
  const SkillTree tree = ReadTree();
  const SkillBook book;
  Grimoire grimoire(tree);

  grimoire.Refresh(kSwordman, book, 0);
  CHECK(grimoire.Reserve(kBash) == ReserveOutcome::kNoPointLeft);

  grimoire.Refresh(kSwordman, book, 5);
  CHECK(grimoire.Reserve(4242) == ReserveOutcome::kUnknownSkill);
}

TEST_CASE("les montées partent une par rang, prérequis d'abord", "[sim][grimoire]") {
  // ⚠ UN PAQUET PAR RANG : c'est ce que fait le client, et le serveur revalide
  // chaque montée. Un paquet pour cinq rangs n'en monterait qu'un.
  const SkillTree tree = ReadTree();
  const SkillBook book;
  Grimoire grimoire(tree);
  grimoire.Refresh(kKnight, book, 20);
  CHECK(grimoire.Reserve(kBowlingBash) == ReserveOutcome::kReserved);

  const std::vector<std::uint16_t> upgrades = grimoire.PendingUpgrades();
  REQUIRE(upgrades.size() == 9);
  // Les cinq premiers sont Bash : la base de la chaîne part avant son sommet,
  // sinon le serveur refuserait les montées du sommet.
  for (int rank = 0; rank < 5; ++rank) {
    CHECK(upgrades[static_cast<std::size_t>(rank)] == kBash);
  }
  CHECK(upgrades[5] == kMagnum);
  CHECK(upgrades[7] == kMagnum);
  CHECK(upgrades[8] == kBowlingBash);
}

TEST_CASE("une réservation périmée cesse de compter des points", "[sim][grimoire]") {
  // Le serveur peut monter la compétence lui-même — une quête, un @allskills —
  // et la réservation compterait alors des points pour rien.
  const SkillTree tree = ReadTree();
  SkillBook book;
  Grimoire grimoire(tree);
  grimoire.Refresh(kSwordman, book, 10);
  CHECK(grimoire.Reserve(kBash, /*up_to_max=*/true) == ReserveOutcome::kReserved);
  CHECK(grimoire.reserved_points() == 10);

  book.Learn(Known(kBash, 10, "SM_BASH"));
  grimoire.Refresh(kSwordman, book, 0);
  CHECK(grimoire.reserved_points() == 0);
  CHECK_FALSE(grimoire.has_reservations());
  CHECK(grimoire.Find(kBash)->learned == 10);
}

TEST_CASE("une réservation survit à un paquet du serveur", "[sim][grimoire]") {
  // Elle ne vit PAS dans les lignes, qui sont refaites à chaque paquet : les y
  // ranger les effacerait au premier changement de points de vie venu.
  const SkillTree tree = ReadTree();
  SkillBook book;
  Grimoire grimoire(tree);
  grimoire.Refresh(kSwordman, book, 10);
  CHECK(grimoire.Reserve(kSwordMastery) == ReserveOutcome::kReserved);

  book.Learn(Known(kTwoHand, 1, "SM_TWOHAND"));
  grimoire.Refresh(kSwordman, book, 10);
  CHECK(grimoire.Find(kSwordMastery)->reserved == 1);
  CHECK(grimoire.reserved_points() == 1);

  grimoire.CancelReservations();
  CHECK(grimoire.reserved_points() == 0);
  CHECK(grimoire.Find(kSwordMastery)->reserved == 0);
}
