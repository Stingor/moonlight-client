#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/formats/gr2_model.h"
#include "ro/render/actor_model_motion.h"
#include "ro/render/actor_model_pose.h"
#include "ro/render/actor_model_scene.h"
#include "ro/render/actor_pick.h"
#include "ro/render/camera.h"
#include "ro/render/sprite_renderer.h"

#include "support/gr2_builder.h"

namespace {

using Catch::Approx;
using ro::render::ActorModelMotion;

ro::formats::GrannyCurve Curve(std::int32_t degree, std::vector<float> knots,
                               std::vector<float> controls) {
  ro::formats::GrannyCurve curve;
  curve.degree = degree;
  curve.dimension = static_cast<std::int32_t>(controls.size() / knots.size());
  curve.knots = std::move(knots);
  curve.controls = std::move(controls);
  return curve;
}

TEST_CASE("Modèle d'acteur : une courbe se lit comme une B-spline", "[render][actor_model]") {
  // Une seule valeur : la courbe vaut son point à tout instant.
  const auto constant = ro::render::EvaluateGrannyCurve(Curve(0, {0.0F}, {1.0F, 2.0F, 3.0F}), 5.0F);
  REQUIRE(constant.has_value());
  CHECK((*constant)[2] == Approx(3.0F));
  // Au premier degré, l'interpolation est linéaire entre deux nœuds.
  const auto line = ro::render::EvaluateGrannyCurve(Curve(1, {0.0F, 1.0F}, {0.0F, 10.0F}), 0.25F);
  REQUIRE(line.has_value());
  CHECK((*line)[0] == Approx(2.5F));
  // Sans nœud, la courbe ne dit rien.
  CHECK_FALSE(ro::render::EvaluateGrannyCurve(ro::formats::GrannyCurve{}, 0.0F).has_value());

  // UN DEGRÉ DÉMESURÉ, lu d'un fichier abîmé, ne réserve rien : au-delà du second
  // degré, la courbe vaut le point de son nœud.
  const auto third = ro::render::EvaluateGrannyCurve(Curve(3, {0.0F, 1.0F}, {0.0F, 10.0F}), 0.25F);
  const auto huge =
      ro::render::EvaluateGrannyCurve(Curve(1'000'000'000, {0.0F, 1.0F}, {0.0F, 10.0F}), 0.25F);
  REQUIRE(third.has_value());
  REQUIRE(huge.has_value());
  CHECK(*huge == *third);
  // Un quaternion sort renormalisé au premier ordre.
  const auto quaternion =
      ro::render::EvaluateGrannyCurve(Curve(0, {0.0F}, {0.0F, 0.0F, 0.0F, 0.9F}), 0.0F);
  REQUIRE(quaternion.has_value());
  CHECK((*quaternion)[3] == Approx(0.9F * (3.0F - 0.81F) * 0.5F));
}

TEST_CASE("Modèle d'acteur : le second degré passe par ses contrôles constants",
          "[render][actor_model]") {
  const auto flat = ro::render::EvaluateGrannyCurve(
      Curve(2, {0.0F, 1.0F, 2.0F, 3.0F}, {4.0F, 4.0F, 4.0F, 4.0F}), 1.4F);
  REQUIRE(flat.has_value());
  CHECK((*flat)[0] == Approx(4.0F));
}

TEST_CASE("Modèle d'acteur : au repos, la matrice composite est l'identité",
          "[render][actor_model]") {
  ro::formats::GrannyScene scene;
  ro::formats::GrannyBone root;
  root.name = "racine";
  root.local.position = {0.0F, 0.0F, 5.0F};
  // L'inverse de la pose de repos : une translation de −5 sur Z.
  root.inverse_world = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -5, 1};
  scene.bones.push_back(root);
  const std::vector<ro::formats::GrannyTransform> pose =
      ro::render::SampleLocalPose(scene, nullptr, 0.0F);
  const std::vector<glm::mat4> composites = ro::render::CompositeMatrices(scene, pose);
  REQUIRE(composites.size() == 1);
  CHECK(composites[0][3][2] == Approx(0.0F));
  CHECK(composites[0][0][0] == Approx(1.0F));
}

TEST_CASE("Modèle d'acteur : un sommet se mélange entre ses os", "[render][actor_model]") {
  ro::formats::GrannyMesh mesh;
  ro::formats::GrannyVertex vertex;
  vertex.position = {1.0F, 0.0F, 0.0F};
  vertex.bone_indices = {0, 1, 0, 0};
  vertex.bone_weights = {51, 204, 0, 0};
  mesh.vertices.push_back(vertex);
  const std::vector<std::int32_t> map = {0, 1};
  const std::vector<glm::mat4> composites = {
      glm::mat4(1.0F), glm::mat4(glm::vec4(1, 0, 0, 0), glm::vec4(0, 1, 0, 0),
                                 glm::vec4(0, 0, 1, 0), glm::vec4(0, 0, 10, 1))};
  const ro::render::PosedMesh posed = ro::render::PoseMesh(mesh, map, composites);
  REQUIRE(posed.vertices.size() == 1);
  // Un cinquième reste en place, quatre cinquièmes montent de dix.
  CHECK(posed.vertices[0].position.z == Approx(8.0F));
  CHECK(posed.vertices[0].position.x == Approx(1.0F));
}

TEST_CASE("Modèle d'acteur : un maillage rigide suit son premier os d'un bloc",
          "[render][actor_model]") {
  ro::formats::GrannyMesh mesh;
  mesh.rigid = true;
  ro::formats::GrannyVertex vertex;
  vertex.normal = {0.0F, 1.0F, 0.0F};
  mesh.vertices.push_back(vertex);
  const std::vector<std::int32_t> map = {1};
  const glm::mat4 lifted(glm::vec4(1, 0, 0, 0), glm::vec4(0, 1, 0, 0), glm::vec4(0, 0, 1, 0),
                         glm::vec4(3, 0, 0, 1));
  const std::vector<glm::mat4> composites = {glm::mat4(1.0F), lifted};
  const ro::render::PosedMesh posed = ro::render::PoseMesh(mesh, map, composites);
  CHECK(posed.mesh_matrix[3][0] == Approx(3.0F));
  // Les sommets et leurs normales restent ceux du repos.
  CHECK(posed.vertices[0].position.x == Approx(0.0F));
  CHECK(posed.vertices[0].normal.y == Approx(1.0F));
}

TEST_CASE("Modèle d'acteur : le temps local boucle ou s'arrête", "[render][actor_model]") {
  CHECK(ro::render::AnimationLocalSeconds(12.5F, 5.0F, false) == Approx(2.5F));
  CHECK(ro::render::AnimationLocalSeconds(12.5F, 5.0F, true) == Approx(5.0F));
  CHECK(ro::render::AnimationLocalSeconds(1.0F, 0.0F, false) == Approx(0.0F));
}

TEST_CASE("Modèle d'acteur : les actions du client choisissent le mouvement",
          "[render][actor_model]") {
  CHECK(ro::render::ActorModelMotionOfAction(0) == ActorModelMotion::kStand);
  CHECK(ro::render::ActorModelMotionOfAction(8) == ActorModelMotion::kMove);
  CHECK(ro::render::ActorModelMotionOfAction(80) == ActorModelMotion::kAttack);
  CHECK(ro::render::ActorModelMotionOfAction(88) == ActorModelMotion::kAttack);
  CHECK(ro::render::ActorModelMotionOfAction(64) == ActorModelMotion::kDead);
  CHECK(ro::render::ActorModelMotionOfAction(48) == ActorModelMotion::kDamage);
  // Ramasser, se tenir prêt, geler : aucun mouvement de modèle.
  CHECK_FALSE(ro::render::ActorModelMotionOfAction(24).has_value());
  CHECK_FALSE(ro::render::ActorModelMotionOfAction(32).has_value());
  CHECK_FALSE(ro::render::ActorModelMotionOfAction(40).has_value());
}

TEST_CASE("Modèle d'acteur : le numéro se lit après le premier soulignement",
          "[render][actor_model]") {
  CHECK(ro::render::ActorModelIndex("Empelium90_0.gr2") == 0);
  CHECK(ro::render::ActorModelIndex("Guildflag90_1.gr2") == 1);
  CHECK(ro::render::ActorModelIndex("TREASUREBOX_2.gr2") == 2);
  CHECK(ro::render::ActorModelIndex("Kguardian90_7.gr2") == 7);
  CHECK_FALSE(ro::render::ActorModelIndex("Emperium.gr2").has_value());
  CHECK(ro::render::ActorModelStandJob(0) == 1288U);
  CHECK(ro::render::ActorModelStandJob(8) == 1285U);
  CHECK_FALSE(ro::render::ActorModelStandJob(10).has_value());
}

TEST_CASE("Modèle d'acteur : les animations de mouvement ont leur dossier",
          "[render][actor_model]") {
  CHECK(ro::render::ActorModelMotionPath(7, ActorModelMotion::kAttack) ==
        "data\\model\\3dmob_bone\\7_attack.gr2");
  CHECK(ro::render::ActorModelMotionPath(2, ActorModelMotion::kDead) ==
        "data\\model\\3dmob_bone\\2_dead.gr2");
  // L'attente vient du fichier du modèle, et le drapeau n'en cherche aucune.
  CHECK_FALSE(ro::render::ActorModelMotionPath(7, ActorModelMotion::kStand).has_value());
  CHECK_FALSE(ro::render::ActorModelMotionPath(1, ActorModelMotion::kMove).has_value());
  CHECK_FALSE(ro::render::ActorModelMotionPath(12, ActorModelMotion::kMove).has_value());
}

TEST_CASE("Modèle d'acteur : l'horloge d'une attaque brève accélère", "[render][actor_model]") {
  CHECK(ro::render::ActorModelAttackSpeed(216) == Approx(0.5F));
  CHECK(ro::render::ActorModelAttackSpeed(1000) == Approx(1.0F));
  // Une cadence nulle se lit comme l'ordinaire, plafonnée à l'unité.
  CHECK(ro::render::ActorModelAttackSpeed(0) == Approx(1.0F));
}

TEST_CASE("Modèle d'acteur : l'orientation part à l'opposé, puis rejoint le corps",
          "[render][actor_model]") {
  CHECK(ro::render::ActorBodyDegrees(0) == Approx(180.0F));
  CHECK(ro::render::ActorBodyDegrees(2) == Approx(270.0F));
  CHECK(ro::render::InitialActorModelYaw(180.0F) == Approx(0.0F));
  // Le but vaut 180 − 90 = 90 ; à 60, l'écart de −30 se ramène à 330 : on monte.
  CHECK(ro::render::TurnActorModelYaw(60.0F, 90.0F) == Approx(66.0F));
  // À 150, l'écart de 60 fait descendre.
  CHECK(ro::render::TurnActorModelYaw(150.0F, 90.0F) == Approx(144.0F));
  // Dans la marge de douze degrés, rien ne bouge.
  CHECK(ro::render::TurnActorModelYaw(100.0F, 90.0F) == Approx(100.0F));
  // Un angle négatif se ramène d'abord dans le tour : −10 devient 350, et
  // l'écart de 440 au but de −90 se réduit à 80.
  CHECK(ro::render::TurnActorModelYaw(-10.0F, 270.0F) == Approx(344.0F));
  const std::optional<float> toward = ro::render::ActorBodyDegreesToward(0.0F, 0.0F, 10.0F, 0.0F);
  REQUIRE(toward.has_value());
  CHECK(*toward == Approx(90.0F));
}

TEST_CASE("Modèle d'acteur : le reste tient, puis s'efface par un octet qui déborde",
          "[render][actor_model]") {
  CHECK(ro::render::ActorModelRemnantAlpha(0) == 255);
  CHECK(ro::render::ActorModelRemnantAlpha(355) == 255);
  CHECK(ro::render::ActorModelRemnantAlpha(356) == 244);
  CHECK(ro::render::ActorModelRemnantAlpha(599) == 1);
  CHECK(ro::render::ActorModelRemnantAlpha(600) == 0);
}

TEST_CASE("Modèle d'acteur : l'écart d'un effet tourne avec la table du client",
          "[render][actor_model]") {
  const glm::vec3 same = ro::render::RotatedSpawnOffset({1.0F, 2.0F, 0.0F}, 0.0F);
  CHECK(same.x == Approx(1.0F));
  CHECK(same.y == Approx(2.0F));
  const glm::vec3 quarter = ro::render::RotatedSpawnOffset({1.0F, 0.0F, 0.0F}, 90.0F);
  CHECK(quarter.x == Approx(0.0F).margin(1e-5));
  CHECK(quarter.z == Approx(1.0F));
  // UN ANGLE NÉGATIF TOMBE UN DEGRÉ À CÔTÉ : −90 se lit comme −89.
  const glm::vec3 negative = ro::render::RotatedSpawnOffset({1.0F, 0.0F, 0.0F}, -90.0F);
  CHECK(negative.x == Approx(std::cos(-89.0 * 3.14159265358979 / 180.0)).margin(1e-4));
}

// Un triangle debout, dont un sommet est au sommet de l'axe du modèle.
ro::render::ActorModelResource SyntheticResource(std::string_view file_name) {
  ro::test_support::Gr2Spec spec;
  spec.bones.push_back(ro::test_support::Gr2BoneSpec{"Dummy01"});
  ro::test_support::Gr2TextureSpec texture;
  texture.file_name = "gris.tif";
  texture.width = 1;
  texture.height = 1;
  texture.rgba = {128, 128, 128, 255};
  spec.textures.push_back(texture);
  ro::test_support::Gr2MeshSpec mesh;
  mesh.bone_names = {"Dummy01"};
  mesh.diffuse_texture = "gris.tif";
  ro::test_support::Gr2VertexSpec top;
  top.position = {0.0F, 0.0F, 10.0F};
  top.normal = {0.0F, 0.0F, 1.0F};
  mesh.vertices = {top, ro::test_support::Gr2VertexSpec{}, ro::test_support::Gr2VertexSpec{}};
  mesh.indices = {0, 1, 2};
  spec.meshes.push_back(mesh);
  auto scene = ro::formats::ReadGrannyScene(ro::test_support::BuildGr2(spec));
  REQUIRE(scene.has_value());
  return ro::render::MakeActorModelResource(std::move(*scene), file_name);
}

TEST_CASE("Modèle d'acteur : un modèle « 90 » se dresse, décalé de deux unités",
          "[render][actor_model]") {
  const ro::render::ActorModelResource resource = SyntheticResource("Empelium90_0.gr2");
  CHECK(resource.quarter_turn_upright);
  ro::render::ActorModelPlacement placement;
  placement.ground_position = {100.0F, 5.0F, 200.0F};
  placement.yaw_degrees = 37.0F;
  ro::render::Camera camera;
  camera.look_at = placement.ground_position;
  const ro::render::DrawnActorModel drawn = ro::render::ComposeActorModel(
      resource, nullptr, 0.0F, placement, camera, ro::render::MapEffectFrame{}, nullptr);
  REQUIRE(drawn.meshes.size() == 1);
  REQUIRE(drawn.meshes[0].vertices.size() == 3);
  // Le sommet de l'axe monte de dix au-dessus des pieds, et l'orientation ne le
  // déplace pas : seul le décalage du client le pousse, de −2 en X et +2 chez
  // nous en Z.
  const glm::vec3 top = drawn.meshes[0].vertices[0].position;
  CHECK(top.x == Approx(98.0F));
  CHECK(top.y == Approx(15.0F));
  CHECK(top.z == Approx(202.0F));
  // Une teinte grise, entre le plancher et le plein.
  const std::uint8_t luminance = drawn.meshes[0].vertices[0].color[0];
  CHECK(luminance >= 127);
  CHECK(drawn.meshes[0].vertices[0].color[3] == 255);
}

TEST_CASE("Modèle d'acteur : un modèle sans « 90 » se retourne d'un demi-tour",
          "[render][actor_model]") {
  const ro::render::ActorModelResource resource = SyntheticResource("TREASUREBOX_2.gr2");
  CHECK_FALSE(resource.quarter_turn_upright);
  ro::render::ActorModelPlacement placement;
  placement.stance = ro::render::ActorModelStance::kRemnant;
  ro::render::Camera camera;
  const ro::render::DrawnActorModel drawn = ro::render::ComposeActorModel(
      resource, nullptr, 0.0F, placement, camera, ro::render::MapEffectFrame{}, nullptr);
  REQUIRE(drawn.meshes.size() == 1);
  // Un demi-tour autour de X COUCHE l'axe vertical du fichier : un tel modèle a
  // été exporté debout sur son autre axe. Et le reste n'est pas décalé.
  const glm::vec3 top = drawn.meshes[0].vertices[0].position;
  CHECK(top.x == Approx(0.0F).margin(1e-3));
  CHECK(top.y == Approx(0.0F).margin(1e-3));
  CHECK(std::abs(top.z) == Approx(10.0F));
}

TEST_CASE("Modèle d'acteur : la luminance a son plancher et son plafond", "[render][actor_model]") {
  const glm::vec3 up{0.0F, 1.0F, 0.0F};
  CHECK(ro::render::ActorModelLuminance(ro::render::ActorModelStance::kUpright, up, up) == 255);
  CHECK(ro::render::ActorModelLuminance(ro::render::ActorModelStance::kUpright, up, -up) == 127);
  CHECK(ro::render::ActorModelLuminance(ro::render::ActorModelStance::kGrounded, up, -up) == 155);
}

TEST_CASE("Modèle d'acteur : l'emprise est un carré autour de son origine",
          "[render][actor_model]") {
  ro::render::Camera camera;
  camera.look_at = {0.0F, 0.0F, 0.0F};
  ro::render::ComposedActor no_quads;
  ro::render::DrawnActor drawn{&no_quads, nullptr, {0.0F, 0.0F, 0.0F}};
  drawn.model_pick_side = ro::render::kActorModelPickBoxSide;
  drawn.model_pick_center = {0.0F, 0.0F, 0.0F};
  const auto bounds = ro::render::ActorScreenBounds(camera, 1.0F, drawn, 800.0F, 800.0F);
  REQUIRE(bounds.has_value());
  const glm::vec2 size = bounds->max - bounds->min;
  CHECK(size.x == Approx(size.y));
  CHECK((bounds->min.x + bounds->max.x) * 0.5F == Approx(400.0F));
  // L'emprise du clic l'élargit jusqu'au plancher de quarante pixels.
  const auto rect = ro::render::ActorScreenRect(camera, 1.0F, drawn, 800.0F, 800.0F);
  REQUIRE(rect.has_value());
  CHECK(rect->max.x - rect->min.x >= ro::render::kMinimumPickSizePixels - 1e-3F);
}

}  // namespace
