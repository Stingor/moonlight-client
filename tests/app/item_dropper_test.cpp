#include "world/item_dropper.h"

#include <cstdint>
#include <cstdlib>
#include <set>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/world.h"

using moonclient::DropperNear;
using moonclient::GroundItemAmount;
using moonclient::kDropSearchCells;
using moonclient::kFakeLootCount;
using moonclient::kFakeLootCyclingMode;
using moonclient::kFakeLootFirstId;
using moonclient::kFakeLootItemId;
using moonclient::kItemArcSourceRangeCells;
using moonclient::SowFakeLoot;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::CellPosition;
using ro::sim::GroundItem;
using ro::sim::World;

namespace {

constexpr std::uint32_t kMonsterId = 110'001;
constexpr std::uint32_t kPlayerId = 150'001;
constexpr std::uint32_t kDroppedId = 700'001;
constexpr std::uint32_t kNeighbourId = 700'002;
constexpr std::uint32_t kNowMs = 10'000;

// La case sur laquelle tout se passe. Le chiffre n'a rien de remarquable : on
// mesure des ÉCARTS.
constexpr std::uint16_t kCellX = 100;
constexpr std::uint16_t kCellY = 100;

Actor Someone(std::uint32_t id, ActorKind kind, std::uint16_t x, std::uint16_t y) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.position = CellPosition{x, y, 0};
  return actor;
}

Actor Dropped(std::uint32_t id, std::uint16_t x, std::uint16_t y) {
  Actor actor = Someone(id, ActorKind::kItem, x, y);
  actor.ground_item = GroundItem{.since_ms = kNowMs, .falling = true};
  return actor;
}

}  // namespace

TEST_CASE("Butin : il jaillit de la créature posée sur sa case", "[app][item_drop]") {
  // Le serveur pose le PREMIER butin sur la case même de la créature
  // (`mob_process_drop_list`, `DIR_CENTER`) et ne la retire qu'à +250 ms : elle
  // est donc encore là quand le paquet arrive.
  World world;
  world.Spawn(Someone(kMonsterId, ActorKind::kMonster, kCellX, kCellY));
  const Actor item = Dropped(kDroppedId, kCellX, kCellY);
  world.Spawn(Actor{item});

  const Actor* dropper = DropperNear(world, item, kNowMs);
  REQUIRE(dropper != nullptr);
  CHECK(dropper->id == kMonsterId);
}

TEST_CASE("Butin : un objet déjà au sol n'est jamais la source", "[app][item_drop]") {
  // 🔴 SANS CE FILTRE, UNE PLUIE DE BUTIN PART EN CHAÎNE : chaque pièce
  // jaillirait de la précédente au lieu de la créature, et la gerbe se
  // déroulerait en file indienne.
  World world;
  world.Spawn(Dropped(kNeighbourId, kCellX, kCellY));
  const Actor item = Dropped(kDroppedId, kCellX, kCellY);
  world.Spawn(Actor{item});

  CHECK(DropperNear(world, item, kNowMs) == nullptr);
}

TEST_CASE("Butin : la plus proche des deux l'emporte", "[app][item_drop]") {
  World world;
  world.Spawn(Someone(kMonsterId, ActorKind::kMonster, kCellX + 2, kCellY));
  world.Spawn(Someone(kPlayerId, ActorKind::kPlayer, kCellX + 1, kCellY));
  const Actor item = Dropped(kDroppedId, kCellX, kCellY);
  world.Spawn(Actor{item});

  const Actor* dropper = DropperNear(world, item, kNowMs);
  REQUIRE(dropper != nullptr);
  // ⚠ ON NE FILTRE PAS SUR L'ESPÈCE : un joueur qui jette un objet à ses pieds
  // le fait jaillir de lui autant qu'une créature qui meurt.
  CHECK(dropper->id == kPlayerId);
}

TEST_CASE("Butin : hors du rayon, personne ne l'a lâché", "[app][item_drop]") {
  // Au-delà, c'est une entité d'un autre combat : l'objet part alors d'à côté
  // de sa case, dans une direction tirée de son identifiant.
  World world;
  const auto far = static_cast<std::uint16_t>(kCellX + kItemArcSourceRangeCells + 1);
  world.Spawn(Someone(kMonsterId, ActorKind::kMonster, far, kCellY));
  const Actor item = Dropped(kDroppedId, kCellX, kCellY);
  world.Spawn(Actor{item});

  CHECK(DropperNear(world, item, kNowMs) == nullptr);
}

namespace {

// Un monde où le joueur se tient sur la case d'essai.
World WithPlayer() {
  World world;
  world.set_player_id(kPlayerId);
  world.Spawn(Someone(kPlayerId, ActorKind::kPlayer, kCellX, kCellY));
  return world;
}

}  // namespace

TEST_CASE("Faux butin : huit leurres en cercle autour du joueur", "[app][item_drop][fake]") {
  World world = WithPlayer();
  CHECK(SowFakeLoot(world, kFakeLootCount, kFakeLootItemId, kFakeLootCyclingMode, kNowMs) ==
        kFakeLootCount);

  for (std::size_t rank = 0; rank < kFakeLootCount; ++rank) {
    const Actor* loot = world.Find(kFakeLootFirstId + static_cast<std::uint32_t>(rank));
    REQUIRE(loot != nullptr);
    CHECK(loot->kind == ActorKind::kItem);
    REQUIRE(loot->ground_item.has_value());
    CHECK(loot->ground_item->item_id == kFakeLootItemId);
    CHECK(loot->ground_item->falling);
    // À UNE case du joueur, jamais dessus : semés sur la même case ils se
    // superposeraient, et le jaillissement ne se jugerait pas.
    const auto dx = static_cast<int>(loot->position.x) - static_cast<int>(kCellX);
    const auto dy = static_cast<int>(loot->position.y) - static_cast<int>(kCellY);
    CHECK(dx * dx + dy * dy > 0);
    CHECK(std::abs(dx) <= 1);
    CHECK(std::abs(dy) <= 1);
  }
}

TEST_CASE("Faux butin : le mode tournant montre les cinq couleurs", "[app][item_drop][fake]") {
  World world = WithPlayer();
  SowFakeLoot(world, kFakeLootCount, kFakeLootItemId, kFakeLootCyclingMode, kNowMs);

  std::set<std::uint16_t> modes;
  for (std::size_t rank = 0; rank < kFakeLootCount; ++rank) {
    const Actor* loot = world.Find(kFakeLootFirstId + static_cast<std::uint32_t>(rank));
    REQUIRE(loot != nullptr);
    REQUIRE(loot->ground_item.has_value());
    // 🔴 JAMAIS ZÉRO : le mode nul ne plante aucun pilier, et le premier butin
    // d'une salve n'aurait alors rien à montrer.
    CHECK(loot->ground_item->drop_effect_mode >= 1);
    CHECK(loot->ground_item->drop_effect_mode <= 5);
    CHECK(loot->ground_item->show_drop_effect);
    modes.insert(loot->ground_item->drop_effect_mode);
  }
  CHECK(modes.size() == 5);
}

TEST_CASE("Faux butin : un mode imposé vaut pour tous, et zéro n'en plante aucun",
          "[app][item_drop][fake]") {
  World world = WithPlayer();
  constexpr std::uint16_t kPurple = 3;
  SowFakeLoot(world, 3, kFakeLootItemId, kPurple, kNowMs);
  for (std::size_t rank = 0; rank < 3; ++rank) {
    const Actor* loot = world.Find(kFakeLootFirstId + static_cast<std::uint32_t>(rank));
    REQUIRE(loot != nullptr);
    CHECK(loot->ground_item->drop_effect_mode == kPurple);
  }

  constexpr std::uint16_t kNoPillar = 0;
  SowFakeLoot(world, 3, kFakeLootItemId, kNoPillar, kNowMs);
  const Actor* first = world.Find(kFakeLootFirstId);
  REQUIRE(first != nullptr);
  CHECK_FALSE(first->ground_item->show_drop_effect);
}

TEST_CASE("Faux butin : chaque salve efface la précédente", "[app][item_drop][fake]") {
  // 🔴 SANS CELA, UNE SALVE PLUS COURTE EN LAISSERAIT TRAÎNER : les leurres du
  // tour d'avant resteraient sur la carte, et rien ne les retirerait plus.
  World world = WithPlayer();
  SowFakeLoot(world, kFakeLootCount, kFakeLootItemId, kFakeLootCyclingMode, kNowMs);
  CHECK(SowFakeLoot(world, 2, kFakeLootItemId, kFakeLootCyclingMode, kNowMs) == 2);
  CHECK(world.Find(kFakeLootFirstId + 2) == nullptr);

  // Un compte NUL nettoie la carte.
  CHECK(SowFakeLoot(world, 0, kFakeLootItemId, kFakeLootCyclingMode, kNowMs) == 0);
  CHECK(world.Find(kFakeLootFirstId) == nullptr);
  // Et le joueur, lui, reste.
  CHECK(world.Find(kPlayerId) != nullptr);
}

TEST_CASE("Faux butin : sans joueur, rien ne tombe", "[app][item_drop][fake]") {
  // Avant l'entrée en jeu, il n'y a personne autour de qui semer.
  World world;
  CHECK(SowFakeLoot(world, kFakeLootCount, kFakeLootItemId, kFakeLootCyclingMode, kNowMs) == 0);
}

TEST_CASE("Butin : un objet JETÉ à deux cases en diagonale trouve encore son lâcheur",
          "[app][item_drop]") {
  // 🔴 LE CAS QUE DEUX CASES ET DEMIE PERDAIT, et il n'est pas rare : c'est
  // celui d'un objet qu'on jette de son sac. `pc_dropitem` passe `DIR_MAX` et
  // le drapeau 2, ce qui envoie `map_search_freecell_dist` chercher une case
  // libre à DEUX — en diagonale, cela fait 2,83 cases, et le sous-positionnement
  // en ajoute encore.
  //
  // La trace en jeu l'a montré : un butin sur trois repartait « de nulle part »,
  // donc d'à côté de sa propre case, et sortait hors du sprite.
  World world;
  world.Spawn(Someone(kPlayerId, ActorKind::kPlayer, kCellX, kCellY));
  const auto corner = static_cast<std::uint16_t>(kCellX + kDropSearchCells);
  const auto far_corner = static_cast<std::uint16_t>(kCellY + kDropSearchCells);
  const Actor item = Dropped(kDroppedId, corner, far_corner);
  world.Spawn(Actor{item});

  const Actor* dropper = DropperNear(world, item, kNowMs);
  REQUIRE(dropper != nullptr);
  CHECK(dropper->id == kPlayerId);
}

TEST_CASE("Butin : le rayon couvre la diagonale que le serveur peut poser", "[app][item_drop]") {
  // ⚠ UN SEUIL SE DÉRIVE D'UNE BORNE DU PROTOCOLE. Celui d'avant — deux cases et
  // demie — séparait bien au cas ordinaire et se retournait au cas limite, ce
  // qui est la pire des deux façons d'être faux.
  constexpr float kDiagonal = 1.41421356F;
  CHECK(kItemArcSourceRangeCells > kDropSearchCells * kDiagonal);
  // Et il ne va pas chercher une entité d'un autre combat : quatre cases sont
  // au-delà de tout ce que le serveur pose.
  CHECK(kItemArcSourceRangeCells < 4.0F);
}

TEST_CASE("Butin : la pile dit combien elle porte, au-delà d'un", "[app][item_drop][amount]") {
  Actor stack = Dropped(kDroppedId, kCellX, kCellY);
  stack.ground_item->amount = 5;
  CHECK(GroundItemAmount(stack) == "5");
  stack.ground_item->amount = 30000;
  CHECK(GroundItemAmount(stack) == "30000");
}

TEST_CASE("Butin : une pile d'UN SEUL ne dit rien", "[app][item_drop][amount]") {
  // ⚠ « Red Potion (1) » n'apprend rien et alourdirait chaque butin d'une carte
  // encombrée. Un serveur qui se tairait — zéro sur le fil — ne dit rien non
  // plus, plutôt que d'écrire un compte que personne n'a annoncé.
  Actor lone = Dropped(kDroppedId, kCellX, kCellY);
  lone.ground_item->amount = 1;
  CHECK(GroundItemAmount(lone).empty());
  lone.ground_item->amount = 0;
  CHECK(GroundItemAmount(lone).empty());
}

TEST_CASE("Butin : rien d'autre qu'un objet au sol n'a de quantité", "[app][item_drop][amount]") {
  // 🔴 LA FONCTION EST APPELÉE POUR TOUT CE QUE LA SOURIS DÉSIGNE, joueurs et
  // créatures compris : sans ce filtre, un monstre porterait entre parenthèses
  // ce qu'un butin d'à côté a laissé dans le champ.
  Actor monster = Someone(kMonsterId, ActorKind::kMonster, kCellX, kCellY);
  CHECK(GroundItemAmount(monster).empty());
  Actor player = Someone(kPlayerId, ActorKind::kPlayer, kCellX, kCellY);
  CHECK(GroundItemAmount(player).empty());
}
