#include "ro/render/model_veil.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/image.h"
#include "ro/formats/rsm.h"
#include "ro/formats/rsw.h"
#include "ro/render/model_bake.h"
#include "ro/render/model_mesh.h"
#include "ro/render/scene_cutout.h"
#include "ro/render/veiled_prims.h"

#include "support/map_builder.h"
#include "support/model_builder.h"

using ro::render::EmitVeiledPose;
using ro::render::kVeilFloor;
using ro::render::kVeilFrameMs;
using ro::render::kVeilOpaque;
using ro::render::ModelInstances;
using ro::render::ModelMesh;
using ro::render::ModelVeil;
using ro::render::ModelVeils;
using ro::render::ModelVertex;
using ro::render::SightFrustum;
using ro::render::SightSegment;
using ro::render::StepVeil;
using ro::render::VeiledPose;
using ro::render::VeiledPrim;
using ro::test_support::RsmBuildOptions;
using ro::test_support::RsmFaceSpec;
using ro::test_support::RsmMeshSpec;

namespace {

constexpr std::int32_t kFirstFamily = 0x0104;
constexpr std::int32_t kSecondFamily = 0x0202;
// Du plein au plancher, par crans de cinq.
constexpr std::size_t kStepsToFloor = 33;

// Une vue qui voit tout, et une qui ne voit rien.
[[nodiscard]] SightFrustum SeeingAll() {
  SightFrustum frustum;
  frustum.planes.fill(glm::vec4(0.0F, 0.0F, 0.0F, 1.0F));
  return frustum;
}

[[nodiscard]] SightFrustum SeeingNothing() {
  SightFrustum frustum;
  frustum.planes.fill(glm::vec4(0.0F, 0.0F, 0.0F, -1.0F));
  return frustum;
}

// Une carte de quatre cubes sur six, sans quadtree au fichier, et un modèle de
// deux unités de côté posé une fois à sa place (0, 0, 0) du .rsw.
[[nodiscard]] ro::render::MapScene OneModelScene() {
  ro::test_support::GndBuildOptions ground;
  ground.width = 4;
  ground.height = 6;
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd(ground));
  REQUIRE(gnd.has_value());
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw({}));
  REQUIRE(rsw.has_value());
  ro::render::MapScene scene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
  ModelInstances model;
  model.mesh.bounds_min = {-1.0F, -2.0F, -1.0F};
  model.mesh.bounds_max = {1.0F, 0.0F, 1.0F};
  model.rsm_version = kFirstFamily;
  model.transforms.push_back(
      ro::render::ModelInstanceMatrix(ro::formats::RswModel{}, 4, 6, model.mesh, kFirstFamily));
  scene.models.push_back(std::move(model));
  return scene;
}

// Le segment du regard qui traverse ce modèle de part en part, et un qui passe
// à côté. ⚠ UN PEU PENCHÉS : parallèles à une tranche du quadtree, ils ne
// compteraient que ses sept dixièmes, et manqueraient la feuille du modèle.
[[nodiscard]] glm::vec3 Slant() { return glm::normalize(glm::vec3(1.0F, 0.01F, 0.01F)); }

[[nodiscard]] SightSegment Through() {
  return SightSegment{glm::vec3(-5.0F, -1.05F, 0.0F), Slant(), 20.0F};
}

[[nodiscard]] SightSegment Beside() {
  return SightSegment{glm::vec3(-5.0F, -1.05F, 9.0F), Slant(), 20.0F};
}

// Un maillage de deux faces, l'une qui tourne à gauche vue de face, l'autre à
// droite, chacune seule dans sa suite.
[[nodiscard]] ModelInstances TwoFaces(bool two_sided, std::int32_t rsm_version) {
  ModelInstances model;
  model.rsm_version = rsm_version;
  const auto vertex = [](float x, float y, float z) { return ModelVertex{.position = {x, y, z}}; };
  model.mesh.vertices = {vertex(0, 0, 0), vertex(1, 0, 0), vertex(0, 1, 0),
                         vertex(0, 0, 0), vertex(0, 1, 0), vertex(1, 0, 0)};
  model.mesh.two_sided = {static_cast<std::uint8_t>(two_sided ? 1 : 0),
                          static_cast<std::uint8_t>(two_sided ? 1 : 0)};
  model.mesh.prims = {{.texture_index = 0, .first_vertex = 0, .vertex_count = 3},
                      {.texture_index = 1, .first_vertex = 3, .vertex_count = 3}};
  model.transforms.emplace_back(1.0F);
  return model;
}

[[nodiscard]] std::vector<VeiledPrim> Emitted(const ModelInstances& model,
                                              const glm::mat4& view_projection,
                                              bool across = false) {
  std::vector<ModelVertex> vertices;
  std::vector<VeiledPrim> prims;
  EmitVeiledPose(VeiledPose{.opacity = kVeilFloor, .across = across}, model, {}, view_projection,
                 vertices, prims);
  return prims;
}

}  // namespace

TEST_CASE("Voile : marqué, il descend jusqu'au plancher puis la marque tombe", "[render][veil]") {
  ModelVeil veil;
  veil.marked = true;
  for (std::size_t step = 0; step < kStepsToFloor; ++step) {
    StepVeil(veil);
  }
  CHECK(veil.opacity == kVeilFloor);
  CHECK(veil.marked);
  StepVeil(veil);
  CHECK(veil.opacity == kVeilFloor);
  CHECK_FALSE(veil.marked);
  for (std::size_t step = 0; step < kStepsToFloor; ++step) {
    StepVeil(veil);
  }
  CHECK(veil.opacity == kVeilOpaque);
  StepVeil(veil);
  CHECK(veil.opacity == kVeilOpaque);
}

TEST_CASE("Voile : effleuré une seule image, il descend quand même jusqu'au bout",
          "[render][veil]") {
  ModelVeil veil;
  veil.marked = true;
  StepVeil(veil);
  CHECK(veil.opacity == kVeilOpaque - 5);
  for (std::size_t step = 1; step < kStepsToFloor; ++step) {
    StepVeil(veil);
  }
  CHECK(veil.opacity == kVeilFloor);
}

TEST_CASE("Voile : une pose traversée descend d'un cran par image du client", "[render][veil]") {
  const ro::render::MapScene scene = OneModelScene();
  ModelVeils veils;
  veils.Prepare(scene, nullptr);
  REQUIRE(veils.boxes().size() == 1);
  CHECK(veils.opacities()[0][0] == kVeilOpaque);

  veils.Advance(kVeilFrameMs * 3.5F, Through(), SeeingAll());
  CHECK(veils.opacities()[0][0] == kVeilOpaque - 15);
  REQUIRE(veils.veiled().size() == 1);
  CHECK(veils.veiled()[0].opacity == kVeilOpaque - 15);
  CHECK_FALSE(veils.veiled()[0].group.has_value());
  // La demi-image restée en compte fait le cran suivant — et la marque, restée,
  // fait descendre la pose que le segment ne traverse plus.
  veils.Advance(kVeilFrameMs * 0.6F, Beside(), SeeingAll());
  CHECK(veils.opacities()[0][0] == kVeilOpaque - 20);
}

TEST_CASE("Voile : hors de la vue, une pose reste où elle en est", "[render][veil]") {
  const ro::render::MapScene scene = OneModelScene();
  ModelVeils veils;
  veils.Prepare(scene, nullptr);
  veils.Advance(kVeilFrameMs * 2.0F, Through(), SeeingAll());
  veils.Advance(kVeilFrameMs * 4.0F, Through(), SeeingNothing());
  CHECK(veils.opacities()[0][0] == kVeilOpaque - 10);
  CHECK(veils.veiled().empty());
  veils.Clear();
  CHECK(veils.opacities()[0][0] == kVeilOpaque);
}

TEST_CASE("Voile : sans joueur rien ne se marque, mais le voile revient", "[render][veil]") {
  const ro::render::MapScene scene = OneModelScene();
  ModelVeils veils;
  veils.Prepare(scene, nullptr);
  // Une image traversée, puis trente-cinq sans segment : trente-deux crans
  // jusqu'au plancher, une image où la marque tombe, deux crans de retour.
  constexpr float kFramesWithout = 35.5F;
  veils.Advance(kVeilFrameMs * 1.0F, Through(), SeeingAll());
  veils.Advance(kVeilFrameMs * kFramesWithout, std::nullopt, SeeingAll());
  CHECK(veils.opacities()[0][0] == kVeilFloor + 10);
}

TEST_CASE("Voile : plusieurs regards marquent chacun, sans descendre plus vite", "[render][veil]") {
  const ro::render::MapScene scene = OneModelScene();
  ModelVeils veils;
  veils.Prepare(scene, nullptr);
  const std::array<SightSegment, 2> aside{Beside(), Beside()};
  veils.Advance(kVeilFrameMs * 2.0F, std::span<const SightSegment>(aside), SeeingAll());
  CHECK(veils.opacities()[0][0] == kVeilOpaque);
  // Le second regard seul traverse la pose ; trois qui la traversent ne la
  // font pas descendre plus d'un cran par image.
  const std::array<SightSegment, 2> one_through{Beside(), Through()};
  veils.Advance(kVeilFrameMs * 2.0F, std::span<const SightSegment>(one_through), SeeingAll());
  CHECK(veils.opacities()[0][0] == kVeilOpaque - 10);
  const std::array<SightSegment, 3> all_through{Through(), Through(), Through()};
  veils.Advance(kVeilFrameMs * 2.0F, std::span<const SightSegment>(all_through), SeeingAll());
  CHECK(veils.opacities()[0][0] == kVeilOpaque - 20);
}

TEST_CASE("Passe voilée : une face simple ne se montre que tournée vers l'œil", "[render][veil]") {
  const glm::mat4 flat(1.0F);
  const std::vector<VeiledPrim> single = Emitted(TwoFaces(false, kFirstFamily), flat);
  REQUIRE(single.size() == 1);
  CHECK(single[0].texture_index == 0);
  CHECK(Emitted(TwoFaces(true, kFirstFamily), flat).size() == 2);

  // EN MIROIR, la pose retourne ses faces à l'écran, et le client retourne son
  // tri avec : c'est la même face qui se montre.
  ModelInstances mirrored = TwoFaces(false, kFirstFamily);
  mirrored.transforms.front() = glm::scale(glm::mat4(1.0F), glm::vec3(-1.0F, 1.0F, 1.0F));
  const std::vector<VeiledPrim> shown = Emitted(mirrored, flat);
  REQUIRE(shown.size() == 1);
  CHECK(shown[0].texture_index == 0);

  // LA SECONDE FAMILLE retourne l'axe vertical de ses modèles dans leur pose,
  // et le client retourne son tri avec : une pose ordinaire y montre encore la
  // même face.
  ModelInstances second = TwoFaces(false, kSecondFamily);
  second.transforms.front() = glm::scale(glm::mat4(1.0F), glm::vec3(1.0F, -1.0F, 1.0F));
  const std::vector<VeiledPrim> second_shown = Emitted(second, flat);
  REQUIRE(second_shown.size() == 1);
  CHECK(second_shown[0].texture_index == 0);
}

TEST_CASE("Passe voilée : à cheval sur la vue, une face hors de l'écran s'écarte",
          "[render][veil]") {
  const glm::mat4 aside = glm::translate(glm::mat4(1.0F), glm::vec3(3.0F, 0.0F, 0.0F));
  const ModelInstances model = TwoFaces(true, kFirstFamily);
  CHECK(Emitted(model, aside, false).size() == 2);
  CHECK(Emitted(model, aside, true).empty());
}

TEST_CASE("Passe voilée : une TGA se mêle au fond, une BMP se découpe", "[render][veil]") {
  CHECK(ro::render::SceneCutoutOf(ro::formats::ImageFile::kTga).blended);
  CHECK_FALSE(ro::render::SceneCutoutOf(ro::formats::ImageFile::kBmp).blended);
  CHECK_FALSE(ro::render::SceneCutoutOf(ro::formats::ImageFile::kJpeg).blended);

  // Une plaque à deux textures, une BMP puis une TGA — le verre d'un puits —,
  // posée deux fois.
  ModelInstances model = TwoFaces(true, kFirstFamily);
  model.transforms.emplace_back(1.0F);
  model.textures = {std::make_shared<const ro::formats::Image>(
                        ro::formats::Image{.file = ro::formats::ImageFile::kBmp}),
                    std::make_shared<const ro::formats::Image>(
                        ro::formats::Image{.file = ro::formats::ImageFile::kTga})};
  const std::vector<std::uint8_t> blended = ro::render::BlendedTextures(model);
  CHECK(blended == std::vector<std::uint8_t>{0, 1});

  // LA PASSE TRANSLUCIDE N'EN REÇOIT QUE LA FACE À TGA, et de toute pose
  // pleine : la passe opaque l'a sautée.
  const std::vector<VeiledPose> poses = ro::render::BlendedPoses(model, 0, {});
  REQUIRE(poses.size() == 2);
  CHECK(poses[0].opacity == kVeilOpaque);
  std::vector<ModelVertex> vertices;
  std::vector<VeiledPrim> prims;
  EmitVeiledPose(poses[0], model, {}, glm::mat4(1.0F), vertices, prims, blended);
  REQUIRE(prims.size() == 1);
  CHECK(prims[0].texture_index == 1);

  // UNE POSE VOILÉE y passe déjà tout entière : elle n'y revient pas.
  const VeiledPose veiled{.index = 1, .opacity = kVeilFloor};
  const std::vector<VeiledPose> rest = ro::render::BlendedPoses(model, 0, std::span(&veiled, 1));
  REQUIRE(rest.size() == 1);
  CHECK(rest[0].index == 0);

  // UNE POSE D'OPACITÉ NULLE n'a plus aucune face : elle n'y passe pas.
  const ro::render::ModelOpacities first_hidden{{0, kVeilOpaque}};
  const std::vector<VeiledPose> shown = ro::render::BlendedPoses(model, 0, {}, &first_hidden);
  REQUIRE(shown.size() == 1);
  CHECK(shown[0].index == 1);

  // Sans TGA, aucune pose pleine n'y passe.
  model.textures.pop_back();
  CHECK(ro::render::BlendedPoses(model, 0, {}).empty());
}

TEST_CASE("Passe voilée : la suite la plus lointaine passe d'abord", "[render][veil]") {
  // L'inverse de la profondeur du premier sommet ÉMIS : une face écartée ne
  // compte pas, c'est la suivante qui donne la clé.
  std::vector<VeiledPrim> prims = {
      VeiledPrim{.texture_index = 0, .sort_key = 0.5F},
      VeiledPrim{.texture_index = 1, .sort_key = 0.1F},
      VeiledPrim{.texture_index = 2, .sort_key = 0.5F},
  };
  ro::render::SortVeiledPrims(prims);
  CHECK(prims[0].texture_index == 1);
  CHECK(prims[1].texture_index == 0);
  CHECK(prims[2].texture_index == 2);

  const glm::mat4 perspective = glm::perspective(glm::radians(60.0F), 1.0F, 1.0F, 100.0F) *
                                glm::translate(glm::mat4(1.0F), glm::vec3(0.0F, 0.0F, -4.0F));
  const std::vector<VeiledPrim> emitted = Emitted(TwoFaces(true, kFirstFamily), perspective);
  REQUIRE(emitted.size() == 2);
  CHECK(emitted[0].sort_key == 0.25F);
}

TEST_CASE("Maillage : une suite regroupe les faces consécutives d'une même texture",
          "[render][veil]") {
  RsmMeshSpec mesh;
  mesh.name = "corps";
  mesh.texture_indices = {0, 1};
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
  mesh.texture_vertices = {{0.0F, 0.0F}};
  const auto face = [](std::uint16_t texture, bool two_sided) {
    return RsmFaceSpec{.vertex_ids = {0, 1, 2}, .texture_id = texture, .two_sided = two_sided};
  };
  mesh.faces = {face(0, false), face(0, true), face(1, true), face(0, false)};
  RsmBuildOptions options;
  options.textures = {"a.bmp", "b.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};
  auto model = ro::formats::RsmModel::Parse(ro::test_support::BuildRsm(options));
  REQUIRE(model.has_value());
  const ModelMesh built = ro::render::BuildModelMesh(*model);

  REQUIRE(built.prims.size() == 3);
  CHECK(built.prims[0].vertex_count == 6);
  CHECK(built.prims[1].texture_index == 1);
  CHECK(built.prims[1].first_vertex == 9);
  CHECK(built.prims[2].texture_index == 0);
  CHECK(built.prims[2].first_vertex == 6);
  CHECK(built.two_sided == std::vector<std::uint8_t>{0, 1, 0, 1});
}

TEST_CASE("Maillage : le mot du double face se lit selon la famille", "[render][veil]") {
  // La première famille tient pour double tout mot non nul ; la seconde, le
  // seul 1.
  RsmMeshSpec mesh;
  mesh.name = "corps";
  mesh.texture_indices = {0};
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
  mesh.texture_vertices = {{0.0F, 0.0F}};
  const auto face = [](std::int32_t word) {
    return RsmFaceSpec{.vertex_ids = {0, 1, 2}, .two_sided_word = word};
  };
  mesh.faces = {face(0), face(1), face(2), face(-1)};
  RsmBuildOptions options;
  options.textures = {"a.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};
  const auto sides = [&options](std::uint8_t major, std::uint8_t minor) {
    options.major = major;
    options.minor = minor;
    auto model = ro::formats::RsmModel::Parse(ro::test_support::BuildRsm(options));
    REQUIRE(model.has_value());
    return ro::render::BuildModelMesh(*model).two_sided;
  };
  CHECK(sides(1, 4) == std::vector<std::uint8_t>{0, 1, 1, 1});
  CHECK(sides(2, 2) == std::vector<std::uint8_t>{0, 1, 0, 0});
}

TEST_CASE("Maillage : le miroir d'une pose se lit selon sa famille", "[render][veil]") {
  ModelMesh mesh;
  ro::formats::RswModel placement;
  for (const std::int32_t version : {kFirstFamily, kSecondFamily}) {
    placement.scale = {1.0F, 2.0F, 3.0F};
    CHECK_FALSE(ro::render::InstanceMirrored(
        ro::render::ModelInstanceMatrix(placement, 4, 6, mesh, version), version));
    placement.scale = {-1.0F, 2.0F, 3.0F};
    CHECK(ro::render::InstanceMirrored(
        ro::render::ModelInstanceMatrix(placement, 4, 6, mesh, version), version));
    placement.scale = {-1.0F, -2.0F, 3.0F};
    CHECK_FALSE(ro::render::InstanceMirrored(
        ro::render::ModelInstanceMatrix(placement, 4, 6, mesh, version), version));
  }
}

TEST_CASE("Cuisson : chaque surface garde son bloc, pose après pose", "[render][veil]") {
  ModelMesh mesh;
  mesh.vertices.resize(9);
  mesh.surfaces = {{.texture_index = 0, .first_vertex = 0, .vertex_count = 3},
                   {.texture_index = 1, .first_vertex = 3, .vertex_count = 6}};
  const std::vector<glm::mat4> poses(2, glm::mat4(1.0F));
  const ro::render::BakedModel baked = ro::render::BakeInstances(mesh, poses, kFirstFamily);
  REQUIRE(baked.surfaces.size() == 2);
  CHECK(baked.surfaces[0].first_vertex == 0);
  CHECK(baked.surfaces[0].vertices_per_pose == 3);
  CHECK(baked.surfaces[1].first_vertex == 6);
  CHECK(baked.surfaces[1].vertices_per_pose == 6);
}
