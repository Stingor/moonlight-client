// L'écran choisi se retrouve parmi les écrans branchés, ou retombe sur le
// principal.

#include "options/screen_choice.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using moonclient::OfferScreens;
using moonclient::PickScreen;
using moonclient::ScreenChoice;
using moonclient::ScreenChoiceAt;
using moonclient::ScreenLabel;
using moonclient::ScreenOffer;
using ro::render::Resolution;
using ro::render::ScreenInfo;

namespace {

constexpr Resolution kFullHd{.width = 1920, .height = 1080};
constexpr Resolution kQuadHd{.width = 2560, .height = 1440};

[[nodiscard]] ScreenInfo Screen(const char* name, Resolution desktop, bool primary = false) {
  return ScreenInfo{.name = name, .desktop = desktop, .primary = primary};
}

}  // namespace

TEST_CASE("Écran : jamais choisi, la fenêtre va sur le principal", "[app][ecran]") {
  const std::vector<ScreenInfo> branches = {Screen("DELL", kFullHd), Screen("LG", kQuadHd, true)};
  CHECK(PickScreen(ScreenChoice{}, branches) == 1);
}

TEST_CASE("Écran : le même nom au même rang est retrouvé", "[app][ecran]") {
  const std::vector<ScreenInfo> branches = {Screen("DELL", kFullHd, true), Screen("LG", kQuadHd)};
  CHECK(PickScreen(ScreenChoice{.name = "LG", .rank = 1}, branches) == 1);
}

TEST_CASE("Écran : une liste réordonnée ne fait pas changer d'écran", "[app][ecran]") {
  // 🔴 LE NOM PRIME SUR LE RANG : le système a remis le LG en tête.
  const std::vector<ScreenInfo> branches = {Screen("LG", kQuadHd), Screen("DELL", kFullHd, true)};
  CHECK(PickScreen(ScreenChoice{.name = "LG", .rank = 1}, branches) == 0);
}

TEST_CASE("Écran : deux moniteurs du même modèle se départagent par le rang", "[app][ecran]") {
  const std::vector<ScreenInfo> branches = {Screen("DELL", kFullHd, true), Screen("DELL", kFullHd),
                                            Screen("LG", kQuadHd)};
  CHECK(PickScreen(ScreenChoice{.name = "DELL", .rank = 0}, branches) == 0);
  CHECK(PickScreen(ScreenChoice{.name = "DELL", .rank = 1}, branches) == 1);
  // Un rang qui désigne maintenant un autre modèle : le DELL le plus proche.
  CHECK(PickScreen(ScreenChoice{.name = "DELL", .rank = 2}, branches) == 1);
}

TEST_CASE("Écran : un écran débranché retombe sur le principal", "[app][ecran]") {
  const std::vector<ScreenInfo> branches = {Screen("DELL", kFullHd), Screen("LG", kQuadHd, true)};
  CHECK(PickScreen(ScreenChoice{.name = "SAMSUNG", .rank = 0}, branches) == 1);
}

TEST_CASE("Écran : sans principal déclaré, le premier ; sans écran, aucun", "[app][ecran]") {
  const std::vector<ScreenInfo> branches = {Screen("DELL", kFullHd), Screen("LG", kQuadHd)};
  CHECK(PickScreen(ScreenChoice{.name = "SAMSUNG", .rank = 1}, branches) == 0);
  CHECK(PickScreen(ScreenChoice{.name = "LG", .rank = 1}, {}) == -1);
}

TEST_CASE("Écran : le libellé est numéroté à partir de un", "[app][ecran]") {
  CHECK(ScreenLabel(0, Screen("DELL U2419H", kFullHd)) == "1 — DELL U2419H (1920×1080)");
  CHECK(ScreenLabel(1, Screen("", kQuadHd)) == "2 (2560×1440)");
  CHECK(ScreenLabel(2, Screen("LG", Resolution{})) == "3 — LG");
}

TEST_CASE("Écran : l'offre désigne ses libellés, et un rang hors liste ne change rien",
          "[app][ecran]") {
  const std::vector<ScreenInfo> branches = {Screen("DELL", kFullHd, true), Screen("LG", kQuadHd)};
  ScreenOffer offer;
  OfferScreens(branches, offer);
  REQUIRE(offer.choices.size() == 2);
  CHECK(std::string(offer.choices[1]) == "2 — LG (2560×1440)");

  const ScreenChoice kept{.name = "DELL", .rank = 0};
  CHECK(ScreenChoiceAt(offer, 1, kept) == ScreenChoice{.name = "LG", .rank = 1});
  CHECK(ScreenChoiceAt(offer, 2, kept) == kept);
  CHECK(ScreenChoiceAt(offer, -1, kept) == kept);
}
