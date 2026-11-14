#include "ro/render/model_pose.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/formats/rsm.h"
#include "ro/render/model_clock.h"
#include "ro/render/model_mesh.h"

#include "support/model_builder.h"

using ro::formats::RsmModel;
using ro::formats::RsmRotationKey;
using ro::render::AxisAngleQuaternion;
using ro::render::FirstFamilyNode;
using ro::render::FirstFamilyRotationAt;
using ro::render::FirstFamilyStaysStill;
using ro::render::kModelPlayLooped;
using ro::render::kModelPlayOnce;
using ro::render::NextFirstFamilyFrame;
using ro::render::PoseFirstFamily;
using ro::render::QuaternionMatrix;

namespace {

constexpr float kQuarterTurn = std::numbers::pi_v<float> / 2.0F;

bool NearlyEqual(float actual, float expected) { return std::abs(actual - expected) < 1e-4F; }

// Où la matrice envoie le point (1, 0, 0).
glm::vec3 ImageOfX(const glm::mat4& matrix) {
  return glm::vec3(matrix * glm::vec4{1.0F, 0.0F, 0.0F, 1.0F});
}

bool SamePoint(const glm::vec3& actual, const glm::vec3& expected) {
  return NearlyEqual(actual.x, expected.x) && NearlyEqual(actual.y, expected.y) &&
         NearlyEqual(actual.z, expected.z);
}

std::array<float, 4> TurnAroundY(float angle_radians) {
  return AxisAngleQuaternion({0.0F, 1.0F, 0.0F}, angle_radians);
}

}  // namespace

TEST_CASE("Pose : un quart de tour autour de Y envoie X vers -Z", "[render][model][pose]") {
  // La convention se vérifie sur un point, pas sur une matrice : c'est ce que
  // voit l'œil. glm tourne pareil, et le natif aussi (formule D3DX).
  const glm::mat4 turn = QuaternionMatrix(TurnAroundY(kQuarterTurn));
  CHECK(SamePoint(ImageOfX(turn), {0.0F, 0.0F, -1.0F}));
}

TEST_CASE("Pose : un angle infime n'est pas jugé négligeable", "[render][model][pose]") {
  // Le natif fabrique son quaternion sans seuil. Écarter les angles sous un
  // centième de radian, comme on le faisait, redressait des nœuds penchés.
  FirstFamilyNode node;
  node.rest_rotation = TurnAroundY(0.005F);
  CHECK(FirstFamilyRotationAt(node, 0)[0][2] < -0.004F);
}

TEST_CASE("Pose : la première image-clé l'emporte sur l'angle au repos", "[render][model][pose]") {
  // Dès qu'un nœud porte des images-clés, le natif ignore son angle : il n'en
  // fait une image-clé QUE s'il n'en a aucune. 27 331 objets des cartes sont
  // dans ce cas, et leur pose au repos était fausse.
  FirstFamilyNode node;
  node.rest_rotation = TurnAroundY(kQuarterTurn);
  node.rotation_keys = {RsmRotationKey{.frame = 0, .quaternion = {0.0F, 0.0F, 0.0F, 1.0F}},
                        RsmRotationKey{.frame = 100, .quaternion = TurnAroundY(kQuarterTurn)}};
  CHECK(SamePoint(ImageOfX(FirstFamilyRotationAt(node, 0)), {1.0F, 0.0F, 0.0F}));
}

TEST_CASE("Pose : à mi-chemin de deux images-clés, la moitié de l'angle", "[render][model][pose]") {
  FirstFamilyNode node;
  node.rotation_keys = {RsmRotationKey{.frame = 0, .quaternion = {0.0F, 0.0F, 0.0F, 1.0F}},
                        RsmRotationKey{.frame = 100, .quaternion = TurnAroundY(kQuarterTurn)}};
  const float half = std::numbers::sqrt2_v<float> / 2.0F;
  CHECK(SamePoint(ImageOfX(FirstFamilyRotationAt(node, 50)), {half, 0.0F, -half}));
}

TEST_CASE("Pose : l'interpolation prend le chemin le plus court", "[render][model][pose]") {
  // q et -q décrivent la même rotation. Sans retourner l'arrivée quand le
  // cosinus est négatif, le nœud ferait trois quarts de tour à l'envers.
  FirstFamilyNode node;
  const std::array<float, 4> quarter = TurnAroundY(kQuarterTurn);
  node.rotation_keys = {
      RsmRotationKey{.frame = 0, .quaternion = {0.0F, 0.0F, 0.0F, 1.0F}},
      RsmRotationKey{.frame = 100,
                     .quaternion = {-quarter[0], -quarter[1], -quarter[2], -quarter[3]}}};
  const float half = std::numbers::sqrt2_v<float> / 2.0F;
  CHECK(SamePoint(ImageOfX(FirstFamilyRotationAt(node, 50)), {half, 0.0F, -half}));
}

TEST_CASE("Pose : passé sa dernière image-clé, le nœud ne tourne PLUS", "[render][model][pose]") {
  // Le natif rend l'identité, pas la dernière rotation. Garder la
  // dernière, comme le ferait une interpolation ordinaire, figerait le nœud de
  // travers sur la fin d'une boucle.
  FirstFamilyNode node;
  node.rotation_keys = {RsmRotationKey{.frame = 0, .quaternion = TurnAroundY(kQuarterTurn)},
                        RsmRotationKey{.frame = 100, .quaternion = TurnAroundY(kQuarterTurn)}};
  CHECK(SamePoint(ImageOfX(FirstFamilyRotationAt(node, 99)), {0.0F, 0.0F, -1.0F}));
  CHECK(SamePoint(ImageOfX(FirstFamilyRotationAt(node, 100)), {1.0F, 0.0F, 0.0F}));
  CHECK(SamePoint(ImageOfX(FirstFamilyRotationAt(node, 150)), {1.0F, 0.0F, 0.0F}));
}

TEST_CASE("Pose : une image-clé unique vaut pour toutes les images", "[render][model][pose]") {
  FirstFamilyNode node;
  node.rotation_keys = {RsmRotationKey{.frame = 37, .quaternion = TurnAroundY(kQuarterTurn)}};
  CHECK(SamePoint(ImageOfX(FirstFamilyRotationAt(node, 0)), {0.0F, 0.0F, -1.0F}));
  CHECK(SamePoint(ImageOfX(FirstFamilyRotationAt(node, 5000)), {0.0F, 0.0F, -1.0F}));
}

TEST_CASE("Pose : deux images-clés de partie réelle UN donnent l'identité",
          "[render][model][pose]") {
  // Le natif ne les interpole pas, quel que soit le reste de leurs composantes.
  // ⚠ La composante qui dépasse porte sur Y : sur X, elle tournerait autour de
  // l'axe même que l'essai observe, et l'essai passerait sans le raccourci.
  FirstFamilyNode node;
  node.rotation_keys = {RsmRotationKey{.frame = 0, .quaternion = {0.0F, 0.5F, 0.0F, 1.0F}},
                        RsmRotationKey{.frame = 100, .quaternion = {0.0F, 0.5F, 0.0F, 1.0F}}};
  CHECK(SamePoint(ImageOfX(FirstFamilyRotationAt(node, 50)), {1.0F, 0.0F, 0.0F}));
}

TEST_CASE("Pose : un enfant tourne avec son parent", "[render][model][pose]") {
  // Le cœur de l'animation : l'aile d'un moulin est l'ENFANT de son moyeu.
  FirstFamilyNode hub;
  hub.rotation_keys = {RsmRotationKey{.frame = 0, .quaternion = {0.0F, 0.0F, 0.0F, 1.0F}},
                       RsmRotationKey{.frame = 100, .quaternion = TurnAroundY(kQuarterTurn)}};
  FirstFamilyNode blade;
  blade.parent = 0;
  blade.position = {10.0F, 0.0F, 0.0F};
  const std::vector<FirstFamilyNode> nodes = {hub, blade};

  const std::vector<glm::mat4> poses = PoseFirstFamily(nodes, 50);
  const glm::vec3 blade_origin = glm::vec3(poses[1] * glm::vec4{0.0F, 0.0F, 0.0F, 1.0F});
  const float half = 10.0F * std::numbers::sqrt2_v<float> / 2.0F;
  CHECK(SamePoint(blade_origin, {half, 0.0F, -half}));
}

TEST_CASE("Horloge RSM1 : trente-trois images par pas à la vitesse 1", "[render][model][clock]") {
  // `vitesse / 3 × 100`, tronqué APRÈS l'addition.
  CHECK(NextFirstFamilyFrame(0, kModelPlayLooped, 1.0F, 1000) == 33);
  CHECK(NextFirstFamilyFrame(33, kModelPlayLooped, 1.0F, 1000) == 66);
  CHECK(NextFirstFamilyFrame(0, kModelPlayLooped, 2.0F, 1000) == 66);
}

TEST_CASE("Horloge RSM1 : en boucle, l'image repart de ZÉRO, le reste perdu",
          "[render][model][clock]") {
  // 99 + 33 = 132 dépasse 100 : le natif repart de zéro, pas de 32.
  CHECK(NextFirstFamilyFrame(99, kModelPlayLooped, 1.0F, 100) == 0);
}

TEST_CASE("Horloge RSM1 : jouée une fois, l'image s'arrête sur la dernière",
          "[render][model][clock]") {
  CHECK(NextFirstFamilyFrame(99, kModelPlayOnce, 1.0F, 100) == 99);
  CHECK(NextFirstFamilyFrame(80, kModelPlayOnce, 1.0F, 100) == 99);
}

TEST_CASE("Horloge RSM1 : l'horloge de prontera, à 0,01, ne bouge jamais",
          "[render][model][clock]") {
  // Un cran de 0,33 image, tronqué, ne fait jamais avancer d'une image entière.
  CHECK(FirstFamilyStaysStill(kModelPlayLooped, 0.01F, 768000));
  CHECK_FALSE(FirstFamilyStaysStill(kModelPlayLooped, 1.0F, 768000));
  // Tout autre type que 1 ou 2 fige.
  CHECK(NextFirstFamilyFrame(40, 0, 1.0F, 1000) == 40);
  CHECK(FirstFamilyStaysStill(0, 1.0F, 1000));
}

namespace {

using ro::test_support::BuildRsm;
using ro::test_support::RsmBuildOptions;
using ro::test_support::RsmFaceSpec;
using ro::test_support::RsmMeshSpec;
using ro::test_support::RsmRotationKeySpec;

// Un modèle d'un seul nœud qui fait un quart de tour en cent images.
ro::render::ModelInstances TurningModel() {
  RsmMeshSpec mesh;
  mesh.name = "moyeu";
  mesh.texture_indices = {0};
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
  mesh.texture_vertices = {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}};
  mesh.faces = {RsmFaceSpec{.vertex_ids = {0, 1, 2}, .texture_vertex_ids = {0, 1, 2}}};
  mesh.rotation_keys = {RsmRotationKeySpec{.frame = 0, .quaternion = {0.0F, 0.0F, 0.0F, 1.0F}},
                        RsmRotationKeySpec{.frame = 1000, .quaternion = TurnAroundY(kQuarterTurn)}};
  RsmBuildOptions options;
  options.animation_length = 1000;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"moyeu"};
  options.meshes = {mesh};
  auto model = RsmModel::Parse(BuildRsm(options));
  REQUIRE(model.has_value());

  ro::render::ModelInstances instances;
  instances.mesh = ro::render::BuildModelMesh(*model);
  instances.rig = ro::render::BuildModelRig(*model, instances.mesh);
  REQUIRE_FALSE(instances.rig.empty());
  instances.animated.push_back({kModelPlayLooped, 1.0F, {glm::mat4{1.0F}}});
  return instances;
}

}  // namespace

TEST_CASE("Horloge : UN cran par appel, même après une longue attente", "[render][model][clock]") {
  // Le natif avance une fois par image dessinée, pourvu qu'un pas de 16 ms
  // soit passé, et ne rattrape jamais : une image qui traîne ralentit le décor
  // au lieu de le faire sauter.
  const std::vector<ro::render::ModelInstances> models = {TurningModel()};
  ro::render::ModelClock clock;
  clock.Start(models, 5000);
  CHECK(clock.Frame(0, 0) == 0);

  clock.Advance(models, 5010);  // moins d'un pas : rien
  CHECK(clock.Frame(0, 0) == 0);
  clock.Advance(models, 6000);  // soixante pas écoulés : un seul cran
  CHECK(clock.Frame(0, 0) == 33);
  clock.Advance(models, 6000);  // aucun pas de plus : rien
  CHECK(clock.Frame(0, 0) == 33);
  clock.Advance(models, 6016);
  CHECK(clock.Frame(0, 0) == 66);
}

TEST_CASE("Horloge : une autre liste du même décor garde l'image de chaque fichier",
          "[render][model][clock]") {
  // LE SOL GÉNÉRÉ SE RECENTRE : la nouvelle fenêtre range ses modèles
  // autrement, en pose d'autres ; le moulin qui tournait ne repart pas de zéro.
  ro::render::ModelInstances mill = TurningModel();
  mill.rsm_file = "moulin.rsm";
  ro::render::ModelInstances other = TurningModel();
  other.rsm_file = "puits.rsm";
  const std::vector<ro::render::ModelInstances> before = {mill};
  ro::render::ModelClock clock;
  clock.Start(before, 0);
  clock.Advance(before, 16);
  clock.Advance(before, 32);
  REQUIRE(clock.Frame(0, 0) == 66);

  const std::vector<ro::render::ModelInstances> after = {other, mill};
  clock.Rebase(before, after);
  CHECK(clock.Frame(0, 0) == 0);
  CHECK(clock.Frame(1, 0) == 66);
  // L'horloge garde son origine : le pas suivant est un cran, pas un départ.
  clock.Advance(after, 48);
  CHECK(clock.Frame(1, 0) == 99);
  CHECK(clock.Frame(0, 0) == 33);
}

TEST_CASE("Horloge : la pose suit l'image du groupe", "[render][model][clock]") {
  const std::vector<ro::render::ModelInstances> models = {TurningModel()};
  ro::render::ModelClock clock;
  clock.Start(models, 0);
  REQUIRE(clock.Pose(0, 0).size() == 1);
  CHECK(SamePoint(ImageOfX(clock.Pose(0, 0)[0]), {1.0F, 0.0F, 0.0F}));

  // Quinze crans : 495 images sur mille, donc presque la moitié du quart de tour.
  for (std::uint32_t beat = 16; beat <= 16 * 15; beat += 16) {
    clock.Advance(models, beat);
  }
  REQUIRE(clock.Frame(0, 0) == 495);
  const glm::vec3 turned = ImageOfX(clock.Pose(0, 0)[0]);
  const float angle = std::atan2(-turned.z, turned.x);
  CHECK(NearlyEqual(angle, kQuarterTurn * 0.495F));
  // Rien hors de ce que l'horloge connaît.
  CHECK(clock.Pose(1, 0).empty());
  CHECK(clock.Pose(0, 1).empty());
}

namespace {

using ro::render::ModelClockBlend;

constexpr std::uint32_t kStepMs = ro::render::kModelStepMs;
constexpr std::uint32_t kHalfStepMs = kStepMs / 2;
// Le cran d'une vitesse de un, et la longueur du modèle tournant.
constexpr float kNotchFrames = 33.0F;
constexpr float kTurningLength = 1000.0F;
// Les pas qui mènent la boucle du modèle tournant au bord de sa fin, puis au
// pas qui la fait repartir.
constexpr std::uint32_t kStepsBeforeLoopEnd = 30;

// L'angle que la pose du nœud a fait tourner.
float TurnedAngle(const ro::render::ModelClock& clock) {
  const glm::vec3 turned = ImageOfX(clock.Pose(0, 0)[0]);
  return std::atan2(-turned.z, turned.x);
}

}  // namespace

TEST_CASE("Horloge : entre deux pas, la pose glisse d'un cran à l'autre",
          "[render][model][clock][tick_blend]") {
  const std::vector<ro::render::ModelInstances> models = {TurningModel()};
  ro::render::ModelClock clock;
  clock.Start(models, 0);

  // Au pas : le cran est franchi, la pose montre encore le précédent.
  clock.Advance(models, kStepMs, ModelClockBlend::kBetweenSteps);
  CHECK(clock.Frame(0, 0) == kNotchFrames);
  CHECK(clock.ShownFrame(0, 0) == 0.0F);
  CHECK(NearlyEqual(TurnedAngle(clock), 0.0F));

  // À mi-pas : à mi-chemin.
  clock.Advance(models, kStepMs + kHalfStepMs, ModelClockBlend::kBetweenSteps);
  CHECK(clock.Frame(0, 0) == kNotchFrames);
  CHECK(clock.ShownFrame(0, 0) == kNotchFrames / 2.0F);
  // La première famille pose en images ENTIÈRES : 16,5 se lit 16.
  CHECK(NearlyEqual(TurnedAngle(clock),
                    kQuarterTurn * std::trunc(kNotchFrames / 2.0F) / kTurningLength));

  // ⚠ LE MÉLANGE COUPÉ, la pose revient au cran entier dès l'appel suivant.
  clock.Advance(models, kStepMs + kHalfStepMs, ModelClockBlend::kWholeSteps);
  CHECK(clock.ShownFrame(0, 0) == kNotchFrames);
  CHECK(NearlyEqual(TurnedAngle(clock), kQuarterTurn * kNotchFrames / kTurningLength));
}

TEST_CASE("Horloge : en crans entiers, les mêmes poses qu'une horloge sans mélange",
          "[render][model][clock][tick_blend]") {
  const std::vector<ro::render::ModelInstances> models = {TurningModel()};
  ro::render::ModelClock plain;
  ro::render::ModelClock whole;
  plain.Start(models, 0);
  whole.Start(models, 0);
  for (std::uint32_t now_ms = 0; now_ms <= kStepMs * kStepsBeforeLoopEnd; now_ms += kHalfStepMs) {
    plain.Advance(models, now_ms);
    whole.Advance(models, now_ms, ModelClockBlend::kWholeSteps);
    REQUIRE(whole.Frame(0, 0) == plain.Frame(0, 0));
    REQUIRE(whole.ShownFrame(0, 0) == plain.Frame(0, 0));
    REQUIRE(whole.Pose(0, 0)[0] == plain.Pose(0, 0)[0]);
  }
}

TEST_CASE("Horloge : une boucle qui repart ne se rejoue pas à rebours",
          "[render][model][clock][tick_blend]") {
  const std::vector<ro::render::ModelInstances> models = {TurningModel()};
  ro::render::ModelClock clock;
  clock.Start(models, 0);
  for (std::uint32_t step = 1; step <= kStepsBeforeLoopEnd + 1; ++step) {
    clock.Advance(models, step * kStepMs, ModelClockBlend::kBetweenSteps);
  }
  // Trente crans menaient à 990 ; le suivant dépasse la longueur et repart.
  REQUIRE(clock.Frame(0, 0) == 0.0F);
  clock.Advance(models, (kStepsBeforeLoopEnd + 1) * kStepMs + kHalfStepMs,
                ModelClockBlend::kBetweenSteps);
  CHECK(clock.ShownFrame(0, 0) == 0.0F);
}
