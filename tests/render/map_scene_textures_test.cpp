// Les textures des modèles d'une carte : lues une fois par nom, et partagées.
//
// 🔴 DEUX MODÈLES CITENT SOUVENT LA MÊME TEXTURE — morocc, 256 citations pour
// 146 fichiers —, et chaque citation la relisait, la redécodait et la gardait en
// double. Un nom se lit désormais une fois pour toute la carte.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"
#include "ro/render/map_scene.h"
#include "ro/vfs/vfs.h"

#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/map_builder.h"
#include "support/model_builder.h"
#include "support/temp_directory.h"

using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;
using ro::test_support::BuildGnd;
using ro::test_support::BuildGrf;
using ro::test_support::BuildRsm;
using ro::test_support::BuildRsw;
using ro::test_support::GndBuildOptions;
using ro::test_support::GrfFileSpec;
using ro::test_support::RsmBuildOptions;
using ro::test_support::RsmFaceSpec;
using ro::test_support::RsmMeshSpec;
using ro::test_support::RswBuildOptions;
using ro::test_support::RswModelSpec;
using ro::test_support::TempDirectory;

namespace {

// Un modèle de deux triangles, un par texture : celle qu'on partage, dans la
// casse demandée, et une qui n'existe pas. ⚠ UNE FACE PAR TEXTURE : le maillage
// ne garde que les textures qu'une face emploie.
std::vector<std::uint8_t> ModelCiting(const std::string& shared_name) {
  RsmMeshSpec mesh;
  mesh.name = "corps";
  mesh.texture_indices = {0, 1};
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
  mesh.texture_vertices = {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}};
  mesh.faces = {
      RsmFaceSpec{.vertex_ids = {0, 1, 2}, .texture_vertex_ids = {0, 1, 2}, .texture_id = 0},
      RsmFaceSpec{.vertex_ids = {0, 2, 1}, .texture_vertex_ids = {0, 2, 1}, .texture_id = 1}};
  RsmBuildOptions options;
  options.textures = {shared_name, "absente.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};
  return BuildRsm(options);
}

// Une carte qui pose deux modèles DISTINCTS, et leur texture commune.
void FillClient(const TempDirectory& client) {
  RswBuildOptions rsw;
  rsw.models = {RswModelSpec{.name = "a", .model_name = "a.rsm"},
                RswModelSpec{.name = "b", .model_name = "b.rsm"}};
  const std::vector<BmpPixel> pixels(4, BmpPixel{.red = 200, .green = 120, .blue = 40});
  client.WriteFile("carte.grf",
                   BuildGrf({GrfFileSpec{"data\\essai.rsw", BuildRsw(rsw)},
                             GrfFileSpec{"data\\essai.gnd", BuildGnd(GndBuildOptions{})},
                             GrfFileSpec{"data\\model\\a.rsm", ModelCiting("Partagee.bmp")},
                             GrfFileSpec{"data\\model\\b.rsm", ModelCiting("partagee.bmp")},
                             GrfFileSpec{"data\\texture\\partagee.bmp", BuildBmp(2, 2, pixels)}}));
}

// L'image que ce modèle porte pour ce nom, écrit dans l'une ou l'autre casse, ou
// nul si le maillage ne le cite pas.
// ⚠ PAR LE NOM, JAMAIS PAR LE RANG : le maillage range ses textures par nom, et
// une majuscule change leur ordre.
const std::shared_ptr<const ro::formats::Image>* ImageNamed(const ro::render::ModelInstances& model,
                                                            std::string_view lower,
                                                            std::string_view upper) {
  for (std::size_t rank = 0; rank < model.mesh.textures.size(); ++rank) {
    if (model.mesh.textures[rank] == lower || model.mesh.textures[rank] == upper) {
      return &model.textures[rank];
    }
  }
  return nullptr;
}

}  // namespace

TEST_CASE("Carte : une texture que deux modèles citent se lit une fois", "[render][map]") {
  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"carte.grf"}});
  REQUIRE(vfs.has_value());

  auto scene = ro::render::LoadMapScene(*vfs, "essai");
  REQUIRE(scene.has_value());
  REQUIRE(scene->models.size() == 2);
  const ro::render::ModelInstances& a = scene->models[0];
  const ro::render::ModelInstances& b = scene->models[1];
  REQUIRE(a.textures.size() == 2);
  REQUIRE(b.textures.size() == 2);

  // ⚠ LES DEUX CASSES DÉSIGNENT LE MÊME FICHIER, et c'est la même image que les
  // deux modèles portent — pas deux copies égales.
  const auto* shared_a = ImageNamed(a, "partagee.bmp", "Partagee.bmp");
  const auto* shared_b = ImageNamed(b, "partagee.bmp", "Partagee.bmp");
  REQUIRE(shared_a != nullptr);
  REQUIRE(shared_b != nullptr);
  REQUIRE(*shared_a != nullptr);
  CHECK(*shared_a == *shared_b);
  CHECK((*shared_a)->width == 2);

  // L'absente aussi ne se cherche qu'une fois, et reste nulle pour les deux.
  const auto* absent_a = ImageNamed(a, "absente.bmp", "absente.bmp");
  const auto* absent_b = ImageNamed(b, "absente.bmp", "absente.bmp");
  REQUIRE(absent_a != nullptr);
  REQUIRE(absent_b != nullptr);
  CHECK(*absent_a == nullptr);
  CHECK(*absent_b == nullptr);
  CHECK(scene->missing_model_textures == std::vector<std::string>{"absente.bmp"});

  CHECK(scene->model_textures_cited == 4);
  CHECK(scene->model_textures_read == 2);
}

TEST_CASE("Carte : une BMP du sol ou du décor prend la clé du jeu, un effet le magenta pur",
          "[render][map]") {
  // Un pixel presque magenta : le jeu, qui téléverse en cinq bits par canal,
  // le confond avec le magenta. Un effet, lui, ne connaît que le magenta pur.
  const TempDirectory client;
  const std::vector<BmpPixel> pixels = {{.red = 250, .green = 5, .blue = 252},
                                        {.red = 200, .green = 120, .blue = 40}};
  client.WriteFile("carte.grf",
                   BuildGrf({GrfFileSpec{"data\\texture\\cle.bmp", BuildBmp(2, 1, pixels)}}));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"carte.grf"}});
  REQUIRE(vfs.has_value());

  const auto decor = ro::render::LoadSceneTexture(*vfs, "cle.bmp");
  REQUIRE(decor.has_value());
  CHECK(decor->file == ro::formats::ImageFile::kBmp);
  CHECK(decor->pixels == std::vector<std::uint8_t>{0, 0, 0, 0, 200, 120, 40, 255});

  const auto effect =
      ro::render::LoadSceneTexture(*vfs, "cle.bmp", ro::render::SceneTextureKey::kEffect);
  REQUIRE(effect.has_value());
  CHECK(effect->pixels == std::vector<std::uint8_t>{250, 5, 252, 255, 200, 120, 40, 255});
}
