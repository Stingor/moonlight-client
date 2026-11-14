// UNE SCÈNE ASSEMBLÉE EN MÉMOIRE EST CELLE QUE LA CARTE LUE DONNE : le sol et
// le décor passés à `AssembleMapScene` rendent, champ à champ, la scène que
// `LoadMapScene` tire des mêmes fichiers. C'est la voie d'un sol généré.

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/mapedit/map_edit.h"
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

// Un modèle d'un triangle texturé.
std::vector<std::uint8_t> OneTriangle() {
  RsmMeshSpec mesh;
  mesh.name = "corps";
  mesh.texture_indices = {0};
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {4.0F, 0.0F, 0.0F}, {0.0F, 4.0F, 0.0F}};
  mesh.texture_vertices = {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}};
  mesh.faces = {
      RsmFaceSpec{.vertex_ids = {0, 1, 2}, .texture_vertex_ids = {0, 1, 2}, .texture_id = 0}};
  RsmBuildOptions options;
  options.textures = {"ecorce.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};
  return BuildRsm(options);
}

// Le sol : quatre cubes sur deux textures, deux lightmaps, des tuiles teintées
// et un cube plus haut.
GndBuildOptions Ground() {
  GndBuildOptions options;
  options.major = 1;
  options.minor = 7;
  options.width = 4;
  options.height = 3;
  options.texture_paths = {"herbe.bmp", "terre.bmp"};
  options.lightmap_count = 2;
  options.lightmap_shadow = 0xC0;
  options.tile_count = 2;
  options.tile_texture_indices = {0, 1};
  options.tile_colors = {{0xFF, 0xFF, 0xFF, 0xFF}, {0x80, 0xC0, 0xA0, 0xFF}};
  for (std::int32_t rank = 0; rank < options.width * options.height; ++rank) {
    const float lifted = rank == 5 ? -6.0F : 0.0F;
    options.cubes.push_back(
        {.corner_height = {lifted, lifted, lifted, lifted}, .tile_up = rank % 2});
  }
  return options;
}

RswBuildOptions Decor() {
  RswBuildOptions options;
  options.light_longitude = 120;
  options.light_latitude = 40;
  options.models = {
      RswModelSpec{.name = "a", .model_name = "arbre.rsm", .position = {-5.0F, 0.0F, 3.0F}},
      RswModelSpec{.name = "b",
                   .model_name = "arbre.rsm",
                   .position = {4.0F, -2.0F, -6.0F},
                   .rotation = {0.0F, 45.0F, 0.0F}},
      RswModelSpec{.name = "c", .model_name = "absent.rsm"}};
  return options;
}

void FillClient(const TempDirectory& client) {
  const std::vector<BmpPixel> grass(4, BmpPixel{.red = 40, .green = 160, .blue = 50});
  const std::vector<BmpPixel> soil(4, BmpPixel{.red = 120, .green = 90, .blue = 50});
  client.WriteFile("carte.grf",
                   BuildGrf({GrfFileSpec{"data\\essai.rsw", BuildRsw(Decor())},
                             GrfFileSpec{"data\\essai.gnd", BuildGnd(Ground())},
                             GrfFileSpec{"data\\model\\arbre.rsm", OneTriangle()},
                             GrfFileSpec{"data\\texture\\herbe.bmp", BuildBmp(2, 2, grass)},
                             GrfFileSpec{"data\\texture\\terre.bmp", BuildBmp(2, 2, soil)},
                             GrfFileSpec{"data\\texture\\ecorce.bmp", BuildBmp(2, 2, soil)}}));
}

}  // namespace

TEST_CASE("Scène : la carte assemblée en mémoire est celle que ses fichiers donnent",
          "[render][map]") {
  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"carte.grf"}});
  REQUIRE(vfs.has_value());
  const ro::render::MapSceneOptions options{.edit = ro::mapedit::MapEdit()};

  auto loaded = ro::render::LoadMapScene(*vfs, "essai", options);
  REQUIRE(loaded.has_value());
  auto gnd = ro::formats::GndMap::Parse(BuildGnd(Ground()));
  REQUIRE(gnd.has_value());
  auto rsw = ro::formats::RswScene::Parse(BuildRsw(Decor()));
  REQUIRE(rsw.has_value());
  const ro::render::MapScene assembled =
      ro::render::AssembleMapScene(*vfs, "essai", std::move(*gnd), std::move(*rsw), options);

  // Le sol, sa lumière et son maillage.
  CHECK(assembled.gnd.width() == loaded->gnd.width());
  CHECK(assembled.atlas.image.pixels == loaded->atlas.image.pixels);
  CHECK(assembled.light.direction == loaded->light.direction);
  CHECK(assembled.light.ambient == loaded->light.ambient);
  CHECK(assembled.terrain_textures.size() == 2);
  CHECK(assembled.missing_terrain_textures == loaded->missing_terrain_textures);
  CHECK(assembled.invisible_tiles == loaded->invisible_tiles);
  REQUIRE_FALSE(assembled.terrain.vertices.empty());
  REQUIRE(assembled.terrain.vertices.size() == loaded->terrain.vertices.size());
  for (std::size_t rank = 0; rank < loaded->terrain.vertices.size(); ++rank) {
    CHECK(assembled.terrain.vertices[rank].position == loaded->terrain.vertices[rank].position);
    CHECK(assembled.terrain.vertices[rank].texture_uv == loaded->terrain.vertices[rank].texture_uv);
    CHECK(assembled.terrain.vertices[rank].lightmap_uv ==
          loaded->terrain.vertices[rank].lightmap_uv);
    CHECK(assembled.terrain.vertices[rank].color == loaded->terrain.vertices[rank].color);
  }

  // Le décor : le modèle lu une fois, ses deux poses, l'absent compté.
  CHECK(assembled.model_instance_count == 2);
  CHECK(assembled.model_instance_count == loaded->model_instance_count);
  CHECK(assembled.unreadable_models == loaded->unreadable_models);
  CHECK(assembled.model_keys == loaded->model_keys);
  REQUIRE(assembled.models.size() == loaded->models.size());
  for (std::size_t rank = 0; rank < loaded->models.size(); ++rank) {
    CHECK(assembled.models[rank].rsm_file == loaded->models[rank].rsm_file);
    CHECK(assembled.models[rank].transforms == loaded->models[rank].transforms);
    CHECK(assembled.models[rank].rsw_ranks == loaded->models[rank].rsw_ranks);
  }
  CHECK(assembled.water.zones.size() == loaded->water.zones.size());
  CHECK(assembled.fog.has_value() == loaded->fog.has_value());
}

TEST_CASE("Scène : une scène déjà lue prête ses textures et ses modèles, sans fichier",
          "[render][map]") {
  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"carte.grf"}});
  REQUIRE(vfs.has_value());
  auto loaded = ro::render::LoadMapScene(*vfs, "essai", {.edit = ro::mapedit::MapEdit()});
  REQUIRE(loaded.has_value());

  // Une installation VIDE : tout ce qui s'y lirait manquerait.
  const TempDirectory empty;
  empty.WriteFile("vide.grf", BuildGrf({}));
  auto bare = ro::vfs::Vfs::Mount({.root = empty.path(), .archives = {"vide.grf"}});
  REQUIRE(bare.has_value());
  auto gnd = ro::formats::GndMap::Parse(BuildGnd(Ground()));
  REQUIRE(gnd.has_value());
  auto rsw = ro::formats::RswScene::Parse(BuildRsw(Decor()));
  REQUIRE(rsw.has_value());
  const ro::render::MapScene kept =
      ro::render::AssembleMapScene(*bare, "essai", std::move(*gnd), std::move(*rsw),
                                   {.edit = ro::mapedit::MapEdit(), .resources = &*loaded});

  CHECK(kept.missing_terrain_textures == 0);
  REQUIRE(kept.terrain_textures.size() == loaded->terrain_textures.size());
  CHECK(kept.terrain_textures[1].pixels == loaded->terrain_textures[1].pixels);
  CHECK(kept.model_instance_count == loaded->model_instance_count);
  REQUIRE(kept.models.size() == loaded->models.size());
  CHECK(kept.models[0].transforms == loaded->models[0].transforms);
  CHECK(kept.models[0].textures == loaded->models[0].textures);
  // Le fichier absent l'est toujours : rien ne l'a prêté.
  CHECK(kept.unreadable_models == 1);
}
