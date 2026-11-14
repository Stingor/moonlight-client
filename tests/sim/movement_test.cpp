#include "ro/sim/movement.h"

#include <array>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ro/sim/combat_guard.h"

using Catch::Matchers::WithinAbs;
using ro::sim::Actor;
using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::DirectionTowards;
using ro::sim::FacingAt;
using ro::sim::InterpolatedCell;
using ro::sim::IsWalking;
using ro::sim::MovementDurationMs;
using ro::sim::WalkStartAfterSpeedChange;

namespace {

// Un acteur qui marche en ligne droite vers l'est, une case toutes les 150 ms.
Actor Walker(CellPosition from, CellPosition to, std::uint32_t started_ms) {
  Actor actor;
  actor.id = 1;
  actor.position = from;
  actor.speed_ms_per_cell = 150;
  // Huit est le centre de la case : ce que le serveur écrit pour une marche
  // neuve.
  actor.movement = CellMovement{from, to, 8, 8};
  actor.movement_started_ms = started_ms;
  return actor;
}

}  // namespace

TEST_CASE("Direction : les huit vecteurs sont ceux du serveur", "[sim][movement]") {
  // La table est celle de dirx/diry, dans src/map/unit.cpp. Zéro pointe au NORD,
  // c'est-à-dire vers les y croissants. Se tromper d'origine ferait regarder
  // tout le monde à quarante-cinq degrés de sa marche.
  const CellPosition origin{100, 100, 0};
  CHECK(DirectionTowards(origin, CellPosition{100, 101, 0}, 9) == 0);
  CHECK(DirectionTowards(origin, CellPosition{99, 101, 0}, 9) == 1);
  CHECK(DirectionTowards(origin, CellPosition{99, 100, 0}, 9) == 2);
  CHECK(DirectionTowards(origin, CellPosition{99, 99, 0}, 9) == 3);
  CHECK(DirectionTowards(origin, CellPosition{100, 99, 0}, 9) == 4);
  CHECK(DirectionTowards(origin, CellPosition{101, 99, 0}, 9) == 5);
  CHECK(DirectionTowards(origin, CellPosition{101, 100, 0}, 9) == 6);
  CHECK(DirectionTowards(origin, CellPosition{101, 101, 0}, 9) == 7);
}

TEST_CASE("Direction : un déplacement nul garde celle d'avant", "[sim][movement]") {
  // Remettre zéro ferait pivoter vers le nord quiconque s'arrête.
  const CellPosition origin{100, 100, 0};
  CHECK(DirectionTowards(origin, origin, 6) == 6);
}

TEST_CASE("Direction : elle se déduit de LOIN comme de près", "[sim][movement]") {
  // Seul le SIGNE des écarts compte : un trajet de vingt cases vers l'est se
  // regarde comme un pas vers l'est.
  const CellPosition origin{100, 100, 0};
  CHECK(DirectionTowards(origin, CellPosition{120, 100, 0}, 9) == 6);
  CHECK(DirectionTowards(origin, CellPosition{120, 120, 0}, 9) == 7);
}

TEST_CASE("Durée : un pas par case, et quatorze dixièmes en diagonale", "[sim][movement]") {
  // Le serveur compte UN pas par case, mais un pas en diagonale dure
  // MOVE_DIAGONAL_COST sur MOVE_COST de plus : 210 ms au lieu de 150. Sommer
  // les deux écarts ferait durer une diagonale deux fois trop longtemps ; les
  // compter comme des pas droits la ferait finir trop tôt.
  CHECK(MovementDurationMs(Walker(CellPosition{100, 100, 0}, CellPosition{110, 100, 0}, 0)) ==
        10 * 150);
  CHECK(MovementDurationMs(Walker(CellPosition{100, 100, 0}, CellPosition{110, 110, 0}, 0)) ==
        10 * 210);
  CHECK(MovementDurationMs(Walker(CellPosition{100, 100, 0}, CellPosition{110, 103, 0}, 0)) ==
        3 * 210 + 7 * 150);
}

TEST_CASE("Durée : une vitesse inconnue rend le déplacement immédiat", "[sim][movement]") {
  // Zéro milliseconde par case veut dire « le serveur ne l'a pas dit ». Diviser
  // par cette valeur planterait ; poser l'acteur à l'arrivée est visible et
  // sans danger.
  Actor actor = Walker(CellPosition{100, 100, 0}, CellPosition{110, 100, 0}, 0);
  actor.speed_ms_per_cell = 0;
  CHECK(MovementDurationMs(actor) == 0);
  CHECK_THAT(InterpolatedCell(actor, 1000).x, WithinAbs(110.0F, 0.001F));
}

TEST_CASE("Interpolation : à mi-chemin, on est au milieu", "[sim][movement]") {
  const Actor actor = Walker(CellPosition{100, 100, 0}, CellPosition{110, 100, 0}, 1000);
  // Dix cases à 150 ms : 1500 ms en tout, donc 750 ms au milieu.
  const auto middle = InterpolatedCell(actor, 1000 + 750);
  CHECK_THAT(middle.x, WithinAbs(105.0F, 0.001F));
  CHECK_THAT(middle.y, WithinAbs(100.0F, 0.001F));
}

TEST_CASE("Interpolation : avant le départ et après l'arrivée, on ne dépasse pas",
          "[sim][movement]") {
  // Sans ces deux bornes, un instant hors de l'intervalle projetterait l'acteur
  // au-delà de sa destination — et un instant ANTÉRIEUR, avec une soustraction
  // non signée, très loin derrière.
  const Actor actor = Walker(CellPosition{100, 100, 0}, CellPosition{110, 100, 0}, 1000);
  CHECK_THAT(InterpolatedCell(actor, 500).x, WithinAbs(100.0F, 0.001F));
  CHECK_THAT(InterpolatedCell(actor, 1000).x, WithinAbs(100.0F, 0.001F));
  CHECK_THAT(InterpolatedCell(actor, 999'999).x, WithinAbs(110.0F, 0.001F));
}

TEST_CASE("Interpolation : sans déplacement, on est sur sa case", "[sim][movement]") {
  Actor actor;
  actor.position = CellPosition{150, 100, 3};
  const auto where = InterpolatedCell(actor, 12'345);
  CHECK_THAT(where.x, WithinAbs(150.0F, 0.001F));
  CHECK_THAT(where.y, WithinAbs(100.0F, 0.001F));
}

TEST_CASE("Regard : celui qui marche regarde où il va", "[sim][movement]") {
  const Actor actor = Walker(CellPosition{100, 100, 0}, CellPosition{100, 90, 0}, 0);
  // Vers le sud, donc la direction 4.
  CHECK(FacingAt(actor, 0) == 4);
  // Même arrivé : il reste tourné vers là où il allait.
  CHECK(FacingAt(actor, 999'999) == 4);
}

TEST_CASE("Regard : celui qui ne marche pas garde sa direction annoncée", "[sim][movement]") {
  Actor actor;
  actor.position = CellPosition{150, 100, 5};
  CHECK(FacingAt(actor, 0) == 5);
}

TEST_CASE("Marche : elle s'arrête à l'arrivée, pas à l'oubli du trajet", "[sim][movement]") {
  // Le trajet annoncé SUBSISTE après l'arrivée : c'est lui qui garde la
  // direction du dernier pas. Juger la marche à sa présence ferait donc agiter
  // les jambes à un personnage immobile, indéfiniment.
  const Actor actor = Walker(CellPosition{10, 10, 0}, CellPosition{14, 10, 0}, 1000);
  const std::uint32_t duration = MovementDurationMs(actor);
  REQUIRE(duration == 4 * 150);

  CHECK(IsWalking(actor, 1000));
  CHECK(IsWalking(actor, 1000 + duration - 1));
  CHECK_FALSE(IsWalking(actor, 1000 + duration));
  CHECK_FALSE(IsWalking(actor, 1000 + duration + 5000));
  // Le trajet est toujours là — c'est bien le temps qui a tranché.
  CHECK(actor.movement.has_value());
}

TEST_CASE("Marche : un acteur sans trajet ne marche pas", "[sim][movement]") {
  Actor still;
  still.id = 2;
  still.position = CellPosition{5, 5, 3};
  CHECK_FALSE(IsWalking(still, 12345));
}

TEST_CASE("Marche : un pas instantané ne se voit jamais", "[sim][movement]") {
  // Vitesse inconnue : le déplacement est immédiat, et l'annoncer comme une
  // marche ferait clignoter l'animation le temps d'une image.
  Actor teleported = Walker(CellPosition{1, 1, 0}, CellPosition{9, 9, 0}, 500);
  teleported.speed_ms_per_cell = 0;
  CHECK_FALSE(IsWalking(teleported, 500));
  CHECK_FALSE(IsWalking(teleported, 600));
}

TEST_CASE("Marche : avant le départ, l'acteur est déjà en chemin", "[sim][movement]") {
  // Une horloge de rejeu peut prendre de l'avance sur le trajet qu'elle vient
  // de lire. L'acteur est alors à son point de départ, pas immobile.
  const Actor actor = Walker(CellPosition{2, 2, 0}, CellPosition{2, 6, 0}, 1000);
  CHECK(IsWalking(actor, 900));
}

TEST_CASE("Interpolation : un objet au sol se tient où il est tombé dans sa case",
          "[sim][movement]") {
  ro::sim::Actor item;
  item.kind = ro::sim::ActorKind::kItem;
  item.position = ro::sim::CellPosition{10, 10, 0};
  item.sub_x = 12;
  item.sub_y = 4;
  const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(item, 0);
  CHECK(cell.x == 10.25F);
  CHECK(cell.y == 9.75F);
}

TEST_CASE("Mouvement : la distance parcourue compte les diagonales", "[sim][movement]") {
  // Le jeu anime la marche sur la DISTANCE ; un pas de biais en couvre la
  // racine de deux, et le compter pour un ferait boiter l'animation en
  // diagonale.
  ro::sim::Actor actor;
  actor.id = 1;
  actor.speed_ms_per_cell = 100;
  actor.movement_started_ms = 0;

  // Immobile : rien de parcouru.
  CHECK(ro::sim::TravelledCells(actor, ro::sim::WalkPath{}, 500) == 0.0F);

  actor.position = ro::sim::CellPosition{10, 10, 0};
  actor.movement = ro::sim::CellMovement{ro::sim::CellPosition{10, 10, 0},
                                         ro::sim::CellPosition{12, 10, 0}, 8, 8};
  const ro::sim::WalkPath straight{{6, 6}};  // deux pas vers l'est
  CHECK(ro::sim::TravelledCells(actor, straight, 0) == Catch::Approx(0.0F));
  CHECK(ro::sim::TravelledCells(actor, straight, 100) == Catch::Approx(1.0F));
  CHECK(ro::sim::TravelledCells(actor, straight, 50) == Catch::Approx(0.5F));
}

// ── LA GARDE APRÈS LE COMBAT ────────────────────────────────────────────────
//
// Ici ne vit que la LISTE des états qu'on quitte pour la garde, et la porte du
// mode PK ; quel état un geste fait quitter, c'est `app` qui le lit dans les
// tables du client. Voir `ro/sim/combat_guard.h`.

TEST_CASE("Garde : les états qu'on quitte pour elle, et ceux qu'on quitte pour l'attente",
          "[sim][garde]") {
  // L'attaque, la blessure, l'incantation ordinaire et cinq poses de compétence.
  constexpr std::array<std::int8_t, 8> guarding = {2, 4, 8, 52, 53, 54, 58, 59};
  for (const std::int8_t state : guarding) {
    CHECK(ro::sim::LeavingStateGuards(state));
  }
  // 🔴 L'EMPLOI ORDINAIRE (7), LE RAMASSAGE (5), LA MARCHE (1), L'ASSISE (6) ET
  // LE REPOS LUI-MÊME (0) ramènent à l'attente : un prêtre qui vient de
  // soigner ne lève pas son arme.
  constexpr std::array<std::int8_t, 13> resting = {0, 1, 5, 6, 7, 9, 12, 19, 27, 45, 55, 56, 57};
  for (const std::int8_t state : resting) {
    CHECK_FALSE(ro::sim::LeavingStateGuards(state));
  }
}

TEST_CASE("Garde : le mode PK la prend de n'importe quel état", "[sim][garde]") {
  ro::sim::Actor actor;
  actor.id = 150'001;
  actor.kind = ro::sim::ActorKind::kPlayer;
  CHECK_FALSE(ro::sim::GuardsLeaving(actor, 7));
  CHECK(ro::sim::GuardsLeaving(actor, 2));
  actor.pk_mode = true;
  CHECK(ro::sim::GuardsLeaving(actor, 7));
  CHECK(ro::sim::GuardsLeaving(actor, 0));
}

TEST_CASE("Mort : le corps reste où la mort l'a pris", "[sim][movement][mort]") {
  // 🔴 UN MONSTRE TUÉ EN PLEINE MARCHE NE GLISSE PAS jusqu'au bout de son
  // chemin, couché.
  //
  // Le client ne le permet pas : son changement de pose rend la main dès son
  // entrée quand l'état vaut trois, si bien qu'un mort ne repasse
  // plus par le cas de la marche et que son chemin cesse d'être échantillonné.
  Actor actor = Walker(CellPosition{100, 100, 6}, CellPosition{104, 100, 6}, 1000);
  actor.posture = ro::sim::Posture::kDead;
  actor.posture_since_ms = 1300;

  // À l'instant de la mort, deux cases sont faites : 300 ms à 150 ms la case.
  const auto at_death = InterpolatedCell(actor, 1300);
  CHECK_THAT(at_death.x, WithinAbs(102.0, 0.01));

  // Et le corps n'avance plus d'un pouce, ni pendant le reste du trajet, ni
  // longtemps après son terme.
  for (const std::uint32_t now_ms : {1400U, 1600U, 5000U}) {
    const auto later = InterpolatedCell(actor, now_ms);
    CHECK_THAT(later.x, WithinAbs(at_death.x, 0.001));
    CHECK_THAT(later.y, WithinAbs(at_death.y, 0.001));
  }
}

TEST_CASE("Mort : un vivant, lui, poursuit sa marche", "[sim][movement][mort]") {
  // Le garde-fou de l'essai d'au-dessus : sans la posture, la même marche avance
  // bien. Sans quoi il passerait aussi sur un code qui fige TOUT LE MONDE.
  Actor actor = Walker(CellPosition{100, 100, 6}, CellPosition{104, 100, 6}, 1000);
  CHECK_THAT(InterpolatedCell(actor, 1300).x, WithinAbs(102.0, 0.01));
  CHECK_THAT(InterpolatedCell(actor, 1450).x, WithinAbs(103.0, 0.01));
}

TEST_CASE("Marche : une vitesse qui baisse ne rembobine pas une marche achevée",
          "[sim][movement]") {
  // Dix cases à 150 ms durent 1500 ms. Lues à 300 ms par case, les 2000 ms écoulées
  // tomberaient à la case 6,67 et le marcheur referait le chemin.
  Actor actor = Walker(CellPosition{100, 100, 6}, CellPosition{110, 100, 6}, 10'000);
  actor.movement_started_ms = WalkStartAfterSpeedChange(actor, nullptr, 300, 12'000);
  actor.speed_ms_per_cell = 300;
  CHECK_FALSE(IsWalking(actor, 12'000));
  CHECK_THAT(InterpolatedCell(actor, 12'000).x, WithinAbs(110.0, 0.01));
}

TEST_CASE("Marche : une vitesse qui change garde la place et règle la suite", "[sim][movement]") {
  // À 750 ms de marche, le marcheur est à la moitié de ses dix cases.
  const Actor walker = Walker(CellPosition{100, 100, 6}, CellPosition{110, 100, 6}, 1000);

  SECTION("plus lent") {
    Actor actor = walker;
    actor.movement_started_ms = WalkStartAfterSpeedChange(actor, nullptr, 300, 1750);
    actor.speed_ms_per_cell = 300;
    CHECK_THAT(InterpolatedCell(actor, 1750).x, WithinAbs(105.0, 0.01));
    CHECK_THAT(InterpolatedCell(actor, 2050).x, WithinAbs(106.0, 0.01));
    CHECK(IsWalking(actor, 2050));
  }

  SECTION("plus rapide") {
    Actor actor = walker;
    actor.movement_started_ms = WalkStartAfterSpeedChange(actor, nullptr, 75, 1750);
    actor.speed_ms_per_cell = 75;
    CHECK_THAT(InterpolatedCell(actor, 1750).x, WithinAbs(105.0, 0.01));
    CHECK_THAT(InterpolatedCell(actor, 1825).x, WithinAbs(106.0, 0.01));
  }
}

TEST_CASE("Marche : rien à convertir, le départ ne bouge pas", "[sim][movement]") {
  const Actor walker = Walker(CellPosition{100, 100, 6}, CellPosition{110, 100, 6}, 1000);

  Actor unknown_speed = walker;
  unknown_speed.speed_ms_per_cell = 0;
  CHECK(WalkStartAfterSpeedChange(unknown_speed, nullptr, 150, 1500) == 1000);

  Actor gliding = walker;
  gliding.glide_ms_per_cell = 15;
  CHECK(WalkStartAfterSpeedChange(gliding, nullptr, 300, 1500) == 1000);

  CHECK(WalkStartAfterSpeedChange(walker, nullptr, 300, 900) == 1000);
}
