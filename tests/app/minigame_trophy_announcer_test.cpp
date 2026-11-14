// LES TROPHÉES DES MINI-JEUX DITS AU SERVEUR (`CZ 0x0F4B`) : quand ils partent,
// et quand ils se taisent.

#include "game/minigame_trophy_announcer.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"

using moonclient::MinigameTrophyAnnouncer;
using ro::sim::MinigameRecord;
using ro::sim::MinigameTrophy;
using ro::test::Connect;
using ro::test::LoopbackServer;

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t kRequestBytes = ro::proto::CzMoonlightMinigameTrophy::kWireSize;
// Assez pour qu'une demande partie arrive ; un silence se juge sur ce délai.
constexpr int kSilenceMs = 200;
// Le compte d'entrées en carte du monde, à la première carte puis à la suivante.
constexpr std::uint32_t kFirstMapEntries = 1;
constexpr std::uint32_t kSecondMapEntries = 2;

// La demande telle que le serveur la lit : [op:2][len:2][trophée:1].
[[nodiscard]] Bytes Request(MinigameTrophy trophy) {
  return {0x4B, 0x0F, static_cast<std::uint8_t>(kRequestBytes), 0x00,
          static_cast<std::uint8_t>(trophy)};
}

[[nodiscard]] MinigameRecord With(std::initializer_list<MinigameTrophy> trophies) {
  MinigameRecord record;
  for (const MinigameTrophy trophy : trophies) {
    record.unlocked[static_cast<std::size_t>(trophy)] = true;
  }
  return record;
}

}  // namespace

TEST_CASE("Trophées des mini-jeux : dits à la vérification, une fois chacun", "[app][minigames]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  MinigameTrophyAnnouncer announcer;
  const MinigameRecord record =
      With({MinigameTrophy::kRojeweledCombo4, MinigameTrophy::kRoggleWon});

  // NON VÉRIFIÉ : le serveur n'écoute pas, rien ne part.
  announcer.Pump(link, false, kFirstMapEntries, record);
  CHECK(server.Receive(kRequestBytes, kSilenceMs).empty());

  // VÉRIFIÉ : chaque trophée gagné part, dans l'ordre du catalogue.
  announcer.Pump(link, true, kFirstMapEntries, record);
  CHECK(server.Receive(kRequestBytes) == Request(MinigameTrophy::kRojeweledCombo4));
  CHECK(server.Receive(kRequestBytes) == Request(MinigameTrophy::kRoggleWon));

  // DÉJÀ DITS : silence, image après image.
  announcer.Pump(link, true, kFirstMapEntries, record);
  CHECK(server.Receive(kRequestBytes, kSilenceMs).empty());
}

TEST_CASE("Trophées des mini-jeux : un trophée neuf part seul", "[app][minigames]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  MinigameTrophyAnnouncer announcer;

  announcer.Pump(link, true, kFirstMapEntries, With({MinigameTrophy::kRoggleWon}));
  CHECK(server.Receive(kRequestBytes) == Request(MinigameTrophy::kRoggleWon));
  announcer.Pump(link, true, kFirstMapEntries,
                 With({MinigameTrophy::kRoggleWon, MinigameTrophy::kRojeweledFinished}));
  CHECK(server.Receive(kRequestBytes) == Request(MinigameTrophy::kRojeweledFinished));
  CHECK(server.Receive(kRequestBytes, kSilenceMs).empty());
}

TEST_CASE("Trophées des mini-jeux : une nouvelle poignée de main les redit", "[app][minigames]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  MinigameTrophyAnnouncer announcer;
  const MinigameRecord record = With({MinigameTrophy::kRoggleThreeCatches});

  announcer.Pump(link, true, kFirstMapEntries, record);
  CHECK(server.Receive(kRequestBytes) == Request(MinigameTrophy::kRoggleThreeCatches));
  // Une entrée en zone refait la poignée de main : un autre serveur, peut-être.
  announcer.Pump(link, false, kFirstMapEntries, record);
  announcer.Pump(link, true, kFirstMapEntries, record);
  CHECK(server.Receive(kRequestBytes) == Request(MinigameTrophy::kRoggleThreeCatches));
}

TEST_CASE("Trophées des mini-jeux : une entrée en carte les redit", "[app][minigames]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  MinigameTrophyAnnouncer announcer;
  const MinigameRecord record = With({MinigameTrophy::kRojeweledScore5000});

  announcer.Pump(link, true, kFirstMapEntries, record);
  CHECK(server.Receive(kRequestBytes) == Request(MinigameTrophy::kRojeweledScore5000));
  // ⚠ MÊME SERVEUR, POIGNÉE DE MAIN TENUE : seule la carte a changé, et ce qui
  // est parti pendant le changement a pu être jeté.
  announcer.Pump(link, true, kSecondMapEntries, record);
  CHECK(server.Receive(kRequestBytes) == Request(MinigameTrophy::kRojeweledScore5000));
  announcer.Pump(link, true, kSecondMapEntries, record);
  CHECK(server.Receive(kRequestBytes, kSilenceMs).empty());
}

TEST_CASE("Trophées des mini-jeux : ceux de survivors partent sous leur rang",
          "[app][minigames][survivors]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  MinigameTrophyAnnouncer announcer;
  // Le premier et le dernier trophée du mode : rangs 8 et 15, succès 910009 et
  // 910016 côté serveur.
  constexpr std::uint8_t kFirstSurvivorsRank = 8;
  constexpr std::uint8_t kLastSurvivorsRank = 15;
  announcer.Pump(link, true, kFirstMapEntries,
                 With({MinigameTrophy::kSurvivorsFiveMinutes, MinigameTrophy::kSurvivorsPureMind}));
  const Bytes first = server.Receive(kRequestBytes);
  REQUIRE(first.size() == kRequestBytes);
  CHECK(first.back() == kFirstSurvivorsRank);
  const Bytes last = server.Receive(kRequestBytes);
  REQUIRE(last.size() == kRequestBytes);
  CHECK(last.back() == kLastSurvivorsRank);
}
