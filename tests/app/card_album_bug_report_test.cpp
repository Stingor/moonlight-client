// LE RAPPORT D'UN BUG DE L'ALBUM DE CARTES : un JSON valable, qui cite la vue
// et la double page que le joueur regardait, borné quoi qu'il montre.

#include "game/card_album_bug_report.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "web/json_value.h"

using moonclient::CardAlbumBugReport;
using ro::ui::CardAlbumPocketView;
using ro::ui::CardAlbumView;
using ro::ui::CardAlbumWindowState;

namespace {

// Ce que le serveur accepte d'un contexte ; le rapport doit rester nettement
// en dessous.
constexpr std::size_t kServerContextBytes = 1024;
constexpr std::size_t kComfortableBytes = 900;

constexpr std::uint32_t kFirstCardId = 4001;
constexpr std::size_t kPocketCount = 40;

[[nodiscard]] std::vector<CardAlbumPocketView> Pockets() {
  std::vector<CardAlbumPocketView> pockets(kPocketCount);
  for (std::size_t index = 0; index < pockets.size(); ++index) {
    pockets[index].card_id = kFirstCardId + static_cast<std::uint32_t>(index);
    pockets[index].unlocked = index % 2 == 0;
    pockets[index].reserve = static_cast<std::uint16_t>(index);
    pockets[index].name = "Card " + std::to_string(index);
  }
  return pockets;
}

[[nodiscard]] CardAlbumWindowState Window(int first, int per_spread) {
  CardAlbumWindowState window;
  for (int rank = 0; rank < static_cast<int>(kPocketCount); ++rank) {
    window.order.push_back(rank);
  }
  window.first = first;
  window.per_spread = per_spread;
  window.family_total = static_cast<int>(kPocketCount);
  window.family_unlocked = static_cast<int>(kPocketCount / 2);
  return window;
}

[[nodiscard]] bool Has(const moonclient::BugReportContext& report, std::string_view piece) {
  return report.json.find(piece) != std::string::npos;
}

}  // namespace

TEST_CASE("Rapport de l'album : la double page, depuis son premier rang", "[app][bugreport]") {
  const std::vector<CardAlbumPocketView> pockets = Pockets();
  CardAlbumView view;
  view.pockets = pockets;
  view.unlocked_count = static_cast<int>(kPocketCount / 2);
  const CardAlbumWindowState window = Window(2, 3);

  const moonclient::BugReportContext report = CardAlbumBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("pockets":40,"unlocked":20)"));
  CHECK(Has(report, R"("spread":{"first":2,"per_spread":3,"pockets":[{"card_id":4003,)"));
  CHECK(Has(report, R"({"card_id":4005,"unlocked":true,"reserve":4}],"more":0})"));
  CHECK_FALSE(Has(report, R"("card_id":4006)"));
  CHECK_FALSE(Has(report, R"("notice")"));
  CHECK(report.label.find("20 / 40") != std::string::npos);
}

TEST_CASE("Rapport de l'album : une grande page et un long compte rendu restent bornés",
          "[app][bugreport]") {
  const std::vector<CardAlbumPocketView> pockets = Pockets();
  CardAlbumView view;
  view.pockets = pockets;
  const std::string notice(kServerContextBytes, 'n');
  view.notice = notice;
  view.notice_tone = ro::ui::CardAlbumTone::kRefusal;
  view.highlight_card = kFirstCardId;
  CardAlbumWindowState window = Window(0, static_cast<int>(kPocketCount));
  std::fill(std::begin(window.filter), std::end(window.filter) - 1, 'p');

  const moonclient::BugReportContext report = CardAlbumBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.json.size() < kComfortableBytes);
  CHECK(Has(report, R"("tone":"refusal","card_id":4001)"));
  CHECK(report.label.find("ppp") != std::string::npos);
}

TEST_CASE("Rapport de l'album : une page au-delà de la vue ne cite rien", "[app][bugreport]") {
  const std::vector<CardAlbumPocketView> pockets = Pockets();
  CardAlbumView view;
  view.pockets = pockets;
  const CardAlbumWindowState window = Window(static_cast<int>(kPocketCount) + 1, 4);

  const moonclient::BugReportContext report = CardAlbumBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("pockets":[],"more":0)"));
}
