#include "ro/render/sprite_billboard.h"

#include <cmath>

#include <catch2/catch_test_macros.hpp>

using ro::render::BillboardCorners;
using ro::render::BillboardDepthCorners;
using ro::render::Camera;
using ro::render::kSpriteGroundClearance;
using ro::render::kSpritePixelsPerWorldUnit;
using ro::render::MakeBillboardBasis;
using ro::render::ScreenRotationOfDirection;
using ro::render::SpriteQuad;

namespace {

bool NearlyEqual(float actual, float expected) { return std::abs(actual - expected) < 1e-4F; }

// Une caméra RO type : elle plonge sur la scène depuis une orientation donnée.
Camera CameraAt(float yaw_degrees, float pitch_degrees) {
  Camera camera;
  camera.look_at = {0.0F, 0.0F, 0.0F};
  camera.distance = 100.0F;
  camera.yaw_degrees = yaw_degrees;
  camera.pitch_degrees = pitch_degrees;
  return camera;
}

// Un quad d'un pixel de large et de dix de haut, dont l'origine est en BAS —
// comme un sprite d'acteur, dont le repère part des pieds.
SpriteQuad StandingQuad() {
  SpriteQuad quad;
  quad.corners = {glm::vec2{0.0F, -10.0F}, glm::vec2{1.0F, -10.0F}, glm::vec2{1.0F, 0.0F},
                  glm::vec2{0.0F, 0.0F}};
  return quad;
}

// Un quad couché SOUS son origine, comme un corps à terre : c'est là que la
// profondeur d'un panneau se juge.
SpriteQuad LyingQuad() {
  SpriteQuad quad;
  quad.corners = {glm::vec2{-10.0F, -2.0F}, glm::vec2{10.0F, -2.0F}, glm::vec2{10.0F, 8.0F},
                  glm::vec2{-10.0F, 8.0F}};
  return quad;
}

// Ce que le tampon de profondeur retiendra d'un point, relativement au point
// d'appui : négatif quand il est PLUS PRÈS de la caméra.
float DepthAgainstGround(const glm::vec3& point, const glm::vec3& ground,
                         const ro::render::BillboardBasis& basis) {
  return glm::dot(point - ground, basis.forward);
}

}  // namespace

TEST_CASE("Panneau : l'axe droit reste horizontal, quelle que soit l'inclinaison",
          "[render][billboard]") {
  // Sinon les personnages pencheraient sur le côté à mesure qu'on plonge.
  for (const float pitch : {0.0F, 45.0F, 80.0F}) {
    const auto basis = MakeBillboardBasis(CameraAt(30.0F, pitch));
    CHECK(NearlyEqual(basis.right.y, 0.0F));
    CHECK(NearlyEqual(glm::length(basis.right), 1.0F));
  }
}

TEST_CASE("Panneau : il s'aligne sur l'ÉCRAN, pas seulement sur la verticale",
          "[render][billboard]") {
  // Le trait le plus visible de RO : un personnage ne s'écrase jamais. Un
  // panneau seulement pivoté autour de la verticale garderait un axe haut
  // vertical, et le sprite se raccourcirait à mesure qu'on plonge.
  const auto flat = MakeBillboardBasis(CameraAt(0.0F, 0.0F));
  const auto steep = MakeBillboardBasis(CameraAt(0.0F, 60.0F));

  CHECK(NearlyEqual(flat.up.y, 1.0F));
  // Vu de haut, l'axe haut du panneau bascule vers l'arrière de la scène.
  CHECK(steep.up.y < 0.9F);
  CHECK(NearlyEqual(glm::length(steep.up), 1.0F));
}

TEST_CASE("Panneau : l'origine du sprite tombe sur le point au sol", "[render][billboard]") {
  // C'est ce qui pose le personnage sur le terrain plutôt qu'à côté.
  const auto basis = MakeBillboardBasis(CameraAt(0.0F, 45.0F));
  const glm::vec3 ground{50.0F, 3.0F, -20.0F};
  const auto corners = BillboardCorners(StandingQuad(), ground, basis);

  // Le coin bas-gauche du quad EST l'origine du sprite.
  CHECK(NearlyEqual(corners[3].x, ground.x));
  CHECK(NearlyEqual(corners[3].y, ground.y));
  CHECK(NearlyEqual(corners[3].z, ground.z));
}

TEST_CASE("Panneau : un sprite s'élève, il ne s'enfonce pas", "[render][billboard]") {
  // L'axe Y d'un sprite DESCEND, celui du monde MONTE. Oublier l'inversion
  // planterait les personnages la tête en bas — le défaut exact qu'ont connu
  // les modèles du décor.
  const auto basis = MakeBillboardBasis(CameraAt(0.0F, 45.0F));
  const auto corners = BillboardCorners(StandingQuad(), glm::vec3{0.0F, 0.0F, 0.0F}, basis);

  // Coin 0 : le HAUT du sprite, en −10 pixels. Il doit être au-dessus du sol.
  CHECK(corners[0].y > corners[3].y);
}

TEST_CASE("Panneau : l'échelle convertit les pixels en unités du monde", "[render][billboard]") {
  // Une caméra à plat et sans lacet aligne le panneau sur les axes du monde :
  // la conversion s'y lit directement.
  const auto basis = MakeBillboardBasis(CameraAt(0.0F, 0.0F));
  const auto corners = BillboardCorners(StandingQuad(), glm::vec3{0.0F, 0.0F, 0.0F}, basis);

  const float height = corners[3].y - corners[0].y;
  CHECK(NearlyEqual(std::abs(height), 10.0F / kSpritePixelsPerWorldUnit));

  // Sept, et pas une calibration : c'est le diviseur du client. Dix pixels font
  // donc un peu plus d'une unité et quatre dixièmes.
  CHECK(kSpritePixelsPerWorldUnit == 7.0F);
}

TEST_CASE("Profondeur : la tête d'un sprite passe devant ses pieds", "[render][billboard]") {
  // Un panneau tout entier à la distance de ses pieds se fait effacer par le
  // moindre décor plus proche qu'eux — dans une ville, à peu près tout.
  const auto basis = MakeBillboardBasis(CameraAt(0.0F, 45.0F));
  const glm::vec3 ground{50.0F, 3.0F, -20.0F};
  const auto corners = BillboardDepthCorners(StandingQuad(), ground, basis);

  CHECK(DepthAgainstGround(corners[0], ground, basis) <
        DepthAgainstGround(corners[3], ground, basis));
}

TEST_CASE("Profondeur : au-dessus de l'origine, le panneau est vraiment debout",
          "[render][billboard]") {
  // La profondeur du haut est celle du PLAN VERTICAL de l'acteur, vu à ce
  // pixel : elle monte comme la tangente du plongeon, et non comme son sinus.
  // Un panneau penché en arrière laissait les pieux d'un brasier, plantés
  // DERRIÈRE un personnage, traverser son buste.
  constexpr float kPitchDegrees = 50.0F;
  const auto basis = MakeBillboardBasis(CameraAt(-40.0F, kPitchDegrees));
  const glm::vec3 ground{50.0F, 3.0F, -20.0F};
  const auto corners = BillboardDepthCorners(StandingQuad(), ground, basis);

  // Le coin du haut, sa marge ôtée, est sur le plan vertical qui passe par
  // les pieds.
  const glm::vec3 level{basis.forward.x, 0.0F, basis.forward.z};
  const glm::vec3 on_plane = corners[0] + basis.forward * ro::render::kSpriteStandingClearance;
  CHECK(std::abs(glm::dot(on_plane - ground, glm::normalize(level))) < 1e-3F);

  // Dix pixels plus haut à l'écran, il est plus près de la caméra de
  // dix pixels × tan(plongeon), comme un vrai mur debout.
  const float height_units = 10.0F / kSpritePixelsPerWorldUnit;
  const float rise =
      DepthAgainstGround(corners[3], ground, basis) - DepthAgainstGround(on_plane, ground, basis);
  // Le coin du bas est À l'origine : il se dresse aussi, avec la même marge.
  const float expected = height_units * std::tan(kPitchDegrees * 3.14159265F / 180.0F) -
                         ro::render::kSpriteStandingClearance;
  CHECK(std::abs(rise - expected) < 1e-3F);
}

TEST_CASE("Profondeur : aucun coin ne passe derrière le sol qu'il touche", "[render][billboard]") {
  // C'est ce qui interdit au terrain de trancher un sprite à hauteur de son
  // origine : les pieds de tout le monde, et la moitié d'un mort, dont le
  // corps est couché SOUS cette origine.
  for (const float pitch : {30.0F, 45.0F, 70.0F}) {
    const auto basis = MakeBillboardBasis(CameraAt(20.0F, pitch));
    const glm::vec3 ground{50.0F, 3.0F, -20.0F};
    for (const SpriteQuad& quad : {StandingQuad(), LyingQuad()}) {
      for (const glm::vec3& corner : BillboardDepthCorners(quad, ground, basis)) {
        CHECK(DepthAgainstGround(corner, ground, basis) <= -kSpriteGroundClearance + 1e-4F);
      }
    }
  }
}

TEST_CASE("Profondeur : sous son origine, un sprite se couche sur le sol", "[render][billboard]") {
  // Il ne s'y enfonce pas : un corps à terre s'étend VERS la caméra, et c'est
  // ce qui lui garde sa moitié basse.
  const auto basis = MakeBillboardBasis(CameraAt(0.0F, 45.0F));
  const glm::vec3 ground{0.0F, 0.0F, 0.0F};
  const auto corners = BillboardDepthCorners(LyingQuad(), ground, basis);

  // Les deux coins du bas sont à huit pixels SOUS l'origine ; leur profondeur
  // ne descend pas pour autant sous le plan du sol.
  CHECK(corners[2].y >= ground.y);
  CHECK(corners[3].y >= ground.y);
  // Et ils sont bien plus PRÈS que le point d'appui, pas plus loin.
  CHECK(DepthAgainstGround(corners[2], ground, basis) < -kSpriteGroundClearance);
}

TEST_CASE("Rotation d'écran : une direction du monde se lit sur la base du billboard",
          "[render][billboard]") {
  // Une image qui pointe vers le HAUT au repos, et un angle positif dans le
  // sens des aiguilles d'une montre : c'est ainsi qu'un sprite d'effet se
  // couche sur son vol. ⚠ Pas la flèche d'un arc, dont l'image pointe vers le
  // bas et que le client tourne du lacet seul : voir
  // `EffectProjectileScreenAngleDegrees`.
  ro::render::BillboardBasis face;
  CHECK(ScreenRotationOfDirection(glm::vec3{0.0F, 1.0F, 0.0F}, face) == 0.0F);
  CHECK(ScreenRotationOfDirection(glm::vec3{1.0F, 0.0F, 0.0F}, face) == 90.0F);
  CHECK(ScreenRotationOfDirection(glm::vec3{-1.0F, 0.0F, 0.0F}, face) == -90.0F);
  CHECK(std::abs(ScreenRotationOfDirection(glm::vec3{0.0F, -1.0F, 0.0F}, face)) == 180.0F);
  // Une direction que la caméra ne montre pas ne tourne rien.
  CHECK(ScreenRotationOfDirection(glm::vec3{0.0F, 0.0F, -1.0F}, face) == 0.0F);

  // ⚠ ET UNE CAMÉRA PENCHÉE MONTRE LE SOL : ce qui s'éloigne d'elle monte à
  // l'écran, ce qui vient vers elle descend. Base d'une caméra qui plonge d'un
  // huitième de tour vers le fond.
  const float half = std::sqrt(0.5F);
  ro::render::BillboardBasis penchee;
  penchee.up = {0.0F, half, -half};
  penchee.forward = {0.0F, -half, -half};
  CHECK(std::abs(ScreenRotationOfDirection(glm::vec3{0.0F, 0.0F, -1.0F}, penchee)) < 0.001F);
  CHECK(std::abs(std::abs(ScreenRotationOfDirection(glm::vec3{0.0F, 0.0F, 1.0F}, penchee)) -
                 180.0F) < 0.001F);
}

TEST_CASE("Élargissement : un pixel d'écran vaut plus de pixels de sprite au loin",
          "[render][billboard]") {
  const Camera camera = CameraAt(0.0F, 45.0F);
  const float here = ro::render::SpritePixelsPerScreenPixel(camera, camera.look_at, 600.0F);
  // Au point visé, la vue fait 2·d·tan(champ/2) unités de haut pour 600 pixels.
  const float expectation = 2.0F * camera.distance *
                            std::tan(glm::radians(camera.fov_degrees) * 0.5F) / 600.0F *
                            kSpritePixelsPerWorldUnit;
  CHECK(NearlyEqual(here, expectation));
  // Derrière l'œil ou sans vue, rien ne s'élargit.
  CHECK(ro::render::SpritePixelsPerScreenPixel(camera, camera.Position() * 2.0F, 600.0F) == 0.0F);
  CHECK(ro::render::SpritePixelsPerScreenPixel(camera, camera.look_at, 0.0F) == 0.0F);
}

TEST_CASE("Élargissement : chaque côté recule de la même marge, le long de ses bords",
          "[render][billboard]") {
  SpriteQuad straight;
  straight.corners = {glm::vec2{-2.0F, -4.0F}, glm::vec2{2.0F, -4.0F}, glm::vec2{2.0F, 0.0F},
                      glm::vec2{-2.0F, 0.0F}};
  const SpriteQuad large = ro::render::GrowSpriteQuad(straight, 1.0F);
  CHECK(large.corners[0] == glm::vec2{-3.0F, -5.0F});
  CHECK(large.corners[1] == glm::vec2{3.0F, -5.0F});
  CHECK(large.corners[2] == glm::vec2{3.0F, 1.0F});
  CHECK(large.corners[3] == glm::vec2{-3.0F, 1.0F});
  // Un quad tourné d'un quart de tour s'élargit dans SON repère.
  SpriteQuad rotated;
  rotated.corners = {glm::vec2{0.0F, 0.0F}, glm::vec2{0.0F, 4.0F}, glm::vec2{-2.0F, 4.0F},
                     glm::vec2{-2.0F, 0.0F}};
  const SpriteQuad turned_wide = ro::render::GrowSpriteQuad(rotated, 1.0F);
  CHECK(turned_wide.corners[0] == glm::vec2{1.0F, -1.0F});
  CHECK(turned_wide.corners[2] == glm::vec2{-3.0F, 5.0F});
}

TEST_CASE("Culbute : un quad tourne sur son centre, le haut passe à droite au quart de tour",
          "[render][billboard]") {
  SpriteQuad upright;
  upright.corners = {glm::vec2{-2.0F, -4.0F}, glm::vec2{2.0F, -4.0F}, glm::vec2{2.0F, 0.0F},
                     glm::vec2{-2.0F, 0.0F}};
  // Le centre est en (0, -2) ; y descend, et le haut-gauche passe en haut-droite.
  const SpriteQuad quarter = ro::render::SpinSpriteQuad(upright, 90.0F);
  CHECK(NearlyEqual(quarter.corners[0].x, 2.0F));
  CHECK(NearlyEqual(quarter.corners[0].y, -4.0F));
  CHECK(NearlyEqual(quarter.corners[2].x, -2.0F));
  CHECK(NearlyEqual(quarter.corners[2].y, 0.0F));
  // Au demi-tour, la tête est en bas : le coin du haut prend la place du bas.
  const SpriteQuad half = ro::render::SpinSpriteQuad(upright, 180.0F);
  CHECK(NearlyEqual(half.corners[0].x, 2.0F));
  CHECK(NearlyEqual(half.corners[0].y, 0.0F));
}

TEST_CASE("Le panneau d'ombre se tient debout, sur la ligne des pieds vue à l'écran",
          "[render][billboard]") {
  const ro::render::BillboardBasis camera_basis = MakeBillboardBasis(CameraAt(0.0F, 50.0F));
  // Un soleil de côté, mais qui éclaire encore franchement la face du panneau.
  const glm::vec3 toward_sun =
      glm::normalize(glm::cross(glm::vec3(0.0F, 1.0F, 0.0F), camera_basis.right) +
                     (camera_basis.right * 0.5F) + glm::vec3(0.0F, 1.0F, 0.0F));
  const ro::render::BillboardBasis basis = ro::render::ShadowPanelBasis(toward_sun, camera_basis);
  // Debout : son haut est la verticale du monde.
  CHECK(NearlyEqual(basis.up.y, 1.0F));
  // Son bord bas suit la droite de l'écran : l'ombre part des pieds qu'on voit.
  CHECK(NearlyEqual(basis.right.x, camera_basis.right.x));
  CHECK(NearlyEqual(basis.right.z, camera_basis.right.z));
}

TEST_CASE("Un soleil rasant fait pivoter le panneau d'ombre juste assez", "[render][billboard]") {
  const ro::render::BillboardBasis camera_basis = MakeBillboardBasis(CameraAt(0.0F, 50.0F));
  // Le soleil dans le plan de l'écran : l'ombre du panneau parallèle serait un trait.
  const glm::vec3 toward_sun = glm::normalize(camera_basis.right + glm::vec3(0.0F, 1.0F, 0.0F));
  const ro::render::BillboardBasis basis = ro::render::ShadowPanelBasis(toward_sun, camera_basis);
  const glm::vec3 sun_way = glm::normalize(glm::vec3(toward_sun.x, 0.0F, toward_sun.z));
  CHECK(NearlyEqual(std::abs(glm::dot(basis.forward, sun_way)),
                    ro::render::kShadowPanelMinimumSunFacing));
  CHECK(NearlyEqual(basis.up.y, 1.0F));
  CHECK(NearlyEqual(basis.right.y, 0.0F));
  // Du même côté que la droite de l'écran : la silhouette ne se retourne pas.
  CHECK(glm::dot(basis.right, camera_basis.right) > 0.0F);
}

TEST_CASE("Un soleil au zénith laisse le panneau d'ombre parallèle à l'écran",
          "[render][billboard]") {
  const ro::render::BillboardBasis camera_basis = MakeBillboardBasis(CameraAt(30.0F, 50.0F));
  const ro::render::BillboardBasis basis =
      ro::render::ShadowPanelBasis(glm::vec3(0.0F, 1.0F, 0.0F), camera_basis);
  CHECK(NearlyEqual(basis.right.x, camera_basis.right.x));
  CHECK(NearlyEqual(basis.right.z, camera_basis.right.z));
  CHECK(NearlyEqual(basis.up.y, 1.0F));
}

TEST_CASE("Une image qui recouvre déjà son ombre ne la recule pas", "[render][billboard]") {
  const ro::render::BillboardBasis camera_basis = MakeBillboardBasis(CameraAt(30.0F, 50.0F));
  constexpr float kFrontSunk =
      ro::render::kShadowFootOverlapPixels / ro::render::kSpritePixelsPerWorldUnit;
  CHECK(ro::render::ShadowFootShift(kFrontSunk, camera_basis) == glm::vec3(0.0F));
  CHECK(ro::render::ShadowFootShift(kFrontSunk * 2.0F, camera_basis) == glm::vec3(0.0F));
}

TEST_CASE("Une image vue de dos recule son ombre sous ses pieds", "[render][billboard]") {
  const ro::render::BillboardBasis camera_basis = MakeBillboardBasis(CameraAt(30.0F, 50.0F));
  // Les pieds pile sur l'ancrage : il manque tout le recouvrement.
  const glm::vec3 shift = ro::render::ShadowFootShift(0.0F, camera_basis);
  // Au sol, et à l'opposé de la caméra.
  CHECK(NearlyEqual(shift.y, 0.0F));
  const glm::vec3 ground_forward =
      glm::normalize(glm::vec3(camera_basis.forward.x, 0.0F, camera_basis.forward.z));
  CHECK(glm::dot(shift, ground_forward) > 0.0F);
  // D'autant que l'écran montre le recouvrement manquant : la projection du
  // recul sur le haut de l'écran le vaut.
  const float missing =
      ro::render::kShadowFootOverlapPixels / ro::render::kSpritePixelsPerWorldUnit;
  CHECK(NearlyEqual(glm::dot(shift, camera_basis.up), missing));
}
