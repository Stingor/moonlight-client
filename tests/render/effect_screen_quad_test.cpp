#include "ro/render/effect_screen_quad.h"

#include <cmath>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using ro::render::ComposeScreenQuad;
using ro::render::EffectScreenQuad;
using ro::render::EffectScreenQuadRequest;
using ro::render::kEffectScreenTexelOffset;

namespace {

constexpr float kPointX = 100.0F;
constexpr float kPointY = 200.0F;
constexpr float kHalfWidth = 10.0F;
constexpr float kHalfHeight = 4.0F;

[[nodiscard]] EffectScreenQuadRequest Request() {
  EffectScreenQuadRequest request;
  request.screen_position = {kPointX, kPointY};
  request.half_width = kHalfWidth;
  request.half_height = kHalfHeight;
  return request;
}

}  // namespace

TEST_CASE("un quad d'ecran sans aire ne se soumet pas", "[render][effect][screen]") {
  EffectScreenQuadRequest request = Request();
  request.half_width = 0.0F;
  CHECK(ComposeScreenQuad(request).empty());
  request = Request();
  request.half_height = 0.0F;
  CHECK(ComposeScreenQuad(request).empty());
}

TEST_CASE("un angle nul laisse la figure d'aplomb", "[render][effect][screen]") {
  const std::vector<EffectScreenQuad> quads = ComposeScreenQuad(Request());
  REQUIRE(quads.size() == 1);
  const EffectScreenQuad& quad = quads.front();

  // 🔴 LA ROTATION EST L'USUELLE, lue dans les tables du client : à angle nul,
  // un point local (u, v) sort en (u, v). Les deux tables du dessinateur ont
  // leurs NOMS échangés, et les prendre au mot faisait tourner la figure d'un
  // quart de tour.
  const float d = kEffectScreenTexelOffset;
  // Coin zéro : local (−10, +4).
  CHECK(quad.corners[0].x == Approx(kPointX - kHalfWidth - d));
  CHECK(quad.corners[0].y == Approx(kPointY + kHalfHeight - d));
  // ⚠ ET LE COIN TROIS EST LE HAUT-DROIT, pas le bas-droit : les coins tournent
  // autour du quad. Local (+10, +4).
  CHECK(quad.corners[3].x == Approx(kPointX + kHalfWidth - d));
  CHECK(quad.corners[3].y == Approx(kPointY + kHalfHeight - d));
  // Le coin deux, lui, est le bas-droit. Local (+10, −4).
  CHECK(quad.corners[2].x == Approx(kPointX + kHalfWidth - d));
  CHECK(quad.corners[2].y == Approx(kPointY - kHalfHeight - d));
}

TEST_CASE("un quart de tour couche la figure", "[render][effect][screen]") {
  EffectScreenQuadRequest request = Request();
  request.turn_degrees = 90;
  const EffectScreenQuad quad = ComposeScreenQuad(request).front();

  // À quatre-vingt-dix degrés, (u, v) sort en (−v, u) : local (−10, +4) tombe en
  // (−4, −10).
  const float d = kEffectScreenTexelOffset;
  CHECK(quad.corners[0].x == Approx(kPointX - kHalfHeight - d).margin(1e-3));
  CHECK(quad.corners[0].y == Approx(kPointY - kHalfWidth - d).margin(1e-3));
  CHECK(quad.corners[3].x == Approx(kPointX - kHalfHeight - d).margin(1e-3));
  CHECK(quad.corners[3].y == Approx(kPointY + kHalfWidth - d).margin(1e-3));
}

TEST_CASE("un angle négatif se lit un degré à côté", "[render][effect][screen]") {
  // Moins quatre-vingt-dix se lit deux cent soixante et onze : la figure
  // s'arrête un degré avant le quart de tour.
  EffectScreenQuadRequest request = Request();
  request.turn_degrees = -90;
  const EffectScreenQuad quad = ComposeScreenQuad(request).front();
  const EffectScreenQuadRequest straight = [] {
    EffectScreenQuadRequest d = Request();
    d.turn_degrees = 271;
    return d;
  }();
  const EffectScreenQuad expected = ComposeScreenQuad(straight).front();
  for (std::size_t corner = 0; corner < 4; ++corner) {
    CHECK(quad.corners[corner].x == Approx(expected.corners[corner].x));
    CHECK(quad.corners[corner].y == Approx(expected.corners[corner].y));
  }
}

TEST_CASE("l'ancrage deplace l'origine, pas la taille", "[render][effect][screen]") {
  const EffectScreenQuad centre = ComposeScreenQuad(Request()).front();
  EffectScreenQuadRequest request = Request();
  request.hangs_below_anchor = true;
  const EffectScreenQuad pendant = ComposeScreenQuad(request).front();

  // À angle nul, l'étendue de la hauteur se mesure sur y. Elle vaut deux
  // demi-hauteurs dans les DEUX cas.
  const auto etendue = [](const EffectScreenQuad& quad) {
    return std::abs(quad.corners[0].y - quad.corners[1].y);
  };
  CHECK(etendue(centre) == Approx(2.0F * kHalfHeight));
  CHECK(etendue(pendant) == Approx(2.0F * kHalfHeight));

  // ⚠ MAIS LE QUAD PENDANT EST TOUT ENTIER D'UN CÔTÉ : son bord « haut » tombe
  // SUR le point, quand celui du centré s'en écarte d'une demi-hauteur. Prendre
  // le drapeau pour une échelle poserait ces effets à mi-hauteur.
  const float d = kEffectScreenTexelOffset;
  CHECK(pendant.corners[0].y == Approx(kPointY - d));
  CHECK(centre.corners[0].y == Approx(kPointY + kHalfHeight - d));
}

TEST_CASE("les coordonnees d'image couvrent l'image entiere", "[render][effect][screen]") {
  const EffectScreenQuad quad = ComposeScreenQuad(Request()).front();
  CHECK(quad.texture_uv[0] == glm::vec2{0.0F, 1.0F});
  CHECK(quad.texture_uv[1] == glm::vec2{0.0F, 0.0F});
  CHECK(quad.texture_uv[2] == glm::vec2{1.0F, 0.0F});
  CHECK(quad.texture_uv[3] == glm::vec2{1.0F, 1.0F});
}

TEST_CASE("les quatre coins tournent autour du quad", "[render][effect][screen]") {
  // ⚠ LE RENDERER DÉCOUPE UN QUAD EN DEUX TRIANGLES qui partagent la diagonale
  // du premier coin au TROISIÈME. Des coins rangés en paires — les deux gauches
  // puis les deux droits — y feraient un papillon au lieu d'un rectangle.
  //
  // Le test le dit en aire : celle du polygone parcouru dans l'ordre des coins
  // vaut la surface entière. Un papillon, lui, rend une aire NULLE, ses deux
  // moitiés s'annulant.
  EffectScreenQuadRequest request = Request();
  request.turn_degrees = 90;
  const EffectScreenQuad quad = ComposeScreenQuad(request).front();
  float area = 0.0F;
  for (std::size_t corner = 0; corner < 4; ++corner) {
    const glm::vec2 here = quad.corners[corner];
    const glm::vec2 next = quad.corners[(corner + 1) % 4];
    area += here.x * next.y - next.x * here.y;
  }
  CHECK(std::abs(area) / 2.0F == Approx(4.0F * kHalfWidth * kHalfHeight));
}

TEST_CASE("un point du monde tombe sur l'ecran, ordonnee retournee", "[render][effect][screen]") {
  ro::render::Camera camera;
  camera.look_at = {0.0F, 0.0F, 0.0F};
  camera.distance = 100.0F;
  camera.pitch_degrees = 45.0F;
  camera.yaw_degrees = 0.0F;
  camera.fov_degrees = 45.0F;
  constexpr float kWidth = 640.0F;
  constexpr float kHeight = 480.0F;

  // Le point VISE tombe au centre de la fenetre.
  const std::optional<glm::vec2> centre =
      ro::render::EffectScreenPositionOf(camera, camera.look_at, kWidth, kHeight);
  REQUIRE(centre.has_value());
  CHECK(centre->x == Approx(kWidth / 2.0F));
  CHECK(centre->y == Approx(kHeight / 2.0F));

  // ⚠ ET L'ORDONNEE SE RETOURNE : un point PLUS HAUT dans le monde tombe PLUS
  // HAUT a l'ecran, c'est-a-dire a une ordonnee PLUS PETITE. C'est le piege du
  // repere, et le seul endroit ou il se verifie.
  const std::optional<glm::vec2> en_l_air = ro::render::EffectScreenPositionOf(
      camera, camera.look_at + glm::vec3{0.0F, 20.0F, 0.0F}, kWidth, kHeight);
  REQUIRE(en_l_air.has_value());
  CHECK(en_l_air->y < centre->y);
  CHECK(en_l_air->x == Approx(centre->x));
}

TEST_CASE("un porteur derriere la camera n'a pas de position d'ecran", "[render][effect][screen]") {
  ro::render::Camera camera;
  camera.look_at = {0.0F, 0.0F, 0.0F};
  camera.distance = 100.0F;
  camera.pitch_degrees = 0.0F;
  camera.yaw_degrees = 0.0F;
  camera.fov_degrees = 45.0F;
  // ⚠ DERRIERE LA CAMERA, la coordonnee homogene est negative : diviser quand
  // meme enverrait le point a l'OPPOSE de l'ecran, ou il se dessinerait
  // retourne. Dire "nulle part" est la seule reponse juste.
  const glm::vec3 behind = camera.Position() + (camera.Position() - camera.look_at);
  CHECK_FALSE(ro::render::EffectScreenPositionOf(camera, behind, 640.0F, 480.0F).has_value());
  // Et une fenetre sans surface n'a pas de position non plus.
  CHECK_FALSE(ro::render::EffectScreenPositionOf(camera, camera.look_at, 0.0F, 480.0F).has_value());
}

TEST_CASE("le quad porte sa teinte, son melange et son image", "[render][effect][screen]") {
  EffectScreenQuadRequest request = Request();
  request.tint = {10, 20, 30, 40};
  request.blend_mode = 5;
  request.texture = 3;
  const EffectScreenQuad quad = ComposeScreenQuad(request).front();
  CHECK(quad.tint == std::array<std::uint8_t, 4>{10, 20, 30, 40});
  CHECK(quad.blend_mode == 5);
  CHECK(quad.texture == 3);
}
