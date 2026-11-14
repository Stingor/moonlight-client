// L'instance du joueur : ses phases, l'ouverture qui ne se redit pas à chaque
// carte, l'écart d'horloge avec le serveur, ses fins, et la destruction en
// attente de réponse.

#include "ro/sim/memorial_dungeon.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::MemorialDetails;
using ro::sim::MemorialDungeon;
using ro::sim::MemorialEnding;
using ro::sim::MemorialPhase;

namespace {

constexpr std::int64_t kNow = 1'800'000'000;
// L'horloge du serveur avance de deux minutes sur celle de la machine.
constexpr std::int64_t kServerAhead = 120;
constexpr std::uint32_t kServerNow = static_cast<std::uint32_t>(kNow + kServerAhead);

}  // namespace

TEST_CASE("Instance : réservée, en file, puis ouverte", "[sim][instance]") {
  MemorialDungeon dungeon;
  const std::uint32_t before = dungeon.arrival_serial();
  dungeon.TellCreated("Old Glast Heim", 3);
  CHECK(dungeon.phase() == MemorialPhase::kWaiting);
  CHECK(dungeon.wait_position() == 3);
  CHECK(dungeon.arrival_serial() != before);
  dungeon.TellWaitPosition(1);
  CHECK(dungeon.wait_position() == 1);
  // En file, aucune échéance.
  CHECK_FALSE(dungeon.SecondsLeft(kNow).has_value());

  const std::uint32_t waiting = dungeon.arrival_serial();
  dungeon.TellOpen("Old Glast Heim", kServerNow + 3600, 0);
  CHECK(dungeon.phase() == MemorialPhase::kOpen);
  CHECK(dungeon.arrival_serial() != waiting);
  // LE SERVEUR REDIT L'INSTANCE À CHAQUE CARTE : ce n'est pas une arrivée.
  const std::uint32_t opened = dungeon.arrival_serial();
  dungeon.TellOpen("Old Glast Heim", kServerNow + 3600, 0);
  CHECK(dungeon.arrival_serial() == opened);
}

TEST_CASE("Instance : les échéances se lisent sur l'horloge du serveur", "[sim][instance]") {
  MemorialDungeon dungeon;
  dungeon.TellOpen("Airship Assault", kServerNow + 3600, kServerNow + 300);
  // Sans détail, l'écart est supposé nul : deux minutes de trop.
  CHECK(dungeon.SecondsLeft(kNow) == 3600 + kServerAhead);
  dungeon.TellDetails(MemorialDetails{.hard_mode = 5, .turbo = true}, kServerNow, kNow);
  CHECK(dungeon.SecondsLeft(kNow) == 3600);
  CHECK(dungeon.IdleSecondsLeft(kNow) == 300);
  CHECK(dungeon.LocalEnd() == kNow + 3600);
  CHECK(dungeon.details()->hard_mode == 5);
  CHECK(dungeon.details()->turbo);
  // Bornée à zéro une fois dépassée.
  CHECK(dungeon.SecondsLeft(kNow + 99'999) == 0);
  // QUELQU'UN ENTRE : plus de fermeture à vide.
  dungeon.TellIdleLimit(0);
  CHECK_FALSE(dungeon.IdleSecondsLeft(kNow).has_value());
}

TEST_CASE("Instance : une autre instance oublie le détail de la précédente", "[sim][instance]") {
  MemorialDungeon dungeon;
  dungeon.TellOpen("Airship Assault", 0, 0);
  dungeon.TellDetails(MemorialDetails{.hard_mode = 7}, kServerNow, kNow);
  dungeon.TellOpen("Old Glast Heim", 0, 0);
  CHECK_FALSE(dungeon.details().has_value());
  // Sans échéance, sans limite.
  CHECK_FALSE(dungeon.SecondsLeft(kNow).has_value());
}

TEST_CASE("Instance : chaque fin se dit une fois, et vide tout", "[sim][instance]") {
  MemorialDungeon dungeon;
  dungeon.TellCreated("Old Glast Heim", 1);
  dungeon.TellReservationCancelled();
  CHECK(dungeon.phase() == MemorialPhase::kNone);
  CHECK(dungeon.TakeEndings() == std::vector{MemorialEnding::kReservationCancelled});
  CHECK(dungeon.TakeEndings().empty());

  dungeon.TellOpen("Old Glast Heim", 0, 0);
  dungeon.TellEnded(MemorialEnding::kDestroyed);
  CHECK(dungeon.name().empty());
  CHECK(dungeon.TakeEndings() == std::vector{MemorialEnding::kDestroyed});

  // UNE ANNULATION SANS INSTANCE ne se dit pas.
  dungeon.TellReservationCancelled();
  CHECK(dungeon.TakeEndings().empty());
}

TEST_CASE("Instance : la destruction attend sa réponse, un temps", "[sim][instance]") {
  MemorialDungeon dungeon;
  dungeon.TellOpen("Old Glast Heim", 0, 0);
  CHECK_FALSE(dungeon.DestroyPending(kNow));
  dungeon.NoteDestroyAsked(kNow);
  CHECK(dungeon.DestroyPending(kNow));
  CHECK(dungeon.DestroyPending(kNow + ro::sim::kMemorialDestroyAnswerS - 1));
  // Un refus muet du serveur rallume le bouton.
  CHECK_FALSE(dungeon.DestroyPending(kNow + ro::sim::kMemorialDestroyAnswerS));
}
