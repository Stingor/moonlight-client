// LES NOMBRES JAILLISSANTS DANS LA SCÈNE : de quel côté ils dérivent, et dans
// quel ordre ils se recouvrent.

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"

#include "world/hit_popup_rules.h"

TEST_CASE("Dégâts : la dérive d'une cloche se choisit sur la direction de la cible",
          "[app][degats]") {
  using moonclient::BurstDriftInWorld;
  using moonclient::ClientFacingDegrees;
  using Diagonal = std::array<float, 2>;

  // Les directions du serveur — nord, nord-ouest, ouest, sud-ouest, sud,
  // sud-est, est, nord-est — dans les angles du client, où le sud vaut zéro et
  // l'est quatre-vingt-dix.
  CHECK(ClientFacingDegrees(4) == Catch::Approx(0.0F));
  CHECK(ClientFacingDegrees(6) == Catch::Approx(90.0F));
  CHECK(ClientFacingDegrees(0) == Catch::Approx(180.0F));
  CHECK(ClientFacingDegrees(2) == Catch::Approx(270.0F));
  CHECK(ClientFacingDegrees(5) == Catch::Approx(45.0F));
  CHECK(ClientFacingDegrees(12) == ClientFacingDegrees(4));

  // Dans notre monde, la profondeur retournée, pour un angle ramené au tour —
  // celui d'une marche : vers le sud, le nombre part à gauche et au nord ; vers
  // l'est, à gauche et au sud ; vers le nord, à droite et au sud ; vers
  // l'ouest, à droite et au nord.
  CHECK(BurstDriftInWorld(ClientFacingDegrees(4)) == Diagonal{-0.8F, -0.8F});
  CHECK(BurstDriftInWorld(ClientFacingDegrees(6)) == Diagonal{-0.8F, 0.8F});
  CHECK(BurstDriftInWorld(ClientFacingDegrees(0)) == Diagonal{0.8F, 0.8F});
  CHECK(BurstDriftInWorld(ClientFacingDegrees(2)) == Diagonal{0.8F, -0.8F});
  // Les diagonales du serveur tombent avec leur voisine de gauche sur le
  // cadran : le sud-est avec le sud, le nord-est avec l'est.
  CHECK(BurstDriftInWorld(ClientFacingDegrees(5)) == BurstDriftInWorld(ClientFacingDegrees(4)));
  CHECK(BurstDriftInWorld(ClientFacingDegrees(7)) == BurstDriftInWorld(ClientFacingDegrees(6)));
}

TEST_CASE("Dégâts : chaque type prend l'allure du client, et sa dérive de côté", "[app][degats]") {
  using moonclient::DamagePath;
  using ro::render::NumberStyle;
  // L'allure se lit sur le type : elle ne donne plus que la taille où le nombre
  // s'arrête et le point où il naît.
  CHECK(moonclient::StyleOfPath(DamagePath::kOnMonster) == NumberStyle::kBursting);
  CHECK(moonclient::StyleOfPath(DamagePath::kCritical) == NumberStyle::kCriticalBurst);
  CHECK(moonclient::StyleOfPath(DamagePath::kTotal) == NumberStyle::kRising);
  CHECK(moonclient::StyleOfPath(DamagePath::kHeal) == NumberStyle::kHealing);

  // Le client fait dériver ses cloches, et elles seules : c'est le défaut de la
  // trajectoire de chaque type.
  CHECK(moonclient::ClientPathOf(DamagePath::kOnPlayer).facing_drift);
  CHECK(moonclient::ClientPathOf(DamagePath::kCritical).facing_drift);
  CHECK_FALSE(moonclient::ClientPathOf(DamagePath::kTotal).facing_drift);
  CHECK_FALSE(moonclient::ClientPathOf(DamagePath::kWords).facing_drift);

  // Et le plan d'un critique le porte jusqu'au nombre.
  const moonclient::HitPopupLook look;
  ro::sim::Strike blow;
  blow.damage = 10;
  blow.type = ro::render::kCriticalBlow;
  const moonclient::StrikePlan plan = moonclient::PlanStrike(
      blow, moonclient::StrikeSides{.attacker_is_known = true, .attacker_is_player = true}, look);
  CHECK(plan.path == DamagePath::kCritical);
  CHECK(plan.number.style == NumberStyle::kCriticalBurst);
}

TEST_CASE("Dégâts : les nombres se dessinent dans l'ordre de leur naissance", "[app][degats]") {
  // Un coup en trois frappes arrive à mille, et range aussitôt ses trois parts,
  // chacune suivie de son cumul ; un coup seul arrive ensuite et naît à mille
  // cent, ENTRE la première frappe et la deuxième. Le dernier né se dessine
  // par-dessus : le coup seul passe donc sous la deuxième frappe, et non
  // au-dessus de tout.
  struct Born {
    std::uint32_t born_ms = 0;
    char name = ' ';
    bool operator==(const Born&) const = default;
  };
  std::vector<Born> popups = {{1000, 'a'}, {1000, 'A'}, {1200, 'b'}, {1200, 'B'},
                              {1400, 'c'}, {1400, 'C'}, {1100, 's'}};
  moonclient::OrderByBirth(popups);
  CHECK(popups == std::vector<Born>{{1000, 'a'},
                                    {1000, 'A'},
                                    {1100, 's'},
                                    {1200, 'b'},
                                    {1200, 'B'},
                                    {1400, 'c'},
                                    {1400, 'C'}});
}

TEST_CASE("Dégâts : une direction du serveur envoie la cloche à droite et au nord",
          "[app][degats]") {
  // 🔴 LE CAS DU MANNEQUIN : une cible immobile, tournée par le serveur vers
  // l'est. Le client pose alors quatre cent cinquante degrés, sans les ramener
  // au tour, et tout angle d'un tour et plus prend le dernier quadrant : le
  // nombre part à droite et VERS LE NORD, loin de la caméra. Lu comme quatre-
  // vingt-dix degrés, il partait à gauche et vers le sud, et finissait devant.
  using moonclient::BurstDriftInWorld;
  using moonclient::ClientBodyDegrees;
  using Diagonal = std::array<float, 2>;
  constexpr std::uint8_t kNorth = 0;
  constexpr std::uint8_t kWest = 2;
  constexpr std::uint8_t kSouth = 4;
  constexpr std::uint8_t kEast = 6;
  constexpr std::uint8_t kNorthEast = 7;

  ro::sim::Actor told;
  told.facing_origin = ro::sim::FacingOrigin::kServer;
  CHECK(ClientBodyDegrees(told, kEast) == Catch::Approx(450.0F));
  CHECK(ClientBodyDegrees(told, kSouth) == Catch::Approx(360.0F));
  CHECK(ClientBodyDegrees(told, kNorthEast) == Catch::Approx(495.0F));
  CHECK(ClientBodyDegrees(told, kNorth) == Catch::Approx(180.0F));
  CHECK(ClientBodyDegrees(told, kWest) == Catch::Approx(270.0F));
  CHECK(BurstDriftInWorld(ClientBodyDegrees(told, kEast)) == Diagonal{0.8F, -0.8F});
  CHECK(BurstDriftInWorld(ClientBodyDegrees(told, kSouth)) == Diagonal{0.8F, -0.8F});
  // Le nord et l'ouest restent dans le tour, et ne changent pas.
  CHECK(BurstDriftInWorld(ClientBodyDegrees(told, kNorth)) == Diagonal{0.8F, 0.8F});

  // La même boussole, donnée par la marche, reste dans le tour.
  ro::sim::Actor walked;
  walked.facing_origin = ro::sim::FacingOrigin::kWalk;
  CHECK(ClientBodyDegrees(walked, kEast) == Catch::Approx(90.0F));
  CHECK(BurstDriftInWorld(ClientBodyDegrees(walked, kEast)) == Diagonal{-0.8F, 0.8F});
  // Et pendant la marche, c'est le pas qui la donne, quelle que soit l'origine.
  told.movement = ro::sim::CellMovement{};
  CHECK(ClientBodyDegrees(told, kEast) == Catch::Approx(90.0F));

  // La visée garde l'angle EXACT : un pas à l'ouest pour trois au sud fait
  // trois cent quarante et un degrés et demi, que la boussole arrondit au sud
  // — zéro, l'autre quadrant.
  ro::sim::Actor aimed;
  aimed.facing_origin = ro::sim::FacingOrigin::kAim;
  aimed.aimed_dx_cells = -1;
  aimed.aimed_dy_cells = -3;
  CHECK(ClientBodyDegrees(aimed, kSouth) == Catch::Approx(341.565F).margin(1e-2F));
  CHECK(BurstDriftInWorld(ClientBodyDegrees(aimed, kSouth)) == Diagonal{0.8F, -0.8F});
}
