// LE CERCLE DE VISÉE : sa taille par compétence, son image, sa teinte qui bat et
// les cases qu'il peint.

#include "ro/render/ground_scope.h"

#include <cmath>
#include <cstdint>
#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

namespace {

constexpr std::uint16_t kMeteorStorm = 83;
constexpr std::uint16_t kPneuma = 25;
constexpr std::uint16_t kVermilion = 85;
constexpr std::uint16_t kNpcComet = 708;
constexpr std::uint16_t kFireWall = 18;
constexpr std::uint16_t kDiamondDust = 2447;
constexpr std::uint16_t kCompetentia = 5278;
constexpr std::uint16_t kPoisonSmoke = 2032;
// Une classe du registre des actions qui répond une portée.
constexpr std::uint16_t kCrossRain = 5267;

}  // namespace

TEST_CASE("Cercle de visée : le côté suit la zone du sort", "[render][ground_scope]") {
  CHECK(ro::render::GroundScopeSide(kMeteorStorm) == 11);
  CHECK(ro::render::GroundScopeSide(kPneuma) == 5);
  CHECK(ro::render::GroundScopeSide(kVermilion) == 13);
  CHECK(ro::render::GroundScopeSide(kNpcComet) == 20);
  CHECK(ro::render::GroundScopeSide(kCompetentia) == 21);
  CHECK(ro::render::GroundScopeSide(kCrossRain) == 11);
  // Une compétence que la table ne nomme pas, et une qui n'en montre aucun.
  CHECK(ro::render::GroundScopeSide(kFireWall) == 2);
  CHECK(ro::render::GroundScopeSide(kDiamondDust) == 0);
}

TEST_CASE("Cercle de visée : la cible ordinaire, ou le cercle de trois compétences",
          "[render][ground_scope]") {
  CHECK(ro::render::GroundScopeTexture(kMeteorStorm, false) == "effect\\magic_target.tga");
  CHECK(ro::render::GroundScopeTexture(kPoisonSmoke, true).starts_with("effect\\mineffect\\"));
  CHECK_FALSE(
      ro::render::GroundScopeTexture(kPoisonSmoke, false).starts_with("effect\\mineffect\\"));
  CHECK(ro::render::GroundScopeTexture(kCompetentia, false).find("com_rune_01") !=
        std::string_view::npos);
}

TEST_CASE("Cercle de visée : l'opacité bat avec la rotation", "[render][ground_scope]") {
  const float quarter_turn = std::numbers::pi_v<float> / 2.0F;
  CHECK(ro::render::GroundScopeColor(kMeteorStorm, 0.0F)[3] == 128);
  CHECK(ro::render::GroundScopeColor(kMeteorStorm, quarter_turn)[3] == 168);
  // Un sinus négatif ne retranche rien.
  CHECK(ro::render::GroundScopeColor(kMeteorStorm, -quarter_turn)[3] == 128);
  // Competentia n'en garde que six dixièmes.
  CHECK(ro::render::GroundScopeColor(kCompetentia, quarter_turn)[3] == 100);
  // Un demi-radian par tour et par case de côté.
  CHECK(ro::render::GroundScopeAngle(5, 10) == Approx(1.0F));
}

TEST_CASE("Cercle de visée : une case par case du carré, autour de la case visée",
          "[render][ground_scope]") {
  const auto cells = ro::render::ComposeGroundScopeCells(5, 0.0F, {255, 255, 255, 128});
  REQUIRE(cells.size() == 25);
  CHECK(cells.front().cell_offset_x == -2);
  CHECK(cells.front().cell_offset_y == -2);
  CHECK(cells.back().cell_offset_x == 2);
  CHECK(cells.back().cell_offset_y == 2);
  // Sans rotation, l'image couvre tout le carré d'un coin à l'autre.
  CHECK(cells.front().texture_uv[0].x == Approx(0.0F));
  CHECK(cells.front().texture_uv[0].y == Approx(0.0F));
  CHECK(cells.back().texture_uv[3].x == Approx(1.0F));
  CHECK(cells.back().texture_uv[3].y == Approx(1.0F));
  CHECK(ro::render::ComposeGroundScopeCells(0, 0.0F, {255, 255, 255, 128}).empty());
}
