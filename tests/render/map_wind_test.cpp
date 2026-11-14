#include "ro/render/map_wind.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/map_effect_math.h"

using Catch::Approx;
using ro::render::EffectWorldQuad;
using ro::render::MapEffectFrame;
using ro::render::MapWind;
using ro::render::MapWindSetting;

namespace {

// Un anneau à plat, sans bascule, qui respire jusqu'à un seul cran : sa phase
// de départ ne peut valoir que zéro.
MapWindSetting Flat() {
  MapWindSetting setting;
  setting.rings = 1;
  setting.alpha_peak = 1;
  setting.radius = 70.0F;
  setting.thickness = 50.0F;
  setting.height = 0.0F;
  setting.speed = 0.5F;
  setting.full_angle = 360;
  return setting;
}

constexpr glm::vec3 kPlace{0.0F, 0.0F, 0.0F};
constexpr glm::vec3 kPlayerNear{0.0F, 0.0F, 0.0F};
constexpr glm::vec3 kPlayerFar{1000.0F, 0.0F, 0.0F};
constexpr MapEffectFrame kFrame{200, 200};

std::vector<EffectWorldQuad> QuadsOf(const MapWind& wind) {
  std::vector<EffectWorldQuad> quads;
  wind.AppendQuads(kFrame, quads);
  return quads;
}

}  // namespace

TEST_CASE("vingt et un segments par anneau, le premier ne se peint pas") {
  MapWindSetting setting = Flat();
  setting.rings = 2;
  std::uint32_t draws = 1;
  MapWind wind(kPlace, setting, draws);
  CHECK(wind.segment_count() == 42);
  CHECK(QuadsOf(wind).empty());  // Jamais mis à jour : rien à peindre.
  wind.Tick(kPlayerNear);
  CHECK(QuadsOf(wind).size() == 40);
}

TEST_CASE("le ruban relie chaque segment au précédent") {
  std::uint32_t draws = 1;
  MapWind wind(kPlace, Flat(), draws);
  wind.Tick(kPlayerNear);
  const std::vector<EffectWorldQuad> quads = QuadsOf(wind);
  REQUIRE(quads.size() == 20);
  for (std::size_t index = 1; index < quads.size(); ++index) {
    // En tour : bord extérieur précédent, extérieur, intérieur, intérieur
    // précédent.
    CHECK(quads[index].corners[0] == quads[index - 1].corners[1]);
    CHECK(quads[index].corners[3] == quads[index - 1].corners[2]);
  }
  CHECK(quads.front().texture_uv[0].x == Approx(0.0F));
  CHECK(quads.back().texture_uv[1].x == Approx(20.0F / 21.0F));
}

TEST_CASE("le bord extérieur s'écarte de l'épaisseur, en rayon et vers le bas") {
  std::uint32_t draws = 1;
  MapWind wind(kPlace, Flat(), draws);
  wind.Tick(kPlayerNear);
  const std::vector<EffectWorldQuad> quads = QuadsOf(wind);
  REQUIRE_FALSE(quads.empty());
  const glm::vec3 center = kFrame.ToWorld(kPlace);
  const glm::vec3 outer = quads.front().corners[1];
  const glm::vec3 inner = quads.front().corners[2];
  CHECK(glm::length(glm::vec2{inner.x - center.x, inner.z - center.z}) == Approx(70.0F));
  CHECK(glm::length(glm::vec2{outer.x - center.x, outer.z - center.z}) == Approx(120.0F));
  CHECK(outer.y == Approx(inner.y - 50.0F));
}

TEST_CASE("l'alpha d'un anneau respire : un cran de plus, puis un de moins") {
  // ⚠ UN PIC À TROIS, PAS À UN : à un, « descendre d'un cran » et « tomber à
  // zéro » donnent la même suite, et l'essai ne distinguait rien.
  MapWindSetting setting = Flat();
  setting.alpha_peak = 3;
  std::uint32_t draws = 1;
  MapWind wind(kPlace, setting, draws);
  // La phase de départ est tirée : on monte jusqu'au pic, en trois pas au plus.
  std::uint8_t alpha = 0;
  for (int step = 0; step < 4 && alpha != 3; ++step) {
    wind.Tick(kPlayerNear);
    alpha = QuadsOf(wind).front().tint[3];
  }
  REQUIRE(alpha == 3);
  wind.Tick(kPlayerNear);
  CHECK(QuadsOf(wind).front().tint[3] == 2);
  wind.Tick(kPlayerNear);
  CHECK(QuadsOf(wind).front().tint[3] == 1);
  wind.Tick(kPlayerNear);
  CHECK(QuadsOf(wind).front().tint[3] == 0);
  wind.Tick(kPlayerNear);
  CHECK(QuadsOf(wind).front().tint[3] == 1);
}

TEST_CASE("la place de la table remplace celle de l'objet") {
  MapWindSetting setting = Flat();
  setting.position = glm::vec3{5.0F, -6.0F, 7.0F};
  std::uint32_t draws = 1;
  const MapWind placed(kPlace, setting, draws);
  CHECK(placed.native_position() == glm::vec3{5.0F, -6.0F, 7.0F});
  const MapWind unplaced(kPlace, Flat(), draws);
  CHECK(unplaced.native_position() == kPlace);
}

TEST_CASE("loin du joueur, l'anneau ne se peint pas") {
  std::uint32_t draws = 1;
  MapWind wind(kPlace, Flat(), draws);
  wind.Tick(kPlayerFar);
  CHECK_FALSE(wind.drawn());
  CHECK(QuadsOf(wind).empty());
}
