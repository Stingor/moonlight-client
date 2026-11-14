#include "ro/sim/server_clock.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::sim::ServerClock;

TEST_CASE("Horloge : le passage par zéro du tic serveur ne date rien avant l'origine",
          "[sim][clock]") {
  // 🔴 LE `gettick` DU SERVEUR EST TRONQUÉ À 32 BITS, et repasse par zéro au
  // bout de 49,7 jours d'uptime. Calculé en entiers larges, le décalage mesuré
  // juste avant le passage datait chaque départ suivant AVANT l'origine : tous
  // les marcheurs sautaient à destination.
  constexpr std::uint32_t kJustBeforeWrap = 0xFFFF'FF00U;
  ServerClock clock;
  clock.NoteServerTick(kJustBeforeWrap, 10'000);

  // 356 ms plus tard chez le serveur, son tic vaut 100 : la marche a commencé
  // à 10 356 chez nous.
  CHECK(clock.LocalTimeOf(100, 10'500) == 10'356);
}

TEST_CASE("Horloge : l'estimation suit une horloge qui dérive", "[sim][clock]") {
  // 🔴 UN MINIMUM GARDÉ À VIE SE FIGERAIT. Ici le serveur prend du retard : le
  // vrai décalage grandit de 500 ms. Au bout de deux fenêtres, l'estimation
  // doit l'avoir suivi, sans quoi chaque départ se daterait trop tôt et un
  // acteur rapide serait dessiné en avance sur sa vraie position.
  constexpr std::uint32_t kEarlyOffset = 1'000;
  constexpr std::uint32_t kLateOffset = 1'500;
  constexpr std::uint32_t kHeartbeatMs = 10'000;
  // Assez de battements pour clore trois fenêtres.
  constexpr int kBeats = 3 * static_cast<int>(ServerClock::kOffsetWindowMs / kHeartbeatMs);
  ServerClock clock;
  std::uint32_t local = 20'000;
  clock.NoteServerTick(local - kEarlyOffset, local);
  for (int beat = 0; beat < kBeats; ++beat) {
    local += kHeartbeatMs;
    clock.NoteServerTick(local - kLateOffset, local);
  }
  CHECK(clock.LocalTimeOf(local - kLateOffset, local) == local);
}

TEST_CASE("Horloge : un trajet lent dans la fenêtre ne dégrade pas l'estimation", "[sim][clock]") {
  // Le contrepoint du cas précédent : dans une même fenêtre, le meilleur trajet
  // reste la référence, et une trame retardée n'y change rien.
  ServerClock clock;
  clock.NoteServerTick(1'000, 5'200);
  clock.NoteServerTick(2'000, 6'900);
  CHECK(clock.LocalTimeOf(2'000, 7'000) == 6'200);
}

TEST_CASE("Horloge : un départ de marche cale l'heure sans fermer l'aller-retour", "[sim][clock]") {
  // ⚠ UNE MARCHE N'EST PAS LA RÉPONSE AU BATTEMENT. La prendre pour telle
  // fermerait l'aller-retour à la première marche reçue, et la latence
  // affichée serait trop courte.
  ServerClock clock;
  clock.NoteTickSent(1'000);
  clock.NoteServerTimestamp(500, 1'010);
  CHECK(clock.known());
  CHECK(clock.round_trip_ms() < 0);

  clock.NoteServerTick(600, 1'080);
  CHECK(clock.round_trip_ms() == 80);
}

TEST_CASE("Horloge : rien ne se date avant l'origine de l'appelant", "[sim][clock]") {
  ServerClock clock;
  clock.NoteServerTick(50'000, 1'000);
  // Le tic 40 000 du serveur tombe 9 000 ms avant notre origine.
  CHECK(clock.LocalTimeOf(40'000, 1'000) == 0);
}

TEST_CASE("Horloge : une trame lue à une heure périmée ne descend pas sous le plancher",
          "[sim][clock]") {
  ServerClock clock;
  clock.NoteTickSent(1'000);
  clock.NoteServerTick(500'010, 1'020);
  // La trame date de 3 000 ms après l'heure à laquelle on la croit lue.
  clock.NoteServerTimestamp(503'110, 1'100);
  // Le plancher du battement : le tic 500 010 ne précède pas notre envoi à 1 000.
  CHECK(clock.LocalTimeOf(503'200, 4'300) == 4'190);
}

TEST_CASE("Horloge : le premier battement détrompe un calage déjà faussé", "[sim][clock]") {
  ServerClock clock;
  clock.NoteServerTimestamp(503'110, 1'100);
  CHECK(clock.LocalTimeOf(503'200, 4'000) == 1'190);

  clock.NoteTickSent(4'100);
  clock.NoteServerTick(503'110, 4'120);
  // Le serveur a répondu après 4 100 : son tic 503 110 ne précède pas cet instant.
  CHECK(clock.LocalTimeOf(503'200, 4'300) == 4'190);
}

TEST_CASE("Horloge : le plancher ne change rien à une estimation juste", "[sim][clock]") {
  ServerClock clock;
  clock.NoteServerTimestamp(500'000, 1'005);
  clock.NoteTickSent(1'100);
  clock.NoteServerTick(500'120, 1'140);
  // Le meilleur trajet reste celui des 5 ms, au-dessus du plancher.
  CHECK(clock.LocalTimeOf(500'200, 1'300) == 1'205);
}
