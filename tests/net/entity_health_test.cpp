// LA VIE QUE PORTE UN PAQUET D'APPARITION, ET CE QU'UNE RÉANNONCE N'EFFACE PAS.
//
// 🔴 TROIS SOURCES POUR UNE SEULE JAUGE, et deux d'entre elles se marchaient
// dessus. `ZC_HP_INFO 0x0977` dit la vie d'un monstre qu'on a frappé,
// `ZC_NOTIFY_HP_TO_GROUPM 0x080E` celle d'un compagnon de groupe — et le PAQUET
// D'APPARITION la porte lui aussi depuis PACKETVER 20120221, ce que nous
// jetions. Un monstre déjà entamé qui entrait dans le champ de vision restait
// donc sans jauge, et pas seulement une image : jusqu'au coup suivant, et pas du
// tout si c'était un autre joueur qui le frappait.
//
// 🔴 ET LE SERVEUR ÉCRIT MOINS UN QUAND IL SE TAIT (`clif_set_unit_idle` :
// `p.maxHP = -1; p.HP = -1;`). Une réannonce muette — celle que le serveur
// envoie à CHAQUE mise en marche — ne doit donc pas effacer ce que les deux
// autres paquets ont dit, sans quoi la jauge d'un monstre clignoterait à chaque
// pas.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::CellPosition;
using ro::sim::kUnknownHealth;
using ro::sim::World;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;

namespace {

// Ce que le serveur écrit quand il ne dit RIEN de la vie. ⚠ PAS ZÉRO : voir
// `clif_set_unit_idle`.
constexpr std::int32_t kServerSaysNothing = -1;

// Un monstre au repos, entamé ou muet selon ce qu'on lui passe.
[[nodiscard]] ro::proto::PacketIdleUnit Monster(std::uint32_t id, std::int32_t health,
                                                std::int32_t max_health) {
  ro::proto::PacketIdleUnit unit = IdleUnit(id, CellPosition{10, 10, 0});
  unit.objecttype = 5;  // un monstre
  unit.HP = health;
  unit.maxHP = max_health;
  return unit;
}

}  // namespace

TEST_CASE("Apparition : un monstre entamé arrive avec sa vie", "[net][vie]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Monster(200'001, 640, 1600))), world, 0) ==
          FrameOutcome::kApplied);

  const ro::sim::Actor* actor = world.Find(200'001);
  REQUIRE(actor != nullptr);
  CHECK(actor->health == 640);
  CHECK(actor->max_health == 1600);
}

TEST_CASE("Apparition : un serveur muet laisse la vie inconnue", "[net][vie]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Monster(200'002, kServerSaysNothing, kServerSaysNothing))),
                     world, 0) == FrameOutcome::kApplied);

  const ro::sim::Actor* actor = world.Find(200'002);
  REQUIRE(actor != nullptr);
  // ⚠ INCONNU, ET NON ZÉRO : une jauge vide annoncerait une mort.
  CHECK(actor->health == kUnknownHealth);
  CHECK(actor->max_health == kUnknownHealth);
}

TEST_CASE("Réannonce : une annonce muette n'efface pas une vie connue", "[net][vie]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Monster(200'003, 300, 1000))), world, 0) ==
          FrameOutcome::kApplied);

  // Le serveur réannonce l'unité à chaque mise en marche, et son annonce peut
  // être muette : la jauge ne doit pas s'éteindre sous les yeux du joueur.
  REQUIRE(ApplyFrame(FrameOf(Encode(Monster(200'003, kServerSaysNothing, kServerSaysNothing))),
                     world, 0) == FrameOutcome::kApplied);

  const ro::sim::Actor* actor = world.Find(200'003);
  REQUIRE(actor != nullptr);
  CHECK(actor->health == 300);
  CHECK(actor->max_health == 1000);
}

TEST_CASE("Réannonce : une vie fraîche remplace l'ancienne", "[net][vie]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(Monster(200'004, 900, 1000))), world, 0) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyFrame(FrameOf(Encode(Monster(200'004, 120, 1000))), world, 0) ==
          FrameOutcome::kApplied);

  const ro::sim::Actor* actor = world.Find(200'004);
  REQUIRE(actor != nullptr);
  // ⚠ GARDER L'ANCIENNE FIGERAIT LA JAUGE d'un monstre qui marche, et la
  // reculerait d'un coup à chaque pas.
  CHECK(actor->health == 120);
}

TEST_CASE("Apparition : le rang de boss arrive avec l'unité", "[net][vie]") {
  World world;
  ro::proto::PacketIdleUnit mini = Monster(200'005, 5'000, 9'000);
  mini.isBoss = 1;
  REQUIRE(ApplyFrame(FrameOf(Encode(mini)), world, 0) == FrameOutcome::kApplied);

  const ro::sim::Actor* boss = world.Find(200'005);
  REQUIRE(boss != nullptr);
  // ⚠ IL VAUT POUR TOUTE UNITÉ EN VUE, et non pour la seule cible : c'est ce
  // qui permet de régler les barres des MVP à part.
  CHECK(boss->boss == ro::sim::BossRank::kMiniBoss);

  REQUIRE(ApplyFrame(FrameOf(Encode(Monster(200'006, 10, 20))), world, 0) ==
          FrameOutcome::kApplied);
  const ro::sim::Actor* ordinary = world.Find(200'006);
  REQUIRE(ordinary != nullptr);
  CHECK(ordinary->boss == ro::sim::BossRank::kNone);
}

// 🔴 DEUX RANGS, ET NON UN DRAPEAU. `clif_set_unit_idle` écrit `get_bosstype()`
// — `e_mob_bosstype` — et c'est ce qui décide de l'insigne : tête de mort
// ARGENT sur un mini-boss, DORÉE sur un MVP. Le lire comme un booléen mettait
// le même insigne sur les deux, et rien ne l'aurait dit.
TEST_CASE("Apparition : mini-boss et MVP ne se confondent pas", "[net][vie]") {
  World world;
  ro::proto::PacketIdleUnit mvp = Monster(200'007, 50'000, 50'000);
  mvp.isBoss = 2;
  REQUIRE(ApplyFrame(FrameOf(Encode(mvp)), world, 0) == FrameOutcome::kApplied);
  const ro::sim::Actor* seen = world.Find(200'007);
  REQUIRE(seen != nullptr);
  CHECK(seen->boss == ro::sim::BossRank::kMvp);

  // ⚠ UNE VALEUR QUE LE SERVEUR N'EMPLOIE PAS NE DOIT PAS DÉCIDER D'UN
  // INSIGNE : elle se lit « aucun rang », et non « le dernier de la liste ».
  ro::proto::PacketIdleUnit strange = Monster(200'008, 10, 20);
  strange.isBoss = 7;
  REQUIRE(ApplyFrame(FrameOf(Encode(strange)), world, 0) == FrameOutcome::kApplied);
  const ro::sim::Actor* unknown = world.Find(200'008);
  REQUIRE(unknown != nullptr);
  CHECK(unknown->boss == ro::sim::BossRank::kNone);
}

// 🔴 LE NIVEAU D'UN MONSTRE EST DANS LE PAQUET D'APPARITION, et nous le
// jetions : `clif_setlevel` y écrit `status_get_lv` de qui apparaît. C'est la
// seule source de la couleur d'écart de niveau — celle qui peint en rouge ce
// qui nous dépasse de dix niveaux, et que le joueur prend pour la marque d'un
// boss.
TEST_CASE("Apparition : le niveau arrive avec l'unité", "[net][vie]") {
  World world;
  ro::proto::PacketIdleUnit beast = Monster(200'009, 800, 1'000);
  beast.clevel = 50;
  REQUIRE(ApplyFrame(FrameOf(Encode(beast)), world, 0) == FrameOutcome::kApplied);
  const ro::sim::Actor* seen = world.Find(200'009);
  REQUIRE(seen != nullptr);
  CHECK(seen->level == 50);
}
