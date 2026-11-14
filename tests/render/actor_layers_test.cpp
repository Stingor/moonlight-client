#include "ro/render/actor_layers.h"

#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::render::ActorPart;
using ro::render::GarmentPlacement;
using ro::render::IsBackView;
using ro::render::PartDrawOrder;

namespace {

// Un acteur complet : les neuf parties du client.
std::vector<ActorPart> AllParts() {
  return {ActorPart::kBody,           ActorPart::kHead,        ActorPart::kHeadgearLow,
          ActorPart::kHeadgearMiddle, ActorPart::kHeadgearTop, ActorPart::kWeapon,
          ActorPart::kWeaponTrail,    ActorPart::kShield,      ActorPart::kGarment};
}

// Le rang de dessin d'une partie, ou la taille de la liste si elle est absente.
std::size_t RankOf(const std::vector<ActorPart>& order, ActorPart part) {
  for (std::size_t index = 0; index < order.size(); ++index) {
    if (order[index] == part) {
      return index;
    }
  }
  return order.size();
}

// L'ordre sans la cape.
std::vector<ActorPart> WithoutGarment(const std::vector<ActorPart>& order) {
  std::vector<ActorPart> rest;
  for (const ActorPart part : order) {
    if (part != ActorPart::kGarment) {
      rest.push_back(part);
    }
  }
  return rest;
}

// La cape passe devant : le cas ordinaire.
constexpr bool kGarmentOnTop = false;
constexpr bool kGarmentBehind = true;

}  // namespace

TEST_CASE("Direction : seules les directions 2 à 5 montrent le dos", "[render][layers]") {
  CHECK_FALSE(IsBackView(0));
  CHECK_FALSE(IsBackView(1));
  CHECK(IsBackView(2));
  CHECK(IsBackView(5));
  CHECK_FALSE(IsBackView(6));
  CHECK_FALSE(IsBackView(7));
  // Une direction hors bornes revient dans le groupe.
  CHECK(IsBackView(10));
}

TEST_CASE("Ordre : la tête se dessine PAR-DESSUS le corps", "[render][layers]") {
  // De face comme de dos : le corps porte la base du cou, et l'émettre après
  // la tête lui recouvrirait le bas du visage.
  CHECK(RankOf(PartDrawOrder(AllParts(), 0, kGarmentOnTop), ActorPart::kHead) >
        RankOf(PartDrawOrder(AllParts(), 0, kGarmentOnTop), ActorPart::kBody));
  CHECK(RankOf(PartDrawOrder(AllParts(), 3, kGarmentOnTop), ActorPart::kHead) >
        RankOf(PartDrawOrder(AllParts(), 3, kGarmentOnTop), ActorPart::kBody));
}

TEST_CASE("Ordre : le bouclier est la seule pièce qui change de camp", "[render][layers]") {
  // De face il passe devant le corps ; de dos il passe DERRIÈRE, le
  // personnage le portant de l'autre côté. Aucune autre partie ne bouge.
  const std::vector<ActorPart> front = PartDrawOrder(AllParts(), 0, kGarmentOnTop);
  const std::vector<ActorPart> back = PartDrawOrder(AllParts(), 3, kGarmentOnTop);

  CHECK(RankOf(front, ActorPart::kShield) > RankOf(front, ActorPart::kBody));
  CHECK(RankOf(back, ActorPart::kShield) < RankOf(back, ActorPart::kBody));
}

TEST_CASE("Ordre : de dos, coiffes et cape passent au-dessus de l'arme", "[render][layers]") {
  // C'est l'effet du retri : elles reçoivent une clé bien supérieure à celle
  // des autres parties.
  const std::vector<ActorPart> order = PartDrawOrder(AllParts(), 3, kGarmentOnTop);
  CHECK(RankOf(order, ActorPart::kHeadgearLow) > RankOf(order, ActorPart::kWeapon));
  CHECK(RankOf(order, ActorPart::kGarment) > RankOf(order, ActorPart::kWeapon));
}

TEST_CASE("Ordre : de face, ce que les mains tiennent passe devant coiffes et cape",
          "[render][layers]") {
  for (const std::size_t direction :
       {std::size_t{0}, std::size_t{1}, std::size_t{6}, std::size_t{7}}) {
    const std::vector<ActorPart> order = PartDrawOrder(AllParts(), direction, kGarmentOnTop);
    CHECK(RankOf(order, ActorPart::kWeapon) > RankOf(order, ActorPart::kHeadgearTop));
    CHECK(RankOf(order, ActorPart::kWeapon) > RankOf(order, ActorPart::kGarment));
    CHECK(RankOf(order, ActorPart::kShield) > RankOf(order, ActorPart::kWeapon));
    // Une traînée reste sous les coiffes.
    CHECK(RankOf(order, ActorPart::kWeaponTrail) < RankOf(order, ActorPart::kHeadgearLow));

    // La seconde arme d'un ambidextre, elle, monte entre l'arme et le bouclier.
    const std::vector<ActorPart> dual =
        PartDrawOrder(AllParts(), direction, kGarmentOnTop, /*off_hand_weapon=*/true);
    CHECK(RankOf(dual, ActorPart::kWeaponTrail) > RankOf(dual, ActorPart::kWeapon));
    CHECK(RankOf(dual, ActorPart::kWeaponTrail) < RankOf(dual, ActorPart::kShield));
  }
}

TEST_CASE("Ordre : les coiffes s'empilent bas, milieu, haut", "[render][layers]") {
  const std::vector<ActorPart> order = PartDrawOrder(AllParts(), 0, kGarmentOnTop);
  CHECK(RankOf(order, ActorPart::kHeadgearLow) < RankOf(order, ActorPart::kHeadgearMiddle));
  CHECK(RankOf(order, ActorPart::kHeadgearMiddle) < RankOf(order, ActorPart::kHeadgearTop));
  // La cape passe au-dessus des trois.
  CHECK(RankOf(order, ActorPart::kGarment) > RankOf(order, ActorPart::kHeadgearTop));
}

TEST_CASE("Ordre : mise derrière, la cape précède tout, bouclier compris", "[render][layers]") {
  // Le jeu la dessine alors dans une passe à part, avant l'acteur entier — de
  // dos aussi, où le bouclier passe pourtant derrière le corps.
  for (const std::size_t direction : {std::size_t{0}, std::size_t{3}}) {
    const std::vector<ActorPart> behind = PartDrawOrder(AllParts(), direction, kGarmentBehind);
    REQUIRE_FALSE(behind.empty());
    CHECK(behind.front() == ActorPart::kGarment);
    // Le reste garde son ordre.
    CHECK(WithoutGarment(behind) ==
          WithoutGarment(PartDrawOrder(AllParts(), direction, kGarmentOnTop)));
  }
  // Sans cape, la consigne n'invente rien.
  const std::vector<ActorPart> present = {ActorPart::kBody, ActorPart::kHead};
  CHECK(PartDrawOrder(present, 0, kGarmentBehind) == PartDrawOrder(present, 0, kGarmentOnTop));
}

TEST_CASE("Ordre : les parties absentes ne sont pas inventées", "[render][layers]") {
  // Un personnage sans arme ni bouclier ne doit pas se voir attribuer un rang
  // pour elles : l'appelant boucle sur ce que rend cette fonction.
  const std::vector<ActorPart> present = {ActorPart::kBody, ActorPart::kHead};
  const std::vector<ActorPart> order = PartDrawOrder(present, 0, kGarmentOnTop);
  REQUIRE(order.size() == 2);
  CHECK(order[0] == ActorPart::kBody);
  CHECK(order[1] == ActorPart::kHead);
}

TEST_CASE("Ordre : aucune partie donne une liste vide", "[render][layers]") {
  CHECK(PartDrawOrder({}, 0, kGarmentOnTop).empty());
}

TEST_CASE("Placement : la cape passe derrière pour les images listées, devant sinon",
          "[render][layers]") {
  GarmentPlacement placement;
  placement.behind_frames[0] = {0, 1};
  placement.behind_frames[8] = {3};
  CHECK(placement.DrawsBehind(0, 0));
  CHECK(placement.DrawsBehind(0, 1));
  CHECK_FALSE(placement.DrawsBehind(0, 2));
  CHECK(placement.DrawsBehind(8, 3));
  CHECK_FALSE(placement.DrawsBehind(8, 0));
  CHECK_FALSE(placement.DrawsBehind(1, 0));
  // Sans table, devant, toujours.
  CHECK_FALSE(GarmentPlacement{}.DrawsBehind(0, 0));
}
