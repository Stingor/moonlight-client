// L'ANGLE D'ÉCRAN D'UN PROJECTILE D'ACTEUR — la flèche d'un arc — et ce que le
// dessinateur des sprites en fait : la face du vol vue de la caméra, du lacet
// seul, et une image qui pointe vers le BAS au repos.

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_sprite.h"

#include "support/sprite_builder.h"

using ro::formats::ActAnimation;
using ro::formats::SprSprite;
using ro::render::ComposeEffectNodeSprite;
using ro::render::EffectNode;
using ro::render::EffectProjectileScreenAngle;
using ro::render::EffectProjectileScreenAngleDegrees;
using ro::render::SpriteQuad;
using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildSpr;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;

namespace {

// Les quatre vols à plat, dans notre repère : le fond de l'écran est vers −z
// pour la caméra par défaut, la droite vers +x.
constexpr glm::vec3 kTowardFar{0.0F, 0.0F, -1.0F};
constexpr glm::vec3 kTowardNear{0.0F, 0.0F, 1.0F};
constexpr glm::vec3 kTowardRight{1.0F, 0.0F, 0.0F};
constexpr glm::vec3 kTowardLeft{-1.0F, 0.0F, 0.0F};
constexpr float kDefaultYaw = 0.0F;
constexpr float kQuarterTurnYaw = 90.0F;

// Une image de 4 sur 6 : dimensions PAIRES et DIFFÉRENTES, pour que toute
// confusion entre les deux axes se voie.
SprSprite TallSprite() {
  SprIndexedFrameSpec frame{.width = 4, .height = 6};
  frame.indices.assign(24, 1);
  SprBuildOptions options;
  options.indexed = {frame};
  options.palette = {SpriteColor{}, SpriteColor{.red = 255}};
  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

ActAnimation OneLayerAnimation() {
  ActBuildOptions options;
  options.actions = {ActActionSpec{.frames = {ActFrameSpec{.layers = {ActLayerSpec{}}}}}};
  options.speeds = {6.0F};
  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

// DU MILIEU DU BORD HAUT DE L'IMAGE AU MILIEU DE SON BORD BAS, en pixels de
// sprite, l'ordonnée vers le bas : là où la pointe d'une flèche dessinée tête en
// bas se retrouve à l'écran, une fois le quad tourné.
glm::vec2 HeadVector(const SpriteQuad& quad) {
  const glm::vec2 tail = (quad.corners[0] + quad.corners[1]) * 0.5F;
  const glm::vec2 head = (quad.corners[2] + quad.corners[3]) * 0.5F;
  return head - tail;
}

SpriteQuad ArrowQuadTurnedBy(std::int32_t screen_angle_degrees) {
  EffectNode node;
  node.draws_rotated = true;
  node.spin_degrees = static_cast<float>(screen_angle_degrees);
  const std::vector<SpriteQuad> quads =
      ComposeEffectNodeSprite(node, TallSprite(), OneLayerAnimation());
  REQUIRE(quads.size() == 1);
  return quads.front();
}

}  // namespace

TEST_CASE("Projectile : la face de son vol vue de la caméra, du lacet seul",
          "[render][effectlaunch][projectile]") {
  // Devant la caméra par défaut, un vol qui VIENT vers le joueur a la face zéro
  // et se dessine tel quel ; les trois autres tournent d'un quart de tour chacun,
  // dans le sens des aiguilles d'une montre.
  CHECK(EffectProjectileScreenAngleDegrees(kTowardNear, kDefaultYaw) == 0);
  CHECK(EffectProjectileScreenAngleDegrees(kTowardFar, kDefaultYaw) == 180);
  CHECK(EffectProjectileScreenAngleDegrees(kTowardRight, kDefaultYaw) == 270);
  // ⚠ UN DEGRÉ DE TRAVERS VERS LA GAUCHE, et c'est celui du client : sa face
  // vaut `atan2 / 3,141592 × 180`, un rien au-delà de moins quatre-vingt-dix,
  // soit 269,99998 sur le tour ; l'opposé se tronque à −269, donc 91.
  CHECK(EffectProjectileScreenAngleDegrees(kTowardLeft, kDefaultYaw) == 91);

  // LA CAMÉRA TOURNE, L'ANGLE AUSSI : son lacet s'ajoute, celui du client étant
  // l'opposé du nôtre.
  CHECK(EffectProjectileScreenAngleDegrees(kTowardFar, kQuarterTurnYaw) == 270);
  CHECK(EffectProjectileScreenAngleDegrees(kTowardNear, kQuarterTurnYaw) == 90);

  // UNE DIAGONALE TOMBE À MI-CHEMIN DES DEUX QUARTS — deux cent vingt-cinq entre
  // le fond et la droite —, quelle que soit la plongée de la caméra : le client
  // ne connaît que le lacet.
  CHECK(EffectProjectileScreenAngleDegrees(glm::vec3{1.0F, 0.0F, -1.0F}, kDefaultYaw) == 225);
  // La hauteur du vol ne compte pas.
  CHECK(EffectProjectileScreenAngleDegrees(glm::vec3{1.0F, 30.0F, -1.0F}, kDefaultYaw) == 225);

  // L'ANGLE SE TRONQUE À L'ENTIER, vers zéro, comme chez le client.
  CHECK(EffectProjectileScreenAngleDegrees(kTowardNear, 10.7F) == 10);
  CHECK(EffectProjectileScreenAngleDegrees(kTowardNear, -10.7F) == 350);
  // ET L'ANGLE RÉEL, celui de la munition en sprite d'un monstre, garde ses
  // décimales et son signe : ni tronqué, ni ramené sur le tour.
  CHECK(EffectProjectileScreenAngle(kTowardNear, 10.7F) == Catch::Approx(10.7F));
  CHECK(EffectProjectileScreenAngle(kTowardNear, -10.7F) == Catch::Approx(-10.7F));
  CHECK(EffectProjectileScreenAngle(kTowardFar, kDefaultYaw) == Catch::Approx(-180.0F));

  // UN VOL VERTICAL N'A PAS DE FACE : elle reste à zéro.
  CHECK(EffectProjectileScreenAngleDegrees(glm::vec3{0.0F, 1.0F, 0.0F}, kDefaultYaw) == 0);
}

TEST_CASE("Projectile : la pointe, en bas de l'image, mène le vol",
          "[render][effectnode][projectile]") {
  // 🔴 LA FLÈCHE GÉNÉRIQUE PORTE SA POINTE EN BAS : au repos, elle vole vers le
  // joueur. Le quad d'un vol qui s'éloigne doit donc porter le bord bas de
  // l'image EN HAUT de l'écran — c'est le demi-tour du client, et c'est ce
  // qu'une image supposée pointer vers le haut inversait.
  const glm::vec2 toward_far =
      HeadVector(ArrowQuadTurnedBy(EffectProjectileScreenAngleDegrees(kTowardFar, kDefaultYaw)));
  CHECK(toward_far.y < 0.0F);
  CHECK(std::abs(toward_far.x) < 1e-3F);

  // Vers la droite de l'écran, la pointe va à droite.
  const glm::vec2 toward_right =
      HeadVector(ArrowQuadTurnedBy(EffectProjectileScreenAngleDegrees(kTowardRight, kDefaultYaw)));
  CHECK(toward_right.x > 0.0F);
  CHECK(std::abs(toward_right.y) < 1e-3F);

  // Vers le joueur, l'image reste telle quelle : la pointe en bas.
  const glm::vec2 toward_near =
      HeadVector(ArrowQuadTurnedBy(EffectProjectileScreenAngleDegrees(kTowardNear, kDefaultYaw)));
  CHECK(toward_near.y > 0.0F);
  CHECK(std::abs(toward_near.x) < 1e-3F);

  // Et la longueur de la flèche ne change pas en tournant : six pixels.
  CHECK(std::abs(glm::length(toward_far) - 6.0F) < 1e-3F);
  CHECK(std::abs(glm::length(toward_right) - 6.0F) < 1e-3F);
}

TEST_CASE("Projectile : la munition en sprite tourne de son angle réel, la flèche du degré",
          "[render][effectnode][projectile]") {
  // Le dessinateur des acteurs lit l'angle tronqué dans la table du client ;
  // celui des particules tourne son quad de l'angle flottant. Dix degrés et
  // demi donnent dix à l'une, dix et demi à l'autre.
  constexpr float kHalfDegree = 10.5F;
  constexpr float kTableMarginDegrees = 0.05F;
  const auto head_angle_degrees = [](const SpriteQuad& quad) {
    const glm::vec2 head = HeadVector(quad);
    return std::atan2(-head.x, head.y) * 180.0F / std::numbers::pi_v<float>;
  };
  EffectNode arrow;
  arrow.draws_rotated = true;
  arrow.spin_degrees = kHalfDegree;
  const std::vector<SpriteQuad> arrow_quads =
      ComposeEffectNodeSprite(arrow, TallSprite(), OneLayerAnimation());
  REQUIRE(arrow_quads.size() == 1);
  CHECK(head_angle_degrees(arrow_quads.front()) ==
        Catch::Approx(10.0F).margin(kTableMarginDegrees));

  EffectNode bullet = arrow;
  bullet.spins_by_exact_degrees = true;
  const std::vector<SpriteQuad> bullet_quads =
      ComposeEffectNodeSprite(bullet, TallSprite(), OneLayerAnimation());
  REQUIRE(bullet_quads.size() == 1);
  CHECK(head_angle_degrees(bullet_quads.front()) ==
        Catch::Approx(kHalfDegree).margin(kTableMarginDegrees));
}
