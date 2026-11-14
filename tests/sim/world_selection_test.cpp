// La désignation : ce que le joueur a cliqué, et ce qui l'éteint.
//
// 🔴 CE BANC EXISTE POUR UNE RÈGLE QU'UNE IMPLANTATION PLUS COURTE TRAHIRAIT.
// Déduire la désignation de la présence de l'acteur tiendrait tous les essais
// évidents — et rallumerait la cible dès que le même identifiant reparaît. Sur
// une entité qui se cache, cela donnerait un détecteur. C'est le dernier essai
// de ce fichier qui tient cette différence, et lui seul.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::World;

namespace {

// Deux identifiants qui ne se confondent pas à la lecture.
constexpr std::uint32_t kWolf = 2000123;
constexpr std::uint32_t kPriest = 2000456;

[[nodiscard]] Actor Standing(std::uint32_t id) {
  Actor actor;
  actor.id = id;
  actor.position.x = 100;
  actor.position.y = 100;
  return actor;
}

[[nodiscard]] World Peopled() {
  World world;
  world.Spawn(Standing(kWolf));
  world.Spawn(Standing(kPriest));
  return world;
}

}  // namespace

TEST_CASE("Désignation : redésigner la même entité garde son dossier", "[sim][cible]") {
  World world = Peopled();
  REQUIRE(world.Select(kWolf));
  ro::sim::TargetDetail told;
  told.id = kWolf;
  told.told = true;
  told.reachable = true;
  told.health = 42;
  REQUIRE(world.TellTarget(told));
  REQUIRE(world.RefuseTargetStatuses(kWolf));

  REQUIRE(world.Select(kWolf));
  CHECK(world.target_detail().health == 42U);
  CHECK(world.target_detail().statuses_refusals == 1U);
  // Une autre entité repart de rien.
  REQUIRE(world.Select(kPriest));
  CHECK_FALSE(world.target_detail().health.has_value());
  CHECK(world.target_detail().statuses_refusals == 0U);
  // Un refus qui nomme une autre entité ne se compte pas.
  CHECK_FALSE(world.RefuseTargetStatuses(kWolf));
}

TEST_CASE("Désignation : on ne désigne que ce qui existe", "[sim][cible]") {
  World world = Peopled();
  CHECK(world.selected_id() == 0);
  CHECK(world.selected() == nullptr);

  REQUIRE(world.Select(kWolf));
  CHECK(world.selected_id() == kWolf);
  REQUIRE(world.selected() != nullptr);
  CHECK(world.selected()->id == kWolf);

  // Un identifiant que personne ne porte est refusé, et — c'est le point — la
  // désignation précédente TIENT. Cliquer à côté ne défait pas ce qu'on a
  // désigné.
  CHECK_FALSE(world.Select(999999));
  CHECK(world.selected_id() == kWolf);

  // Zéro n'est pas un identifiant : c'est l'absence, et elle ne se désigne pas.
  CHECK_FALSE(world.Select(0));
  CHECK(world.selected_id() == kWolf);
}

TEST_CASE("Désignation : seul Deselect l'efface à la main", "[sim][cible]") {
  World world = Peopled();
  REQUIRE(world.Select(kWolf));

  // Désigner quelqu'un d'autre remplace, sans passer par l'absence.
  REQUIRE(world.Select(kPriest));
  CHECK(world.selected_id() == kPriest);

  world.Deselect();
  CHECK(world.selected_id() == 0);
  CHECK(world.selected() == nullptr);
}

TEST_CASE("Désignation : ce qui s'en va cesse d'être désigné", "[sim][cible]") {
  World world = Peopled();
  REQUIRE(world.Select(kWolf));

  // Le départ d'un AUTRE ne touche à rien.
  REQUIRE(world.Remove(kPriest));
  CHECK(world.selected_id() == kWolf);

  // Celui de la cible l'éteint.
  REQUIRE(world.Remove(kWolf));
  CHECK(world.selected_id() == 0);
  CHECK(world.selected() == nullptr);
}

TEST_CASE("Désignation : les deux purges l'emportent", "[sim][cible]") {
  SECTION("le vidage du monde") {
    World world = Peopled();
    REQUIRE(world.Select(kWolf));
    world.Clear();
    CHECK(world.selected_id() == 0);
  }

  SECTION("le changement de carte, même sur place") {
    World world = Peopled();
    world.set_player_id(kPriest);
    REQUIRE(world.Select(kWolf));
    world.EnterMap("prontera", 150, 150);
    CHECK(world.selected_id() == 0);
    // ⚠ Le JOUEUR, lui, traverse : c'est la désignation seule qui tombe.
    CHECK(world.player_id() == kPriest);
  }
}

TEST_CASE("Désignation : le retour de la même entité ne la rallume pas", "[sim][cible]") {
  // 🔴 L'ESSAI QUI SÉPARE UN CHAMP D'UNE RECHERCHE. Une cible sortie de la vue
  // est PERDUE : le serveur la fait disparaître, puis la réannonce quand elle
  // revient — avec le MÊME identifiant. Une désignation déduite de la présence
  // de l'acteur repousserait derrière elle toute seule.
  //
  // 🔴 ET SUR UNE ENTITÉ QUI SE CACHE, CE SERAIT UN DÉTECTEUR : le Hiding passe
  // par le même retrait, et une cible qui se rallume dirait où elle est.
  World world = Peopled();
  REQUIRE(world.Select(kWolf));

  REQUIRE(world.Remove(kWolf));
  REQUIRE(world.selected_id() == 0);

  world.Spawn(Standing(kWolf));
  CHECK(world.selected_id() == 0);
  CHECK(world.selected() == nullptr);

  // Elle se redésigne, évidemment — mais à la main.
  REQUIRE(world.Select(kWolf));
  CHECK(world.selected_id() == kWolf);
}
