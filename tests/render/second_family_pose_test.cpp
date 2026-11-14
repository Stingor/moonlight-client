#include "ro/render/second_family_pose.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/formats/rsm.h"
#include "ro/formats/rsw.h"
#include "ro/render/model_clock.h"
#include "ro/render/model_mesh.h"
#include "ro/render/model_pose.h"

#include "support/model_builder.h"

using ro::formats::RsmModel;
using ro::formats::RsmPositionKey;
using ro::formats::RsmRotationKey;
using ro::formats::RsmScaleKey;
using ro::formats::RsmTextureAnimation;
using ro::render::AxisAngleQuaternion;
using ro::render::kModelPlayLooped;
using ro::render::kModelPlayOnce;
using ro::render::MoveTextureCoordinate;
using ro::render::PoseSecondFamily;
using ro::render::SecondFamilyDurationMs;
using ro::render::SecondFamilyFrameAt;
using ro::render::SecondFamilyNode;
using ro::render::SecondFamilyNodePose;
using ro::render::SecondFamilyPlayedAt;
using ro::render::TextureMotion;
using ro::render::TextureMotionAt;
using ro::test_support::BuildRsm;
using ro::test_support::RsmBuildOptions;
using ro::test_support::RsmFaceSpec;
using ro::test_support::RsmMeshSpec;
using ro::test_support::RsmTextureAnimationSpec;

namespace {

constexpr float kQuarterTurn = std::numbers::pi_v<float> / 2.0F;

bool NearlyEqual(float actual, float expected, float tolerance = 1e-4F) {
  return std::abs(actual - expected) < tolerance;
}

glm::vec3 Apply(const glm::mat4& matrix, const glm::vec3& point) {
  return glm::vec3(matrix * glm::vec4{point, 1.0F});
}

bool SamePoint(const glm::vec3& actual, const glm::vec3& expected) {
  return NearlyEqual(actual.x, expected.x) && NearlyEqual(actual.y, expected.y) &&
         NearlyEqual(actual.z, expected.z);
}

std::array<float, 4> TurnAroundY(float angle_radians) {
  return AxisAngleQuaternion({0.0F, 1.0F, 0.0F}, angle_radians);
}

// Un triangle posé à l'origine de son nœud.
RsmMeshSpec Triangle(const std::string& name) {
  RsmMeshSpec mesh;
  mesh.name = name;
  mesh.textures = {"eau.bmp"};
  mesh.texture_indices = {0};
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
  mesh.texture_vertices = {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}};
  mesh.faces = {RsmFaceSpec{.vertex_ids = {0, 1, 2}, .texture_vertex_ids = {0, 1, 2}}};
  return mesh;
}

RsmModel Parse(const RsmBuildOptions& options) {
  auto model = RsmModel::Parse(BuildRsm(options));
  REQUIRE(model.has_value());
  return *model;
}

// Un tronc à dix unités de l'origine, et son houppier à 57 unités dans le
// fichier — la forme exacte des arbres de wolfvill.
RsmBuildOptions Tree() {
  RsmMeshSpec trunk = Triangle("tronc");
  trunk.local_position = {10.0F, 0.0F, 0.0F};
  RsmMeshSpec crown = Triangle("houppier");
  crown.parent_name = "tronc";
  crown.local_position = {0.0F, 57.0F, 0.0F};
  RsmBuildOptions options;
  options.major = 2;
  options.minor = 3;
  options.animation_length = 100;
  options.frame_rate = 30.0F;
  options.main_mesh_names = {"tronc"};
  options.meshes = {trunk, crown};
  return options;
}

}  // namespace

TEST_CASE("RSM2 : la matrice du fichier se lit en vecteur-ligne", "[render][model][rsm2]") {
  // Rangée pour le vecteur-LIGNE : la première rangée est l'image de l'axe X.
  RsmMeshSpec spec = Triangle("corps");
  spec.transform = {0.0F, 1.0F, 0.0F, -1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
  spec.local_position = {5.0F, 0.0F, 0.0F};
  RsmBuildOptions options = Tree();
  options.meshes = {spec};
  options.main_mesh_names = {"corps"};
  const RsmModel model = Parse(options);
  const glm::mat4 matrix = ro::render::FileNodeMatrix(model.meshes()[0]);
  CHECK(SamePoint(Apply(matrix, {1.0F, 0.0F, 0.0F}), {5.0F, 1.0F, 0.0F}));
}

TEST_CASE("RSM2 : au repos, un nœud se pose par SA matrice du fichier", "[render][model][rsm2]") {
  // Le natif compose « matrice du nœud · inverse de celle du parent » puis
  // remonte la hiérarchie : les deux s'annulent, et l'enfant finit posé par
  // sa seule matrice. Ajouter celle du parent le décalerait de dix.
  const std::vector<SecondFamilyNode> nodes = ro::render::SecondFamilyNodes(Parse(Tree()));
  REQUIRE(nodes.size() == 2);
  const std::vector<glm::mat4> poses = PoseSecondFamily(nodes, 0.0F);
  CHECK(SamePoint(Apply(poses[0], {0.0F, 0.0F, 0.0F}), {10.0F, 0.0F, 0.0F}));
  CHECK(SamePoint(Apply(poses[1], {0.0F, 0.0F, 0.0F}), {0.0F, 57.0F, 0.0F}));
}

TEST_CASE("RSM2 : le maillage cuit pose le houppier à sa hauteur", "[render][model][rsm2]") {
  // Prendre les sommets tels quels laissait le houppier au pied du tronc.
  const ro::render::ModelMesh mesh = ro::render::BuildModelMesh(Parse(Tree()));
  REQUIRE(mesh.vertices.size() == 6);
  CHECK(NearlyEqual(mesh.bounds_max[1], 58.0F));
  CHECK(NearlyEqual(mesh.bounds_max[0], 11.0F));
}

TEST_CASE("RSM2 : la rotation s'interpole entre deux images-clés", "[render][model][rsm2]") {
  SecondFamilyNode node;
  node.rotation_keys = {RsmRotationKey{.frame = 0, .quaternion = {0.0F, 0.0F, 0.0F, 1.0F}},
                        RsmRotationKey{.frame = 30, .quaternion = TurnAroundY(kQuarterTurn)}};
  const float half = std::numbers::sqrt2_v<float> / 2.0F;
  CHECK(
      SamePoint(Apply(SecondFamilyNodePose(node, 15.0F), {1.0F, 0.0F, 0.0F}), {half, 0.0F, -half}));
}

TEST_CASE("RSM2 : proches, deux rotations s'interpolent en LIGNE droite", "[render][model][rsm2]") {
  // Au-dessus d'un cosinus de 0,95 le natif mélange les quaternions sans les
  // renormaliser. À 36° d'écart, le milieu n'est donc pas tout à fait 18°.
  constexpr float kThirtySix = 36.0F * std::numbers::pi_v<float> / 180.0F;
  SecondFamilyNode node;
  node.rotation_keys = {RsmRotationKey{.frame = 0, .quaternion = {0.0F, 0.0F, 0.0F, 1.0F}},
                        RsmRotationKey{.frame = 10, .quaternion = TurnAroundY(kThirtySix)}};
  const glm::mat4 pose = SecondFamilyNodePose(node, 5.0F);
  CHECK(NearlyEqual(pose[0][0], 0.95225F));
  CHECK(NearlyEqual(pose[0][2], -0.30145F));
}

TEST_CASE("RSM2 : passé la dernière image-clé, le nœud GARDE sa pose", "[render][model][rsm2]") {
  // Contrairement à la première famille, qui retombe à l'identité.
  SecondFamilyNode node;
  node.rotation_keys = {RsmRotationKey{.frame = 0, .quaternion = {0.0F, 0.0F, 0.0F, 1.0F}},
                        RsmRotationKey{.frame = 30, .quaternion = TurnAroundY(kQuarterTurn)}};
  node.position_keys = {RsmPositionKey{.frame = 0, .position = {0.0F, 0.0F, 0.0F}},
                        RsmPositionKey{.frame = 30, .position = {0.0F, 8.0F, 0.0F}}};
  const glm::mat4 pose = SecondFamilyNodePose(node, 40.0F);
  CHECK(SamePoint(Apply(pose, {1.0F, 0.0F, 0.0F}), {0.0F, 8.0F, -1.0F}));
}

TEST_CASE("RSM2 : avant la première image-clé, position et échelle partent du repos",
          "[render][model][rsm2]") {
  // Position : celle du repos à l'image zéro. Échelle : l'unité.
  SecondFamilyNode node;
  node.rest = glm::mat4{1.0F};
  node.rest[3] = glm::vec4{0.0F, 4.0F, 0.0F, 1.0F};
  node.position_keys = {RsmPositionKey{.frame = 20, .position = {0.0F, 8.0F, 0.0F}},
                        RsmPositionKey{.frame = 40, .position = {0.0F, 12.0F, 0.0F}}};
  node.scale_keys = {RsmScaleKey{.frame = 20, .scale = {3.0F, 3.0F, 3.0F}},
                     RsmScaleKey{.frame = 40, .scale = {5.0F, 5.0F, 5.0F}}};
  const glm::mat4 pose = SecondFamilyNodePose(node, 10.0F);
  // Mi-chemin de 4 à 8 pour la position, de 1 à 3 pour l'échelle.
  CHECK(SamePoint(Apply(pose, {0.0F, 0.0F, 0.0F}), {0.0F, 6.0F, 0.0F}));
  CHECK(SamePoint(Apply(pose, {1.0F, 0.0F, 0.0F}), {2.0F, 6.0F, 0.0F}));
}

TEST_CASE("RSM2 : une texture tourne, PUIS s'étire, PUIS glisse", "[render][model][rsm2]") {
  // L'ordre du natif. Étirer avant de tourner enverrait ce point à 1,5.
  const TextureMotion motion{
      .offset_u = 0.1F, .offset_v = 0.0F, .scale_u = 2.0F, .scale_v = 1.0F, .angle = kQuarterTurn};
  const std::array<float, 2> moved = MoveTextureCoordinate(motion, {1.0F, 0.5F});
  CHECK(NearlyEqual(moved[0], 0.6F));
  CHECK(NearlyEqual(moved[1], 1.0F));
}

TEST_CASE("RSM2 : les grandeurs de texture s'interpolent et se figent au bout",
          "[render][model][rsm2]") {
  const std::vector<RsmTextureAnimation> animations = {
      RsmTextureAnimation{.texture_index = 1,
                          .motion = 1,
                          .keys = {{.frame = 0, .value = 0.0F}, {.frame = 60, .value = 1.0F}}},
      RsmTextureAnimation{.texture_index = 1, .motion = 2, .keys = {{.frame = 10, .value = 3.0F}}},
      RsmTextureAnimation{.texture_index = 0, .motion = 0, .keys = {{.frame = 0, .value = 9.0F}}}};
  const TextureMotion middle = TextureMotionAt(animations, 1, 15.0F);
  CHECK(NearlyEqual(middle.offset_v, 0.25F));
  // Une seule image-clé vaut aussi avant son numéro.
  CHECK(NearlyEqual(TextureMotionAt(animations, 1, 0.0F).scale_u, 3.0F));
  CHECK(NearlyEqual(TextureMotionAt(animations, 1, 500.0F).offset_v, 1.0F));
  // Une autre texture du nœud ne s'en mêle pas.
  CHECK(NearlyEqual(middle.offset_u, 0.0F));
  CHECK(NearlyEqual(TextureMotionAt(animations, 0, 5.0F).offset_u, 9.0F));
}

TEST_CASE("Horloge RSM2 : des millisecondes, pas des crans", "[render][model][clock][rsm2]") {
  // 100 images à 30 par seconde : une boucle de 3 333 ms.
  const double duration = SecondFamilyDurationMs(100, 30.0F);
  CHECK(NearlyEqual(static_cast<float>(duration), 3333.333F, 0.01F));
  CHECK(SecondFamilyPlayedAt(1000, kModelPlayLooped, 1.0F, duration).played_ms == 1000);
  CHECK(NearlyEqual(SecondFamilyFrameAt(1000, 30.0F), 30.0F));
  // La vitesse multiplie le temps, sans le tiers de la première famille.
  CHECK(SecondFamilyPlayedAt(1000, kModelPlayLooped, 2.0F, duration).played_ms == 2000);
  // Au bout de la boucle, le départ se reporte ; jouée une fois, elle s'arrête
  // une milliseconde avant la fin.
  CHECK(SecondFamilyPlayedAt(3400, kModelPlayLooped, 1.0F, duration).restart);
  CHECK(SecondFamilyPlayedAt(3400, kModelPlayOnce, 1.0F, duration).played_ms == 3332);
  // Tout autre type fige à l'image zéro.
  CHECK(SecondFamilyPlayedAt(1000, 0, 1.0F, duration).played_ms == 0);
}

TEST_CASE("Horloge RSM2 : une texture qui glisse s'anime, même à vitesse infime",
          "[render][model][clock][rsm2]") {
  RsmBuildOptions options = Tree();
  options.meshes[0].texture_animations = {RsmTextureAnimationSpec{
      .texture_index = 0,
      .motion = 0,
      .keys = {{.frame = 0, .value = 0.0F}, {.frame = 100, .value = 1.0F}}}};
  const RsmModel model = Parse(options);
  ro::render::ModelInstances instances;
  instances.mesh = ro::render::BuildModelMesh(model);
  instances.rig = ro::render::BuildModelRig(model, instances.mesh);
  REQUIRE_FALSE(instances.rig.empty());
  REQUIRE(instances.rig.second_family());

  ro::formats::RswModel crawling;
  crawling.animation_type = kModelPlayLooped;
  crawling.animation_speed = 0.01F;
  CHECK(ro::render::AddInstance(instances, glm::mat4{1.0F}, crawling));

  const std::vector<ro::render::ModelInstances> models = {instances};
  ro::render::ModelClock clock;
  clock.Start(models, 0);
  clock.Advance(models, 100000);
  // Cent secondes à 0,01 : une seconde jouée, trente images.
  CHECK(NearlyEqual(clock.Frame(0, 0), 30.0F));
  const auto motions = clock.TextureMotions(0, 0);
  REQUIRE_FALSE(motions.empty());
  CHECK(NearlyEqual(motions[0].offset_u, 0.3F));
}
