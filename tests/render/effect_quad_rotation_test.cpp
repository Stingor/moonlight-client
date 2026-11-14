#include "ro/render/effect_quad_rotation.h"

#include <array>
#include <cmath>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_particle_figures.h"
#include "ro/render/effect_particle_quad.h"

using ro::render::RotateEffectQuadCorners;
using ro::render::RotateEffectQuadPoint;

namespace {

constexpr std::int16_t kQuarterTurn = 90;

// Le carré que le client bâtit à plat avant de le coucher : deux unités de
// côté, centré sur l'origine, dans le plan de l'abscisse et de la profondeur.
std::array<glm::vec3, 4> FlatQuad() {
  return {glm::vec3{-1.0F, 0.0F, -1.0F}, glm::vec3{1.0F, 0.0F, -1.0F}, glm::vec3{1.0F, 0.0F, 1.0F},
          glm::vec3{-1.0F, 0.0F, 1.0F}};
}

}  // namespace

TEST_CASE("Rotation de quad : deux angles nuls ne bougent rien", "[render][quadrotation]") {
  const glm::vec3 point = RotateEffectQuadPoint({3.0F, 4.0F, 5.0F}, 0, 0);
  CHECK(point.x == Catch::Approx(3.0F));
  CHECK(point.y == Catch::Approx(4.0F));
  CHECK(point.z == Catch::Approx(5.0F));
}

TEST_CASE("Rotation de quad : L'INCLINAISON S'APPLIQUE EN PREMIER", "[render][quadrotation]") {
  // Le seul piège de cette fonction, et il ne se voit pas : les deux rotations
  // ne commutent pas, et le client incline AVANT de tourner alors que sa
  // signature reçoit le tour d'abord. Prendre l'ordre de la signature donne une
  // figure penchée du mauvais côté — et une figure penchée reste plausible.
  //
  // Un quart de tour sur chaque angle sépare les deux ordres complètement :
  // l'abscisse part en profondeur par le bon ordre, et en hauteur par l'autre.
  const glm::vec3 point = RotateEffectQuadPoint({1.0F, 0.0F, 0.0F}, kQuarterTurn, kQuarterTurn);
  CHECK(point.x == Catch::Approx(0.0F).margin(1e-6));
  CHECK(point.y == Catch::Approx(0.0F).margin(1e-6));
  CHECK(point.z == Catch::Approx(1.0F));
}

TEST_CASE("Rotation de quad : l'inclinaison mêle la HAUTEUR et la profondeur",
          "[render][quadrotation]") {
  // C'est elle qui couche un quad bâti à plat. Si elle touchait l'abscisse, le
  // quad tournerait sur lui-même au lieu de se redresser.
  const glm::vec3 point = RotateEffectQuadPoint({2.0F, 0.0F, 1.0F}, 0, kQuarterTurn);
  CHECK(point.x == Catch::Approx(2.0F));
  CHECK(point.y == Catch::Approx(1.0F));
  CHECK(point.z == Catch::Approx(0.0F).margin(1e-6));
}

TEST_CASE("Rotation de quad : le tour mêle l'ABSCISSE et la profondeur", "[render][quadrotation]") {
  const glm::vec3 point = RotateEffectQuadPoint({1.0F, 7.0F, 0.0F}, kQuarterTurn, 0);
  CHECK(point.x == Catch::Approx(0.0F).margin(1e-6));
  CHECK(point.y == Catch::Approx(7.0F));
  CHECK(point.z == Catch::Approx(1.0F));
}

TEST_CASE("Rotation de quad : un angle NÉGATIF tourne, il ne casse pas", "[render][quadrotation]") {
  // Le client indexe une table de trois cent soixante entrées et ses appelants
  // ramènent l'angle eux-mêmes. Un angle négatif y lirait avant le début de la
  // table ; ici il vaut son complément, et moins quatre-vingt-dix doit défaire
  // quatre-vingt-dix.
  const glm::vec3 rotated = RotateEffectQuadPoint({1.0F, 0.0F, 0.0F}, kQuarterTurn, 0);
  const glm::vec3 revenu = RotateEffectQuadPoint(rotated, -kQuarterTurn, 0);
  CHECK(revenu.x == Catch::Approx(1.0F));
  CHECK(revenu.z == Catch::Approx(0.0F).margin(1e-6));
}

TEST_CASE("Rotation de quad : un TOUR ENTIER revient au même point", "[render][quadrotation]") {
  const glm::vec3 point = RotateEffectQuadPoint({3.0F, 4.0F, 5.0F}, 360, 360);
  CHECK(point.x == Catch::Approx(3.0F));
  CHECK(point.y == Catch::Approx(4.0F));
  CHECK(point.z == Catch::Approx(5.0F));
}

TEST_CASE("Rotation de quad : les LONGUEURS se conservent", "[render][quadrotation]") {
  // Une rotation ne change pas la taille de la figure. Une erreur de signe dans
  // l'une des deux moitiés de la formule la ferait grandir ou rétrécir selon
  // l'angle, et cela passerait pour un effet.
  std::array<glm::vec3, 4> corners = FlatQuad();
  const float before = glm::length(corners[2] - corners[0]);
  RotateEffectQuadCorners(corners, 37, 61);
  CHECK(glm::length(corners[2] - corners[0]) == Catch::Approx(before));
  for (const glm::vec3& corner : corners) {
    CHECK(glm::length(corner) == Catch::Approx(std::sqrt(2.0F)));
  }
}

TEST_CASE("Rotation de quad : LES QUATRE COINS subissent la même chose", "[render][quadrotation]") {
  // Le client applique chaque rotation aux quatre coins avant de passer à la
  // suivante ; nous les traitons un par un. Les deux donnent le même résultat
  // parce que la transformation est la même pour tous, et ce test le tient.
  std::array<glm::vec3, 4> corners = FlatQuad();
  const std::array<glm::vec3, 4> start = corners;
  RotateEffectQuadCorners(corners, 25, 40);

  for (std::size_t rang = 0; rang < corners.size(); ++rang) {
    const glm::vec3 single = RotateEffectQuadPoint(start[rang], 25, 40);
    CHECK(corners[rang].x == Catch::Approx(single.x));
    CHECK(corners[rang].y == Catch::Approx(single.y));
    CHECK(corners[rang].z == Catch::Approx(single.z));
  }
}

TEST_CASE("Rotation de quad : un quad COUCHÉ se redresse", "[render][quadrotation]") {
  // La raison d'être de l'inclinaison : un carré bâti dans le plan du sol se
  // dresse face au regard. Sans elle, toute la voie A resterait à plat et se
  // verrait par la tranche.
  std::array<glm::vec3, 4> corners = FlatQuad();
  RotateEffectQuadCorners(corners, 0, kQuarterTurn);

  for (const glm::vec3& corner : corners) {
    CHECK(corner.z == Catch::Approx(0.0F).margin(1e-6));
  }
  CHECK(corners[0].y == Catch::Approx(-1.0F));
  CHECK(corners[2].y == Catch::Approx(1.0F));
}

namespace {

// LES CAMÉRAS D'ÉPREUVE : des lacets sur tout le tour, négatifs compris, et des
// élévations de part et d'autre de celle du jeu.
struct TestView {
  float yaw_degrees = 0.0F;
  float pitch_degrees = 0.0F;
};
constexpr std::array<TestView, 7> kTestViews = {{
    {0.0F, 50.0F},
    {37.0F, 45.0F},
    {90.0F, 30.0F},
    {200.0F, 60.0F},
    {-75.0F, 50.0F},
    {315.0F, 20.0F},
    {123.0F, 80.0F},
}};

constexpr float kParallelTolerance = 1e-4F;

[[nodiscard]] ro::render::Camera CameraOf(const TestView& view) {
  ro::render::Camera camera;
  camera.yaw_degrees = view.yaw_degrees;
  camera.pitch_degrees = view.pitch_degrees;
  return camera;
}

// LA MATRICE d'une application linéaire, colonne par colonne.
template <typename Map>
[[nodiscard]] glm::mat3 MatrixOf(Map map) {
  return glm::mat3{map(glm::vec3{1.0F, 0.0F, 0.0F}), map(glm::vec3{0.0F, 1.0F, 0.0F}),
                   map(glm::vec3{0.0F, 0.0F, 1.0F})};
}

}  // namespace

TEST_CASE("Rotation de quad : les angles de panneau, puis S, font face à l'œil sans miroir",
          "[render][quadrotation]") {
  // ⚠ LES ANGLES SONT CEUX DE LA CAMÉRA DU CLIENT, et le carré tourné est donc un
  // carré DU CLIENT : il revient au nôtre par `S = diag(1, −1, −1)` entier. Sa
  // normale est alors celle de notre œil, et l'application entière est une
  // rotation — déterminant un.
  for (const TestView& view : kTestViews) {
    INFO("lacet " << view.yaw_degrees << ", élévation " << view.pitch_degrees);
    const ro::render::Camera camera = CameraOf(view);
    const std::int16_t turn = ro::render::EffectBillboardTurnDegrees(camera);
    const std::int16_t tilt = ro::render::EffectBillboardTiltDegrees(camera);

    std::array<glm::vec3, 4> corners = FlatQuad();
    RotateEffectQuadCorners(corners, turn, tilt);
    for (glm::vec3& corner : corners) {
      corner = ro::render::EffectPointFromClient(corner);
    }
    const glm::vec3 normal =
        glm::normalize(glm::cross(corners[1] - corners[0], corners[3] - corners[0]));
    const glm::vec3 eye = glm::normalize(camera.Position() - camera.look_at);
    CHECK(std::abs(std::abs(glm::dot(normal, eye)) - 1.0F) < kParallelTolerance);

    const glm::mat3 proper = MatrixOf([&](glm::vec3 point) {
      return ro::render::EffectPointFromClient(RotateEffectQuadPoint(point, turn, tilt));
    });
    CHECK(glm::determinant(proper) == Catch::Approx(1.0F).margin(kParallelTolerance));
  }
}

TEST_CASE("Rotation de quad : l'ancienne combinaison était un miroir, plat seulement en apparence",
          "[render][quadrotation]") {
  // Les compléments de NOTRE caméra — `360 − lacet`, `450 − élévation` — suivis
  // du seul retournement de l'ordonnée : une application de déterminant MOINS
  // un. Sur le carré à plat elle coïncide avec la bonne, et c'est ce qui l'a
  // cachée ; elle diverge dès qu'un terme sort du plan — une hauteur posée
  // avant la rotation, un décalage ajouté après dans le repère du client.
  constexpr std::int32_t kTurnComplement = 360;
  constexpr std::int32_t kTiltComplement = 450;
  for (const TestView& view : kTestViews) {
    INFO("lacet " << view.yaw_degrees << ", élévation " << view.pitch_degrees);
    const ro::render::Camera camera = CameraOf(view);
    const auto old_turn =
        static_cast<std::int16_t>(kTurnComplement - static_cast<std::int32_t>(view.yaw_degrees));
    const auto old_tilt =
        static_cast<std::int16_t>(kTiltComplement - static_cast<std::int32_t>(view.pitch_degrees));
    const auto mirrored = [&](glm::vec3 point) {
      const glm::vec3 turned = RotateEffectQuadPoint(point, old_turn, old_tilt);
      return glm::vec3{turned.x, -turned.y, turned.z};
    };
    CHECK(glm::determinant(MatrixOf(mirrored)) == Catch::Approx(-1.0F).margin(kParallelTolerance));

    const std::int16_t turn = ro::render::EffectBillboardTurnDegrees(camera);
    const std::int16_t tilt = ro::render::EffectBillboardTiltDegrees(camera);
    const auto proper = [&](glm::vec3 point) {
      return ro::render::EffectPointFromClient(RotateEffectQuadPoint(point, turn, tilt));
    };
    const glm::vec3 flat{2.0F, 0.0F, -3.0F};
    CHECK(glm::length(mirrored(flat) - proper(flat)) < kParallelTolerance);
    const glm::vec3 raised{0.0F, 1.0F, 0.0F};
    CHECK(glm::length(mirrored(raised) - proper(raised)) > 1.0F);
  }
}
