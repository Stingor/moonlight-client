// Ce que le HUD de cible sait de l'entité désignée : le serveur d'abord, puis la
// jauge du client, puis la plaque que le serveur écrit sous le nom d'un monstre.

#include "world/target_facts.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"
#include "ro/vfs/item_words.h"

#include "support/monster_words_fixture.h"

using moonclient::FactsOfTarget;
using moonclient::MobPlate;
using moonclient::ParseMobPlate;
using moonclient::TargetFacts;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::TargetDetail;
using ro::sim::World;
using ro::vfs::ItemWords;

namespace {

constexpr std::uint32_t kWolf = 2000123;
constexpr std::uint32_t kPriest = 150002;

[[nodiscard]] ItemWords English() { return ro::test_support::EnglishMonsterWords(); }

[[nodiscard]] World With(const Actor& actor) {
  World world;
  world.Spawn(actor);
  REQUIRE(world.Select(actor.id));
  return world;
}

[[nodiscard]] Actor Wolf() {
  Actor wolf;
  wolf.id = kWolf;
  wolf.kind = ActorKind::kMonster;
  wolf.name = "Wolf";
  ro::sim::NameCard card;
  card.aside = "Lv. 25 | HP: 63%";
  card.banner = "Brute";
  card.badge = "Earth";
  wolf.card = card;
  return wolf;
}

[[nodiscard]] TargetDetail Told(std::uint32_t id, ro::sim::TargetKind kind) {
  TargetDetail detail;
  detail.id = id;
  detail.told = true;
  detail.reachable = true;
  detail.kind = kind;
  return detail;
}

}  // namespace

TEST_CASE("Plaque de monstre : chaque morceau se cherche seul", "[app][cible]") {
  const MobPlate both = ParseMobPlate("10Def 10Mdef (Lv. 10 | HP: 63%)");
  CHECK(both.level == 10);
  CHECK(both.health_percent == 63);
  CHECK_FALSE(both.health.has_value());

  const MobPlate absolute = ParseMobPlate("HP: 1234/2000");
  CHECK_FALSE(absolute.level.has_value());
  CHECK(absolute.health == 1234U);
  CHECK(absolute.max_health == 2000U);
  CHECK_FALSE(absolute.health_percent.has_value());

  const MobPlate nothing = ParseMobPlate("Lv. x | HP: ?");
  CHECK_FALSE(nothing.level.has_value());
  CHECK_FALSE(nothing.health.has_value());
  CHECK_FALSE(nothing.health_percent.has_value());
}

TEST_CASE("Faits de cible : sans réponse du serveur, la plaque tient lieu", "[app][cible]") {
  const World world = With(Wolf());
  const TargetFacts facts = FactsOfTarget(world, English());
  REQUIRE(facts.present);
  CHECK(facts.is_monster);
  CHECK(facts.level == 25);
  CHECK(facts.health == 63U);
  CHECK(facts.max_health == 100U);
  // La race dans la guilde, l'élément dans le rang.
  CHECK(facts.species == "Brute · Earth");
  CHECK_FALSE(facts.magic.has_value());
}

TEST_CASE("Faits de cible : la jauge du client passe avant la plaque", "[app][cible]") {
  Actor wolf = Wolf();
  wolf.health = 300;
  wolf.max_health = 1200;
  const TargetFacts facts = FactsOfTarget(With(wolf), English());
  CHECK(facts.health == 300U);
  CHECK(facts.max_health == 1200U);
}

TEST_CASE("Faits de cible : dès qu'il répond, le serveur classe et chiffre", "[app][cible]") {
  World world = With(Wolf());
  TargetDetail told = Told(kWolf, ro::sim::TargetKind::kMonster);
  told.level = std::int16_t{31};
  told.health = 500;
  told.max_health = 900;
  told.magic = 1;
  told.max_magic = 1;
  told.nature = TargetDetail::Nature{
      .race = 2, .element = 2, .element_level = 1, .size = 1, .monster_class = 1};
  REQUIRE(world.TellTarget(told));

  const TargetFacts facts = FactsOfTarget(world, English());
  CHECK(facts.level == 31);
  CHECK(facts.health == 500U);
  CHECK(facts.max_health == 900U);
  CHECK(facts.species == "Brute · Earth 1 · Medium · MVP");
}

TEST_CASE("Faits de cible : un pourcentage du serveur remplit la jauge sans chiffrer",
          "[app][cible]") {
  Actor wolf = Wolf();
  wolf.card.reset();
  World world = With(wolf);
  TargetDetail told = Told(kWolf, ro::sim::TargetKind::kMonster);
  told.health = 37;
  told.max_health = 100;
  told.health_in_percent = true;
  REQUIRE(world.TellTarget(told));

  const TargetFacts facts = FactsOfTarget(world, English());
  CHECK(facts.health == 37U);
  CHECK(facts.max_health == 100U);
  CHECK(facts.health_in_percent);
}

TEST_CASE("Faits de cible : un pourcentage ne passe pas devant une vie exacte", "[app][cible]") {
  Actor wolf = Wolf();
  wolf.health = 300;
  wolf.max_health = 1200;
  World world = With(wolf);
  TargetDetail told = Told(kWolf, ro::sim::TargetKind::kMonster);
  told.health = 25;
  told.max_health = 100;
  told.health_in_percent = true;
  REQUIRE(world.TellTarget(told));

  const TargetFacts facts = FactsOfTarget(world, English());
  CHECK(facts.health == 300U);
  CHECK(facts.max_health == 1200U);
  CHECK_FALSE(facts.health_in_percent);
}

TEST_CASE("Faits de cible : le pourcentage de la plaque se dit tel", "[app][cible]") {
  const TargetFacts facts = FactsOfTarget(With(Wolf()), English());
  CHECK(facts.health_in_percent);
}

namespace {

[[nodiscard]] World WithTold(const TargetDetail::Nature& nature) {
  World world = With(Wolf());
  TargetDetail told = Told(kWolf, ro::sim::TargetKind::kMonster);
  told.nature = nature;
  REQUIRE(world.TellTarget(told));
  return world;
}

}  // namespace

TEST_CASE("Faits de cible : la race, l'élément et la taille sont les mots de la table",
          "[app][cible]") {
  // 🔴 AUCUN MOT ANGLAIS EN DUR : la même cible, sous une table française, se lit
  // en français ; sous une table sans ces clés, la plaque n'écrit pas de mot.
  const World world = WithTold(TargetDetail::Nature{
      .race = 7, .element = 6, .element_level = 2, .size = 2, .monster_class = 0});

  CHECK(FactsOfTarget(world, ro::test_support::FrenchMonsterWords()).species ==
        "Demi-Humain · Saint 2 · Grand");
  CHECK(FactsOfTarget(world, English()).species == "Demi-Human · Holy 2 · Large");
  const ItemWords none;
  CHECK(FactsOfTarget(world, none).species.empty());
}

TEST_CASE("Faits de cible : une race joueur est un texte à nous, un indice inconnu se tait",
          "[app][cible]") {
  // Le client d'origine n'écrit rien pour un joueur ni pour un Doram : ces deux
  // noms ne dépendent pas de la table.
  const ItemWords none;
  const World player = WithTold(TargetDetail::Nature{.race = 10, .element = 0, .size = 1});
  CHECK(FactsOfTarget(player, none).species == "Player");
  const World doram = WithTold(TargetDetail::Nature{.race = 11, .element = 0, .size = 1});
  CHECK(FactsOfTarget(doram, none).species == "Doram Player");
  const World unknown = WithTold(TargetDetail::Nature{.race = 40, .element = 40, .size = 40});
  CHECK(FactsOfTarget(unknown, English()).species.empty());
}

TEST_CASE("Faits de cible : un joueur montre sa guilde, et son allégeance se lit au SP",
          "[app][cible]") {
  Actor priest;
  priest.id = kPriest;
  priest.kind = ActorKind::kPlayer;
  priest.name = "Priest";
  ro::sim::NameCard card;
  card.aside = "Groupe";
  card.banner = "Guilde";
  card.badge = "Rang";
  priest.card = card;
  World world = With(priest);

  TargetFacts facts = FactsOfTarget(world, English());
  CHECK(facts.is_player);
  CHECK(facts.guild == "Guilde");
  CHECK_FALSE(facts.ally);
  // Ni plaque ni race pour un joueur.
  CHECK(facts.species.empty());
  CHECK_FALSE(facts.level.has_value());

  TargetDetail told = Told(kPriest, ro::sim::TargetKind::kPlayer);
  told.health = 100;
  told.max_health = 100;
  REQUIRE(world.TellTarget(told));
  CHECK_FALSE(FactsOfTarget(world, English()).ally);
  told.magic = 20;
  told.max_magic = 40;
  REQUIRE(world.TellTarget(told));
  CHECK(FactsOfTarget(world, English()).ally);
}

TEST_CASE("Faits de cible : un PNJ ne l'est que sur la parole du serveur", "[app][cible]") {
  Actor npc;
  npc.id = 110001;
  npc.kind = ActorKind::kNonPlayer;
  npc.name = "Kafra";
  World world = With(npc);
  CHECK_FALSE(FactsOfTarget(world, English()).is_npc);
  REQUIRE(world.TellTarget(Told(npc.id, ro::sim::TargetKind::kNpc)));
  CHECK(FactsOfTarget(world, English()).is_npc);
}
