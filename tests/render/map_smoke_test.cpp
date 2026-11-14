#include "ro/render/map_smoke.h"

#include <cmath>
#include <cstdint>
#include <set>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/sprite_billboard.h"

using Catch::Approx;
using ro::render::ChimneySmokeLook;
using ro::render::EffectWorldQuad;
using ro::render::MapChimney;
using ro::render::MapEffectFrame;

namespace {

constexpr glm::vec3 kChimneyPlace{0.0F, -90.0F, 0.0F};
constexpr glm::vec3 kPlayerNear{0.0F, 0.0F, 0.0F};
constexpr glm::vec3 kPlayerFar{1000.0F, 0.0F, 0.0F};
constexpr MapEffectFrame kFrame{200, 200};
// Le premier pas où les bouffées renaissent : leur âge atteint cinquante.
constexpr int kFirstBirthTick = 51;

// L'image du sprite des cheminées : un calque neutre, 64 × 64.
ChimneySmokeLook Look() {
  ChimneySmokeLook look;
  look.image.width = 64;
  look.image.height = 64;
  look.image.pixels.assign(static_cast<std::size_t>(64 * 64 * 4), 255);
  return look;
}

ro::render::BillboardBasis Basis() {
  ro::render::Camera camera;
  camera.pitch_degrees = 50.0F;
  camera.look_at = kFrame.ToWorld(kChimneyPlace);
  return ro::render::MakeBillboardBasis(camera);
}

std::vector<EffectWorldQuad> QuadsOf(const MapChimney& chimney) {
  std::vector<EffectWorldQuad> quads;
  chimney.AppendQuads(kFrame, Basis(), Look(), quads);
  return quads;
}

void Tick(MapChimney& chimney, std::uint32_t& draws, int ticks, glm::vec3 player = kPlayerNear) {
  for (int tick = 0; tick < ticks; ++tick) {
    chimney.Tick(player, draws);
  }
}

// L'angle, sur l'écran, du bord supérieur d'un quad : de son premier coin au
// second, mesuré de la droite de la caméra vers son bas.
float TopEdgeDegrees(const EffectWorldQuad& quad, const ro::render::BillboardBasis& basis) {
  const glm::vec3 edge = quad.corners[1] - quad.corners[0];
  return std::atan2(glm::dot(edge, -basis.up), glm::dot(edge, basis.right)) * 180.0F / 3.14159265F;
}

}  // namespace

TEST_CASE("une cheminée ne montre rien avant la renaissance du pas cinquante") {
  std::uint32_t draws = 1;
  MapChimney chimney(kChimneyPlace);
  Tick(chimney, draws, kFirstBirthTick - 1);
  CHECK(QuadsOf(chimney).empty());
  Tick(chimney, draws, 1);
  const std::size_t shown = QuadsOf(chimney).size();
  CHECK(shown >= 1);
  CHECK(shown <= ro::render::kChimneyPuffs);
}

TEST_CASE("le nombre de bouffées peintes est tiré entre une et quatre") {
  std::uint32_t draws = 1;
  MapChimney chimney(kChimneyPlace);
  std::set<std::size_t> seen;
  Tick(chimney, draws, kFirstBirthTick);
  for (int cycle = 0; cycle < 40; ++cycle) {
    const std::size_t shown = QuadsOf(chimney).size();
    CHECK(shown >= 1);
    CHECK(shown <= ro::render::kChimneyPuffs);
    seen.insert(shown);
    Tick(chimney, draws, 50);
  }
  CHECK(seen.size() >= 2);
}

TEST_CASE("une bouffée est large de soixante-quatre pixels à un septième, fois 1,5") {
  std::uint32_t draws = 1;
  MapChimney chimney(kChimneyPlace);
  Tick(chimney, draws, kFirstBirthTick);
  const std::vector<EffectWorldQuad> quads = QuadsOf(chimney);
  REQUIRE_FALSE(quads.empty());
  const float width = glm::length(quads.front().corners[1] - quads.front().corners[0]);
  CHECK(width == Approx(64.0F / 7.0F * 1.5F));
}

TEST_CASE("l'image est centrée sur son ancre, neuf unités au-dessus de l'objet") {
  std::uint32_t draws = 1;
  MapChimney chimney(kChimneyPlace);
  Tick(chimney, draws, kFirstBirthTick);
  const std::vector<EffectWorldQuad> quads = QuadsOf(chimney);
  REQUIRE_FALSE(quads.empty());
  const ro::render::BillboardBasis basis = Basis();
  // Au pas de la renaissance, l'écart est nul : l'ancre est le CENTRE de
  // l'image, neuf unités au-dessus de l'objet. Le chargeur du client retranche
  // la demi-image du décalage du calque, que le dessin rajoute.
  const glm::vec3 anchor = kFrame.ToWorld(kChimneyPlace + glm::vec3{0.0F, -9.0F, 0.0F});
  const glm::vec3 centre = (quads.front().corners[0] + quads.front().corners[1] +
                            quads.front().corners[2] + quads.front().corners[3]) *
                           0.25F;
  CHECK(glm::length(centre - (anchor + basis.forward)) <= 1e-3F);
  // Et l'image est à l'endroit : son haut au-dessus de son bas.
  float image_top = 0.0F;
  float image_bottom = 0.0F;
  for (std::size_t corner = 0; corner < 4; ++corner) {
    if (quads.front().texture_uv[corner] == glm::vec2{0.0F, 0.0F}) {
      image_top = quads.front().corners[corner].y;
    }
    if (quads.front().texture_uv[corner] == glm::vec2{0.0F, 1.0F}) {
      image_bottom = quads.front().corners[corner].y;
    }
  }
  CHECK(image_top > image_bottom);
}

TEST_CASE("le roulis tourne l'image sur place, il ne la promène pas") {
  std::uint32_t draws = 1;
  MapChimney chimney(kChimneyPlace);
  Tick(chimney, draws, kFirstBirthTick);
  // Une renaissance sur deux cents pas : le roulis a tourné de 150 degrés sans
  // jamais repartir de zéro. Si l'image n'était pas centrée sur son ancre, son
  // centre décrirait un cercle large comme elle.
  const ro::render::BillboardBasis basis = Basis();
  for (int cycle = 0; cycle < 4; ++cycle) {
    const std::vector<EffectWorldQuad> quads = QuadsOf(chimney);
    REQUIRE_FALSE(quads.empty());
    const glm::vec3 anchor = kFrame.ToWorld(kChimneyPlace + glm::vec3{0.0F, -9.0F, 0.0F});
    const glm::vec3 centre = (quads.front().corners[0] + quads.front().corners[1] +
                              quads.front().corners[2] + quads.front().corners[3]) *
                             0.25F;
    CHECK(glm::length(centre - (anchor + basis.forward)) <= 1e-3F);
    Tick(chimney, draws, 50);
  }
}

TEST_CASE("une bouffée monte, tourne de trois quarts de degré, s'efface passé trente-quatre") {
  std::uint32_t draws = 1;
  MapChimney chimney(kChimneyPlace);
  Tick(chimney, draws, kFirstBirthTick);
  const ro::render::BillboardBasis basis = Basis();
  const EffectWorldQuad born = QuadsOf(chimney).front();
  CHECK(born.tint[3] == 255);

  Tick(chimney, draws, 1);
  const EffectWorldQuad next = QuadsOf(chimney).front();
  CHECK(TopEdgeDegrees(next, basis) - TopEdgeDegrees(born, basis) == Approx(0.75F).margin(1e-2));

  // Dix pas de 0,3 à 0,5 unité, penchés de 75 à 104 degrés : elle monte.
  Tick(chimney, draws, 9);
  const EffectWorldQuad risen = QuadsOf(chimney).front();
  const float climb = risen.corners[0].y - born.corners[0].y;
  CHECK(climb > 10.0F * 0.3F * 0.96F - 0.5F);
  CHECK(climb < 10.0F * 0.5F + 0.5F);
  CHECK(risen.tint[3] == 255);

  // L'âge trente-quatre garde l'alpha plein, le trente-cinquième retire quinze.
  Tick(chimney, draws, 24);
  CHECK(QuadsOf(chimney).front().tint[3] == 255);
  Tick(chimney, draws, 1);
  CHECK(QuadsOf(chimney).front().tint[3] == 240);
}

TEST_CASE("loin du joueur, une cheminée se fige et ne se peint pas") {
  std::uint32_t draws = 1;
  MapChimney chimney(kChimneyPlace);
  Tick(chimney, draws, 200, kPlayerFar);
  CHECK_FALSE(chimney.drawn());
  CHECK(QuadsOf(chimney).empty());
  // Figée : ses bouffées n'ont pas vieilli, la première renaissance est encore
  // à cinquante pas.
  Tick(chimney, draws, kFirstBirthTick - 1);
  CHECK(QuadsOf(chimney).empty());
  Tick(chimney, draws, 1);
  CHECK_FALSE(QuadsOf(chimney).empty());
}
