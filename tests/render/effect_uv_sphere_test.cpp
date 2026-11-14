#include "ro/render/effect_uv_sphere.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"

// LA SPHÈRE UV DU SOUS-TYPE 14 : le dôme de feu de Magnum Break.
using Catch::Approx;
using ro::render::ComposeEffectNodeUvSphere;
using ro::render::EffectNode;
using ro::render::EffectWorldQuad;

namespace {

EffectNode SphereOf(float radius, float step_degrees) {
  EffectNode node;
  node.ring_radius.half_size = radius;
  node.ring_step_degrees = step_degrees;
  node.alpha = 200.0F;
  return node;
}

float DistanceTo(const glm::vec3& point, const glm::vec3& centre) {
  return std::sqrt((point.x - centre.x) * (point.x - centre.x) +
                   (point.y - centre.y) * (point.y - centre.y) +
                   (point.z - centre.z) * (point.z - centre.z));
}

}  // namespace

TEST_CASE("Sphère UV : au pas du constructeur, cinq rangs de dix colonnes",
          "[render][effectnode]") {
  // Trente-six degrés : la latitude va de moins quatre-vingt-dix à
  // cinquante-quatre, la longitude de zéro à trois cent vingt-quatre — les
  // deux boucles s'arrêtent à « étendue moins pas », et ferment exactement.
  const glm::vec3 centre{10.0F, 2.0F, -4.0F};
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectNodeUvSphere(SphereOf(20.0F, 36.0F), centre, 0.0F);
  REQUIRE(quads.size() == 50);
  float highest = centre.y;
  float lowest = centre.y;
  for (const EffectWorldQuad& quad : quads) {
    for (const glm::vec3& corner : quad.corners) {
      // Tous les coins sont SUR la sphère.
      CHECK(DistanceTo(corner, centre) == Approx(20.0F).margin(0.01F));
      highest = std::max(highest, corner.y);
      lowest = std::min(lowest, corner.y);
    }
    CHECK(quad.tint[3] == 200);
  }
  // Pôle à pôle : la sphère est ENTIÈRE, et c'est le sol qui en mangera la
  // moitié basse.
  CHECK(highest == Approx(centre.y + 20.0F).margin(0.01F));
  CHECK(lowest == Approx(centre.y - 20.0F).margin(0.01F));
}

TEST_CASE("Sphère UV : l'image se déroule une fois autour, du pôle bas au pôle haut",
          "[render][effectnode]") {
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectNodeUvSphere(SphereOf(5.0F, 36.0F), glm::vec3{0.0F}, 0.0F);
  REQUIRE(quads.size() == 50);
  // Première colonne du premier rang : un dixième de large, un cinquième de
  // haut, comme le client l'écrit — `1 / (360 / pas)` et `1 / (180 / pas)`.
  CHECK(quads.front().texture_uv[0] == glm::vec2{0.0F, 0.0F});
  CHECK(quads.front().texture_uv[1].x == Approx(0.1F));
  CHECK(quads.front().texture_uv[2].y == Approx(0.2F));
  // Dernier quad : le coin haut-droit de l'image.
  CHECK(quads.back().texture_uv[2].x == Approx(1.0F));
  CHECK(quads.back().texture_uv[2].y == Approx(1.0F));
  // Le premier rang est en BAS : sa latitude part de moins quatre-vingt-dix.
  CHECK(quads.front().corners[0].y == Approx(-5.0F).margin(0.01F));
}

TEST_CASE("Sphère UV : le lacet la tourne sur son axe", "[render][effectnode]") {
  const std::vector<EffectWorldQuad> right =
      ComposeEffectNodeUvSphere(SphereOf(5.0F, 36.0F), glm::vec3{0.0F}, 0.0F);
  const std::vector<EffectWorldQuad> tournee =
      ComposeEffectNodeUvSphere(SphereOf(5.0F, 36.0F), glm::vec3{0.0F}, 36.0F);
  REQUIRE(right.size() == tournee.size());
  // Un pas de lacet décale la sphère d'UNE colonne : la colonne k tournée
  // occupe la place de la colonne k + 1 droite, sur le même rang.
  for (std::size_t rang = 0; rang < 5; ++rang) {
    for (std::size_t column = 0; column + 1 < 10; ++column) {
      const glm::vec3& a = tournee[rang * 10 + column].corners[0];
      const glm::vec3& b = right[rang * 10 + column + 1].corners[0];
      CHECK(DistanceTo(a, b) == Approx(0.0F).margin(0.01F));
    }
  }
}

TEST_CASE("Sphère UV : sans pas rien, à rayon nul des quads sans aire", "[render][effectnode]") {
  // Le client bouclerait sans fin sur un pas nul. Un rayon nul, lui, est le
  // point d'où le dôme part : le client dessine ses cinquante quads aplatis,
  // et le tic suivant les ouvre.
  CHECK(ComposeEffectNodeUvSphere(SphereOf(5.0F, 0.0F), glm::vec3{0.0F}, 0.0F).empty());
  const std::vector<EffectWorldQuad> point =
      ComposeEffectNodeUvSphere(SphereOf(0.0F, 36.0F), glm::vec3{1.0F, 2.0F, 3.0F}, 0.0F);
  REQUIRE(point.size() == 50);
  for (const EffectWorldQuad& quad : point) {
    for (const glm::vec3& corner : quad.corners) {
      CHECK(DistanceTo(corner, glm::vec3{1.0F, 2.0F, 3.0F}) == Approx(0.0F).margin(0.001F));
    }
  }
}
